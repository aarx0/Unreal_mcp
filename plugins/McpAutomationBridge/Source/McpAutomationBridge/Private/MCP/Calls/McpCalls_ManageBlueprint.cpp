// LINT-TOOL: manage_blueprint
// LINT-SCHEMA-DERIVED
// manage_blueprint as FMcpCall classes, now on schema-from-decls
// (docs/action-declarations.md) — the final classed family and the one
// DELEGATION-wired family. Each class AUTHORS its schema fragment in a S_<Suffix>()
// function; the published facade schema (Tools/McpTool_ManageBlueprint.cpp) folds
// those fragments and GetDecl() derives the validation decl from the same fragment
// via McpDeriveDecl(), so schema and decl are one source and cannot drift.
//
// Unlike the extracted families, the four route dispatchers cannot be retired —
// HandleBlueprintAction recurses into HandleBlueprintGraphAction/HandleSCSAction and
// is called externally by EditorFunctionHandlers.cpp, so all four survive as members.
// Each class Run() therefore delegates to its route's surviving dispatcher, passing
// EXACTLY the args the retired registration lambda passed (byte-behaviour-identical
// for canonical inputs); no branch body was extracted:
//   Core     -> HandleBlueprintAction(RequestId, "manage_blueprint", ...)
//   Graph    -> HandleBlueprintGraphAction(RequestId, "manage_blueprint", ...)
//   Widget   -> HandleManageWidgetAuthoringAction(RequestId, "manage_widget_authoring", ...)
//   CommonUi -> HandleCommonUiAction(RequestId, "manage_common_ui", ...)
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ManageBlueprint
{

// --- Schema fragments -------------------------------------------------------
// One S_<Suffix>() per action, authoring the params that action's route handler
// actually reads. Names came from the retired P_* decls; builder method +
// description were ported from the facade schema. McpDeriveDecl() reads the param
// kinds + required-set back out of these to build the transport validation decl.
//
// Drift corrected against the live dispatchers (BlueprintHandlers /
// BlueprintGraphHandlers / WidgetAuthoringHandlers / SCSHandlers): dropped decl
// params no reachable branch reads — get's spuriously-required actorName;
// connect_pins' Niagara emitterName/scriptType/inputName/fromExpression/
// toExpression; get_node_details' material expressionIndex; add_animation_track's
// manage_sequence sequencePath/bindingGuid/animSequencePath/startTime — and the
// stale top-level facade param attachTo (only read inside modify_scs 'operations'
// items). Added the handler-read aliases the facade lacked: candidates (legacy
// alias of blueprintCandidates), the SCS snake_case aliases, and source/
// targetNodeGuid. The five zero-param actions (bind_*/create_property_binding)
// author nothing.

// Core (HandleBlueprintAction, Action arg "manage_blueprint")

static void S_Create(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("parentClass"), TEXT("Path or name of the parent class."))
	 .String(TEXT("blueprintType"), TEXT("Path or name of the parent class."))
	 .FreeformObject(TEXT("properties"), TEXT(""))
	 .Bool(TEXT("waitForCompletion"), TEXT("create/create_blueprint: wait for a coalesced concurrent creation to finish before responding."))
	 .Required({TEXT("name")});
}

static void S_Get(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."));
}

static void S_Compile(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .Bool(TEXT("saveAfterCompile"), TEXT(""))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."));
}

static void S_AddComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .String(TEXT("componentType"), TEXT(""))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .Required({TEXT("componentType"), TEXT("componentName")});
}

static void S_SetDefault(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
	 // Discriminated value: populate exactly ONE typed field matching the target
	 // property's reflected type. structValue/arrayValue are the escape for structs,
	 // instanced subobjects ({"__class",...}) and arrays; ApplyJsonValueToProperty
	 // is the gate for whether the value fits the resolved property.
	 .Bool(TEXT("boolValue"), TEXT("Set a bool property."))
	 .Number(TEXT("intValue"), TEXT("Set an integer property."))
	 .Number(TEXT("floatValue"), TEXT("Set a float/double property."))
	 .String(TEXT("stringValue"), TEXT("Set a string / name / text / enum / object-reference-by-path property."))
	 .Object(TEXT("colorValue"), TEXT("Set an FLinearColor/FColor property (r,g,b,a, 0..1)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .Object(TEXT("vectorValue"), TEXT("Set an FVector property (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("structValue"), TEXT("Set a struct / instanced subobject ({\"__class\",...}) / map property."))
	 .Array(TEXT("arrayValue"), TEXT("Set an array property (items are structs/instanced objects)."), TEXT("object"))
	 .Required({TEXT("propertyName")});
}

static void S_GetDefault(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
	 .Required({TEXT("propertyName")});
}

static void S_ListFunctions(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."));
}

static void S_ModifyScs(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .ArrayOfObjects(TEXT("operations"), TEXT(""))
	 .Bool(TEXT("compile"), TEXT("Compile the blueprint(s) after the operation."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("operations")});
}

static void S_GetScs(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."));
}

static void S_AddScsComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprint_path"), TEXT("snake_case alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("component_class"), TEXT("snake_case alias of componentClass."))
	 .String(TEXT("componentClass"), TEXT(""))
	 .String(TEXT("component_name"), TEXT("snake_case alias of componentName."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .String(TEXT("parent_component"), TEXT("snake_case alias of parentComponent."))
	 .String(TEXT("parentComponent"), TEXT(""))
	 .String(TEXT("mesh_path"), TEXT("snake_case alias of meshPath."))
	 .String(TEXT("meshPath"), TEXT("Mesh asset path."))
	 .String(TEXT("material_path"), TEXT("snake_case alias of materialPath."))
	 .String(TEXT("materialPath"), TEXT("Material asset path."));
}

static void S_RemoveScsComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprint_path"), TEXT("snake_case alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("component_name"), TEXT("snake_case alias of componentName."))
	 .String(TEXT("componentName"), TEXT("Name of the component."));
}

static void S_ReparentScsComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprint_path"), TEXT("snake_case alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("component_name"), TEXT("snake_case alias of componentName."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .String(TEXT("new_parent"), TEXT("snake_case alias of newParent."))
	 .String(TEXT("newParent"), TEXT(""));
}

static void S_SetScsTransform(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprint_path"), TEXT("snake_case alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("component_name"), TEXT("snake_case alias of componentName."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x"), TEXT("X coordinate."))
			 .Number(TEXT("y"), TEXT("Y coordinate."))
			 .Number(TEXT("z"), TEXT("Z coordinate."))
			 .Required({TEXT("x"), TEXT("y"), TEXT("z")});
		})
	 .Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("pitch"), TEXT("Pitch."))
			 .Number(TEXT("yaw"), TEXT("Yaw."))
			 .Number(TEXT("roll"), TEXT("Roll."))
			 .Required({TEXT("pitch"), TEXT("yaw"), TEXT("roll")});
		})
	 .Object(TEXT("scale"), TEXT("3D scale (x, y, z)."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x"), TEXT("X scale."))
			 .Number(TEXT("y"), TEXT("Y scale."))
			 .Number(TEXT("z"), TEXT("Z scale."))
			 .Required({TEXT("x"), TEXT("y"), TEXT("z")});
		});
}

