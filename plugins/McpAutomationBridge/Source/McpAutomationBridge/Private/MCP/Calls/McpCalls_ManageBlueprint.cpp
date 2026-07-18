// LINT-TOOL: manage_blueprint
// LINT-SCHEMA-DERIVED
// manage_blueprint as FMcpCall classes, now on schema-from-decls
// (docs/action-declarations.md) — the final classed family and the one
// DELEGATION-wired family. Each class AUTHORS its schema fragment in a S_<Suffix>()
// function; the published facade schema (Tools/McpTool_ManageBlueprint.cpp) folds
// those fragments and GetDecl() derives the validation decl from the same fragment
// via McpDeriveDecl(), so schema and decl are one source and cannot drift.
//
// Dispatch is direct: Core, Graph, CommonUi, and every widget action call a
// per-action member directly (MCP_BP_CORE_CALL / MCP_BP_GRAPH_CALL /
// MCP_BP_CUI_CALL / MCP_BP_WIDGET_CALL). The hoisted HandleBlueprintModifyScs
// member is also called externally by EditorFunctionHandlers.cpp:
//   Core       -> HandleBlueprint<Action> member (RequestId, Payload, ...)
//   Graph      -> HandleBlueprintGraph<Action> member (RequestId, Payload, ...)
//   CommonUi   -> HandleCommonUi<Action> member (RequestId, Payload, ...)
//   Widget     -> HandleWidgetAuthoring<Action> member (RequestId, Payload, ...)
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_BlueprintHandlers.h"
#include "McpAutomationBridge_BlueprintGraphHandlers.h"
#include "McpAutomationBridge_WidgetAuthoringHandlers.h"
#include "McpAutomationBridge_CommonUIHandlers.h"

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
// alias of blueprintCandidates) and the SCS snake_case aliases. The five
// zero-param actions (bind_*/create_property_binding) author nothing.

// Core (per-action HandleBlueprint* members, BlueprintHandlers.cpp)

static void S_Create(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("path"), TEXT("Destination folder (alias of savePath; falls back to savePath, then folder, then /Game)."))
	 .String(TEXT("savePath"), TEXT("Path to save the asset."))
	 .String(TEXT("folder"), TEXT("Destination folder fallback after path/savePath."))
	 .String(TEXT("parentClass"), TEXT("Path or name of the parent class."))
	 .String(TEXT("blueprintType"), TEXT("Path or name of the parent class."))
	 .Object(TEXT("properties"), TEXT("create: class-default (CDO) property values applied to the new blueprint, keyed by property name; failures are reported in failedProperties."))
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
	 .Bool(TEXT("saveAfterCompile"), TEXT("compile: save the blueprint to disk after compiling, bypassing the save throttle (default false; save is an alias)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."));
}

static void S_AddComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .String(TEXT("componentType"), TEXT("add_component: component class to add to the blueprint's SCS (StaticMeshComponent, SceneComponent, ArrowComponent, or a loadable UActorComponent class path)."))
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
	 .Integer(TEXT("intValue"), TEXT("Set an integer property."))
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
	 .ArrayOfObjects(TEXT("operations"), TEXT("modify_scs: SCS edits; each item needs type (add_component|remove_component|modify_component|attach_component) plus componentName and per-type keys — componentClass/attachTo (add), transform{location,rotation,scale} (modify), parentComponent/attachTo (attach)."))
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
	 .String(TEXT("componentClass"), TEXT("add_scs_component: UActorComponent subclass to add (short name or class path, e.g. StaticMeshComponent)."))
	 .String(TEXT("component_name"), TEXT("snake_case alias of componentName."))
	 .String(TEXT("componentName"), TEXT("Name of the component."))
	 .String(TEXT("parent_component"), TEXT("snake_case alias of parentComponent."))
	 .String(TEXT("parentComponent"), TEXT("add_scs_component: variable name of the existing SCS component to attach under; omitted = added as a root node."))
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
	 .String(TEXT("newParent"), TEXT("reparent_scs_component: SCS component to become the new parent; Root/RootComponent/DefaultSceneRoot target the root node."));
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
	 .Integer(TEXT("intValue"), TEXT("Set an integer property."))
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
	 // Discriminated default value: populate exactly ONE typed field matching variableType.
	 .Bool(TEXT("boolValue"), TEXT("Default for a Boolean variable."))
	 .Integer(TEXT("intValue"), TEXT("Default for an Integer variable."))
	 .Number(TEXT("floatValue"), TEXT("Default for a Float variable."))
	 .String(TEXT("stringValue"), TEXT("Default for a String / Name / enum / object-path variable."))
	 .Object(TEXT("colorValue"), TEXT("Default for an FLinearColor/FColor variable (r,g,b,a)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .Object(TEXT("vectorValue"), TEXT("Default for an FVector variable (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("structValue"), TEXT("Default for a struct / instanced subobject variable."))
	 .Array(TEXT("arrayValue"), TEXT("Default for an array variable."), TEXT("object"))
	 .String(TEXT("category"), TEXT("add_variable: editor category for the variable (default none)."))
	 .Bool(TEXT("isReplicated"), TEXT("add_variable: mark the variable replicated (sets CPF_Net; default false)."))
	 .Bool(TEXT("isPublic"), TEXT("add_variable: make the variable instance-editable (the editor's eyeball; default false)."))
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
	 .String(TEXT("oldName"), TEXT("rename_variable: current name of the variable to rename."))
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
	 .String(TEXT("memberName"), TEXT("add_function: alias of functionName (checked after functionName and name)."))
	 .ArrayOfObjects(TEXT("inputs"), TEXT("add_function: input parameters; items are {name, type} objects (ignored when override is true)."))
	 .ArrayOfObjects(TEXT("outputs"), TEXT("add_function: output parameters; items are {name, type} objects (ignored when override is true)."))
	 .Bool(TEXT("isPublic"), TEXT("add_function: recorded as the function's public flag in the response/registry only — does not change the graph's access specifier (default false)."))
	 .Bool(TEXT("override"), TEXT("add_function: implement an inherited BlueprintImplementableEvent/BlueprintNativeEvent instead of creating a new function."));
}

static void S_RemoveFunction(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .String(TEXT("functionName"), TEXT("Name of the function."))
	 .Required({TEXT("functionName")});
}

static void S_AddEvent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .String(TEXT("eventType"), TEXT("add_event: \"custom\" (default) for a custom event, or a parent event name to override (BeginPlay/Tick/EndPlay alias to Receive*)."))
	 .String(TEXT("customEventName"), TEXT("Name of the event."))
	 .String(TEXT("eventName"), TEXT("Name of the event."))
	 .ArrayOfObjects(TEXT("parameters"), TEXT("add_event: custom-event parameters; items are {name, type} objects added as output data pins."))
	 .Number(TEXT("posX"), TEXT("Node X position (graph units)."))
	 .Number(TEXT("posY"), TEXT("Node Y position (graph units)."))
	 .Object(TEXT("location"), TEXT("2D graph-node position (x, y)."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x"), TEXT("X coordinate."))
			 .Number(TEXT("y"), TEXT("Y coordinate."))
			 .Required({TEXT("x"), TEXT("y")});
		})
	 .Number(TEXT("x"), TEXT("Graph node X coordinate (fallback after posX/location)."))
	 .Number(TEXT("y"), TEXT("Graph node Y coordinate (fallback after posY/location)."));
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
	 .Object(TEXT("metadata"), TEXT("set_variable_metadata: metadata key/value pairs to set on the variable (e.g. tooltip, ExposeOnSpawn); an empty value removes the key."))
	 .Required({TEXT("variableName"), TEXT("metadata")});
}