static void S_SetScsProperty(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprint_path"), TEXT("snake_case alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("component_name"), TEXT("snake_case alias of componentName."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .String(TEXT("property_name"), TEXT("snake_case alias of propertyName."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
	 // Discriminated value: populate exactly ONE typed field matching the target
	 // property's reflected type. structValue/arrayValue are the escape for structs,
	 // instanced subobjects ({"__class",...}) and arrays; ApplyJsonValueToProperty
	 // is the gate for whether the value fits the resolved property.
	 .Bool(TEXT("boolValue"), TEXT("Set a bool property."))
	 .Number(TEXT("intValue"), TEXT("Set an integer property."))
	 .Number(TEXT("floatValue"), TEXT("Set a float/double property."))
	 .String(TEXT("stringValue"), TEXT("Set a string / name / text / enum / object-reference-by-path property."))
	 .Object(TEXT("colorValue"), TEXT("Set an FLinearColor/FColor property (r,g,b,a, 0..1)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .Object(TEXT("vectorValue"), TEXT("Set an FVector property (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("structValue"), TEXT("Set a struct / instanced subobject ({\"__class\",...}) / map property."))
	 .Array(TEXT("arrayValue"), TEXT("Set an array property (items are structs/instanced objects)."), TEXT("object"));
}

static void S_EnsureExists(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .String(TEXT("parentClass"), TEXT("Path or name of the parent class."))
	 .Bool(TEXT("createIfMissing"), TEXT("ensure_exists: create the blueprint if it doesn't exist (default true)."));
}

static void S_ProbeHandle(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."));
}

static void S_AddVariable(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .String(TEXT("variableName"), TEXT("Name of the variable."))
	 .String(TEXT("variableType"),
		TEXT("Variable type (e.g., Boolean, Float, Integer, Vector, String, Object)"))
	 .FreeformObject(TEXT("defaultValue"), TEXT("Generic value (any type)."))
	 .String(TEXT("category"), TEXT(""))
	 .Bool(TEXT("isReplicated"), TEXT(""))
	 .Bool(TEXT("isPublic"), TEXT(""))
	 .Required({TEXT("variableName")});
}

static void S_RemoveVariable(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .String(TEXT("variableName"), TEXT("Name of the variable."))
	 .Required({TEXT("variableName")});
}

static void S_RenameVariable(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .String(TEXT("oldName"), TEXT(""))
	 .String(TEXT("newName"), TEXT("New name for renaming."))
	 .Required({TEXT("oldName"), TEXT("newName")});
}

static void S_AddFunction(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .String(TEXT("functionName"), TEXT("Name of the function."))
	 .String(TEXT("memberName"), TEXT(""))
	 .ArrayOfObjects(TEXT("inputs"), TEXT(""))
	 .ArrayOfObjects(TEXT("outputs"), TEXT(""))
	 .Bool(TEXT("isPublic"), TEXT(""))
	 .Bool(TEXT("override"), TEXT("add_function: implement an inherited BlueprintImplementableEvent/BlueprintNativeEvent instead of creating a new function."));
}

static void S_RemoveFunction(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .String(TEXT("functionName"), TEXT("Name of the function."));
}

static void S_AddEvent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .String(TEXT("eventType"), TEXT(""))
	 .String(TEXT("customEventName"), TEXT("Name of the event."))
	 .String(TEXT("eventName"), TEXT("Name of the event."))
	 .ArrayOfObjects(TEXT("parameters"), TEXT(""))
	 .Number(TEXT("posX"), TEXT("Node X position (graph units)."))
	 .Number(TEXT("posY"), TEXT("Node Y position (graph units)."))
	 .Object(TEXT("location"), TEXT("3D location (x, y, z)."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x"), TEXT("X coordinate."))
			 .Number(TEXT("y"), TEXT("Y coordinate."))
			 .Number(TEXT("z"), TEXT("Z coordinate."))
			 .Required({TEXT("x"), TEXT("y"), TEXT("z")});
		})
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""));
}

static void S_RemoveEvent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .String(TEXT("eventName"), TEXT("Name of the event."))
	 .Required({TEXT("eventName")});
}

static void S_AddConstructionScript(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."));
}

static void S_SetVariableMetadata(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .String(TEXT("variableName"), TEXT("Name of the variable."))
	 .FreeformObject(TEXT("metadata"), TEXT(""))
	 .Required({TEXT("variableName"), TEXT("metadata")});
}

static void S_SetMetadata(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .FreeformObject(TEXT("metadata"), TEXT(""))
	 .Required({TEXT("metadata")});
}

static void S_AddNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .String(TEXT("nodeType"), TEXT(""))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("functionName"), TEXT("Name of the function."))
	 .String(TEXT("variableName"), TEXT("Name of the variable."))
	 .String(TEXT("nodeName"), TEXT("add_node: name for a CustomEvent node (nodeType=CustomEvent)."))
	 .Number(TEXT("posX"), TEXT("Node X position (graph units)."))
	 .Number(TEXT("posY"), TEXT("Node Y position (graph units)."))
	 .String(TEXT("memberName"), TEXT(""))
	 .Bool(TEXT("autoConnect"), TEXT("add_node: auto-wire the new node's exec-in to an open event chain (default false)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Required({TEXT("nodeType")});
}

// BlueprintGraph (HandleBlueprintGraphAction, Action arg "manage_blueprint")

static void S_CreateNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("nodeType"), TEXT(""))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("posX"), TEXT("Node X position (graph units)."))
	 .Number(TEXT("y"), TEXT(""))
	 .Number(TEXT("posY"), TEXT("Node Y position (graph units)."))
	 .String(TEXT("variableName"), TEXT("Name of the variable."))
	 .String(TEXT("memberClass"), TEXT(""))
	 .String(TEXT("memberName"), TEXT(""))
	 .String(TEXT("eventName"), TEXT("Name of the event."))
	 .ArrayOfObjects(TEXT("parameters"), TEXT(""))
	 .String(TEXT("targetClass"), TEXT(""))
	 .String(TEXT("inputAxisName"), TEXT(""))
	 .Required({TEXT("assetPath"), TEXT("blueprintPath"), TEXT("nodeType")});
}

static void S_DeleteNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .Required({TEXT("assetPath"), TEXT("blueprintPath"), TEXT("nodeId")});
}

static void S_ConnectPins(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("sourceNodeId"), TEXT("ID of the source node."))
	 .String(TEXT("sourcePinName"), TEXT("Name of the source pin."))
	 .String(TEXT("targetNodeId"), TEXT("ID of the target node."))
	 .String(TEXT("targetPinName"), TEXT("Name of the target pin."))
	 .Bool(TEXT("autoConnect"), TEXT("add_node: auto-wire the new node's exec-in to an open event chain (default false)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .String(TEXT("sourceNodeGuid"), TEXT("connect_pins: source node GUID (alias of sourceNodeId)."))
	 .String(TEXT("targetNodeGuid"), TEXT("connect_pins: target node GUID (alias of targetNodeId)."))
	 .Required({TEXT("assetPath"), TEXT("blueprintPath"), TEXT("sourceNodeId"), TEXT("sourcePinName"), TEXT("targetNodeId"), TEXT("targetPinName"), TEXT("sourceNodeGuid"), TEXT("targetNodeGuid")});
}

static void S_BreakPinLinks(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .String(TEXT("pinName"), TEXT("Name of the pin."))
	 .Required({TEXT("assetPath"), TEXT("blueprintPath"), TEXT("nodeId"), TEXT("pinName")});
}

static void S_SetNodeProperty(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
	 .String(TEXT("stringValue"), TEXT("Property value (node properties like Comment are string-serialized)."))
	 .Required({TEXT("assetPath"), TEXT("blueprintPath"), TEXT("nodeId"), TEXT("propertyName")});
}

static void S_CreateRerouteNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("posX"), TEXT("Node X position (graph units)."))
	 .Number(TEXT("y"), TEXT(""))
	 .Number(TEXT("posY"), TEXT("Node Y position (graph units)."))
	 .Required({TEXT("assetPath"), TEXT("blueprintPath")});
}

static void S_GetNodeDetails(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .Required({TEXT("assetPath"), TEXT("blueprintPath"), TEXT("nodeId")});
}

static void S_GetGraphDetails(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .Bool(TEXT("includePinLinks"), TEXT("get_graph_details: include per-node pins with defaults and \"<nodeGuid>:<pinName>\" link refs (always on inside get_transition_rule_graph's ruleGraph)."))
	 .Required({TEXT("assetPath"), TEXT("blueprintPath")});
}

static void S_GetPinDetails(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .String(TEXT("pinName"), TEXT("Name of the pin."))
	 .Required({TEXT("assetPath"), TEXT("blueprintPath"), TEXT("nodeId")});
}

static void S_ListNodeTypes(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."));
}

static void S_SetPinDefaultValue(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .String(TEXT("pinName"), TEXT("Name of the pin."))
	 .String(TEXT("stringValue"), TEXT("Pin default value (UE serializes pin defaults as strings)."))
	 .Required({TEXT("assetPath"), TEXT("blueprintPath"), TEXT("nodeId"), TEXT("pinName")});
}

static void S_ArrangeGraph(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .Required({TEXT("assetPath"), TEXT("blueprintPath")});
}

static void S_ListAnimbpGraphs(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .Required({TEXT("assetPath"), TEXT("blueprintPath")});
}

static void S_GetTransitionRuleGraph(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("fromState"), TEXT("get_transition_rule_graph: source state name (paired with toState)."))
	 .String(TEXT("toState"), TEXT("get_transition_rule_graph: target state name (paired with fromState)."))
	 .String(TEXT("transitionName"), TEXT("get_transition_rule_graph: transition node title or rule-graph name, as an alternative to fromState/toState."))
	 .String(TEXT("stateMachine"), TEXT("get_transition_rule_graph: state machine graph name to search; all state machines searched when omitted."))
	 .Number(TEXT("transitionIndex"), TEXT("get_transition_rule_graph: 0-based pick when parallel transitions between the same states match (see matchCount in the response)."))
	 .Required({TEXT("assetPath"), TEXT("blueprintPath")});
}

// WidgetAuthoring (HandleManageWidgetAuthoringAction, Action arg "manage_widget_authoring")

static void S_CreateWidgetBlueprint(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset (create_widget_blueprint, create_common_button_style/create_common_text_style, create_*_menu, etc.); falls back to savePath, then folder."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("folder"), TEXT("Destination folder fallback after path/savePath."))
	 .String(TEXT("parentClass"), TEXT("Path or name of the parent class."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("name")});
}

static void S_SetWidgetParentClass(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("parentClass"), TEXT("Path or name of the parent class."))
	 .Required({TEXT("widgetPath"), TEXT("parentClass")});
}