static void S_SetMetadata(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .Object(TEXT("metadata"), TEXT("set_blueprint_metadata: metadata key/value pairs (string/bool/number) set on the blueprint's generated class."))
	 .Required({TEXT("metadata")});
}

static void S_AddNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("requestedPath"), TEXT("Explicit blueprint path override, checked before name/blueprintPath/blueprintCandidates (most blueprint_* actions)."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Array(TEXT("blueprintCandidates"), TEXT("Ordered list of candidate blueprint paths to try when blueprintPath/name is absent."))
	 .Array(TEXT("candidates"), TEXT("Legacy alias of blueprintCandidates: ordered candidate blueprint paths to try."))
	 .String(TEXT("nodeType"), TEXT("add_node: node type — CallFunction (or a \"Class::Function\" spec), VariableGet, VariableSet, CustomEvent, Literal, Cast, or an UEdGraphNode class name; Cast/variable/memberName forms delegate to create_node."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("functionName"), TEXT("Name of the function."))
	 .String(TEXT("variableName"), TEXT("Name of the variable."))
	 .String(TEXT("nodeName"), TEXT("add_node: name for a CustomEvent node (nodeType=CustomEvent)."))
	 .Number(TEXT("posX"), TEXT("Node X position (graph units)."))
	 .Number(TEXT("posY"), TEXT("Node Y position (graph units)."))
	 .String(TEXT("memberName"), TEXT("add_node: member spec for the delegated create_node forms — the function to call (CallFunction, with optional memberClass) or the variable name (VariableGet/VariableSet)."))
	 .Bool(TEXT("autoConnect"), TEXT("add_node: auto-wire the new node's exec-in to an open event chain (default false)."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Number(TEXT("x"), TEXT("Graph node X coordinate (alias of posX on the delegated create_node forms)."))
	 .Number(TEXT("y"), TEXT("Graph node Y coordinate (alias of posY on the delegated create_node forms)."))
	 .Required({TEXT("nodeType")});
}

// BlueprintGraph (per-action HandleBlueprintGraph* members, BlueprintGraphHandlers.cpp)

static void S_CreateNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("nodeType"), TEXT("create_node: node type — CallFunction, VariableGet/VariableSet, Event, CustomEvent, Cast/CastTo<Class>, InputAxisEvent, a shorthand function (PrintString, Delay, ...), a flow alias (Branch, Sequence, ForLoop, ...), or any K2Node class name."))
	 .Number(TEXT("x"), TEXT("Graph node X coordinate (checked before posX)."))
	 .Number(TEXT("posX"), TEXT("Node X position (graph units)."))
	 .Number(TEXT("y"), TEXT("Graph node Y coordinate (checked before posY)."))
	 .Number(TEXT("posY"), TEXT("Node Y position (graph units)."))
	 .String(TEXT("variableName"), TEXT("Name of the variable."))
	 .String(TEXT("memberClass"), TEXT("create_node: owning class that memberName/eventName/variableName is looked up on (CallFunction, Event, VariableGet/VariableSet); restricts the search to that class's chain."))
	 .String(TEXT("memberName"), TEXT("create_node: UFUNCTION name for a CallFunction node; without memberClass only the blueprint's class, KismetSystemLibrary, GameplayStatics and KismetMathLibrary are searched."))
	 .String(TEXT("eventName"), TEXT("Name of the event."))
	 .ArrayOfObjects(TEXT("parameters"), TEXT("create_node: CustomEvent parameters; items are {name, type} objects (both keys required) added as output data pins."))
	 .String(TEXT("targetClass"), TEXT("create_node: class for a Cast node's TargetType (required for Cast/DynamicCast) or for a ConstructObjectFromClass node's Class pin (SpawnActorFromClass, CreateWidget, ...)."))
	 .String(TEXT("inputAxisName"), TEXT("create_node: legacy input axis mapping name for an InputAxisEvent node (required for that nodeType)."))
	 .Required({TEXT("nodeType")})
	 .RequiredAnyOf({TEXT("assetPath"), TEXT("blueprintPath")});
}

static void S_DeleteNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .Required({TEXT("nodeId")})
	 .RequiredAnyOf({TEXT("assetPath"), TEXT("blueprintPath")});
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
	 .Required({TEXT("sourceNodeId"), TEXT("sourcePinName"), TEXT("targetNodeId"), TEXT("targetPinName")})
	 .RequiredAnyOf({TEXT("assetPath"), TEXT("blueprintPath")});
}