static void S_AddCanvasPanel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddHorizontalBox(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddVerticalBox(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddOverlay(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddGridPanel(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddUniformGrid(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Object(TEXT("slotPadding"), TEXT("add_uniform_grid: per-slot padding."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("left")).Number(TEXT("top")).Number(TEXT("right")).Number(TEXT("bottom"));
		})
	 .Number(TEXT("minDesiredSlotWidth"), TEXT("add_uniform_grid: minimum slot width."))
	 .Number(TEXT("minDesiredSlotHeight"), TEXT("add_uniform_grid: minimum slot height."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddWrapBox(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Object(TEXT("innerSlotPadding"), TEXT("add_wrap_box: inner slot padding."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y"));
		})
	 .Number(TEXT("wrapSize"), TEXT("add_wrap_box: explicit wrap size (UE 5.1+)."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddScrollBox(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .String(TEXT("orientation"), TEXT("add_scroll_box: \"Vertical\" (default) or \"Horizontal\"."))
	 .String(TEXT("scrollBarVisibility"), TEXT("add_scroll_box: Visible, Collapsed, or Hidden."))
	 .Bool(TEXT("alwaysShowScrollbar"), TEXT("add_scroll_box: force the scrollbar visible."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddSizeBox(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Number(TEXT("widthOverride"), TEXT("add_size_box: fixed width override."))
	 .Number(TEXT("heightOverride"), TEXT("add_size_box: fixed height override."))
	 .Number(TEXT("minDesiredWidth"), TEXT("add_size_box: minimum desired width."))
	 .Number(TEXT("minDesiredHeight"), TEXT("add_size_box: minimum desired height."))
	 .Number(TEXT("maxDesiredWidth"), TEXT("add_size_box: maximum desired width."))
	 .Number(TEXT("maxDesiredHeight"), TEXT("add_size_box: maximum desired height."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddScaleBox(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .String(TEXT("stretch"), TEXT("add_scale_box: stretch mode (None, Fill, ScaleToFit, ScaleToFitX, ScaleToFitY, ScaleToFill, UserSpecified)."))
	 .Number(TEXT("userSpecifiedScale"), TEXT("add_scale_box: scale used when stretch is UserSpecified."))
	 .String(TEXT("stretchDirection"), TEXT("add_scale_box: Both, DownOnly, or UpOnly."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddBorder(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Object(TEXT("brushColor"), TEXT("add_border: border brush color, 0-1 rgba."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .Object(TEXT("contentColorAndOpacity"), TEXT("add_border: content tint, 0-1 rgba."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .Object(TEXT("padding"), TEXT("set_padding: padding (alternative to pad/padLeft/padTop/padRight/padBottom)."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("left")).Number(TEXT("top")).Number(TEXT("right")).Number(TEXT("bottom"));
		})
	 .Required({TEXT("widgetPath")});
}

static void S_AddTextBlock(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .String(TEXT("text"), TEXT("Initial text (add_common_text, add_text_block, add_rich_text_block, add_interaction_prompt, add_widget_component)."))
	 .Number(TEXT("fontSize"), TEXT("Font size (add_text_block, set_font)."))
	 .Object(TEXT("colorAndOpacity"), TEXT("Widget tint color, 0-1 components (add_text_block, add_image, add_button)."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a"));
		})
	 .Bool(TEXT("autoWrap"), TEXT("add_text_block: enable auto text wrapping."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddRichTextBlock(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .String(TEXT("text"), TEXT("Initial text (add_common_text, add_text_block, add_rich_text_block, add_interaction_prompt, add_widget_component)."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddImage(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .String(TEXT("texturePath"), TEXT("add_image: texture asset path for the brush."))
	 .Object(TEXT("colorAndOpacity"), TEXT("Widget tint color, 0-1 components (add_text_block, add_image, add_button)."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a"));
		})
	 .Required({TEXT("widgetPath")});
}

static void S_AddButton(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Bool(TEXT("isEnabled"), TEXT("add_button: initial enabled state."))
	 .Object(TEXT("colorAndOpacity"), TEXT("Widget tint color, 0-1 components (add_text_block, add_image, add_button)."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a"));
		})
	 .Required({TEXT("widgetPath")});
}

static void S_AddCheckBox(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Bool(TEXT("isChecked"), TEXT("add_check_box: initial checked state."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddSlider(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Number(TEXT("floatValue"), TEXT("add_slider: initial value (default 0.5)."))
	 .Number(TEXT("minValue"), TEXT("add_slider/add_spin_box: minimum value."))
	 .Number(TEXT("maxValue"), TEXT("add_slider/add_spin_box: maximum value."))
	 .Number(TEXT("stepSize"), TEXT("add_slider: step increment."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddProgressBar(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Number(TEXT("percent"), TEXT("add_progress_bar: initial fill percent (0-1)."))
	 .Object(TEXT("fillColorAndOpacity"), TEXT("add_progress_bar: fill bar color, 0-1 components."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a"));
		})
	 .Bool(TEXT("isMarquee"), TEXT("add_progress_bar: indeterminate marquee mode."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddTextInput(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Bool(TEXT("multiLine"), TEXT("add_text_input: use a multi-line editable text box."))
	 .String(TEXT("hintText"), TEXT("add_text_input: placeholder hint text."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddComboBox(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Array(TEXT("options"), TEXT("add_combo_box: option strings to populate."))
	 .String(TEXT("selectedOption"), TEXT("add_combo_box: initially selected option."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddSpinBox(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Number(TEXT("floatValue"), TEXT("add_spin_box: initial value (default 0)."))
	 .Number(TEXT("minValue"), TEXT("add_slider/add_spin_box: minimum value."))
	 .Number(TEXT("maxValue"), TEXT("add_slider/add_spin_box: maximum value."))
	 .Number(TEXT("delta"), TEXT("add_spin_box: increment/decrement step."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddListView(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddTreeView(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("widgetPath")});
}

static void S_SetAnchor(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .Object(TEXT("anchorMin"), TEXT("set_anchor: minimum anchor (alternative to anchorMinX/anchorMinY)."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y"));
		})
	 .Object(TEXT("anchorMax"), TEXT("set_anchor: maximum anchor (alternative to anchorMaxX/anchorMaxY)."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y"));
		})
	 .Number(TEXT("anchorMinX"), TEXT("set_anchor: anchor minimum X (0..1)."))
	 .Number(TEXT("anchorMinY"), TEXT("set_anchor: anchor minimum Y (0..1)."))
	 .Number(TEXT("anchorMaxX"), TEXT("set_anchor: anchor maximum X (0..1)."))
	 .Number(TEXT("anchorMaxY"), TEXT("set_anchor: anchor maximum Y (0..1)."))
	 .String(TEXT("preset"), TEXT("set_anchor: named anchor preset (TopLeft, TopCenter, TopRight, CenterLeft, Center, CenterRight, BottomLeft, BottomCenter, BottomRight, StretchHorizontal, StretchVertical, StretchAll)."))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_SetAlignment(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .Object(TEXT("alignment"), TEXT("set_alignment: pivot (alternative to alignmentX/alignmentY)."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y"));
		})
	 .Number(TEXT("alignmentX"), TEXT("set_alignment: pivot X (0..1)."))
	 .Number(TEXT("alignmentY"), TEXT("set_alignment: pivot Y (0..1)."))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_SetPosition(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .Object(TEXT("position"), TEXT("set_position: Canvas slot position (alternative to posX/posY)."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y"));
		})
	 .Number(TEXT("posX"), TEXT("Node X position (graph units)."))
	 .Number(TEXT("posY"), TEXT("Node Y position (graph units)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_SetSize(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .FreeformObject(TEXT("size"), TEXT("Either a number (add_crosshair/add_minimap/add_ammo_counter: icon size) or an {x,y} object (set_size: canvas slot pixel size)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Bool(TEXT("fill"), TEXT("set_size: stretch a HBox/VBox child to fill (vs Automatic)."))
	 .Number(TEXT("fillWeight"), TEXT("set_size: fill weight when fill is true."))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_SetPadding(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .Object(TEXT("padding"), TEXT("set_padding: padding (alternative to pad/padLeft/padTop/padRight/padBottom)."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("left")).Number(TEXT("top")).Number(TEXT("right")).Number(TEXT("bottom"));
		})
	 .Number(TEXT("pad"), TEXT("set_padding: uniform padding on all sides."))
	 .Number(TEXT("padLeft"), TEXT("set_padding: left padding."))
	 .Number(TEXT("padTop"), TEXT("set_padding: top padding."))
	 .Number(TEXT("padRight"), TEXT("set_padding: right padding."))
	 .Number(TEXT("padBottom"), TEXT("set_padding: bottom padding."))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_SetZOrder(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .Number(TEXT("zOrder"), TEXT("set_z_order: Canvas slot Z-order."))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_SetRenderTransform(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .Object(TEXT("translation"), TEXT("set_render_transform: 2D translation."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y"));
		})
	 .Object(TEXT("scale"), TEXT("3D scale (x, y, z)."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x"), TEXT("X scale."))
			 .Number(TEXT("y"), TEXT("Y scale."))
			 .Number(TEXT("z"), TEXT("Z scale."))
			 .Required({TEXT("x"), TEXT("y"), TEXT("z")});
		})
	 .Object(TEXT("shear"), TEXT("set_render_transform: 2D shear."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y"));
		})
	 .Number(TEXT("angle"), TEXT("set_render_transform: rotation angle in degrees."))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_SetVisibility(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("visibility"), TEXT("set_visibility: Visible, Hidden, Collapsed, HitTestInvisible, or SelfHitTestInvisible."))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_SetStyle(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
	 .FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
	 .String(TEXT("style"), TEXT("set_style: legacy value alias for propertyName=\"Style\" when propertyName is omitted."))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_SetClipping(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("clipping"), TEXT("set_clipping: Inherit, ClipToBounds, ClipToBoundsWithoutIntersecting, ClipToBoundsAlways, or OnDemand."))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_CreatePropertyBinding(FMcpSchemaBuilder&) {}

static void S_BindText(FMcpSchemaBuilder&) {}

static void S_BindVisibility(FMcpSchemaBuilder&) {}

static void S_BindColor(FMcpSchemaBuilder&) {}

static void S_BindEnabled(FMcpSchemaBuilder&) {}

static void S_BindOnClicked(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("delegateName"), TEXT("Multicast delegate name (bind_on_clicked/bind_on_hovered default OnClicked/OnHovered; bind_on_value_changed default OnValueChanged; bind_event_to_delegate)."))
	 .String(TEXT("targetFunction"), TEXT("bind_on_clicked/bind_on_hovered: self UFUNCTION to call (alias functionName)."))
	 .String(TEXT("functionName"), TEXT("Name of the function."))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_BindOnHovered(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("delegateName"), TEXT("Multicast delegate name (bind_on_clicked/bind_on_hovered default OnClicked/OnHovered; bind_on_value_changed default OnValueChanged; bind_event_to_delegate)."))
	 .String(TEXT("targetFunction"), TEXT("bind_on_clicked/bind_on_hovered: self UFUNCTION to call (alias functionName)."))
	 .String(TEXT("functionName"), TEXT("Name of the function."))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_BindOnValueChanged(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("delegateName"), TEXT("Multicast delegate name (bind_on_clicked/bind_on_hovered default OnClicked/OnHovered; bind_on_value_changed default OnValueChanged; bind_event_to_delegate)."))
	 .String(TEXT("targetText"), TEXT("bind_on_value_changed: text widget to live-update with the new value."))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_BindEventToDelegate(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("eventName"), TEXT("Name of the event."))
	 .String(TEXT("ownerPin"), TEXT("bind_event_to_delegate: output pin on the source event exposing the delegate's owner object."))
	 .String(TEXT("delegateName"), TEXT("Multicast delegate name (bind_on_clicked/bind_on_hovered default OnClicked/OnHovered; bind_on_value_changed default OnValueChanged; bind_event_to_delegate)."))
	 .String(TEXT("handlerEventName"), TEXT("bind_event_to_delegate: name for the generated handler custom event (default On_<delegateName>)."))
	 .String(TEXT("handlerTargetWidget"), TEXT("bind_event_to_delegate: optional widget the handler acts on."))
	 .String(TEXT("handlerFunction"), TEXT("bind_event_to_delegate: optional function the handler calls."))
	 .Required({TEXT("widgetPath"), TEXT("eventName"), TEXT("ownerPin"), TEXT("delegateName")});
}

static void S_CreateWidgetAnimation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("animationName"), TEXT("Widget animation name (create_widget_animation default NewAnimation; add_animation_track/get_animation_info/delete_animation/etc.)."))
	 .Number(TEXT("duration"), TEXT("create_widget_animation: length in seconds (default 1.0)."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddAnimationTrack(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("animationName"), TEXT("Widget animation name (create_widget_animation default NewAnimation; add_animation_track/get_animation_info/delete_animation/etc.)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
	 .Required({TEXT("widgetPath"), TEXT("animationName"), TEXT("slotName")});
}

static void S_AddAnimationKeyframe(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("animationName"), TEXT("Widget animation name (create_widget_animation default NewAnimation; add_animation_track/get_animation_info/delete_animation/etc.)."))
	 .Number(TEXT("time"), TEXT("add_animation_keyframe: keyframe time in seconds."))
	 .FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
	 .Required({TEXT("widgetPath"), TEXT("animationName")});
}

static void S_SetAnimationLoop(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("animationName"), TEXT("Widget animation name (create_widget_animation default NewAnimation; add_animation_track/get_animation_info/delete_animation/etc.)."))
	 .Bool(TEXT("loop"), TEXT("set_animation_loop: whether the animation loops."))
	 .Number(TEXT("loopCount"), TEXT("set_animation_loop: loop count (0 = infinite)."))
	 .Required({TEXT("widgetPath"), TEXT("animationName")});
}

static void S_CreateMainMenu(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("title"), TEXT("create_main_menu: title text (default \"Main Menu\")."))
	 .Required({TEXT("widgetPath")});
}

static void S_CreatePauseMenu(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .Required({TEXT("widgetPath")});
}

static void S_CreateSettingsMenu(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset (create_widget_blueprint, create_common_button_style/create_common_text_style, create_*_menu, etc.); falls back to savePath, then folder."))
	 .String(TEXT("folder"), TEXT("Destination folder fallback after path/savePath."));
}

static void S_CreateLoadingScreen(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset (create_widget_blueprint, create_common_button_style/create_common_text_style, create_*_menu, etc.); falls back to savePath, then folder."))
	 .String(TEXT("folder"), TEXT("Destination folder fallback after path/savePath."));
}

static void S_CreateHudWidget(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddHealthBar(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("parentName"), TEXT("Name of an existing panel widget to attach into (add_widget_component, add_health_bar/add_crosshair/add_minimap/etc.)."))
	 .Number(TEXT("x"), TEXT(""))
	 .Number(TEXT("y"), TEXT(""))
	 .Number(TEXT("width"), TEXT("add_health_bar: bar width in pixels."))
	 .Number(TEXT("height"), TEXT("add_health_bar: bar height in pixels."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddAmmoCounter(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("parentName"), TEXT("Name of an existing panel widget to attach into (add_widget_component, add_health_bar/add_crosshair/add_minimap/etc.)."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddMinimap(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .Number(TEXT("iconSize"), TEXT("Icon size in pixels (add_crosshair/add_minimap)."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddCrosshair(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("parentName"), TEXT("Name of an existing panel widget to attach into (add_widget_component, add_health_bar/add_crosshair/add_minimap/etc.)."))
	 .Number(TEXT("iconSize"), TEXT("Icon size in pixels (add_crosshair/add_minimap)."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddCompass(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddInteractionPrompt(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("text"), TEXT("Initial text (add_common_text, add_text_block, add_rich_text_block, add_interaction_prompt, add_widget_component)."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddObjectiveTracker(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddDamageIndicator(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .Required({TEXT("widgetPath")});
}

static void S_CreateInventoryUi(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset (create_widget_blueprint, create_common_button_style/create_common_text_style, create_*_menu, etc.); falls back to savePath, then folder."))
	 .String(TEXT("folder"), TEXT("Destination folder fallback after path/savePath."))
	 .Number(TEXT("columns"), TEXT("create_inventory_ui/create_shop_ui: grid column count."))
	 .Number(TEXT("rows"), TEXT("create_inventory_ui: grid row count."));
}

static void S_CreateDialogWidget(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset (create_widget_blueprint, create_common_button_style/create_common_text_style, create_*_menu, etc.); falls back to savePath, then folder."))
	 .String(TEXT("folder"), TEXT("Destination folder fallback after path/savePath."));
}

static void S_CreateRadialMenu(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset (create_widget_blueprint, create_common_button_style/create_common_text_style, create_*_menu, etc.); falls back to savePath, then folder."))
	 .String(TEXT("folder"), TEXT("Destination folder fallback after path/savePath."))
	 .Number(TEXT("segments"), TEXT("create_radial_menu: number of radial segments."));
}

static void S_GetWidgetInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .Array(TEXT("propertyNames"), TEXT("get_widget_info + widgetName: only these properties (explicit null when missing); omit for all exportable properties."))
	 .Required({TEXT("widgetPath")});
}

static void S_PreviewWidget(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .Required({TEXT("widgetPath")});
}

static void S_RemoveWidget(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_ReparentWidget(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("newParent"), TEXT(""))
	 .Required({TEXT("widgetPath"), TEXT("slotName"), TEXT("newParent")});
}

static void S_RenameWidget(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("oldName"), TEXT(""))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("newName"), TEXT("New name for renaming."))
	 .Required({TEXT("widgetPath"), TEXT("oldName"), TEXT("newName")});
}

static void S_GetWidgetSlotInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_ReplaceWidgetClass(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("newWidgetClass"), TEXT("replace_widget_class: class to swap the named widget to."))
	 .Required({TEXT("widgetPath"), TEXT("slotName"), TEXT("newWidgetClass")});
}

static void S_AddWidget(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("widgetClass"), TEXT("Widget class for create_widget_blueprint / set_widget_parent_class."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Required({TEXT("widgetPath"), TEXT("widgetClass"), TEXT("name")});
}

static void S_WrapRoot(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("panelType"), TEXT("wrap_root: new root panel class (Overlay, VerticalBox, HorizontalBox, CanvasPanel)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .Required({TEXT("widgetPath")});
}

static void S_ShowWidget(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("widgetId"), TEXT("show_widget: special id (\"notification\") or target identifier."))
	 .String(TEXT("message"), TEXT("show_widget: notification text when widgetId is \"notification\"."))
	 .String(TEXT("name"), TEXT("Name identifier."));
}

static void S_AddWidgetComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("componentType"), TEXT(""))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .String(TEXT("parentName"), TEXT("Name of an existing panel widget to attach into (add_widget_component, add_health_bar/add_crosshair/add_minimap/etc.)."))
	 .Number(TEXT("positionX"), TEXT("add_widget_component: X position in pixels."))
	 .Number(TEXT("positionY"), TEXT("add_widget_component: Y position in pixels."))
	 .Number(TEXT("sizeX"), TEXT("add_spacer/add_widget_component: width in pixels."))
	 .Number(TEXT("sizeY"), TEXT("add_spacer/add_widget_component: height in pixels."))
	 .String(TEXT("text"), TEXT("Initial text (add_common_text, add_text_block, add_rich_text_block, add_interaction_prompt, add_widget_component)."))
	 .Required({TEXT("widgetPath"), TEXT("componentType")});
}

static void S_AddSafeZone(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddSpacer(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Number(TEXT("sizeX"), TEXT("add_spacer/add_widget_component: width in pixels."))
	 .Number(TEXT("sizeY"), TEXT("add_spacer/add_widget_component: height in pixels."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddWidgetSwitcher(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .Number(TEXT("activeIndex"), TEXT("add_widget_switcher: initially active child index."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("widgetPath")});
}

static void S_SetFont(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("font"), TEXT("set_font: font asset path."))
	 .Number(TEXT("fontSize"), TEXT("Font size (add_text_block, set_font)."))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_SetMargin(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .Number(TEXT("left"), TEXT("set_margin: left margin."))
	 .Number(TEXT("top"), TEXT("set_margin: top margin."))
	 .Number(TEXT("right"), TEXT("set_margin: right margin."))
	 .Number(TEXT("bottom"), TEXT("set_margin: bottom margin."))
	 .Required({TEXT("widgetPath"), TEXT("slotName")});
}

static void S_CreateWidgetStyle(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("styleName"), TEXT("create_widget_style/apply_style_to_widget: style variable name."))
	 .String(TEXT("styleType"), TEXT("create_widget_style: style category (default \"Text\")."))
	 .Required({TEXT("widgetPath")});
}

static void S_ApplyStyleToWidget(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("styleName"), TEXT("create_widget_style/apply_style_to_widget: style variable name."))
	 .Required({TEXT("widgetPath"), TEXT("slotName"), TEXT("styleName")});
}

static void S_SetWidgetBinding(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("targetWidget"), TEXT("set_widget_binding: name of the widget to bind (NOT_IMPLEMENTED — validated only)."))
	 .String(TEXT("property"), TEXT("set_widget_binding: property name to bind (NOT_IMPLEMENTED — validated only)."))
	 .String(TEXT("functionName"), TEXT("Name of the function."))
	 .Required({TEXT("widgetPath"), TEXT("targetWidget"), TEXT("property")});
}

static void S_BindLocalizedText(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("stringTableId"), TEXT("bind_localized_text: string table asset path."))
	 .String(TEXT("stringKey"), TEXT("bind_localized_text: key within the string table."))
	 .Required({TEXT("widgetPath"), TEXT("slotName"), TEXT("stringTableId"), TEXT("stringKey")});
}

static void S_SetLocalizationKey(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("namespace"), TEXT("set_localization_key: localization namespace (default \"Game\")."))
	 .String(TEXT("key"), TEXT("set_localization_key: localization key."))
	 .Required({TEXT("widgetPath"), TEXT("slotName"), TEXT("key")});
}

static void S_GetAnimationInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("animationName"), TEXT("Widget animation name (create_widget_animation default NewAnimation; add_animation_track/get_animation_info/delete_animation/etc.)."))
	 .Required({TEXT("widgetPath")});
}

static void S_SetAnimationSpeed(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("animationName"), TEXT("Widget animation name (create_widget_animation default NewAnimation; add_animation_track/get_animation_info/delete_animation/etc.)."))
	 .Number(TEXT("speed"), TEXT("set_animation_speed: playback speed multiplier."))
	 .Required({TEXT("widgetPath"), TEXT("animationName")});
}

static void S_DeleteAnimation(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("animationName"), TEXT("Widget animation name (create_widget_animation default NewAnimation; add_animation_track/get_animation_info/delete_animation/etc.)."))
	 .Required({TEXT("widgetPath"), TEXT("animationName")});
}

static void S_CreateCreditsScreen(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset (create_widget_blueprint, create_common_button_style/create_common_text_style, create_*_menu, etc.); falls back to savePath, then folder."))
	 .String(TEXT("folder"), TEXT("Destination folder fallback after path/savePath."));
}

static void S_CreateShopUi(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset (create_widget_blueprint, create_common_button_style/create_common_text_style, create_*_menu, etc.); falls back to savePath, then folder."))
	 .String(TEXT("folder"), TEXT("Destination folder fallback after path/savePath."))
	 .Number(TEXT("columns"), TEXT("create_inventory_ui/create_shop_ui: grid column count."));
}

static void S_AddQuestTracker(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .Required({TEXT("widgetPath")});
}

// CommonUi (HandleCommonUiAction, Action arg "manage_common_ui")

static void S_AddCommonButton(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("buttonClass"), TEXT("add_common_button: concrete UCommonButtonBase subclass path."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .String(TEXT("styleClass"), TEXT("Common UI style class (add_common_button/add_common_text/add_common_border/set_common_button_style/set_common_text_style)."))
	 .Required({TEXT("widgetPath"), TEXT("buttonClass")});
}

static void S_AddCommonText(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .String(TEXT("text"), TEXT("Initial text (add_common_text, add_text_block, add_rich_text_block, add_interaction_prompt, add_widget_component)."))
	 .String(TEXT("styleClass"), TEXT("Common UI style class (add_common_button/add_common_text/add_common_border/set_common_button_style/set_common_text_style)."))
	 .Required({TEXT("widgetPath")});
}

static void S_AddCommonBorder(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("parentSlot"), TEXT("Name of the parent panel widget to add the new widget under."))
	 .String(TEXT("styleClass"), TEXT("Common UI style class (add_common_button/add_common_text/add_common_border/set_common_button_style/set_common_text_style)."))
	 .Required({TEXT("widgetPath")});
}

static void S_SetCommonButtonStyle(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("styleClass"), TEXT("Common UI style class (add_common_button/add_common_text/add_common_border/set_common_button_style/set_common_text_style)."))
	 .Required({TEXT("widgetPath"), TEXT("widgetName"), TEXT("styleClass")});
}

static void S_SetCommonTextStyle(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("styleClass"), TEXT("Common UI style class (add_common_button/add_common_text/add_common_border/set_common_button_style/set_common_text_style)."))
	 .Required({TEXT("widgetPath"), TEXT("widgetName"), TEXT("styleClass")});
}

static void S_SetCommonButtonInputAction(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("dataTable"), TEXT("set_common_button_input_action: DataTable asset path (CommonInputActionDataBase row struct)."))
	 .String(TEXT("rowName"), TEXT("set_common_button_input_action: row name within dataTable."))
	 .Required({TEXT("widgetPath"), TEXT("widgetName"), TEXT("dataTable"), TEXT("rowName")});
}

static void S_CreateCommonButtonStyle(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset (create_widget_blueprint, create_common_button_style/create_common_text_style, create_*_menu, etc.); falls back to savePath, then folder."))
	 .Required({TEXT("name")});
}

static void S_CreateCommonTextStyle(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Destination folder for a created asset (create_widget_blueprint, create_common_button_style/create_common_text_style, create_*_menu, etc.); falls back to savePath, then folder."))
	 .Required({TEXT("name")});
}

// --- Classes ----------------------------------------------------------------
// Flags authored per action. RequiresEditor on Core + Graph: BlueprintHandlers.cpp
// (HandleBlueprintAction, whole-body #if WITH_EDITOR) and BlueprintGraphHandlers.cpp
// (HandleBlueprintGraphAction, whole-body #if WITH_EDITOR) are editor-gated. NOT on
// Widget (WidgetAuthoringHandlers.cpp carries no editor gate) or CommonUi
// (CommonUIHandlers.cpp gates on MCP_HAS_COMMON_UI, not WITH_EDITOR) — flagging
// either would newly reject the GEditor-less runs the shim served. Mutating on every
// action except the pure get_*/list_*/preview_*/probe_* reads.
//
// MCP_BP_ACTION_CALL delegates to a surviving 4-arg dispatcher, passing the route's
// Action-arg literal (the manufactured tool-name for Widget/CommonUi, the
// "manage_blueprint" tool name for Core/Graph — each dispatcher gates on it and
// reads the real sub-action from the payload). No body is owned here. AppendSchema
// forwards to the action's S_<Suffix>() fragment; GetDecl derives from the same.

#define MCP_BP_ACTION_CALL(ClassSuffix, ActionLiteral, HandlerFn, ActionArg, Flags) \
class FMcpCall_ManageBlueprint_##ClassSuffix final : public FMcpCall                     \
{                                                                                        \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }        \
	const FMcpCallDecl& GetDecl() const override                                          \
	{                                                                                    \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_blueprint"),              \
			TEXT(ActionLiteral), (Flags), &S_##ClassSuffix);                                 \
		return D;                                                                          \
	}                                                                                    \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                 \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override \
	{                                                                                    \
		return S.HandlerFn(RequestId, TEXT(ActionArg), Payload, Socket);                   \
	}                                                                                    \
};

// Core (HandleBlueprintAction, Action arg "manage_blueprint")
MCP_BP_ACTION_CALL(Create, "create", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(Get, "get", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(Compile, "compile", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddComponent, "add_component", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetDefault, "set_default", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(GetDefault, "get_default", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(ListFunctions, "list_functions", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(ModifyScs, "modify_scs", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(GetScs, "get_scs", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(AddScsComponent, "add_scs_component", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(RemoveScsComponent, "remove_scs_component", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(ReparentScsComponent, "reparent_scs_component", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetScsTransform, "set_scs_transform", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetScsProperty, "set_scs_property", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(EnsureExists, "ensure_exists", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(ProbeHandle, "probe_handle", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(AddVariable, "add_variable", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(RemoveVariable, "remove_variable", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(RenameVariable, "rename_variable", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddFunction, "add_function", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(RemoveFunction, "remove_function", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddEvent, "add_event", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(RemoveEvent, "remove_event", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddConstructionScript, "add_construction_script", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetVariableMetadata, "set_variable_metadata", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetMetadata, "set_blueprint_metadata", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddNode, "add_node", HandleBlueprintAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// BlueprintGraph (HandleBlueprintGraphAction, Action arg "manage_blueprint")
MCP_BP_ACTION_CALL(CreateNode, "create_node", HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(DeleteNode, "delete_node", HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(ConnectPins, "connect_pins", HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BreakPinLinks, "break_pin_links", HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetNodeProperty, "set_node_property", HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateRerouteNode, "create_reroute_node", HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(GetNodeDetails, "get_node_details", HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(GetGraphDetails, "get_graph_details", HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(GetPinDetails, "get_pin_details", HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(ListNodeTypes, "list_node_types", HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(SetPinDefaultValue, "set_pin_default_value", HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(ArrangeGraph, "arrange_graph", HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(ListAnimbpGraphs, "list_animbp_graphs", HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)
MCP_BP_ACTION_CALL(GetTransitionRuleGraph, "get_transition_rule_graph", HandleBlueprintGraphAction, "manage_blueprint", EMcpCallFlags::RequiresEditor)

// WidgetAuthoring (HandleManageWidgetAuthoringAction, Action arg "manage_widget_authoring")
MCP_BP_ACTION_CALL(CreateWidgetBlueprint, "create_widget_blueprint", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetWidgetParentClass, "set_widget_parent_class", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddCanvasPanel, "add_canvas_panel", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddHorizontalBox, "add_horizontal_box", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddVerticalBox, "add_vertical_box", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddOverlay, "add_overlay", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddGridPanel, "add_grid_panel", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddUniformGrid, "add_uniform_grid", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddWrapBox, "add_wrap_box", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddScrollBox, "add_scroll_box", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddSizeBox, "add_size_box", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddScaleBox, "add_scale_box", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddBorder, "add_border", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddTextBlock, "add_text_block", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddRichTextBlock, "add_rich_text_block", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddImage, "add_image", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddButton, "add_button", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddCheckBox, "add_check_box", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddSlider, "add_slider", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddProgressBar, "add_progress_bar", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddTextInput, "add_text_input", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddComboBox, "add_combo_box", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddSpinBox, "add_spin_box", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddListView, "add_list_view", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddTreeView, "add_tree_view", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetAnchor, "set_anchor", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetAlignment, "set_alignment", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetPosition, "set_position", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetSize, "set_size", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetPadding, "set_padding", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetZOrder, "set_z_order", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetRenderTransform, "set_render_transform", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetVisibility, "set_visibility", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetStyle, "set_style", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetClipping, "set_clipping", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreatePropertyBinding, "create_property_binding", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindText, "bind_text", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindVisibility, "bind_visibility", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindColor, "bind_color", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindEnabled, "bind_enabled", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindOnClicked, "bind_on_clicked", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindOnHovered, "bind_on_hovered", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindOnValueChanged, "bind_on_value_changed", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindEventToDelegate, "bind_event_to_delegate", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateWidgetAnimation, "create_widget_animation", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddAnimationTrack, "add_animation_track", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddAnimationKeyframe, "add_animation_keyframe", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetAnimationLoop, "set_animation_loop", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateMainMenu, "create_main_menu", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreatePauseMenu, "create_pause_menu", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateSettingsMenu, "create_settings_menu", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateLoadingScreen, "create_loading_screen", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateHudWidget, "create_hud_widget", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddHealthBar, "add_health_bar", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddAmmoCounter, "add_ammo_counter", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddMinimap, "add_minimap", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddCrosshair, "add_crosshair", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddCompass, "add_compass", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddInteractionPrompt, "add_interaction_prompt", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddObjectiveTracker, "add_objective_tracker", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddDamageIndicator, "add_damage_indicator", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateInventoryUi, "create_inventory_ui", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateDialogWidget, "create_dialog_widget", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateRadialMenu, "create_radial_menu", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(GetWidgetInfo, "get_widget_info", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::None)
MCP_BP_ACTION_CALL(PreviewWidget, "preview_widget", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::None)
MCP_BP_ACTION_CALL(RemoveWidget, "remove_widget", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(ReparentWidget, "reparent_widget", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(RenameWidget, "rename_widget", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(GetWidgetSlotInfo, "get_widget_slot_info", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::None)
MCP_BP_ACTION_CALL(ReplaceWidgetClass, "replace_widget_class", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddWidget, "add_widget", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(WrapRoot, "wrap_root", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(ShowWidget, "show_widget", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddWidgetComponent, "add_widget_component", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddSafeZone, "add_safe_zone", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddSpacer, "add_spacer", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddWidgetSwitcher, "add_widget_switcher", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetFont, "set_font", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetMargin, "set_margin", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateWidgetStyle, "create_widget_style", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(ApplyStyleToWidget, "apply_style_to_widget", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetWidgetBinding, "set_widget_binding", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(BindLocalizedText, "bind_localized_text", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetLocalizationKey, "set_localization_key", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(GetAnimationInfo, "get_animation_info", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::None)
MCP_BP_ACTION_CALL(SetAnimationSpeed, "set_animation_speed", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(DeleteAnimation, "delete_animation", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateCreditsScreen, "create_credits_screen", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateShopUi, "create_shop_ui", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddQuestTracker, "add_quest_tracker", HandleManageWidgetAuthoringAction, "manage_widget_authoring", EMcpCallFlags::Mutating)

// CommonUi (HandleCommonUiAction, Action arg "manage_common_ui")
MCP_BP_ACTION_CALL(AddCommonButton, "add_common_button", HandleCommonUiAction, "manage_common_ui", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddCommonText, "add_common_text", HandleCommonUiAction, "manage_common_ui", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(AddCommonBorder, "add_common_border", HandleCommonUiAction, "manage_common_ui", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetCommonButtonStyle, "set_common_button_style", HandleCommonUiAction, "manage_common_ui", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetCommonTextStyle, "set_common_text_style", HandleCommonUiAction, "manage_common_ui", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(SetCommonButtonInputAction, "set_common_button_input_action", HandleCommonUiAction, "manage_common_ui", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateCommonButtonStyle, "create_common_button_style", HandleCommonUiAction, "manage_common_ui", EMcpCallFlags::Mutating)
MCP_BP_ACTION_CALL(CreateCommonTextStyle, "create_common_text_style", HandleCommonUiAction, "manage_common_ui", EMcpCallFlags::Mutating)

#undef MCP_BP_ACTION_CALL

} // namespace McpCalls::ManageBlueprint

void McpRegisterManageBlueprintCalls()
{
	using namespace McpCalls::ManageBlueprint;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_Create>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_Get>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_Compile>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetDefault>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetDefault>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ListFunctions>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ModifyScs>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetScs>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddScsComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_RemoveScsComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ReparentScsComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetScsTransform>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetScsProperty>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_EnsureExists>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ProbeHandle>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddVariable>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_RemoveVariable>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_RenameVariable>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddFunction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_RemoveFunction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddEvent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_RemoveEvent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddConstructionScript>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetVariableMetadata>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetMetadata>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_DeleteNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ConnectPins>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BreakPinLinks>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetNodeProperty>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateRerouteNode>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetNodeDetails>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetGraphDetails>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetPinDetails>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ListNodeTypes>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetPinDefaultValue>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ArrangeGraph>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ListAnimbpGraphs>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetTransitionRuleGraph>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateWidgetBlueprint>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetWidgetParentClass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddCanvasPanel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddHorizontalBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddVerticalBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddOverlay>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddGridPanel>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddUniformGrid>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddWrapBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddScrollBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddSizeBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddScaleBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddBorder>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddTextBlock>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddRichTextBlock>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddImage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddButton>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddCheckBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddSlider>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddProgressBar>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddTextInput>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddComboBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddSpinBox>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddListView>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddTreeView>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetAnchor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetAlignment>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetPosition>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetSize>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetPadding>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetZOrder>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetRenderTransform>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetVisibility>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetStyle>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetClipping>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreatePropertyBinding>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindText>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindVisibility>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindColor>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindEnabled>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindOnClicked>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindOnHovered>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindOnValueChanged>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindEventToDelegate>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateWidgetAnimation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddAnimationTrack>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddAnimationKeyframe>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetAnimationLoop>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateMainMenu>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreatePauseMenu>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateSettingsMenu>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateLoadingScreen>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateHudWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddHealthBar>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddAmmoCounter>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddMinimap>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddCrosshair>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddCompass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddInteractionPrompt>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddObjectiveTracker>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddDamageIndicator>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateInventoryUi>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateDialogWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateRadialMenu>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetWidgetInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_PreviewWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_RemoveWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ReparentWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_RenameWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetWidgetSlotInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ReplaceWidgetClass>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_WrapRoot>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ShowWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddWidgetComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddSafeZone>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddSpacer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddWidgetSwitcher>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetFont>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetMargin>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateWidgetStyle>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_ApplyStyleToWidget>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetWidgetBinding>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_BindLocalizedText>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetLocalizationKey>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_GetAnimationInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetAnimationSpeed>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_DeleteAnimation>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateCreditsScreen>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateShopUi>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddQuestTracker>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddCommonButton>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddCommonText>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_AddCommonBorder>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetCommonButtonStyle>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetCommonTextStyle>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_SetCommonButtonInputAction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateCommonButtonStyle>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageBlueprint_CreateCommonTextStyle>());
}