static void S_BreakPinLinks(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .String(TEXT("pinName"), TEXT("Name of the pin."))
	 .Required({TEXT("nodeId"), TEXT("pinName")})
	 .RequiredAnyOf({TEXT("assetPath"), TEXT("blueprintPath")});
}

static void S_SetNodeProperty(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
	 .String(TEXT("stringValue"), TEXT("Property value (node properties like Comment are string-serialized)."))
	 .Required({TEXT("nodeId"), TEXT("propertyName")})
	 .RequiredAnyOf({TEXT("assetPath"), TEXT("blueprintPath")});
}

static void S_CreateRerouteNode(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .Number(TEXT("x"), TEXT("Graph node X coordinate (checked before posX)."))
	 .Number(TEXT("posX"), TEXT("Node X position (graph units)."))
	 .Number(TEXT("y"), TEXT("Graph node Y coordinate (checked before posY)."))
	 .Number(TEXT("posY"), TEXT("Node Y position (graph units)."))
	 .RequiredAnyOf({TEXT("assetPath"), TEXT("blueprintPath")});
}

static void S_GetNodeDetails(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .Required({TEXT("nodeId")})
	 .RequiredAnyOf({TEXT("assetPath"), TEXT("blueprintPath")});
}

static void S_GetGraphDetails(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .Bool(TEXT("includePinLinks"), TEXT("get_graph_details: include per-node pins with defaults and \"<nodeGuid>:<pinName>\" link refs (always on inside get_transition_rule_graph's ruleGraph)."))
	 .RequiredAnyOf({TEXT("assetPath"), TEXT("blueprintPath")});
}

static void S_GetPinDetails(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .String(TEXT("nodeId"), TEXT("ID of the node."))
	 .String(TEXT("pinName"), TEXT("Name of the pin."))
	 .Required({TEXT("nodeId")})
	 .RequiredAnyOf({TEXT("assetPath"), TEXT("blueprintPath")});
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
	 .Required({TEXT("nodeId"), TEXT("pinName")})
	 .RequiredAnyOf({TEXT("assetPath"), TEXT("blueprintPath")});
}

static void S_ArrangeGraph(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .Array(TEXT("nodes"), TEXT("arrange_graph: node GUIDs to lay out as one rigid block — anchored at the scope's current top-left, slid down until it clears unmoved nodes; every node outside the list stays put. Omit to arrange the whole graph."))
	 .Bool(TEXT("splitSharedGetters"), TEXT("arrange_graph: before layout, split a VariableGet/Self node whose output feeds several consumers into one copy per consumer (default true; pure reads, semantics-safe — the human idiom). Scoped arranges only split links between in-scope nodes. Response reports gettersSplit + splitNodes."))
	 .RequiredAnyOf({TEXT("assetPath"), TEXT("blueprintPath")});
}

static void S_ListAnimbpGraphs(FMcpSchemaBuilder& B)
{
	B.String(TEXT("assetPath"), TEXT("Blueprint asset path for graph actions (create_node/connect_pins/etc.); alias of blueprintPath."))
	 .String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("graphName"), TEXT("Name of the graph."))
	 .RequiredAnyOf({TEXT("assetPath"), TEXT("blueprintPath")});
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
	 .Integer(TEXT("transitionIndex"), TEXT("get_transition_rule_graph: 0-based pick when parallel transitions between the same states match (see matchCount in the response)."))
	 .RequiredAnyOf({TEXT("assetPath"), TEXT("blueprintPath")});
}

// WidgetAuthoring (per-action HandleWidgetAuthoring* members)

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
	 .Integer(TEXT("fontSize"), TEXT("Font size (add_text_block, set_font)."))
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
	 .Required({TEXT("widgetPath")})
	 .RequiredAnyOf({TEXT("slotName"), TEXT("name"), TEXT("widgetName")});
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
	 .Required({TEXT("widgetPath")})
	 .RequiredAnyOf({TEXT("slotName"), TEXT("name"), TEXT("widgetName")});
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
	 .Number(TEXT("x"), TEXT("set_position: canvas slot X position in pixels (fallback after position/posX)."))
	 .Number(TEXT("y"), TEXT("set_position: canvas slot Y position in pixels (fallback after position/posY)."))
	 .Required({TEXT("widgetPath")})
	 .RequiredAnyOf({TEXT("slotName"), TEXT("name"), TEXT("widgetName")});
}

static void S_SetSize(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .Object(TEXT("size"), TEXT("set_size: canvas slot pixel size as an {x,y} object."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")); })
	 .Number(TEXT("x"), TEXT("set_size: canvas slot width in pixels (alternative to size)."))
	 .Number(TEXT("y"), TEXT("set_size: canvas slot height in pixels (alternative to size)."))
	 .Bool(TEXT("fill"), TEXT("set_size: stretch a HBox/VBox child to fill (vs Automatic)."))
	 .Number(TEXT("fillWeight"), TEXT("set_size: fill weight when fill is true."))
	 .Required({TEXT("widgetPath")})
	 .RequiredAnyOf({TEXT("slotName"), TEXT("name"), TEXT("widgetName")});
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
	 .Required({TEXT("widgetPath")})
	 .RequiredAnyOf({TEXT("slotName"), TEXT("name"), TEXT("widgetName")});
}

static void S_SetZOrder(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .Integer(TEXT("zOrder"), TEXT("set_z_order: Canvas slot Z-order."))
	 .Required({TEXT("widgetPath")})
	 .RequiredAnyOf({TEXT("slotName"), TEXT("name"), TEXT("widgetName")});
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
	 .Object(TEXT("scale"), TEXT("set_render_transform: 2D scale (x, y)."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x"), TEXT("X scale."))
			 .Number(TEXT("y"), TEXT("Y scale."));
		})
	 .Object(TEXT("shear"), TEXT("set_render_transform: 2D shear."),
			[](FMcpSchemaBuilder& S) {
			S.Number(TEXT("x")).Number(TEXT("y"));
		})
	 .Number(TEXT("angle"), TEXT("set_render_transform: rotation angle in degrees."))
	 .Required({TEXT("widgetPath")})
	 .RequiredAnyOf({TEXT("slotName"), TEXT("name"), TEXT("widgetName")});
}

static void S_SetVisibility(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("visibility"), TEXT("set_visibility: Visible, Hidden, Collapsed, HitTestInvisible, or SelfHitTestInvisible."))
	 .Required({TEXT("widgetPath")})
	 .RequiredAnyOf({TEXT("slotName"), TEXT("name"), TEXT("widgetName")});
}

static void S_SetStyle(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("propertyName"), TEXT("Name of the property."))
	 // Discriminated value (set_style): populate exactly ONE typed field.
	 .Bool(TEXT("boolValue"), TEXT("Set a bool style property."))
	 .Integer(TEXT("intValue"), TEXT("Set an integer style property."))
	 .Number(TEXT("floatValue"), TEXT("Set a float style property."))
	 .String(TEXT("stringValue"), TEXT("Set a string / enum / brush-path style property."))
	 .Object(TEXT("colorValue"), TEXT("Set an FLinearColor/FColor style property (r,g,b,a)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a")); })
	 .Object(TEXT("vectorValue"), TEXT("Set an FVector style property (x, y, z)."),
		[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
	 .Object(TEXT("structValue"), TEXT("Set a struct style property (e.g. a Slate brush/margin)."))
	 .Array(TEXT("arrayValue"), TEXT("Set an array style property."), TEXT("object"))
	 .String(TEXT("style"), TEXT("set_style: legacy value alias for propertyName=\"Style\" when propertyName is omitted."))
	 .Required({TEXT("widgetPath")})
	 .RequiredAnyOf({TEXT("slotName"), TEXT("name"), TEXT("widgetName")});
}

static void S_SetClipping(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("clipping"), TEXT("set_clipping: Inherit, ClipToBounds, ClipToBoundsWithoutIntersecting, ClipToBoundsAlways, or OnDemand."))
	 .Required({TEXT("widgetPath")})
	 .RequiredAnyOf({TEXT("slotName"), TEXT("name"), TEXT("widgetName")});
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
	 .Required({TEXT("widgetPath")})
	 .RequiredAnyOf({TEXT("slotName"), TEXT("name"), TEXT("widgetName")});
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
	 .Required({TEXT("widgetPath")})
	 .RequiredAnyOf({TEXT("slotName"), TEXT("name"), TEXT("widgetName")});
}

static void S_BindOnValueChanged(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("widgetName"), TEXT("Target widget's name within the tree (remove/rename/reparent; DesiredFocusWidget; get_widget_info: return this widget's property values + objectPath)."))
	 .String(TEXT("delegateName"), TEXT("Multicast delegate name (bind_on_clicked/bind_on_hovered default OnClicked/OnHovered; bind_on_value_changed default OnValueChanged; bind_event_to_delegate)."))
	 .String(TEXT("targetText"), TEXT("bind_on_value_changed: text widget to live-update with the new value."))
	 .Required({TEXT("widgetPath")})
	 .RequiredAnyOf({TEXT("slotName"), TEXT("name"), TEXT("widgetName")});
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
	 .Required({TEXT("widgetPath"), TEXT("animationName")})
	 .RequiredAnyOf({TEXT("slotName"), TEXT("name"), TEXT("widgetName")});
}

static void S_AddAnimationKeyframe(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("animationName"), TEXT("Widget animation name (create_widget_animation default NewAnimation; add_animation_track/get_animation_info/delete_animation/etc.)."))
	 .Number(TEXT("time"), TEXT("add_animation_keyframe: keyframe time in seconds."))
	 .Number(TEXT("floatValue"), TEXT("add_animation_keyframe: keyframe value."))
	 .Required({TEXT("widgetPath"), TEXT("animationName")});
}

static void S_SetAnimationLoop(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("animationName"), TEXT("Widget animation name (create_widget_animation default NewAnimation; add_animation_track/get_animation_info/delete_animation/etc.)."))
	 .Bool(TEXT("loop"), TEXT("set_animation_loop: whether the animation loops."))
	 .Integer(TEXT("loopCount"), TEXT("set_animation_loop: loop count (0 = infinite)."))
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
	 .Number(TEXT("x"), TEXT("add_health_bar: canvas X position in pixels (default 20)."))
	 .Number(TEXT("y"), TEXT("add_health_bar: canvas Y position in pixels (default 20)."))
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
	 .Integer(TEXT("columns"), TEXT("create_inventory_ui/create_shop_ui: grid column count."))
	 .Integer(TEXT("rows"), TEXT("create_inventory_ui: grid row count."));
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
	 .Integer(TEXT("segments"), TEXT("create_radial_menu: number of radial segments."));
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
	 .String(TEXT("newParent"), TEXT("reparent_widget: name of the panel widget to move the target widget under."))
	 .Required({TEXT("widgetPath"), TEXT("slotName"), TEXT("newParent")});
}

static void S_RenameWidget(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("oldName"), TEXT("rename_widget: current name of the widget to rename (slotName/name accepted as aliases)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("name"), TEXT("Name identifier."))
	 .String(TEXT("newName"), TEXT("New name for renaming."))
	 .Required({TEXT("widgetPath"), TEXT("newName")})
	 .RequiredAnyOf({TEXT("oldName"), TEXT("slotName"), TEXT("name")});
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
	 .String(TEXT("componentType"), TEXT("add_widget_component: UMG widget class to add (TextBlock, Button, Image, ProgressBar, Slider, CheckBox, ComboBox, panel types, ... or any UWidget class name)."))
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
	 .Integer(TEXT("activeIndex"), TEXT("add_widget_switcher: initially active child index."))
	 .Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
	 .Required({TEXT("widgetPath")});
}

static void S_SetFont(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .String(TEXT("font"), TEXT("set_font: font asset path."))
	 .Integer(TEXT("fontSize"), TEXT("Font size (add_text_block, set_font)."))
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
	 .Integer(TEXT("columns"), TEXT("create_inventory_ui/create_shop_ui: grid column count."));
}

static void S_AddQuestTracker(FMcpSchemaBuilder& B)
{
	B.String(TEXT("widgetPath"), TEXT("Widget Blueprint asset path (widget-authoring actions)."))
	 .String(TEXT("slotName"), TEXT("Name for the widget being added / the slot to target."))
	 .Required({TEXT("widgetPath")});
}

// CommonUi schema fragments (per-action HandleCommonUi* members, CommonUIHandlers.cpp)

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
// Flags authored per action. RequiresEditor on Core + Graph: the per-action
// HandleBlueprint* members (BlueprintHandlers.cpp) and HandleBlueprintGraph* members
// (BlueprintGraphHandlers.cpp) are each #if WITH_EDITOR gated. NOT on
// Widget (WidgetAuthoringHandlers.cpp carries no editor gate) or CommonUi
// (CommonUIHandlers.cpp gates on MCP_HAS_COMMON_UI, not WITH_EDITOR) — flagging
// either would newly reject the GEditor-less runs those handlers accept. Mutating on
// every action except the pure get_*/list_*/preview_*/probe_* reads.

// BlueprintGraph: one classed action per HandleBlueprintGraph* member
// (BlueprintGraphHandlers.cpp). Run() calls the member directly — no surviving
// 4-arg dispatcher, no action-string re-parse.
#define MCP_BP_GRAPH_CALL(ClassSuffix, ActionLiteral, HandlerFn, Flags) \
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
		return HandlerFn(S, RequestId, Payload, Socket);                   \
	}                                                                                    \
};

// Core direct-call: one classed action per hoisted HandleBlueprint* member
// (BlueprintHandlers.cpp). Run() calls the member directly — no surviving 4-arg
// dispatcher, no action-string re-parse. Structurally identical to MCP_BP_GRAPH_CALL.
#define MCP_BP_CORE_CALL(ClassSuffix, ActionLiteral, HandlerFn, Flags) \
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
		return HandlerFn(S, RequestId, Payload, Socket);                   \
	}                                                                                    \
};

// Widget direct-call: one classed action per hoisted HandleWidgetAuthoring* member
// (WidgetAuthoringHandlers.cpp; all widget families). Run() calls the member
// directly. Structurally identical to MCP_BP_CORE_CALL.
#define MCP_BP_WIDGET_CALL(ClassSuffix, ActionLiteral, HandlerFn, Flags) \
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
		return HandlerFn(S, RequestId, Payload, Socket);                   \
	}                                                                                    \
};

// CommonUi direct-call: one classed action per hoisted HandleCommonUi* member
// (CommonUIHandlers.cpp). Run() calls the member directly. Structurally identical
// to MCP_BP_CORE_CALL.
#define MCP_BP_CUI_CALL(ClassSuffix, ActionLiteral, HandlerFn, Flags) \
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
		return HandlerFn(S, RequestId, Payload, Socket);                   \
	}                                                                                    \
};

// Core (per-action HandleBlueprint* members, BlueprintHandlers.cpp)
MCP_BP_CORE_CALL(Create, "create", McpHandlers::Blueprint::HandleBlueprintCreateAction, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(Get, "get", McpHandlers::Blueprint::HandleBlueprintGet, EMcpCallFlags::RequiresEditor)
MCP_BP_CORE_CALL(Compile, "compile", McpHandlers::Blueprint::HandleBlueprintCompile, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(AddComponent, "add_component", McpHandlers::Blueprint::HandleBlueprintAddComponent, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(SetDefault, "set_default", McpHandlers::Blueprint::HandleBlueprintSetDefault, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(GetDefault, "get_default", McpHandlers::Blueprint::HandleBlueprintGetDefault, EMcpCallFlags::RequiresEditor)
MCP_BP_CORE_CALL(ListFunctions, "list_functions", McpHandlers::Blueprint::HandleBlueprintListFunctions, EMcpCallFlags::RequiresEditor)
MCP_BP_CORE_CALL(ModifyScs, "modify_scs", McpHandlers::Blueprint::HandleBlueprintModifyScs, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(GetScs, "get_scs", McpHandlers::Blueprint::HandleBlueprintGetScs, EMcpCallFlags::RequiresEditor)
MCP_BP_CORE_CALL(AddScsComponent, "add_scs_component", McpHandlers::Blueprint::HandleBlueprintAddScsComponent, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(RemoveScsComponent, "remove_scs_component", McpHandlers::Blueprint::HandleBlueprintRemoveScsComponent, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(ReparentScsComponent, "reparent_scs_component", McpHandlers::Blueprint::HandleBlueprintReparentScsComponent, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(SetScsTransform, "set_scs_transform", McpHandlers::Blueprint::HandleBlueprintSetScsTransform, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(SetScsProperty, "set_scs_property", McpHandlers::Blueprint::HandleBlueprintSetScsProperty, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(EnsureExists, "ensure_exists", McpHandlers::Blueprint::HandleBlueprintEnsureExists, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(ProbeHandle, "probe_handle", McpHandlers::Blueprint::HandleBlueprintProbeHandle, EMcpCallFlags::RequiresEditor)
MCP_BP_CORE_CALL(AddVariable, "add_variable", McpHandlers::Blueprint::HandleBlueprintAddVariable, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(RemoveVariable, "remove_variable", McpHandlers::Blueprint::HandleBlueprintRemoveVariable, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(RenameVariable, "rename_variable", McpHandlers::Blueprint::HandleBlueprintRenameVariable, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(AddFunction, "add_function", McpHandlers::Blueprint::HandleBlueprintAddFunction, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(RemoveFunction, "remove_function", McpHandlers::Blueprint::HandleBlueprintRemoveFunction, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(AddEvent, "add_event", McpHandlers::Blueprint::HandleBlueprintAddEvent, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(RemoveEvent, "remove_event", McpHandlers::Blueprint::HandleBlueprintRemoveEvent, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(AddConstructionScript, "add_construction_script", McpHandlers::Blueprint::HandleBlueprintAddConstructionScript, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(SetVariableMetadata, "set_variable_metadata", McpHandlers::Blueprint::HandleBlueprintSetVariableMetadata, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(SetMetadata, "set_blueprint_metadata", McpHandlers::Blueprint::HandleBlueprintSetMetadata, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_CORE_CALL(AddNode, "add_node", McpHandlers::Blueprint::HandleBlueprintAddNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)

// BlueprintGraph (per-action HandleBlueprintGraph* members, BlueprintGraphHandlers.cpp)
MCP_BP_GRAPH_CALL(CreateNode, "create_node", McpHandlers::Blueprint::HandleBlueprintGraphCreateNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_GRAPH_CALL(DeleteNode, "delete_node", McpHandlers::Blueprint::HandleBlueprintGraphDeleteNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_GRAPH_CALL(ConnectPins, "connect_pins", McpHandlers::Blueprint::HandleBlueprintGraphConnectPins, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_GRAPH_CALL(BreakPinLinks, "break_pin_links", McpHandlers::Blueprint::HandleBlueprintGraphBreakPinLinks, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_GRAPH_CALL(SetNodeProperty, "set_node_property", McpHandlers::Blueprint::HandleBlueprintGraphSetNodeProperty, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_GRAPH_CALL(CreateRerouteNode, "create_reroute_node", McpHandlers::Blueprint::HandleBlueprintGraphCreateRerouteNode, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_GRAPH_CALL(GetNodeDetails, "get_node_details", McpHandlers::Blueprint::HandleBlueprintGraphGetNodeDetails, EMcpCallFlags::RequiresEditor)
MCP_BP_GRAPH_CALL(GetGraphDetails, "get_graph_details", McpHandlers::Blueprint::HandleBlueprintGraphGetGraphDetails, EMcpCallFlags::RequiresEditor)
MCP_BP_GRAPH_CALL(GetPinDetails, "get_pin_details", McpHandlers::Blueprint::HandleBlueprintGraphGetPinDetails, EMcpCallFlags::RequiresEditor)
MCP_BP_GRAPH_CALL(ListNodeTypes, "list_node_types", McpHandlers::Blueprint::HandleBlueprintGraphListNodeTypes, EMcpCallFlags::RequiresEditor)
MCP_BP_GRAPH_CALL(SetPinDefaultValue, "set_pin_default_value", McpHandlers::Blueprint::HandleBlueprintGraphSetPinDefaultValue, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_GRAPH_CALL(ArrangeGraph, "arrange_graph", McpHandlers::Blueprint::HandleBlueprintGraphArrangeGraph, EMcpCallFlags::RequiresEditor | EMcpCallFlags::Mutating)
MCP_BP_GRAPH_CALL(ListAnimbpGraphs, "list_animbp_graphs", McpHandlers::Blueprint::HandleBlueprintGraphListAnimbpGraphs, EMcpCallFlags::RequiresEditor)
MCP_BP_GRAPH_CALL(GetTransitionRuleGraph, "get_transition_rule_graph", McpHandlers::Blueprint::HandleBlueprintGraphGetTransitionRuleGraph, EMcpCallFlags::RequiresEditor)

// WidgetAuthoring: every action calls a per-action HandleWidgetAuthoring* member
// directly (MCP_BP_WIDGET_CALL, WidgetAuthoringHandlers.cpp).
MCP_BP_WIDGET_CALL(CreateWidgetBlueprint, "create_widget_blueprint", McpHandlers::Blueprint::HandleWidgetAuthoringCreateWidgetBlueprint, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(SetWidgetParentClass, "set_widget_parent_class", McpHandlers::Blueprint::HandleWidgetAuthoringSetWidgetParentClass, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddCanvasPanel, "add_canvas_panel", McpHandlers::Blueprint::HandleWidgetAuthoringAddCanvasPanel, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddHorizontalBox, "add_horizontal_box", McpHandlers::Blueprint::HandleWidgetAuthoringAddHorizontalBox, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddVerticalBox, "add_vertical_box", McpHandlers::Blueprint::HandleWidgetAuthoringAddVerticalBox, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddOverlay, "add_overlay", McpHandlers::Blueprint::HandleWidgetAuthoringAddOverlay, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddGridPanel, "add_grid_panel", McpHandlers::Blueprint::HandleWidgetAuthoringAddGridPanel, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddUniformGrid, "add_uniform_grid", McpHandlers::Blueprint::HandleWidgetAuthoringAddUniformGrid, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddWrapBox, "add_wrap_box", McpHandlers::Blueprint::HandleWidgetAuthoringAddWrapBox, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddScrollBox, "add_scroll_box", McpHandlers::Blueprint::HandleWidgetAuthoringAddScrollBox, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddSizeBox, "add_size_box", McpHandlers::Blueprint::HandleWidgetAuthoringAddSizeBox, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddScaleBox, "add_scale_box", McpHandlers::Blueprint::HandleWidgetAuthoringAddScaleBox, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddBorder, "add_border", McpHandlers::Blueprint::HandleWidgetAuthoringAddBorder, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddTextBlock, "add_text_block", McpHandlers::Blueprint::HandleWidgetAuthoringAddTextBlock, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddRichTextBlock, "add_rich_text_block", McpHandlers::Blueprint::HandleWidgetAuthoringAddRichTextBlock, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddImage, "add_image", McpHandlers::Blueprint::HandleWidgetAuthoringAddImage, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddButton, "add_button", McpHandlers::Blueprint::HandleWidgetAuthoringAddButton, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddCheckBox, "add_check_box", McpHandlers::Blueprint::HandleWidgetAuthoringAddCheckBox, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddSlider, "add_slider", McpHandlers::Blueprint::HandleWidgetAuthoringAddSlider, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddProgressBar, "add_progress_bar", McpHandlers::Blueprint::HandleWidgetAuthoringAddProgressBar, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddTextInput, "add_text_input", McpHandlers::Blueprint::HandleWidgetAuthoringAddTextInput, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddComboBox, "add_combo_box", McpHandlers::Blueprint::HandleWidgetAuthoringAddComboBox, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddSpinBox, "add_spin_box", McpHandlers::Blueprint::HandleWidgetAuthoringAddSpinBox, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddListView, "add_list_view", McpHandlers::Blueprint::HandleWidgetAuthoringAddListView, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddTreeView, "add_tree_view", McpHandlers::Blueprint::HandleWidgetAuthoringAddTreeView, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(SetAnchor, "set_anchor", McpHandlers::Blueprint::HandleWidgetAuthoringSetAnchor, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(SetAlignment, "set_alignment", McpHandlers::Blueprint::HandleWidgetAuthoringSetAlignment, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(SetPosition, "set_position", McpHandlers::Blueprint::HandleWidgetAuthoringSetPosition, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(SetSize, "set_size", McpHandlers::Blueprint::HandleWidgetAuthoringSetSize, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(SetPadding, "set_padding", McpHandlers::Blueprint::HandleWidgetAuthoringSetPadding, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(SetZOrder, "set_z_order", McpHandlers::Blueprint::HandleWidgetAuthoringSetZOrder, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(SetRenderTransform, "set_render_transform", McpHandlers::Blueprint::HandleWidgetAuthoringSetRenderTransform, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(SetVisibility, "set_visibility", McpHandlers::Blueprint::HandleWidgetAuthoringSetVisibility, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(SetStyle, "set_style", McpHandlers::Blueprint::HandleWidgetAuthoringSetStyle, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(SetClipping, "set_clipping", McpHandlers::Blueprint::HandleWidgetAuthoringSetClipping, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(CreatePropertyBinding, "create_property_binding", McpHandlers::Blueprint::HandleWidgetAuthoringCreatePropertyBinding, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(BindText, "bind_text", McpHandlers::Blueprint::HandleWidgetAuthoringBindText, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(BindVisibility, "bind_visibility", McpHandlers::Blueprint::HandleWidgetAuthoringBindVisibility, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(BindColor, "bind_color", McpHandlers::Blueprint::HandleWidgetAuthoringBindColor, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(BindEnabled, "bind_enabled", McpHandlers::Blueprint::HandleWidgetAuthoringBindEnabled, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(BindOnClicked, "bind_on_clicked", McpHandlers::Blueprint::HandleWidgetAuthoringBindOnClicked, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(BindOnHovered, "bind_on_hovered", McpHandlers::Blueprint::HandleWidgetAuthoringBindOnHovered, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(BindOnValueChanged, "bind_on_value_changed", McpHandlers::Blueprint::HandleWidgetAuthoringBindOnValueChanged, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(BindEventToDelegate, "bind_event_to_delegate", McpHandlers::Blueprint::HandleWidgetAuthoringBindEventToDelegate, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(CreateWidgetAnimation, "create_widget_animation", McpHandlers::Blueprint::HandleWidgetAuthoringCreateWidgetAnimation, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddAnimationTrack, "add_animation_track", McpHandlers::Blueprint::HandleWidgetAuthoringAddAnimationTrack, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddAnimationKeyframe, "add_animation_keyframe", McpHandlers::Blueprint::HandleWidgetAuthoringAddAnimationKeyframe, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(SetAnimationLoop, "set_animation_loop", McpHandlers::Blueprint::HandleWidgetAuthoringSetAnimationLoop, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(CreateMainMenu, "create_main_menu", McpHandlers::Blueprint::HandleWidgetAuthoringCreateMainMenu, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(CreatePauseMenu, "create_pause_menu", McpHandlers::Blueprint::HandleWidgetAuthoringCreatePauseMenu, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(CreateSettingsMenu, "create_settings_menu", McpHandlers::Blueprint::HandleWidgetAuthoringCreateSettingsMenu, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(CreateLoadingScreen, "create_loading_screen", McpHandlers::Blueprint::HandleWidgetAuthoringCreateLoadingScreen, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(CreateHudWidget, "create_hud_widget", McpHandlers::Blueprint::HandleWidgetAuthoringCreateHudWidget, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddHealthBar, "add_health_bar", McpHandlers::Blueprint::HandleWidgetAuthoringAddHealthBar, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddAmmoCounter, "add_ammo_counter", McpHandlers::Blueprint::HandleWidgetAuthoringAddAmmoCounter, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddMinimap, "add_minimap", McpHandlers::Blueprint::HandleWidgetAuthoringAddMinimap, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddCrosshair, "add_crosshair", McpHandlers::Blueprint::HandleWidgetAuthoringAddCrosshair, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddCompass, "add_compass", McpHandlers::Blueprint::HandleWidgetAuthoringAddCompass, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddInteractionPrompt, "add_interaction_prompt", McpHandlers::Blueprint::HandleWidgetAuthoringAddInteractionPrompt, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddObjectiveTracker, "add_objective_tracker", McpHandlers::Blueprint::HandleWidgetAuthoringAddObjectiveTracker, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddDamageIndicator, "add_damage_indicator", McpHandlers::Blueprint::HandleWidgetAuthoringAddDamageIndicator, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(CreateInventoryUi, "create_inventory_ui", McpHandlers::Blueprint::HandleWidgetAuthoringCreateInventoryUi, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(CreateDialogWidget, "create_dialog_widget", McpHandlers::Blueprint::HandleWidgetAuthoringCreateDialogWidget, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(CreateRadialMenu, "create_radial_menu", McpHandlers::Blueprint::HandleWidgetAuthoringCreateRadialMenu, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(GetWidgetInfo, "get_widget_info", McpHandlers::Blueprint::HandleWidgetAuthoringGetWidgetInfo, EMcpCallFlags::None)
MCP_BP_WIDGET_CALL(PreviewWidget, "preview_widget", McpHandlers::Blueprint::HandleWidgetAuthoringPreviewWidget, EMcpCallFlags::None)
MCP_BP_WIDGET_CALL(RemoveWidget, "remove_widget", McpHandlers::Blueprint::HandleWidgetAuthoringRemoveWidget, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(ReparentWidget, "reparent_widget", McpHandlers::Blueprint::HandleWidgetAuthoringReparentWidget, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(RenameWidget, "rename_widget", McpHandlers::Blueprint::HandleWidgetAuthoringRenameWidget, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(GetWidgetSlotInfo, "get_widget_slot_info", McpHandlers::Blueprint::HandleWidgetAuthoringGetWidgetSlotInfo, EMcpCallFlags::None)
MCP_BP_WIDGET_CALL(ReplaceWidgetClass, "replace_widget_class", McpHandlers::Blueprint::HandleWidgetAuthoringReplaceWidgetClass, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddWidget, "add_widget", McpHandlers::Blueprint::HandleWidgetAuthoringAddWidget, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(WrapRoot, "wrap_root", McpHandlers::Blueprint::HandleWidgetAuthoringWrapRoot, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(ShowWidget, "show_widget", McpHandlers::Blueprint::HandleWidgetAuthoringShowWidget, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddWidgetComponent, "add_widget_component", McpHandlers::Blueprint::HandleWidgetAuthoringAddWidgetComponent, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddSafeZone, "add_safe_zone", McpHandlers::Blueprint::HandleWidgetAuthoringAddSafeZone, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddSpacer, "add_spacer", McpHandlers::Blueprint::HandleWidgetAuthoringAddSpacer, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddWidgetSwitcher, "add_widget_switcher", McpHandlers::Blueprint::HandleWidgetAuthoringAddWidgetSwitcher, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(SetFont, "set_font", McpHandlers::Blueprint::HandleWidgetAuthoringSetFont, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(SetMargin, "set_margin", McpHandlers::Blueprint::HandleWidgetAuthoringSetMargin, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(CreateWidgetStyle, "create_widget_style", McpHandlers::Blueprint::HandleWidgetAuthoringCreateWidgetStyle, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(ApplyStyleToWidget, "apply_style_to_widget", McpHandlers::Blueprint::HandleWidgetAuthoringApplyStyleToWidget, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(SetWidgetBinding, "set_widget_binding", McpHandlers::Blueprint::HandleWidgetAuthoringSetWidgetBinding, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(BindLocalizedText, "bind_localized_text", McpHandlers::Blueprint::HandleWidgetAuthoringBindLocalizedText, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(SetLocalizationKey, "set_localization_key", McpHandlers::Blueprint::HandleWidgetAuthoringSetLocalizationKey, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(GetAnimationInfo, "get_animation_info", McpHandlers::Blueprint::HandleWidgetAuthoringGetAnimationInfo, EMcpCallFlags::None)
MCP_BP_WIDGET_CALL(SetAnimationSpeed, "set_animation_speed", McpHandlers::Blueprint::HandleWidgetAuthoringSetAnimationSpeed, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(DeleteAnimation, "delete_animation", McpHandlers::Blueprint::HandleWidgetAuthoringDeleteAnimation, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(CreateCreditsScreen, "create_credits_screen", McpHandlers::Blueprint::HandleWidgetAuthoringCreateCreditsScreen, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(CreateShopUi, "create_shop_ui", McpHandlers::Blueprint::HandleWidgetAuthoringCreateShopUi, EMcpCallFlags::Mutating)
MCP_BP_WIDGET_CALL(AddQuestTracker, "add_quest_tracker", McpHandlers::Blueprint::HandleWidgetAuthoringAddQuestTracker, EMcpCallFlags::Mutating)

// CommonUi (per-action HandleCommonUi* members, CommonUIHandlers.cpp)
MCP_BP_CUI_CALL(AddCommonButton, "add_common_button", McpHandlers::Blueprint::HandleCommonUiAddCommonButton, EMcpCallFlags::Mutating)
MCP_BP_CUI_CALL(AddCommonText, "add_common_text", McpHandlers::Blueprint::HandleCommonUiAddCommonText, EMcpCallFlags::Mutating)
MCP_BP_CUI_CALL(AddCommonBorder, "add_common_border", McpHandlers::Blueprint::HandleCommonUiAddCommonBorder, EMcpCallFlags::Mutating)
MCP_BP_CUI_CALL(SetCommonButtonStyle, "set_common_button_style", McpHandlers::Blueprint::HandleCommonUiSetCommonButtonStyle, EMcpCallFlags::Mutating)
MCP_BP_CUI_CALL(SetCommonTextStyle, "set_common_text_style", McpHandlers::Blueprint::HandleCommonUiSetCommonTextStyle, EMcpCallFlags::Mutating)
MCP_BP_CUI_CALL(SetCommonButtonInputAction, "set_common_button_input_action", McpHandlers::Blueprint::HandleCommonUiSetCommonButtonInputAction, EMcpCallFlags::Mutating)
MCP_BP_CUI_CALL(CreateCommonButtonStyle, "create_common_button_style", McpHandlers::Blueprint::HandleCommonUiCreateCommonButtonStyle, EMcpCallFlags::Mutating)
MCP_BP_CUI_CALL(CreateCommonTextStyle, "create_common_text_style", McpHandlers::Blueprint::HandleCommonUiCreateCommonTextStyle, EMcpCallFlags::Mutating)

#undef MCP_BP_GRAPH_CALL
#undef MCP_BP_CORE_CALL
#undef MCP_BP_WIDGET_CALL
#undef MCP_BP_CUI_CALL

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
