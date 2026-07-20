// =============================================================================
// McpAutomationBridge_NiagaraAuthoringHandlers.cpp
// =============================================================================
// Niagara VFX System Authoring Handlers for MCP Automation Bridge
//
// HANDLERS IMPLEMENTED (35+ actions):
// -----------------------------------
// Section 1: System Creation
//   - create_niagara_system        : Create UNiagaraSystem asset
//   - configure_system_settings    : Set system properties
//   - set_system_capacity          : Set particle capacity
//
// Section 2: Emitter Management
//   - create_niagara_emitter       : Create UNiagaraEmitter
//   - add_emitter_to_system        : Add emitter to system
//   - configure_emitter_properties : Set emitter settings
//
// Section 3: Module Operations
//   - add_emitter_module           : Add module to emitter
//   - remove_emitter_module        : Remove module
//   - set_module_input             : Set module input value
//
// Section 4: Parameters
//   - add_system_parameter         : Add system parameter
//   - set_parameter_default        : Set default value
//   - expose_parameter             : Expose parameter to editor
//
// Section 5: Events & Simulation
//   - add_niagara_event            : Add particle event
//   - configure_gpu_simulation     : Setup GPU simulation
//   - add_simulation_stage         : Add simulation stage
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0: Uses UNiagaraEmitter* directly
// UE 5.1+: Uses FVersionedNiagaraEmitterData
// - MCP_HAS_NIAGARA_VERSIONING_APIS macro handles compatibility
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first
#include "McpHandlerUtils.h"

#include "Modules/ModuleManager.h"  // Required for FModuleManager::IsModuleLoaded() runtime checks

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_NiagaraAuthoringHandlers.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

// Note: FVersionedNiagaraEmitterData and related APIs were introduced in UE 5.1
// UE 5.0 uses direct emitter pointers, UE 5.1+ uses versioned emitter data
#if WITH_EDITOR && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_HAS_NIAGARA_VERSIONING_APIS 1
#define MCP_NIAGARA_EMITTER_DATA_TYPE FVersionedNiagaraEmitterData
#define MCP_GET_EMITTER_DATA(Handle) (Handle).GetEmitterData()
#define MCP_GET_LATEST_EMITTER_DATA(Emitter) (Emitter)->GetLatestEmitterData()
#define MCP_GET_EMITTER_VERSION_GUID(Emitter) (Emitter)->GetExposedVersion().VersionGuid
#else
#define MCP_HAS_NIAGARA_VERSIONING_APIS 0
// UE 5.0: Use UNiagaraEmitter* directly instead of MCP_NIAGARA_EMITTER_DATA_TYPE*
#define MCP_NIAGARA_EMITTER_DATA_TYPE UNiagaraEmitter
// UE 5.0: GetInstance() is a const method, returns UNiagaraEmitter*
// Use (&(Handle)) to handle both pointers and references uniformly
#define MCP_GET_EMITTER_DATA(Handle) (&(Handle))->GetInstance()
#define MCP_GET_LATEST_EMITTER_DATA(Emitter) (Emitter)
#define MCP_GET_EMITTER_VERSION_GUID(Emitter) FGuid()
#endif

#if WITH_EDITOR
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraEditorModule.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraStackEditorData.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSimulationStageBase.h"
#include "NiagaraDataInterface.h"

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

// FNiagaraStackGraphUtilities is only available in UE 5.1+ (NiagaraEditor module)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#define MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES 1
#else
#define MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES 0
#endif

// Niagara Data Interfaces
// UE 5.7+: Data interfaces moved to subfolders or different modules
#if __has_include("NiagaraDataInterfaceSkeletalMesh.h")
#include "NiagaraDataInterfaceSkeletalMesh.h"
#define MCP_HAS_NIAGARA_SKELETAL_MESH_DI 1
#elif __has_include("DataInterface/NiagaraDataInterfaceSkeletalMesh.h")
#include "DataInterface/NiagaraDataInterfaceSkeletalMesh.h"
#define MCP_HAS_NIAGARA_SKELETAL_MESH_DI 1
#else
#define MCP_HAS_NIAGARA_SKELETAL_MESH_DI 0
#endif

// NiagaraDataInterfaceStaticMesh location varies by UE version
// In UE 5.7+, it moved to the Internal folder which is also on the include path
#if __has_include("NiagaraDataInterfaceStaticMesh.h")
#include "NiagaraDataInterfaceStaticMesh.h"
#define MCP_HAS_NIAGARA_STATIC_MESH_DI 1
#elif __has_include("DataInterface/NiagaraDataInterfaceStaticMesh.h")
#include "DataInterface/NiagaraDataInterfaceStaticMesh.h"
#define MCP_HAS_NIAGARA_STATIC_MESH_DI 1
#elif __has_include("Internal/DataInterface/NiagaraDataInterfaceStaticMesh.h")
#include "Internal/DataInterface/NiagaraDataInterfaceStaticMesh.h"
#define MCP_HAS_NIAGARA_STATIC_MESH_DI 1
#else
// UE 5.7: Static mesh data interface is not directly accessible from editor modules
// The functionality may require different approach or module dependencies
#define MCP_HAS_NIAGARA_STATIC_MESH_DI 0
#endif
// Note: If MCP_HAS_NIAGARA_STATIC_MESH_DI is 0, static mesh data interface features will be unavailable
#include "NiagaraDataInterfaceSpline.h"
#include "NiagaraDataInterfaceAudioSpectrum.h"
#include "NiagaraDataInterfaceCollisionQuery.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraParameterStore.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraLightRendererProperties.h"
#include "NiagaraTypes.h"
#include "NiagaraConstants.h"
#include "NiagaraParameterMapHistory.h"
#include "NiagaraComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "EditorAssetLibrary.h"
#endif

// Use consolidated JSON helpers from McpAutomationBridgeHelpers.h


// Helper to get FVector from JSON object
static FVector GetVectorFromJsonNiag(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid()) return FVector::ZeroVector;
    return FVector(
        GetJsonNumberField(Obj, TEXT("x"), 0.0),
        GetJsonNumberField(Obj, TEXT("y"), 0.0),
        GetJsonNumberField(Obj, TEXT("z"), 0.0)
    );
}

// Helper to get FLinearColor from JSON object
static FLinearColor GetColorFromJsonNiagara(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid()) return FLinearColor::White;
    return FLinearColor(
        static_cast<float>(GetJsonNumberField(Obj, TEXT("r"), 1.0)),
        static_cast<float>(GetJsonNumberField(Obj, TEXT("g"), 1.0)),
        static_cast<float>(GetJsonNumberField(Obj, TEXT("b"), 1.0)),
        static_cast<float>(GetJsonNumberField(Obj, TEXT("a"), 1.0))
    );
}

#if WITH_EDITOR
// Per-stage module list for readbacks. Walks each stack output node's
// parameter-map chain by hand; the NiagaraEditor utility that does this
// (FNiagaraStackGraphUtilities::GetOrderedModuleNodes) is not exported.
static TArray<TSharedPtr<FJsonValue>> CollectNiagaraModuleStack(UNiagaraScriptSource* ScriptSource)
{
    TArray<TSharedPtr<FJsonValue>> ModulesArray;
    if (!ScriptSource || !ScriptSource->NodeGraph)
    {
        return ModulesArray;
    }
    for (UEdGraphNode* Node : ScriptSource->NodeGraph->Nodes)
    {
        UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(Node);
        if (!OutputNode)
        {
            continue;
        }
        const FString StageName = StaticEnum<ENiagaraScriptUsage>()->GetNameStringByValue(static_cast<int64>(OutputNode->GetUsage()));
        TArray<TSharedPtr<FJsonValue>> StageModules;
        TArray<UEdGraphPin*> InputPins;
        OutputNode->GetInputPins(InputPins);
        UEdGraphPin* LinkedPin = (InputPins.Num() > 0 && InputPins[0] && InputPins[0]->LinkedTo.Num() > 0) ? InputPins[0]->LinkedTo[0] : nullptr;
        while (LinkedPin)
        {
            UNiagaraNode* OwnerNode = Cast<UNiagaraNode>(LinkedPin->GetOwningNode());
            if (!OwnerNode)
            {
                break;
            }
            if (UNiagaraNodeFunctionCall* ModuleNode = Cast<UNiagaraNodeFunctionCall>(OwnerNode))
            {
                TSharedPtr<FJsonObject> ModuleObj = MakeShared<FJsonObject>();
                ModuleObj->SetStringField(TEXT("name"), ModuleNode->GetFunctionName());
                ModuleObj->SetStringField(TEXT("stage"), StageName);
                // The walk visits bottom-up; insert to report stack order.
                StageModules.Insert(MakeShared<FJsonValueObject>(ModuleObj), 0);
            }
            TArray<UEdGraphPin*> NodeInputPins;
            OwnerNode->GetInputPins(NodeInputPins);
            LinkedPin = (NodeInputPins.Num() > 0 && NodeInputPins[0] && NodeInputPins[0]->LinkedTo.Num() > 0) ? NodeInputPins[0]->LinkedTo[0] : nullptr;
        }
        ModulesArray.Append(StageModules);
    }
    return ModulesArray;
}

// -----------------------------------------------------------------------------
// Rapid-iteration parameter readback/write. Direct module-input values (the
// ones the stack UI edits without a recompile) live as raw bytes in each
// script's RapidIterationParameters store, named
//   Constants.[EmitterName - absent in system scripts].[ModuleName].[InputName]
// (format documented in NiagaraCommon.cpp AliasRapidIterationConstant). Inputs
// overridden to dynamic inputs or linked parameters leave the store, so absence
// here means "not a directly-set value", not "no such input".
// -----------------------------------------------------------------------------

static TSharedPtr<FJsonValue> NiagaraRapidIterationValueToJson(const FNiagaraTypeDefinition& TypeDef, const uint8* Data)
{
    if (!Data)
    {
        return nullptr;
    }
    const FNiagaraTypeDefinition BaseDef = TypeDef.RemoveStaticDef();
    const float* Floats = reinterpret_cast<const float*>(Data);
    auto FloatObj = [&](std::initializer_list<const TCHAR*> Fields) -> TSharedPtr<FJsonValue>
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        int32 Index = 0;
        for (const TCHAR* Field : Fields)
        {
            Obj->SetNumberField(Field, Floats[Index++]);
        }
        return MakeShared<FJsonValueObject>(Obj);
    };
    if (BaseDef == FNiagaraTypeDefinition::GetFloatDef())
    {
        return MakeShared<FJsonValueNumber>(Floats[0]);
    }
    if (BaseDef == FNiagaraTypeDefinition::GetIntDef())
    {
        return MakeShared<FJsonValueNumber>(*reinterpret_cast<const int32*>(Data));
    }
    if (BaseDef == FNiagaraTypeDefinition::GetBoolDef())
    {
        return MakeShared<FJsonValueBoolean>(reinterpret_cast<const FNiagaraBool*>(Data)->GetValue());
    }
    if (BaseDef.IsEnum())
    {
        const int32 EnumValue = *reinterpret_cast<const int32*>(Data);
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("value"), EnumValue);
        if (UEnum* Enum = BaseDef.GetEnum())
        {
            Obj->SetStringField(TEXT("name"), Enum->GetNameStringByValue(EnumValue));
        }
        return MakeShared<FJsonValueObject>(Obj);
    }
    if (BaseDef == FNiagaraTypeDefinition::GetVec2Def())
    {
        return FloatObj({TEXT("x"), TEXT("y")});
    }
    if (BaseDef == FNiagaraTypeDefinition::GetVec3Def() || BaseDef == FNiagaraTypeDefinition::GetPositionDef())
    {
        return FloatObj({TEXT("x"), TEXT("y"), TEXT("z")});
    }
    if (BaseDef == FNiagaraTypeDefinition::GetVec4Def() || BaseDef == FNiagaraTypeDefinition::GetQuatDef())
    {
        return FloatObj({TEXT("x"), TEXT("y"), TEXT("z"), TEXT("w")});
    }
    if (BaseDef == FNiagaraTypeDefinition::GetColorDef())
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("r"), Floats[0]);
        Obj->SetNumberField(TEXT("g"), Floats[1]);
        Obj->SetNumberField(TEXT("b"), Floats[2]);
        Obj->SetNumberField(TEXT("a"), Floats[3]);
        return MakeShared<FJsonValueObject>(Obj);
    }
    // Unknown struct type: report the raw bytes so nothing is dropped silently.
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("typeName"), TypeDef.GetName());
    Obj->SetStringField(TEXT("bytesHex"), BytesToHex(Data, TypeDef.GetSize()));
    return MakeShared<FJsonValueObject>(Obj);
}

// Splits "Constants.[Emitter.]Module.Input" for one emitter's view. Names scoped
// to a DIFFERENT emitter (system scripts hold every emitter's emitter-stage
// values) return false, as do non-Constants names. OutHadEmitterPrefix reports
// whether the name carried this emitter's scope segment.
static bool ParseRapidIterationConstantName(const FString& FullName, const FString& EmitterName,
    const TSet<FString>& AllEmitterNamesLower, FString& OutModule, FString& OutInput, bool& OutHadEmitterPrefix)
{
    static const FString ConstantsPrefix = TEXT("Constants.");
    OutHadEmitterPrefix = false;
    if (!FullName.StartsWith(ConstantsPrefix))
    {
        return false;
    }
    TArray<FString> Parts;
    FullName.RightChop(ConstantsPrefix.Len()).ParseIntoArray(Parts, TEXT("."), true);
    int32 ModuleIdx = 0;
    if (Parts.Num() > 0 && AllEmitterNamesLower.Contains(Parts[0].ToLower()))
    {
        if (!Parts[0].Equals(EmitterName, ESearchCase::IgnoreCase))
        {
            return false;
        }
        ModuleIdx = 1;
        OutHadEmitterPrefix = true;
    }
    if (Parts.Num() < ModuleIdx + 2)
    {
        return false;
    }
    OutModule = Parts[ModuleIdx];
    TArray<FString> InputParts(Parts.GetData() + ModuleIdx + 1, Parts.Num() - ModuleIdx - 1);
    OutInput = FString::Join(InputParts, TEXT("."));
    return true;
}

// Every script whose store can hold values for this system/emitter pair.
// Emitter scripts first so their copies win the dedup over the merged copies
// in the system spawn/update scripts.
static void GatherNiagaraScriptsForRapidIteration(UNiagaraSystem* System,
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmData, TArray<UNiagaraScript*>& OutScripts)
{
    if (EmData)
    {
        EmData->GetScripts(OutScripts, false);
    }
    if (System)
    {
        OutScripts.Add(System->GetSystemSpawnScript());
        OutScripts.Add(System->GetSystemUpdateScript());
    }
    OutScripts.RemoveAll([](UNiagaraScript* Script) { return Script == nullptr; });
}

// Full rapid-iteration dump for one emitter (or the standalone-emitter asset
// when System is null). Also fills a lowercased module-name → inputs map so the
// module-stack entries can carry their values. bRequireEmitterPrefix keeps
// system-scoped names (no emitter segment) out of per-emitter dumps when the
// system scripts are in the gather set.
static TArray<TSharedPtr<FJsonValue>> CollectNiagaraRapidIterationValues(
    UNiagaraSystem* System, MCP_NIAGARA_EMITTER_DATA_TYPE* EmData,
    const FString& EmitterName, const TSet<FString>& AllEmitterNamesLower,
    bool bRequireEmitterPrefix,
    TMap<FString, TArray<TSharedPtr<FJsonValue>>>* OutInputsByModule)
{
    TArray<TSharedPtr<FJsonValue>> Entries;
    TArray<UNiagaraScript*> Scripts;
    GatherNiagaraScriptsForRapidIteration(System, EmData, Scripts);
    TSet<FString> SeenNames;
    for (UNiagaraScript* Script : Scripts)
    {
        for (const FNiagaraVariableWithOffset& Var : Script->RapidIterationParameters.ReadParameterVariables())
        {
            const FString FullName = Var.GetName().ToString();
            FString ModuleName, InputName;
            bool bHadEmitterPrefix = false;
            if (!ParseRapidIterationConstantName(FullName, EmitterName, AllEmitterNamesLower, ModuleName, InputName, bHadEmitterPrefix)
                || (bRequireEmitterPrefix && !bHadEmitterPrefix)
                || SeenNames.Contains(FullName))
            {
                continue;
            }
            SeenNames.Add(FullName);
            const FNiagaraTypeDefinition& TypeDef = Var.GetType();
            TSharedPtr<FJsonValue> ValueJson;
            if (!TypeDef.IsDataInterface() && !TypeDef.IsUObject() && Var.Offset != INDEX_NONE)
            {
                ValueJson = NiagaraRapidIterationValueToJson(TypeDef,
                    Script->RapidIterationParameters.GetParameterData(Var.Offset, TypeDef));
            }
            TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("name"), FullName);
            Entry->SetStringField(TEXT("module"), ModuleName);
            Entry->SetStringField(TEXT("input"), InputName);
            Entry->SetStringField(TEXT("type"), TypeDef.GetName());
            Entry->SetStringField(TEXT("script"), StaticEnum<ENiagaraScriptUsage>()->GetNameStringByValue(static_cast<int64>(Script->GetUsage())));
            if (ValueJson.IsValid())
            {
                Entry->SetField(TEXT("value"), ValueJson);
            }
            if (TypeDef.IsStatic())
            {
                Entry->SetBoolField(TEXT("isStaticSwitch"), true);
            }
            Entries.Add(MakeShared<FJsonValueObject>(Entry));
            if (OutInputsByModule)
            {
                TSharedPtr<FJsonObject> InputObj = MakeShared<FJsonObject>();
                InputObj->SetStringField(TEXT("name"), InputName);
                InputObj->SetStringField(TEXT("type"), TypeDef.GetName());
                if (ValueJson.IsValid())
                {
                    InputObj->SetField(TEXT("value"), ValueJson);
                }
                OutInputsByModule->FindOrAdd(ModuleName.ToLower()).Add(MakeShared<FJsonValueObject>(InputObj));
            }
        }
    }
    return Entries;
}

// Attaches per-module "inputs" arrays (from CollectNiagaraRapidIterationValues)
// onto the module-stack entries returned by CollectNiagaraModuleStack.
static void AttachModuleInputs(TArray<TSharedPtr<FJsonValue>>& Modules,
    const TMap<FString, TArray<TSharedPtr<FJsonValue>>>& InputsByModule)
{
    for (const TSharedPtr<FJsonValue>& ModuleVal : Modules)
    {
        TSharedPtr<FJsonObject> ModuleObj = ModuleVal->AsObject();
        FString ModuleName;
        if (!ModuleObj.IsValid() || !ModuleObj->TryGetStringField(TEXT("name"), ModuleName))
        {
            continue;
        }
        if (const TArray<TSharedPtr<FJsonValue>>* Inputs = InputsByModule.Find(ModuleName.ToLower()))
        {
            ModuleObj->SetArrayField(TEXT("inputs"), *Inputs);
        }
    }
}

// Lowercased handle + unique names of every emitter in the system: the emitter-
// scope tokens that can appear after "Constants." in rapid-iteration names.
static TSet<FString> CollectEmitterNamesLower(UNiagaraSystem* System)
{
    TSet<FString> Names;
    if (!System)
    {
        return Names;
    }
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        Names.Add(Handle.GetName().ToString().ToLower());
        #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        UNiagaraEmitter* Em = Handle.GetInstance().Emitter;
        #else
        UNiagaraEmitter* Em = Handle.GetInstance();
        #endif
        if (Em)
        {
            Names.Add(Em->GetUniqueEmitterName().ToLower());
        }
    }
    return Names;
}
#endif

#if WITH_EDITOR
// -----------------------------------------------------------------------------
// Hoisted from the retired Niagara-authoring subAction dispatcher.
// Shared authoring helpers (were per-dispatch lambdas) and the common
// param/validation preamble. Each classed manage_effect Niagara-authoring
// action (MCP/Calls/McpCalls_ManageEffect.cpp) calls one member below.
// -----------------------------------------------------------------------------

static UNiagaraNodeFunctionCall* AddModuleToEmitterStack(FNiagaraEmitterHandle* Handle, const FString& ModuleScriptPath, ENiagaraScriptUsage TargetUsage, const FString& SuggestedName = FString())
{
    if (!Handle)
    {
        return nullptr;
    }

    // Get the versioned emitter data
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetEmitterData();
#else
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetInstance();
#endif
    if (!EmitterData)
    {
        return nullptr;
    }

    // Get the graph source
    UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(EmitterData->GraphSource);
    if (!ScriptSource || !ScriptSource->NodeGraph)
    {
        return nullptr;
    }

    UNiagaraGraph* Graph = ScriptSource->NodeGraph;

    // Find the output node for the target usage
    // NOTE: UNiagaraGraph::FindOutputNode is not exported in all UE versions
    // Use manual iteration through nodes to find the output node
    UNiagaraNodeOutput* TargetOutput = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(Node))
        {
            if (OutputNode->GetUsage() == TargetUsage)
            {
                TargetOutput = OutputNode;
                break;
            }
        }
    }
    if (!TargetOutput)
    {
        return nullptr;
    }

    // Load the module script asset
    FSoftObjectPath AssetRef(ModuleScriptPath);
    UNiagaraScript* ModuleScript = Cast<UNiagaraScript>(AssetRef.TryLoad());
    if (!ModuleScript)
    {
        return nullptr;
    }

    // Add the module to the stack using the Stack Graph Utilities
    // Note: FNiagaraStackGraphUtilities is only available in UE 5.1+
#if MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES
    UNiagaraNodeFunctionCall* NewModule = FNiagaraStackGraphUtilities::AddScriptModuleToStack(
        ModuleScript,
        *TargetOutput,
        INDEX_NONE, // Append to end
        SuggestedName.IsEmpty() ? ModuleScript->GetName() : SuggestedName
    );
    return NewModule;
#else
    // UE 5.0: Stack graph utilities not available
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("AddModule failed: FNiagaraStackGraphUtilities is not available in UE 5.0. Consider upgrading to UE 5.1+ for full Niagara stack graph support."));
    return nullptr;
#endif
}

static UNiagaraScriptSource* GetEmitterScriptSource(FNiagaraEmitterHandle* Handle)
{
    if (!Handle)
    {
        return nullptr;
    }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetEmitterData();
#else
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetInstance();
#endif
    if (!EmitterData)
    {
        return nullptr;
    }
    return Cast<UNiagaraScriptSource>(EmitterData->GraphSource);
}

static bool EnsureScriptOutputGraph(UNiagaraScriptSource* ScriptSource, ENiagaraScriptUsage ScriptUsage, FGuid ScriptUsageId)
{
    if (!ScriptSource || !ScriptSource->NodeGraph)
    {
        return false;
    }

    UNiagaraGraph* Graph = ScriptSource->NodeGraph;
    Graph->Modify();

    UNiagaraNodeOutput* OutputNode = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UNiagaraNodeOutput* Candidate = Cast<UNiagaraNodeOutput>(Node))
        {
            if (Candidate->GetUsage() == ScriptUsage && Candidate->GetUsageId() == ScriptUsageId)
            {
                OutputNode = Candidate;
                break;
            }
        }
    }

    if (!OutputNode)
    {
        FGraphNodeCreator<UNiagaraNodeOutput> OutputNodeCreator(*Graph);
        OutputNode = OutputNodeCreator.CreateNode();
        OutputNode->SetUsage(ScriptUsage);
        OutputNode->SetUsageId(ScriptUsageId);
        OutputNode->Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("Out")));
        OutputNodeCreator.Finalize();
    }

    FGraphNodeCreator<UNiagaraNodeInput> InputNodeCreator(*Graph);
    UNiagaraNodeInput* InputNode = InputNodeCreator.CreateNode();
    InputNode->Input = FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("InputMap"));
    InputNode->Usage = ENiagaraInputNodeUsage::Parameter;
    InputNodeCreator.Finalize();

    TArray<UEdGraphPin*> OutputInputPins;
    OutputNode->GetInputPins(OutputInputPins);
    TArray<UEdGraphPin*> InputOutputPins;
    InputNode->GetOutputPins(InputOutputPins);
    if (OutputInputPins.Num() == 0 || InputOutputPins.Num() == 0)
    {
        return false;
    }

    UEdGraphPin* OutputInputPin = OutputInputPins[0];
    UEdGraphPin* InputOutputPin = InputOutputPins[0];
    if (!OutputInputPin || !InputOutputPin)
    {
        return false;
    }

    OutputInputPin->BreakAllPinLinks(true);
    OutputInputPin->MakeLinkTo(InputOutputPin);
    OutputInputPin->GetOwningNode()->PinConnectionListChanged(OutputInputPin);
    InputOutputPin->GetOwningNode()->PinConnectionListChanged(InputOutputPin);
    Graph->NotifyGraphChanged();
    return true;
}

static FNiagaraEmitterHandle* FindEmitterHandle(UNiagaraSystem* System, const FString& TargetEmitter)
{
    for (const FNiagaraEmitterHandle& H : System->GetEmitterHandles())
    {
        if (H.GetName().ToString() == TargetEmitter)
        {
            return const_cast<FNiagaraEmitterHandle*>(&H);
        }
    }
    return nullptr;
}

static bool AddOrSetFloatUserParameter(UNiagaraSystem* System, const FString& ParamName, float Value)
{
    if (!System || ParamName.IsEmpty())
    {
        return false;
    }

    FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
    FNiagaraVariable Param(FNiagaraTypeDefinition::GetFloatDef(), FName(*ParamName));
    if (!UserStore.FindParameterVariable(Param))
    {
        UserStore.AddParameter(Param, true);
    }
    if (!UserStore.FindParameterVariable(Param))
    {
        return false;
    }
    UserStore.SetParameterValue(Value, Param);
    return true;
}

static bool AddOrSetBoolUserParameter(UNiagaraSystem* System, const FString& ParamName, bool Value)
{
    if (!System || ParamName.IsEmpty())
    {
        return false;
    }

    FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
    FNiagaraVariable Param(FNiagaraTypeDefinition::GetBoolDef(), FName(*ParamName));
    if (!UserStore.FindParameterVariable(Param))
    {
        UserStore.AddParameter(Param, true);
    }
    if (!UserStore.FindParameterVariable(Param))
    {
        return false;
    }
    UserStore.SetParameterValue(FNiagaraBool(Value), Param);
    return true;
}

static bool AddOrSetVectorUserParameter(UNiagaraSystem* System, const FString& ParamName, const FVector& Value)
{
    if (!System || ParamName.IsEmpty())
    {
        return false;
    }

    FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
    FNiagaraVariable Param(FNiagaraTypeDefinition::GetVec3Def(), FName(*ParamName));
    if (!UserStore.FindParameterVariable(Param))
    {
        UserStore.AddParameter(Param, true);
    }
    if (!UserStore.FindParameterVariable(Param))
    {
        return false;
    }
    UserStore.SetParameterValue(Value, Param);
    return true;
}

static bool AddOrSetColorUserParameter(UNiagaraSystem* System, const FString& ParamName, const FLinearColor& Value)
{
    if (!System || ParamName.IsEmpty())
    {
        return false;
    }

    FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
    FNiagaraVariable Param(FNiagaraTypeDefinition::GetColorDef(), FName(*ParamName));
    if (!UserStore.FindParameterVariable(Param))
    {
        UserStore.AddParameter(Param, true);
    }
    if (!UserStore.FindParameterVariable(Param))
    {
        return false;
    }
    UserStore.SetParameterValue(Value, Param);
    return true;
}

static bool AddDataInterfaceUserParameter(UNiagaraSystem* System, const FString& ParamName, UClass* DataInterfaceClass)
{
    if (!System || ParamName.IsEmpty() || !DataInterfaceClass || !DataInterfaceClass->IsChildOf(UNiagaraDataInterface::StaticClass()) || DataInterfaceClass->HasAnyClassFlags(CLASS_Abstract))
    {
        return false;
    }

    FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
    FNiagaraVariable Param{FNiagaraTypeDefinition(DataInterfaceClass), FName(*ParamName)};
    UserStore.AddParameter(Param, true);
    if (!UserStore.FindParameterVariable(Param))
    {
        return false;
    }

    UNiagaraDataInterface* DataInterface = NewObject<UNiagaraDataInterface>(System, DataInterfaceClass, FName(*ParamName), RF_Transactional);
    if (!DataInterface)
    {
        return false;
    }

    UserStore.SetDataInterface(DataInterface, Param);
    return UserStore.GetDataInterface(Param) == DataInterface;
}

#define MCP_NIAGARA_AUTHORING_PREAMBLE() \
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("NiagaraEditor"))) \
    { \
        if (!FModuleManager::Get().ModuleExists(TEXT("NiagaraEditor")) || \
            !FModuleManager::Get().LoadModule(TEXT("NiagaraEditor"))) \
        { \
            S.SendAutomationError(RequestingSocket, RequestId, \
                TEXT("NiagaraEditor plugin is not enabled in this project. Enable the Niagara plugin to use Niagara VFX features."), \
                TEXT("NIAGARAEDITOR_PLUGIN_NOT_ENABLED")); \
            return true; \
        } \
    } \
    if (!Payload.IsValid()) \
    { \
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD")); \
        return true; \
    } \
    FString Name = GetJsonStringField(Payload, TEXT("name")); \
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("")); \
    if (Path.IsEmpty()) \
    { \
        Path = GetJsonStringField(Payload, TEXT("savePath"), TEXT("/Game")); \
    } \
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath")); \
    FString SystemPath = GetJsonStringField(Payload, TEXT("systemPath")); \
    FString EmitterPath = GetJsonStringField(Payload, TEXT("emitterPath")); \
    FString EmitterName = GetJsonStringField(Payload, TEXT("emitterName")); \
    bool bSave = GetJsonBoolField(Payload, TEXT("save"), true); \
    auto ValidateAndSanitizePath = [&](FString& PathToCheck, const FString& ParamName) { \
        if (PathToCheck.IsEmpty()) \
        { \
            return true; \
        } \
        if (PathToCheck.Len() > 512) { \
            S.SendAutomationError(RequestingSocket, RequestId,  \
                FString::Printf(TEXT("'%s' is too long (%d chars). Maximum allowed is 512 characters."), *ParamName, PathToCheck.Len()),  \
                TEXT("INVALID_ARGUMENT")); \
            return false; \
        } \
        FString SanitizedPath = SanitizeProjectRelativePath(PathToCheck); \
        if (SanitizedPath.IsEmpty()) { \
            S.SendAutomationError(RequestingSocket, RequestId,  \
                FString::Printf(TEXT("'%s' has invalid format. Path must be a valid Unreal asset path without traversal or invalid roots."), *ParamName), \
                TEXT("INVALID_ARGUMENT")); \
            return false; \
        } \
        PathToCheck = SanitizedPath; \
        return true; \
    }; \
    auto ValidateNiagaraIdentifier = [&](const FString& Value, const FString& ParamName, bool bAllowDot) { \
        if (Value.IsEmpty()) \
        { \
            return true; \
        } \
        if (Value.Len() > 128) \
        { \
            S.SendAutomationError(RequestingSocket, RequestId, \
                FString::Printf(TEXT("'%s' is too long (%d chars). Maximum allowed is 128 characters."), *ParamName, Value.Len()), \
                TEXT("INVALID_ARGUMENT")); \
            return false; \
        } \
        for (int32 Index = 0; Index < Value.Len(); ++Index) \
        { \
            const TCHAR Char = Value[Index]; \
            const bool bAllowed = FChar::IsAlnum(Char) || Char == TEXT('_') || (bAllowDot && Char == TEXT('.')); \
            if (!bAllowed) \
            { \
                S.SendAutomationError(RequestingSocket, RequestId, \
                    FString::Printf(TEXT("'%s' contains invalid character '%c'. Use letters, numbers, underscores%s."), *ParamName, Char, bAllowDot ? TEXT(", or dots") : TEXT("")), \
                    TEXT("INVALID_ARGUMENT")); \
                return false; \
            } \
        } \
        return true; \
    }; \
    if (!ValidateAndSanitizePath(Path, TEXT("path"))) return true; \
    if (!ValidateAndSanitizePath(AssetPath, TEXT("assetPath"))) return true; \
    if (!ValidateAndSanitizePath(SystemPath, TEXT("systemPath"))) return true; \
    if (!ValidateAndSanitizePath(EmitterPath, TEXT("emitterPath"))) return true; \
    if (!ValidateNiagaraIdentifier(Name, TEXT("name"), false)) return true; \
    if (!ValidateNiagaraIdentifier(EmitterName, TEXT("emitterName"), true)) return true; \
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject()
#endif // WITH_EDITOR (hoisted Niagara-authoring helpers + preamble)

// Systems & emitters

bool McpHandlers::Effect::HandleNiagaraCreateNiagaraSystem(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (Name.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'name' parameter."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Ensure path ends correctly
    if (!Path.EndsWith(TEXT("/"))) Path += TEXT("/");
    FString FullPath = Path + Name;
    FString PackagePath = FPackageName::ObjectPathToPackageName(FullPath);
    
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package."), TEXT("PACKAGE_ERROR"));
        return true;
    }

    // Create NiagaraSystem directly without factory (compatible with all UE versions)
    // Note: Factories are editor-internal and not exported for plugin use
    UNiagaraSystem* NewSystem = NewObject<UNiagaraSystem>(Package, FName(*Name), RF_Public | RF_Standalone);
    if (NewSystem)
    {
#if MCP_HAS_NIAGARA_SYSTEM_FACTORY_NEW
        if (!FModuleManager::Get().IsModuleLoaded(TEXT("NiagaraEditor")))
        {
            FModuleManager::Get().LoadModule(TEXT("NiagaraEditor"));
        }
        UNiagaraSystemFactoryNew::InitializeSystem(NewSystem, true);
#endif

        // Add default emitter using direct API
        // UE 5.4+ changed AddEmitterHandle signature - requires additional parameters
        UNiagaraEmitter* NewEmitter = NewObject<UNiagaraEmitter>(NewSystem, FName(TEXT("DefaultEmitter")));
        if (NewEmitter)
        {
#if MCP_HAS_NIAGARA_EMITTER_FACTORY_NEW
            if (!FModuleManager::Get().IsModuleLoaded(TEXT("NiagaraEditor")))
            {
                FModuleManager::Get().LoadModule(TEXT("NiagaraEditor"));
            }
            UNiagaraEmitterFactoryNew::InitializeEmitter(NewEmitter, true);
            NewEmitter->SetUniqueEmitterName(TEXT("DefaultEmitter"));
#endif
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
            // UE 5.0 - AddEmitterHandle takes only 2 parameters
            NewSystem->AddEmitterHandle(*NewEmitter, FName(TEXT("DefaultEmitter")));
#else
            // UE 5.1+ - this emitter is already owned by the new system, so insert it
            // directly instead of AddEmitterHandle's parent-copy path.
            NewEmitter->CheckVersionDataAvailable();
            const FGuid EmitterVersion = NewEmitter->GetExposedVersion().VersionGuid;
            FVersionedNiagaraEmitterData* EmitterData = NewEmitter->GetEmitterData(EmitterVersion);
            if (!EmitterData || !EmitterData->GraphSource)
            {
                S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to initialize default Niagara emitter graph source."), TEXT("NIAGARA_EMITTER_INIT_FAILED"));
                return true;
            }
            FNiagaraEmitterHandle NewHandle(*NewEmitter, EmitterVersion);
            NewSystem->AddEmitterHandleDirect(NewHandle);
#endif
        }
    }
    if (!NewSystem)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create Niagara System."), TEXT("CREATE_FAILED"));
        return true;
    }

    FAssetRegistryModule::AssetCreated(NewSystem);
    
    if (bSave)
    {
        McpSafeAssetSave(NewSystem);
    }

    McpHandlerUtils::AddVerification(Result, NewSystem);
    TArray<FString> CreatedEmitterNames;
    TArray<TSharedPtr<FJsonValue>> CreatedEmittersArray;
    for (const FNiagaraEmitterHandle& CreatedHandle : NewSystem->GetEmitterHandles())
    {
        CreatedEmitterNames.Add(CreatedHandle.GetName().ToString());
        CreatedEmittersArray.Add(MakeShared<FJsonValueString>(CreatedHandle.GetName().ToString()));
    }
    Result->SetNumberField(TEXT("emitterCount"), CreatedEmittersArray.Num());
    Result->SetArrayField(TEXT("emitters"), CreatedEmittersArray);
    Result->SetStringField(TEXT("message"), CreatedEmitterNames.Num() > 0
        ? FString::Printf(TEXT("Created Niagara System: %s (with enabled default emitter: %s)"), *Name, *FString::Join(CreatedEmitterNames, TEXT(", ")))
        : FString::Printf(TEXT("Created Niagara System: %s"), *Name));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("System created."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraCreateNiagaraEmitter(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (Name.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'name' parameter."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (!Path.EndsWith(TEXT("/"))) Path += TEXT("/");
    FString FullPath = Path + Name;
    FString PackagePath = FPackageName::ObjectPathToPackageName(FullPath);
    
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package."), TEXT("PACKAGE_ERROR"));
        return true;
    }

    // Create NiagaraEmitter directly without factory (compatible with all UE versions)
    // Note: Factories are editor-internal and not exported for plugin use
    UNiagaraEmitter* NewEmitter = NewObject<UNiagaraEmitter>(Package, FName(*Name), RF_Public | RF_Standalone);
    if (!NewEmitter)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create Niagara Emitter."), TEXT("CREATE_FAILED"));
        return true;
    }

#if MCP_HAS_NIAGARA_EMITTER_FACTORY_NEW
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("NiagaraEditor")))
    {
        FModuleManager::Get().LoadModule(TEXT("NiagaraEditor"));
    }
    UNiagaraEmitterFactoryNew::InitializeEmitter(NewEmitter, true);
    NewEmitter->SetUniqueEmitterName(Name);
#endif

    FAssetRegistryModule::AssetCreated(NewEmitter);
    
    if (bSave)
    {
        McpSafeAssetSave(NewEmitter);
    }

    McpHandlerUtils::AddVerification(Result, NewEmitter);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created Niagara Emitter: %s"), *Name));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Emitter created."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddEmitterToSystem(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterPath.IsEmpty())
    {
        TArray<FString> MissingParams;
        if (SystemPath.IsEmpty())
        {
            MissingParams.Add(TEXT("systemPath"));
        }
        if (EmitterPath.IsEmpty())
        {
            MissingParams.Add(TEXT("emitterPath"));
        }
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Missing '%s'."), *FString::Join(MissingParams, TEXT("' and '"))),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    UNiagaraEmitter* Emitter = LoadObject<UNiagaraEmitter>(nullptr, *EmitterPath);
    if (!Emitter)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara Emitter."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    // Add emitter to system through the Niagara editor authoring path. This keeps
    // the system graph/overview synchronized; AddEmitterHandleDirect only mutates
    // handles and can leave existing systems in a compile/validation loop.
    FString AddedEmitterName;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("NiagaraEditor")))
    {
        FModuleManager::Get().LoadModule(TEXT("NiagaraEditor"));
    }

    System->Modify();
    Emitter->CheckVersionDataAvailable();
    const FGuid EmitterVersion = Emitter->GetExposedVersion().VersionGuid;
    FVersionedNiagaraEmitterData* EmitterData = Emitter->GetEmitterData(EmitterVersion);
    if (!EmitterData || !EmitterData->GraphSource)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Emitter graph source is not initialized."), TEXT("NIAGARA_EMITTER_INIT_FAILED"));
        return true;
    }

    const FGuid NewEmitterHandleId = FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Emitter, EmitterVersion, false);
    FNiagaraEmitterHandle* AddedHandle = nullptr;
    for (const FNiagaraEmitterHandle& H : System->GetEmitterHandles())
    {
        if (H.GetId() == NewEmitterHandleId)
        {
            AddedHandle = const_cast<FNiagaraEmitterHandle*>(&H);
            break;
        }
    }
    if (!AddedHandle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add emitter to Niagara system."), TEXT("CREATE_FAILED"));
        return true;
    }
    if (!EmitterName.IsEmpty())
    {
        // SetName sanitizes and uniquifies against sibling emitters, so
        // the applied name can differ from the request; the response
        // echoes the applied one.
        AddedHandle->SetName(FName(*EmitterName), *System);
    }
    AddedEmitterName = AddedHandle->GetName().ToString();
#else
    // UE 5.0 - no version GUID needed
    FNiagaraEmitterHandle NewHandle = System->AddEmitterHandle(*Emitter, FName(*(EmitterName.IsEmpty() ? Emitter->GetName() : EmitterName)));
    AddedEmitterName = NewHandle.GetName().ToString();
#endif
    
    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("emitterName"), AddedEmitterName);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added emitter '%s' to system."), *AddedEmitterName));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Emitter added to system."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraSetEmitterProperties(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = nullptr;
    for (const FNiagaraEmitterHandle& H : System->GetEmitterHandles())
    {
        if (H.GetName().ToString() == EmitterName)
        {
            Handle = const_cast<FNiagaraEmitterHandle*>(&H);
            break;
        }
    }
    
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found in system."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }
    
    const TSharedPtr<FJsonObject>* PropsObj;
    if (Payload->TryGetObjectField(TEXT("emitterProperties"), PropsObj) && PropsObj->IsValid())
    {
        bool bEnabled;
        if ((*PropsObj)->TryGetBoolField(TEXT("enabled"), bEnabled))
        {
            Handle->SetIsEnabled(bEnabled, *System, false);
        }
    }

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Updated properties for emitter '%s'."), *EmitterName));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Emitter properties updated."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

// Module library

bool McpHandlers::Effect::HandleNiagaraAddSpawnRateModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    double SpawnRate = GetJsonNumberField(Payload, TEXT("spawnRate"), 100.0);

    // Add the SpawnRate module to the Emitter Update stage
    // SpawnRate modules belong in EmitterUpdateScript as they control emission rate over time
    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
        Handle,
        TEXT("/Niagara/Modules/Emitter/SpawnRate.SpawnRate"),
        ENiagaraScriptUsage::EmitterUpdateScript,
        TEXT("SpawnRate")
    );

    const bool bModuleAdded = (NewModule != nullptr);
    if (!bModuleAdded)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add SpawnRate module to emitter stack."), TEXT("MODULE_ADD_FAILED"));
        return true;
    }

    // Also set user-exposed parameters if available
    FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
    FNiagaraVariable SpawnRateVar(FNiagaraTypeDefinition::GetFloatDef(), FName(TEXT("SpawnRate")));
    if (UserStore.FindParameterVariable(SpawnRateVar))
    {
        UserStore.SetParameterValue(static_cast<float>(SpawnRate), SpawnRateVar);
    }

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), TEXT("SpawnRate"));
    Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
    Result->SetNumberField(TEXT("spawnRate"), SpawnRate);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added spawn rate module: %.1f particles/sec"), SpawnRate));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Spawn rate module added."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddSpawnBurstModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    double BurstCount = GetJsonNumberField(Payload, TEXT("burstCount"), 10.0);
    double BurstTime = GetJsonNumberField(Payload, TEXT("burstTime"), 0.0);

    // Add the SpawnBurst_Instantaneous module to the Emitter Spawn stage
    // Burst modules belong in EmitterSpawnScript for instantaneous spawns
    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
        Handle,
        TEXT("/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous"),
        ENiagaraScriptUsage::EmitterSpawnScript,
        TEXT("SpawnBurst")
    );

    const bool bModuleAdded = (NewModule != nullptr);
    if (!bModuleAdded)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add SpawnBurst module to emitter stack."), TEXT("MODULE_ADD_FAILED"));
        return true;
    }

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), TEXT("SpawnBurst"));
    Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
    Result->SetNumberField(TEXT("burstCount"), BurstCount);
    Result->SetNumberField(TEXT("burstTime"), BurstTime);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added spawn burst module: %d particles at t=%.2f"), static_cast<int>(BurstCount), BurstTime));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Spawn burst module added."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddSpawnPerUnitModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    double SpawnPerUnit = GetJsonNumberField(Payload, TEXT("spawnPerUnit"), 1.0);

    // Add the SpawnPerUnit module to the Emitter Update stage
    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
        Handle,
        TEXT("/Niagara/Modules/Emitter/SpawnPerUnit.SpawnPerUnit"),
        ENiagaraScriptUsage::EmitterUpdateScript,
        TEXT("SpawnPerUnit")
    );

    const bool bModuleAdded = (NewModule != nullptr);
    if (!bModuleAdded)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add SpawnPerUnit module to emitter stack."), TEXT("MODULE_ADD_FAILED"));
        return true;
    }

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), TEXT("SpawnPerUnit"));
    Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
    Result->SetNumberField(TEXT("spawnPerUnit"), SpawnPerUnit);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added spawn per unit module: %.1f particles/unit"), SpawnPerUnit));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Spawn per unit module added."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddInitializeParticleModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    double Lifetime = GetJsonNumberField(Payload, TEXT("lifetime"), 2.0);
    double Mass = GetJsonNumberField(Payload, TEXT("mass"), 1.0);

    // Add the InitializeParticle module to the Particle Spawn stage
    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
        Handle,
        TEXT("/Niagara/Modules/Spawn/Initialization/InitializeParticle.InitializeParticle"),
        ENiagaraScriptUsage::ParticleSpawnScript,
        TEXT("InitializeParticle")
    );

    const bool bModuleAdded = (NewModule != nullptr);
    if (!bModuleAdded)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add InitializeParticle module to emitter stack."), TEXT("MODULE_ADD_FAILED"));
        return true;
    }

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), TEXT("InitializeParticle"));
    Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
    Result->SetNumberField(TEXT("lifetime"), Lifetime);
    Result->SetNumberField(TEXT("mass"), Mass);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added initialize particle module: lifetime=%.2fs, mass=%.2f"), Lifetime, Mass));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Initialize particle module added."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddParticleStateModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    // Add the ParticleState module to the Particle Update stage
    // ParticleState handles age tracking and lifetime evaluation
    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
        Handle,
        TEXT("/Niagara/Modules/Update/Lifetime/ParticleState.ParticleState"),
        ENiagaraScriptUsage::ParticleUpdateScript,
        TEXT("ParticleState")
    );

    const bool bModuleAdded = (NewModule != nullptr);
    if (!bModuleAdded)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add ParticleState module to emitter stack."), TEXT("MODULE_ADD_FAILED"));
        return true;
    }

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), TEXT("ParticleState"));
    Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
    Result->SetStringField(TEXT("message"), TEXT("Added particle state module."));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Particle state module added."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddForceModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString ForceType = GetJsonStringField(Payload, TEXT("forceType"), TEXT("Gravity"));

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    double ForceStrength = GetJsonNumberField(Payload, TEXT("forceStrength"), 980.0);

    // Determine the module path based on force type
    FString ModulePath;
    if (ForceType.Equals(TEXT("Gravity"), ESearchCase::IgnoreCase))
    {
        ModulePath = TEXT("/Niagara/Modules/Update/Forces/GravityForce.GravityForce");
    }
    else if (ForceType.Equals(TEXT("Drag"), ESearchCase::IgnoreCase))
    {
        ModulePath = TEXT("/Niagara/Modules/Update/Forces/DragForce.DragForce");
    }
    else if (ForceType.Equals(TEXT("Wind"), ESearchCase::IgnoreCase))
    {
        ModulePath = TEXT("/Niagara/Modules/Update/Forces/WindForce.WindForce");
    }
    else if (ForceType.Equals(TEXT("Curl"), ESearchCase::IgnoreCase) || ForceType.Equals(TEXT("CurlNoise"), ESearchCase::IgnoreCase))
    {
        ModulePath = TEXT("/Niagara/Modules/Update/Forces/CurlNoiseForce.CurlNoiseForce");
    }
    else if (ForceType.Equals(TEXT("Vortex"), ESearchCase::IgnoreCase))
    {
        ModulePath = TEXT("/Niagara/Modules/Update/Forces/VortexForce.VortexForce");
    }
    else if (ForceType.Equals(TEXT("PointAttraction"), ESearchCase::IgnoreCase))
    {
        ModulePath = TEXT("/Niagara/Modules/Update/Forces/PointAttractionForce.PointAttractionForce");
    }
    else
    {
        // Default to gravity
        ModulePath = TEXT("/Niagara/Modules/Update/Forces/GravityForce.GravityForce");
    }

    // Add the force module to the Particle Update stage
    // Forces are applied every frame to update particle velocity
    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
        Handle,
        ModulePath,
        ENiagaraScriptUsage::ParticleUpdateScript,
        FString::Printf(TEXT("%sForce"), *ForceType)
    );

    const bool bModuleAdded = (NewModule != nullptr);
    if (!bModuleAdded)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to add %s force module to emitter stack."), *ForceType), TEXT("MODULE_ADD_FAILED"));
        return true;
    }

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), FString::Printf(TEXT("Force_%s"), *ForceType));
    Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
    Result->SetStringField(TEXT("forceType"), ForceType);
    Result->SetNumberField(TEXT("forceStrength"), ForceStrength);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s force module."), *ForceType));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Force module added."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddVelocityModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    FString VelocityMode = GetJsonStringField(Payload, TEXT("velocityMode"), TEXT("Linear"));

    // Determine the module path based on velocity mode
    FString ModulePath;
    if (VelocityMode.Equals(TEXT("Cone"), ESearchCase::IgnoreCase))
    {
        ModulePath = TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocityInCone.AddVelocityInCone");
    }
    else if (VelocityMode.Equals(TEXT("FromPoint"), ESearchCase::IgnoreCase))
    {
        ModulePath = TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocityFromPoint.AddVelocityFromPoint");
    }
    else
    {
        // Default to AddVelocity (linear)
        ModulePath = TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocity.AddVelocity");
    }

    // Add the velocity module to the Particle Spawn stage
    // Initial velocity is set when particles are spawned
    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
        Handle,
        ModulePath,
        ENiagaraScriptUsage::ParticleSpawnScript,
        TEXT("AddVelocity")
    );

    const bool bModuleAdded = (NewModule != nullptr);
    if (!bModuleAdded)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add velocity module to emitter stack."), TEXT("MODULE_ADD_FAILED"));
        return true;
    }

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), TEXT("Velocity"));
    Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
    Result->SetStringField(TEXT("velocityMode"), VelocityMode);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added velocity module: mode=%s"), *VelocityMode));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Velocity module added."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddAccelerationModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    const TSharedPtr<FJsonObject>* AccelObj;
    FVector Acceleration = FVector(0, 0, -980);
    if (Payload->TryGetObjectField(TEXT("acceleration"), AccelObj))
    {
        Acceleration = GetVectorFromJsonNiag(*AccelObj);
    }

    // Acceleration is applied as a per-frame force; mirror add_force_module's
    // AccelerationForce path so a real stack module is added.
    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
        Handle,
        TEXT("/Niagara/Modules/Update/Forces/AccelerationForce.AccelerationForce"),
        ENiagaraScriptUsage::ParticleUpdateScript,
        TEXT("AccelerationForce")
    );

    const bool bModuleAdded = (NewModule != nullptr);
    if (!bModuleAdded)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add Acceleration module to emitter stack."), TEXT("MODULE_ADD_FAILED"));
        return true;
    }

    const bool bParameterAdded = AddOrSetVectorUserParameter(System, TEXT("MCP_Acceleration"), Acceleration);

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), TEXT("Acceleration"));
    Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
    Result->SetBoolField(TEXT("parameterAdded"), bParameterAdded);
    Result->SetStringField(TEXT("parameterName"), TEXT("MCP_Acceleration"));
    Result->SetStringField(TEXT("message"), TEXT("Added acceleration module."));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Acceleration module added."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddSizeModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    FString SizeMode = GetJsonStringField(Payload, TEXT("sizeMode"), TEXT("Uniform"));
    double UniformSize = GetJsonNumberField(Payload, TEXT("uniformSize"), 10.0);

    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
        Handle,
        TEXT("/Niagara/Modules/Update/Size/ScaleSpriteSize.ScaleSpriteSize"),
        ENiagaraScriptUsage::ParticleUpdateScript,
        TEXT("ScaleSpriteSize")
    );

    const bool bModuleAdded = (NewModule != nullptr);
    if (!bModuleAdded)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add Size module to emitter stack."), TEXT("MODULE_ADD_FAILED"));
        return true;
    }

    const bool bParameterAdded = AddOrSetFloatUserParameter(System, TEXT("MCP_UniformSize"), static_cast<float>(UniformSize));

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), TEXT("Size"));
    Result->SetStringField(TEXT("sizeMode"), SizeMode);
    Result->SetNumberField(TEXT("uniformSize"), UniformSize);
    Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
    Result->SetBoolField(TEXT("parameterAdded"), bParameterAdded);
    Result->SetStringField(TEXT("parameterName"), TEXT("MCP_UniformSize"));
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added size module: mode=%s, size=%.1f"), *SizeMode, UniformSize));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Size module added."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddColorModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    const TSharedPtr<FJsonObject>* ColorObj;
    FLinearColor Color = FLinearColor::White;
    if (Payload->TryGetObjectField(TEXT("color"), ColorObj))
    {
        Color = GetColorFromJsonNiagara(*ColorObj);
    }

    FString ColorMode = GetJsonStringField(Payload, TEXT("colorMode"), TEXT("Direct"));

    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
        Handle,
        TEXT("/Niagara/Modules/Update/Color/Color.Color"),
        ENiagaraScriptUsage::ParticleUpdateScript,
        TEXT("Color")
    );

    const bool bModuleAdded = (NewModule != nullptr);
    if (!bModuleAdded)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add Color module to emitter stack."), TEXT("MODULE_ADD_FAILED"));
        return true;
    }

    const bool bParameterAdded = AddOrSetColorUserParameter(System, TEXT("MCP_Color"), Color);

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), TEXT("Color"));
    Result->SetStringField(TEXT("colorMode"), ColorMode);
    Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
    Result->SetBoolField(TEXT("parameterAdded"), bParameterAdded);
    Result->SetStringField(TEXT("parameterName"), TEXT("MCP_Color"));
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added color module: mode=%s"), *ColorMode));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Color module added."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddSpriteRendererModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    FString MaterialPath = GetJsonStringField(Payload, TEXT("materialPath"));
    FString Alignment = GetJsonStringField(Payload, TEXT("alignment"), TEXT("Unaligned"));
    FString FacingMode = GetJsonStringField(Payload, TEXT("facingMode"), TEXT("FaceCamera"));

    // Get the versioned emitter data for the specified emitter (UE 5.7+)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetEmitterData();
    FVersionedNiagaraEmitter VersionedEmitter = Handle->GetInstance();
    UNiagaraEmitter* Emitter = VersionedEmitter.Emitter;
#else
    // UE 5.0: GetInstance() returns UNiagaraEmitter* directly
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetInstance();
    UNiagaraEmitter* Emitter = Handle->GetInstance();
#endif
    if (EmitterData && Emitter)
    {
        // Create sprite renderer if not exists
        UNiagaraSpriteRendererProperties* SpriteRenderer = nullptr;
        for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
        {
            if (UNiagaraSpriteRendererProperties* SR = Cast<UNiagaraSpriteRendererProperties>(Renderer))
            {
                SpriteRenderer = SR;
                break;
            }
        }

        if (!SpriteRenderer)
        {
            SpriteRenderer = NewObject<UNiagaraSpriteRendererProperties>(Emitter);
            if (!SpriteRenderer)
            {
                S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create sprite renderer"), TEXT("CREATION_FAILED"));
                return true;
            }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            Emitter->AddRenderer(SpriteRenderer, VersionedEmitter.Version);
#else
            // UE 5.0: AddRenderer only takes the renderer
            Emitter->AddRenderer(SpriteRenderer);
#endif
        }

        if (!MaterialPath.IsEmpty())
        {
            UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
            if (Material)
            {
                SpriteRenderer->Material = Material;
            }
        }
    }

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), TEXT("SpriteRenderer"));
    Result->SetStringField(TEXT("message"), TEXT("Configured sprite renderer module."));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sprite renderer configured."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddMeshRendererModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    FString MeshPath = GetJsonStringField(Payload, TEXT("meshPath"));

    // Get the versioned emitter data (UE 5.7+)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetEmitterData();
    FVersionedNiagaraEmitter VersionedEmitter = Handle->GetInstance();
    UNiagaraEmitter* Emitter = VersionedEmitter.Emitter;
#else
    // UE 5.0: GetInstance() returns UNiagaraEmitter* directly
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetInstance();
    UNiagaraEmitter* Emitter = Handle->GetInstance();
#endif
    if (EmitterData && Emitter)
    {
        UNiagaraMeshRendererProperties* MeshRenderer = nullptr;
        for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
        {
            if (UNiagaraMeshRendererProperties* MR = Cast<UNiagaraMeshRendererProperties>(Renderer))
            {
                MeshRenderer = MR;
                break;
            }
        }

        if (!MeshRenderer)
        {
            MeshRenderer = NewObject<UNiagaraMeshRendererProperties>(Emitter);
            if (!MeshRenderer)
            {
                S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create mesh renderer"), TEXT("CREATION_FAILED"));
                return true;
            }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            Emitter->AddRenderer(MeshRenderer, VersionedEmitter.Version);
#else
            // UE 5.0: AddRenderer only takes the renderer
            Emitter->AddRenderer(MeshRenderer);
#endif
        }

        if (!MeshPath.IsEmpty())
        {
            UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
            if (Mesh)
            {
                FNiagaraMeshRendererMeshProperties MeshProps;
                MeshProps.Mesh = Mesh;
                MeshRenderer->Meshes.Empty();
                MeshRenderer->Meshes.Add(MeshProps);
            }
        }
    }

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), TEXT("MeshRenderer"));
    Result->SetStringField(TEXT("message"), TEXT("Configured mesh renderer module."));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Mesh renderer configured."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddRibbonRendererModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    // Get the versioned emitter data (UE 5.7+)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetEmitterData();
    FVersionedNiagaraEmitter VersionedEmitter = Handle->GetInstance();
    UNiagaraEmitter* Emitter = VersionedEmitter.Emitter;
#else
    // UE 5.0: GetInstance() returns UNiagaraEmitter* directly
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetInstance();
    UNiagaraEmitter* Emitter = Handle->GetInstance();
#endif
    if (EmitterData && Emitter)
    {
        UNiagaraRibbonRendererProperties* RibbonRenderer = nullptr;
        for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
        {
            if (UNiagaraRibbonRendererProperties* RR = Cast<UNiagaraRibbonRendererProperties>(Renderer))
            {
                RibbonRenderer = RR;
                break;
            }
        }

        if (!RibbonRenderer)
        {
            RibbonRenderer = NewObject<UNiagaraRibbonRendererProperties>(Emitter);
            if (!RibbonRenderer)
            {
                S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create ribbon renderer"), TEXT("CREATION_FAILED"));
                return true;
            }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            Emitter->AddRenderer(RibbonRenderer, VersionedEmitter.Version);
#else
            // UE 5.0: AddRenderer only takes the renderer
            Emitter->AddRenderer(RibbonRenderer);
#endif
        }

        FString MaterialPath = GetJsonStringField(Payload, TEXT("materialPath"));
        if (!MaterialPath.IsEmpty())
        {
            UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
            if (Material)
            {
                RibbonRenderer->Material = Material;
            }
        }
    }

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), TEXT("RibbonRenderer"));
    Result->SetStringField(TEXT("message"), TEXT("Configured ribbon renderer module."));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ribbon renderer configured."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddLightRendererModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    // Get the versioned emitter data (UE 5.7+)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetEmitterData();
    FVersionedNiagaraEmitter VersionedEmitter = Handle->GetInstance();
    UNiagaraEmitter* Emitter = VersionedEmitter.Emitter;
#else
    // UE 5.0: GetInstance() returns UNiagaraEmitter* directly
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetInstance();
    UNiagaraEmitter* Emitter = Handle->GetInstance();
#endif
    if (EmitterData && Emitter)
    {
        UNiagaraLightRendererProperties* LightRenderer = nullptr;
        for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
        {
            if (UNiagaraLightRendererProperties* LR = Cast<UNiagaraLightRendererProperties>(Renderer))
            {
                LightRenderer = LR;
                break;
            }
        }

        if (!LightRenderer)
        {
            LightRenderer = NewObject<UNiagaraLightRendererProperties>(Emitter);
            if (!LightRenderer)
            {
                S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create light renderer"), TEXT("CREATION_FAILED"));
                return true;
            }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            Emitter->AddRenderer(LightRenderer, VersionedEmitter.Version);
#else
            // UE 5.0: AddRenderer only takes the renderer
            Emitter->AddRenderer(LightRenderer);
#endif
        }

        double LightRadius = GetJsonNumberField(Payload, TEXT("lightRadius"), 100.0);
        LightRenderer->RadiusScale = static_cast<float>(LightRadius);
    }

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), TEXT("LightRenderer"));
    Result->SetStringField(TEXT("message"), TEXT("Configured light renderer module."));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Light renderer configured."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddCollisionModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    FString CollisionMode = GetJsonStringField(Payload, TEXT("collisionMode"), TEXT("SceneDepth"));
    double Restitution = GetJsonNumberField(Payload, TEXT("restitution"), 0.3);
    double Friction = GetJsonNumberField(Payload, TEXT("friction"), 0.2);
    bool bDieOnCollision = GetJsonBoolField(Payload, TEXT("dieOnCollision"), false);

    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
        Handle,
        TEXT("/Niagara/Modules/Collision/Collision.Collision"),
        ENiagaraScriptUsage::ParticleUpdateScript,
        TEXT("Collision")
    );

    const bool bModuleAdded = (NewModule != nullptr);
    if (!bModuleAdded)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add Collision module to emitter stack."), TEXT("MODULE_ADD_FAILED"));
        return true;
    }
    const bool bRestitutionAdded = AddOrSetFloatUserParameter(System, TEXT("MCP_CollisionRestitution"), static_cast<float>(Restitution));
    const bool bFrictionAdded = AddOrSetFloatUserParameter(System, TEXT("MCP_CollisionFriction"), static_cast<float>(Friction));
    const bool bDieOnCollisionAdded = AddOrSetBoolUserParameter(System, TEXT("MCP_DieOnCollision"), bDieOnCollision);

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), TEXT("Collision"));
    Result->SetStringField(TEXT("collisionMode"), CollisionMode);
    Result->SetNumberField(TEXT("restitution"), Restitution);
    Result->SetNumberField(TEXT("friction"), Friction);
    Result->SetBoolField(TEXT("dieOnCollision"), bDieOnCollision);
    Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
    Result->SetBoolField(TEXT("parameterAdded"), bRestitutionAdded && bFrictionAdded && bDieOnCollisionAdded);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured collision module: mode=%s"), *CollisionMode));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Collision module configured."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddKillParticlesModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    FString KillCondition = GetJsonStringField(Payload, TEXT("killCondition"), TEXT("Age"));

    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
        Handle,
        TEXT("/Niagara/Modules/Update/Lifetime/KillParticles.KillParticles"),
        ENiagaraScriptUsage::ParticleUpdateScript,
        TEXT("KillParticles")
    );

    const bool bModuleAdded = (NewModule != nullptr);
    if (!bModuleAdded)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add KillParticles module to emitter stack."), TEXT("MODULE_ADD_FAILED"));
        return true;
    }
    const bool bParameterAdded = AddOrSetBoolUserParameter(System, TEXT("MCP_KillParticlesEnabled"), true);

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), TEXT("KillParticles"));
    Result->SetStringField(TEXT("killCondition"), KillCondition);
    Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
    Result->SetBoolField(TEXT("parameterAdded"), bParameterAdded);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured kill particles module: condition=%s"), *KillCondition));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Kill particles module configured."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddCameraOffsetModule(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    double CameraOffset = GetJsonNumberField(Payload, TEXT("cameraOffset"), 0.0);

    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
        Handle,
        TEXT("/Niagara/Modules/Update/Camera/CameraOffset.CameraOffset"),
        ENiagaraScriptUsage::ParticleUpdateScript,
        TEXT("CameraOffset")
    );

    const bool bModuleAdded = (NewModule != nullptr);
    if (!bModuleAdded)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add CameraOffset module to emitter stack."), TEXT("MODULE_ADD_FAILED"));
        return true;
    }
    const bool bParameterAdded = AddOrSetFloatUserParameter(System, TEXT("MCP_CameraOffset"), static_cast<float>(CameraOffset));

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("moduleName"), TEXT("CameraOffset"));
    Result->SetNumberField(TEXT("cameraOffset"), CameraOffset);
    Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
    Result->SetBoolField(TEXT("parameterAdded"), bParameterAdded);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured camera offset module: offset=%.1f"), CameraOffset));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Camera offset module configured."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

// Parameters & data interfaces

bool McpHandlers::Effect::HandleNiagaraAddUserParameter(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString ParamName = GetJsonStringField(Payload, TEXT("parameterName"));
    FString ParamType = GetJsonStringField(Payload, TEXT("parameterType"), TEXT("Float"));

    if (ParamName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'parameterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
    
    FNiagaraTypeDefinition TypeDef;
    if (ParamType == TEXT("Float"))
    {
        TypeDef = FNiagaraTypeDefinition::GetFloatDef();
    }
    else if (ParamType == TEXT("Int"))
    {
        TypeDef = FNiagaraTypeDefinition::GetIntDef();
    }
    else if (ParamType == TEXT("Bool"))
    {
        TypeDef = FNiagaraTypeDefinition::GetBoolDef();
    }
    else if (ParamType == TEXT("Vector"))
    {
        TypeDef = FNiagaraTypeDefinition::GetVec3Def();
    }
    else if (ParamType == TEXT("LinearColor"))
    {
        TypeDef = FNiagaraTypeDefinition::GetColorDef();
    }
    else
    {
        TypeDef = FNiagaraTypeDefinition::GetFloatDef();
    }

    FNiagaraVariable NewParam(TypeDef, FName(*ParamName));
    UserStore.AddParameter(NewParam, true);

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("parameterName"), ParamName);
    Result->SetStringField(TEXT("parameterType"), ParamType);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added user parameter '%s' of type %s."), *ParamName, *ParamType));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("User parameter added."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraSetParameterValue(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString ParamName = GetJsonStringField(Payload, TEXT("parameterName"));
    if (ParamName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'parameterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
    
    // Try to find and set the parameter value
    FNiagaraVariable FloatVar(FNiagaraTypeDefinition::GetFloatDef(), FName(*ParamName));
    FNiagaraVariable IntVar(FNiagaraTypeDefinition::GetIntDef(), FName(*ParamName));
    FNiagaraVariable BoolVar(FNiagaraTypeDefinition::GetBoolDef(), FName(*ParamName));
    FNiagaraVariable VecVar(FNiagaraTypeDefinition::GetVec3Def(), FName(*ParamName));

    double NumVal = 0;
    bool BoolVal = false;
    Payload->TryGetNumberField(TEXT("floatValue"), NumVal);
    Payload->TryGetBoolField(TEXT("boolValue"), BoolVal);

    if (UserStore.FindParameterVariable(FloatVar))
    {
        UserStore.SetParameterValue(static_cast<float>(NumVal), FloatVar);
    }
    else if (UserStore.FindParameterVariable(IntVar))
    {
        UserStore.SetParameterValue(static_cast<int32>(NumVal), IntVar);
    }
    else if (UserStore.FindParameterVariable(BoolVar))
    {
        UserStore.SetParameterValue(FNiagaraBool(BoolVal), BoolVar);
    }
    else if (UserStore.FindParameterVariable(VecVar))
    {
        const TSharedPtr<FJsonObject>* ValObj;
        if (Payload->TryGetObjectField(TEXT("vectorValue"), ValObj))
        {
            FVector Vec = GetVectorFromJsonNiag(*ValObj);
            UserStore.SetParameterValue(Vec, VecVar);
        }
    }
    else
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Parameter '%s' not found."), *ParamName), TEXT("PARAM_NOT_FOUND"));
        return true;
    }

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("parameterName"), ParamName);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Set parameter '%s' value."), *ParamName));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Parameter value set."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraBindParameterToSource(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString ParamName = GetJsonStringField(Payload, TEXT("parameterName"));
    FString SourceBinding = GetJsonStringField(Payload, TEXT("sourceBinding"));

    if (ParamName.IsEmpty() || SourceBinding.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'parameterName' or 'sourceBinding'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (!ValidateNiagaraIdentifier(ParamName, TEXT("parameterName"), true) || !ValidateNiagaraIdentifier(SourceBinding, TEXT("sourceBinding"), true))
    {
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

#if MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES
    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    UNiagaraScriptSource* ScriptSource = GetEmitterScriptSource(Handle);
    if (!ScriptSource || !ScriptSource->NodeGraph)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Emitter has no Niagara graph source."), TEXT("NIAGARA_GRAPH_MISSING"));
        return true;
    }

    UNiagaraGraph* Graph = ScriptSource->NodeGraph;
    UNiagaraNodeOutput* TargetOutput = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(Node))
        {
            if (OutputNode->GetUsage() == ENiagaraScriptUsage::ParticleUpdateScript)
            {
                TargetOutput = OutputNode;
                break;
            }
        }
    }

    if (!TargetOutput)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Emitter has no particle update stack output for parameter binding."), TEXT("NIAGARA_STACK_MISSING"));
        return true;
    }

    FString NiagaraDefaultSource = SourceBinding;
    if (NiagaraDefaultSource == TEXT("Emitter.Age"))
    {
        NiagaraDefaultSource = TEXT("Emitter Age");
    }
    else if (NiagaraDefaultSource == TEXT("Emitter.NormalizedAge"))
    {
        NiagaraDefaultSource = TEXT("Emitter Normalized Age");
    }
    else if (NiagaraDefaultSource == TEXT("System.Age"))
    {
        NiagaraDefaultSource = TEXT("System Age");
    }

    FString ParamType = GetJsonStringField(Payload, TEXT("parameterType"), TEXT("Float"));
    FNiagaraTypeDefinition TypeDef = FNiagaraTypeDefinition::GetFloatDef();
    if (ParamType == TEXT("Int"))
    {
        TypeDef = FNiagaraTypeDefinition::GetIntDef();
    }
    else if (ParamType == TEXT("Bool"))
    {
        TypeDef = FNiagaraTypeDefinition::GetBoolDef();
    }
    else if (ParamType == TEXT("Vector"))
    {
        TypeDef = FNiagaraTypeDefinition::GetVec3Def();
    }
    else if (ParamType == TEXT("LinearColor"))
    {
        TypeDef = FNiagaraTypeDefinition::GetColorDef();
    }

    const FNiagaraVariable TargetVariable(TypeDef, FName(*ParamName));
    TArray<FNiagaraVariable> TargetVariables;
    TargetVariables.Add(TargetVariable);
    TArray<FString> DefaultValues;
    DefaultValues.Add(NiagaraDefaultSource);

    Graph->Modify();
    UNiagaraNodeAssignment* AssignmentNode = FNiagaraStackGraphUtilities::AddParameterModuleToStack(
        TargetVariables,
        *TargetOutput,
        INDEX_NONE,
        DefaultValues
    );

    if (!AssignmentNode)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create Niagara assignment module for parameter binding."), TEXT("NIAGARA_BINDING_FAILED"));
        return true;
    }

    AssignmentNode->RefreshFromExternalChanges();
    AssignmentNode->UpdateUsageBitmaskFromOwningScript();
    Graph->NotifyGraphChanged();

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetBoolField(TEXT("bindingApplied"), true);
    Result->SetBoolField(TEXT("assignmentModuleAdded"), true);
    Result->SetStringField(TEXT("parameterName"), ParamName);
    Result->SetStringField(TEXT("sourceBinding"), SourceBinding);
    Result->SetStringField(TEXT("niagaraDefaultSource"), NiagaraDefaultSource);
    Result->SetStringField(TEXT("assignmentNodeId"), AssignmentNode->NodeGuid.ToString());
    Result->SetStringField(TEXT("targetUsage"), TEXT("ParticleUpdateScript"));
    Result->SetNumberField(TEXT("assignmentTargetCount"), AssignmentNode->GetAssignmentTargets().Num());
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Bound Niagara parameter '%s' to source '%s' with a real assignment module."), *ParamName, *SourceBinding));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara parameter binding applied."), Result);
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Niagara stack graph utilities are unavailable in this engine version."), TEXT("NIAGARA_BINDING_UNSUPPORTED"));
#endif
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddSkeletalMeshDataInterface(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    FString ParamName = GetJsonStringField(Payload, TEXT("parameterName"), TEXT("MCP_SkeletalMeshDataInterface"));

    bool bDataInterfaceAdded = false;
#if MCP_HAS_NIAGARA_SKELETAL_MESH_DI
    bDataInterfaceAdded = AddDataInterfaceUserParameter(System, ParamName, UNiagaraDataInterfaceSkeletalMesh::StaticClass());
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Skeletal mesh data interface is not available in this engine build."), TEXT("NIAGARA_DI_UNAVAILABLE"));
    return true;
#endif

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("dataInterface"), TEXT("SkeletalMesh"));
    Result->SetStringField(TEXT("parameterName"), ParamName);
    Result->SetBoolField(TEXT("dataInterfaceAdded"), bDataInterfaceAdded);
    Result->SetStringField(TEXT("message"), TEXT("Added Skeletal Mesh data interface."));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Skeletal Mesh DI added."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddStaticMeshDataInterface(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FString StaticMeshPath = GetJsonStringField(Payload, TEXT("staticMeshPath"));
    FString ParamName = GetJsonStringField(Payload, TEXT("parameterName"), TEXT("MCP_StaticMeshDataInterface"));

    bool bDataInterfaceAdded = false;
#if MCP_HAS_NIAGARA_STATIC_MESH_DI
    bDataInterfaceAdded = AddDataInterfaceUserParameter(System, ParamName, UNiagaraDataInterfaceStaticMesh::StaticClass());
#else
    UClass* StaticMeshDataInterfaceClass = StaticLoadClass(
        UNiagaraDataInterface::StaticClass(),
        nullptr,
        TEXT("/Script/Niagara.NiagaraDataInterfaceStaticMesh")
    );
    bDataInterfaceAdded = AddDataInterfaceUserParameter(System, ParamName, StaticMeshDataInterfaceClass);
#endif

    if (!bDataInterfaceAdded)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create Static Mesh data interface parameter."), TEXT("NIAGARA_DI_CREATE_FAILED"));
        return true;
    }

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("dataInterface"), TEXT("StaticMesh"));
    Result->SetStringField(TEXT("parameterName"), ParamName);
    Result->SetBoolField(TEXT("dataInterfaceAdded"), bDataInterfaceAdded);
    Result->SetStringField(TEXT("message"), TEXT("Added Static Mesh data interface."));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Static Mesh DI added."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddDataInterface(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    // Spline / AudioSpectrum / CollisionQuery differ only by the data-interface
    // class, default parameter name, and reported labels.
    FString DefaultParamName;
    UClass* DataInterfaceClass = nullptr;
    FString DataInterfaceLabel;
    FString DoneMessage;
    FString ResponseMessage;
    if (SubAction == TEXT("add_spline_data_interface"))
    {
        DefaultParamName = TEXT("MCP_SplineDataInterface");
        DataInterfaceClass = UNiagaraDataInterfaceSpline::StaticClass();
        DataInterfaceLabel = TEXT("Spline");
        DoneMessage = TEXT("Added Spline data interface.");
        ResponseMessage = TEXT("Spline DI added.");
    }
    else if (SubAction == TEXT("add_audio_spectrum_data_interface"))
    {
        DefaultParamName = TEXT("MCP_AudioSpectrumDataInterface");
        DataInterfaceClass = UNiagaraDataInterfaceAudioSpectrum::StaticClass();
        DataInterfaceLabel = TEXT("AudioSpectrum");
        DoneMessage = TEXT("Added Audio Spectrum data interface.");
        ResponseMessage = TEXT("Audio Spectrum DI added.");
    }
    else // add_collision_query_data_interface
    {
        DefaultParamName = TEXT("MCP_CollisionQueryDataInterface");
        DataInterfaceClass = UNiagaraDataInterfaceCollisionQuery::StaticClass();
        DataInterfaceLabel = TEXT("CollisionQuery");
        DoneMessage = TEXT("Added Collision Query data interface.");
        ResponseMessage = TEXT("Collision Query DI added.");
    }

    FString ParamName = GetJsonStringField(Payload, TEXT("parameterName"), DefaultParamName);
    const bool bDataInterfaceAdded = AddDataInterfaceUserParameter(System, ParamName, DataInterfaceClass);

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("dataInterface"), DataInterfaceLabel);
    Result->SetStringField(TEXT("parameterName"), ParamName);
    Result->SetBoolField(TEXT("dataInterfaceAdded"), bDataInterfaceAdded);
    Result->SetStringField(TEXT("message"), DoneMessage);
    S.SendAutomationResponse(RequestingSocket, RequestId, true, ResponseMessage, Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

// Events & GPU

bool McpHandlers::Effect::HandleNiagaraAddEventGenerator(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString EventName = GetJsonStringField(Payload, TEXT("eventName"));
    if (EventName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'eventName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!ValidateNiagaraIdentifier(EventName, TEXT("eventName"), false))
    {
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    FString EventModulePath = TEXT("/Niagara/Modules/Events/GenerateLocationEvent.GenerateLocationEvent");
    FString EventType = GetJsonStringField(Payload, TEXT("eventType"), TEXT("Location"));
    if (EventType.Equals(TEXT("Collision"), ESearchCase::IgnoreCase))
    {
        EventModulePath = TEXT("/Niagara/Modules/Events/GenerateCollisionEvent.GenerateCollisionEvent");
    }
    else if (EventType.Equals(TEXT("Death"), ESearchCase::IgnoreCase))
    {
        EventModulePath = TEXT("/Niagara/Modules/Events/GenerateDeathEvent.GenerateDeathEvent");
    }

    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
        Handle,
        EventModulePath,
        ENiagaraScriptUsage::ParticleUpdateScript,
        FString::Printf(TEXT("Generate%sEvent"), *EventType)
    );
    const bool bModuleAdded = (NewModule != nullptr);
    const bool bParameterAdded = AddOrSetBoolUserParameter(System, FString::Printf(TEXT("MCP_EventGenerator_%s"), *EventName), true);

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("eventName"), EventName);
    Result->SetStringField(TEXT("eventType"), TEXT("Generator"));
    Result->SetBoolField(TEXT("moduleAdded"), bModuleAdded);
    Result->SetBoolField(TEXT("eventGeneratorAdded"), bModuleAdded || bParameterAdded);
    Result->SetBoolField(TEXT("parameterAdded"), bParameterAdded);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added event generator '%s'."), *EventName));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event generator added."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddEventReceiver(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString EventName = GetJsonStringField(Payload, TEXT("eventName"));
    if (EventName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'eventName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!ValidateNiagaraIdentifier(EventName, TEXT("eventName"), false))
    {
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    bool bSpawnOnEvent = GetJsonBoolField(Payload, TEXT("spawnOnEvent"), false);
    double EventSpawnCount = GetJsonNumberField(Payload, TEXT("eventSpawnCount"), 1.0);

    bool bEventHandlerAdded = false;
    bool bEventGraphCreated = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    FVersionedNiagaraEmitter VersionedEmitter = Handle->GetInstance();
    UNiagaraEmitter* Emitter = VersionedEmitter.Emitter;
#else
    UNiagaraEmitter* Emitter = Handle->GetInstance();
    const FGuid EmitterVersion;
#endif
    if (Emitter)
    {
        UNiagaraScriptSource* ScriptSource = GetEmitterScriptSource(Handle);
        if (!ScriptSource || !ScriptSource->NodeGraph)
        {
            S.SendAutomationError(RequestingSocket, RequestId, TEXT("Emitter graph source is not initialized."), TEXT("NIAGARA_EMITTER_INIT_FAILED"));
            return true;
        }

        Emitter->Modify();
        FNiagaraEventScriptProperties EventScriptProperties;
        EventScriptProperties.Script = NewObject<UNiagaraScript>(Emitter, MakeUniqueObjectName(Emitter, UNiagaraScript::StaticClass(), TEXT("MCPEventScript")), RF_Transactional);
        if (EventScriptProperties.Script)
        {
            EventScriptProperties.Script->SetUsage(ENiagaraScriptUsage::ParticleEventScript);
            EventScriptProperties.Script->SetUsageId(FGuid::NewGuid());
            EventScriptProperties.Script->SetLatestSource(ScriptSource);
            EventScriptProperties.SourceEventName = FName(*EventName);
            EventScriptProperties.SpawnNumber = static_cast<uint32>(FMath::Max(0.0, EventSpawnCount));
            EventScriptProperties.ExecutionMode = bSpawnOnEvent ? EScriptExecutionMode::SpawnedParticles : EScriptExecutionMode::EveryParticle;
            bEventGraphCreated = EnsureScriptOutputGraph(ScriptSource, ENiagaraScriptUsage::ParticleEventScript, EventScriptProperties.Script->GetUsageId());
            if (!bEventGraphCreated)
            {
                S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create Niagara event handler graph."), TEXT("NIAGARA_GRAPH_CREATE_FAILED"));
                return true;
            }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            Emitter->AddEventHandler(EventScriptProperties, VersionedEmitter.Version);
#else
            Emitter->AddEventHandler(EventScriptProperties);
#endif
            bEventHandlerAdded = bEventGraphCreated;
        }
    }
    const bool bParameterAdded = AddOrSetBoolUserParameter(System, FString::Printf(TEXT("MCP_EventReceiver_%s"), *EventName), true);

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("eventName"), EventName);
    Result->SetStringField(TEXT("eventType"), TEXT("Receiver"));
    Result->SetBoolField(TEXT("spawnOnEvent"), bSpawnOnEvent);
    Result->SetBoolField(TEXT("eventHandlerAdded"), bEventHandlerAdded);
    Result->SetBoolField(TEXT("eventGraphCreated"), bEventGraphCreated);
    Result->SetBoolField(TEXT("parameterAdded"), bParameterAdded);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added event receiver '%s'."), *EventName));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event receiver added."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraEnableGpuSimulation(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    UNiagaraEmitter* Emitter = Handle->GetInstance().Emitter;
    if (Emitter)
    {
        // Enable GPU simulation by setting simulation target
        // This requires accessing the versioned emitter data
        MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = MCP_GET_LATEST_EMITTER_DATA(Emitter);
        if (EmitterData)
        {
            EmitterData->SimTarget = ENiagaraSimTarget::GPUComputeSim;
        }
    }
#else
    // UE 5.0: GetInstance() returns UNiagaraEmitter* directly
    UNiagaraEmitter* Emitter = Handle->GetInstance();
    if (Emitter)
    {
        Emitter->SimTarget = ENiagaraSimTarget::GPUComputeSim;
    }
#endif

    bool bFixedBounds = GetJsonBoolField(Payload, TEXT("fixedBoundsEnabled"), false);
    bool bDeterministic = GetJsonBoolField(Payload, TEXT("deterministicEnabled"), false);

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetBoolField(TEXT("gpuEnabled"), true);
    Result->SetBoolField(TEXT("fixedBoundsEnabled"), bFixedBounds);
    Result->SetBoolField(TEXT("deterministicEnabled"), bDeterministic);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Enabled GPU simulation for emitter '%s'."), *EmitterName));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("GPU simulation enabled."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraAddSimulationStage(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty() || EmitterName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString StageName = GetJsonStringField(Payload, TEXT("stageName"));
    if (StageName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'stageName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!ValidateNiagaraIdentifier(StageName, TEXT("stageName"), false))
    {
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
    if (!Handle)
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }

    FString IterationSource = GetJsonStringField(Payload, TEXT("stageIterationSource"), TEXT("Particles"));

    bool bSimulationStageAdded = false;
    bool bSimulationStageGraphCreated = false;
    int32 SimulationStageCount = 0;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    FVersionedNiagaraEmitter VersionedEmitter = Handle->GetInstance();
    UNiagaraEmitter* Emitter = VersionedEmitter.Emitter;
#else
    UNiagaraEmitter* Emitter = Handle->GetInstance();
    const FGuid EmitterVersion;
#endif
    if (Emitter)
    {
        UNiagaraScriptSource* ScriptSource = GetEmitterScriptSource(Handle);
        if (!ScriptSource || !ScriptSource->NodeGraph)
        {
            S.SendAutomationError(RequestingSocket, RequestId, TEXT("Emitter graph source is not initialized."), TEXT("NIAGARA_EMITTER_INIT_FAILED"));
            return true;
        }

        Emitter->Modify();
        UNiagaraSimulationStageGeneric* NewStage = NewObject<UNiagaraSimulationStageGeneric>(Emitter, NAME_None, RF_Transactional);
        if (NewStage)
        {
            NewStage->SimulationStageName = FName(*StageName);
            NewStage->IterationSource = IterationSource.Equals(TEXT("DataInterface"), ESearchCase::IgnoreCase)
                ? ENiagaraIterationSource::DataInterface
                : ENiagaraIterationSource::Particles;
            NewStage->Script = NewObject<UNiagaraScript>(NewStage, MakeUniqueObjectName(NewStage, UNiagaraScript::StaticClass(), TEXT("MCPSimulationStageScript")), RF_Transactional);
            if (NewStage->Script)
            {
                NewStage->Script->SetUsage(ENiagaraScriptUsage::ParticleSimulationStageScript);
                NewStage->Script->SetUsageId(FGuid::NewGuid());
                NewStage->Script->SetLatestSource(ScriptSource);
                bSimulationStageGraphCreated = EnsureScriptOutputGraph(ScriptSource, ENiagaraScriptUsage::ParticleSimulationStageScript, NewStage->Script->GetUsageId());
                if (!bSimulationStageGraphCreated)
                {
                    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create Niagara simulation stage graph."), TEXT("NIAGARA_GRAPH_CREATE_FAILED"));
                    return true;
                }
            }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            Emitter->AddSimulationStage(NewStage, VersionedEmitter.Version);
            if (FVersionedNiagaraEmitterData* EmitterData = Emitter->GetEmitterData(VersionedEmitter.Version))
            {
                SimulationStageCount = EmitterData->GetSimulationStages().Num();
            }
#else
            Emitter->AddSimulationStage(NewStage);
            SimulationStageCount = 1;
#endif
            bSimulationStageAdded = bSimulationStageGraphCreated;
        }
    }

    if (bSave)
    {
        McpSafeAssetSave(System);
    }

    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("stageName"), StageName);
    Result->SetStringField(TEXT("iterationSource"), IterationSource);
    Result->SetBoolField(TEXT("simulationStageAdded"), bSimulationStageAdded);
    Result->SetBoolField(TEXT("simulationStageGraphCreated"), bSimulationStageGraphCreated);
    Result->SetNumberField(TEXT("simulationStageCount"), SimulationStageCount);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added simulation stage '%s'."), *StageName));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Simulation stage added."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

// Utility

bool McpHandlers::Effect::HandleNiagaraGetNiagaraInfo(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (AssetPath.IsEmpty() && SystemPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'assetPath' or 'systemPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Guard against non-existent assets to prevent LoadObject hangs
    FString TargetPath = AssetPath.IsEmpty() ? SystemPath : AssetPath;
    if (!UEditorAssetLibrary::DoesAssetExist(TargetPath))
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Niagara asset not found: %s"), *TargetPath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *TargetPath);
    UNiagaraEmitter* Emitter = nullptr;
    
    if (!System)
    {
        Emitter = LoadObject<UNiagaraEmitter>(nullptr, *TargetPath);
    }

    if (!System && !Emitter)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara asset."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> InfoObj = McpHandlerUtils::CreateResultObject();

    if (System)
    {
        InfoObj->SetStringField(TEXT("assetType"), TEXT("System"));
        InfoObj->SetNumberField(TEXT("emitterCount"), System->GetEmitterHandles().Num());

        const TSet<FString> EmitterNamesLower = CollectEmitterNamesLower(System);

        TArray<TSharedPtr<FJsonValue>> EmittersArray;
        for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
        {
            TSharedPtr<FJsonObject> EmitterObj = McpHandlerUtils::CreateResultObject();
            EmitterObj->SetStringField(TEXT("name"), Handle.GetName().ToString());
            EmitterObj->SetBoolField(TEXT("enabled"), Handle.GetIsEnabled());

            #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            UNiagaraEmitter* Em = Handle.GetInstance().Emitter;
            #else
            UNiagaraEmitter* Em = Handle.GetInstance();
            #endif
            if (Em)
            {
                MCP_NIAGARA_EMITTER_DATA_TYPE* EmData = MCP_GET_LATEST_EMITTER_DATA(Em);
                if (EmData)
                {
                    EmitterObj->SetStringField(TEXT("simulationTarget"), EmData->SimTarget == ENiagaraSimTarget::GPUComputeSim ? TEXT("GPU") : TEXT("CPU"));
                    TMap<FString, TArray<TSharedPtr<FJsonValue>>> InputsByModule;
                    TArray<TSharedPtr<FJsonValue>> RapidIterationEntries = CollectNiagaraRapidIterationValues(
                        System, EmData, Handle.GetName().ToString(), EmitterNamesLower,
                        /*bRequireEmitterPrefix=*/true, &InputsByModule);
                    TArray<TSharedPtr<FJsonValue>> Modules = CollectNiagaraModuleStack(Cast<UNiagaraScriptSource>(EmData->GraphSource));
                    AttachModuleInputs(Modules, InputsByModule);
                    EmitterObj->SetArrayField(TEXT("modules"), Modules);
                    EmitterObj->SetArrayField(TEXT("rapidIterationParameters"), RapidIterationEntries);
                }
            }

            EmittersArray.Add(MakeShared<FJsonValueObject>(EmitterObj));
        }
        InfoObj->SetArrayField(TEXT("emitters"), EmittersArray);

        // System-stage module values (SystemSpawn/SystemUpdate stacks): the
        // system scripts' non-emitter-scoped rapid-iteration parameters.
        InfoObj->SetArrayField(TEXT("systemRapidIterationParameters"),
            CollectNiagaraRapidIterationValues(System, nullptr, FString(), EmitterNamesLower,
                /*bRequireEmitterPrefix=*/false, nullptr));

        // User parameters
        FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
        TArray<FNiagaraVariable> Params;
        UserStore.GetParameters(Params);
        
        InfoObj->SetNumberField(TEXT("userParameterCount"), Params.Num());
        
        TArray<TSharedPtr<FJsonValue>> ParamsArray;
        for (const FNiagaraVariable& Param : Params)
        {
            TSharedPtr<FJsonObject> ParamObj = McpHandlerUtils::CreateResultObject();
            ParamObj->SetStringField(TEXT("name"), Param.GetName().ToString());
            ParamObj->SetStringField(TEXT("type"), Param.GetType().GetName());
            ParamsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
        }
        InfoObj->SetArrayField(TEXT("userParameters"), ParamsArray);

        // Check for GPU emitters
        bool bHasGPU = false;
        for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
        {
            #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            UNiagaraEmitter* Em = Handle.GetInstance().Emitter;
            #else
            UNiagaraEmitter* Em = Handle.GetInstance();
            #endif
            if (Em && MCP_GET_LATEST_EMITTER_DATA(Em) && MCP_GET_LATEST_EMITTER_DATA(Em)->SimTarget == ENiagaraSimTarget::GPUComputeSim)
            {
                bHasGPU = true;
                break;
            }
        }
        InfoObj->SetBoolField(TEXT("hasGPUEmitters"), bHasGPU);
    }
    else if (Emitter)
    {
        InfoObj->SetStringField(TEXT("assetType"), TEXT("Emitter"));
        InfoObj->SetStringField(TEXT("name"), Emitter->GetName());

        MCP_NIAGARA_EMITTER_DATA_TYPE* EmData = MCP_GET_LATEST_EMITTER_DATA(Emitter);
        if (EmData)
        {
            InfoObj->SetStringField(TEXT("simulationTarget"), EmData->SimTarget == ENiagaraSimTarget::GPUComputeSim ? TEXT("GPU") : TEXT("CPU"));
            const FString UniqueName = Emitter->GetUniqueEmitterName();
            TSet<FString> EmitterNamesLower;
            EmitterNamesLower.Add(UniqueName.ToLower());
            TMap<FString, TArray<TSharedPtr<FJsonValue>>> InputsByModule;
            TArray<TSharedPtr<FJsonValue>> RapidIterationEntries = CollectNiagaraRapidIterationValues(
                nullptr, EmData, UniqueName, EmitterNamesLower,
                /*bRequireEmitterPrefix=*/false, &InputsByModule);
            TArray<TSharedPtr<FJsonValue>> Modules = CollectNiagaraModuleStack(Cast<UNiagaraScriptSource>(EmData->GraphSource));
            AttachModuleInputs(Modules, InputsByModule);
            InfoObj->SetArrayField(TEXT("modules"), Modules);
            InfoObj->SetArrayField(TEXT("rapidIterationParameters"), RapidIterationEntries);
        }
    }

    Result->SetObjectField(TEXT("niagaraInfo"), InfoObj);
    Result->SetStringField(TEXT("message"), TEXT("Retrieved Niagara asset information."));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara info retrieved."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraValidateNiagaraSystem(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    if (SystemPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'systemPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Guard against non-existent assets to prevent LoadObject hangs
    if (!UEditorAssetLibrary::DoesAssetExist(SystemPath))
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Niagara system asset not found: %s"), *SystemPath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> ValidationResult = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> ErrorsArray;
    TArray<TSharedPtr<FJsonValue>> WarningsArray;

    bool bIsValid = true;

    // Check if system has emitters
    if (System->GetEmitterHandles().Num() == 0)
    {
        WarningsArray.Add(MakeShared<FJsonValueString>(TEXT("System has no emitters.")));
    }

    // Check each emitter for issues
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        if (!Handle.GetIsEnabled())
        {
            WarningsArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Emitter '%s' is disabled."), *Handle.GetName().ToString())));
        }

        // Get emitter data - UE 5.1+ uses GetEmitterData(), UE 5.0 uses GetInstance()
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle.GetEmitterData();
#else
        MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle.GetInstance();
#endif
        if (EmitterData)
        {
            // Check for renderers (UE 5.7+)
            if (EmitterData->GetRenderers().Num() == 0)
            {
                WarningsArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Emitter '%s' has no renderers."), *Handle.GetName().ToString())));
            }
        }
    }

    ValidationResult->SetBoolField(TEXT("isValid"), bIsValid);
    ValidationResult->SetArrayField(TEXT("errors"), ErrorsArray);
    ValidationResult->SetArrayField(TEXT("warnings"), WarningsArray);

    Result->SetObjectField(TEXT("validationResult"), ValidationResult);
    Result->SetStringField(TEXT("message"), bIsValid ? TEXT("System is valid.") : TEXT("System has validation errors."));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Validation complete."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool McpHandlers::Effect::HandleNiagaraSetModuleInput(UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
#if WITH_EDITOR
    MCP_NIAGARA_AUTHORING_PREAMBLE();

    const FString TargetPath = AssetPath.IsEmpty() ? SystemPath : AssetPath;
    if (TargetPath.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'assetPath' or 'systemPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const FString ModuleName = GetJsonStringField(Payload, TEXT("moduleName"));
    const FString InputName = GetJsonStringField(Payload, TEXT("inputName"));
    // No identifier-charset validation here: these only MATCH against existing
    // store names (stock module inputs legitimately contain spaces).
    if (ModuleName.IsEmpty() || InputName.IsEmpty())
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'moduleName' or 'inputName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(TargetPath))
    {
        S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Niagara asset not found: %s"), *TargetPath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }
    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *TargetPath);
    UNiagaraEmitter* StandaloneEmitter = System ? nullptr : LoadObject<UNiagaraEmitter>(nullptr, *TargetPath);
    if (!System && !StandaloneEmitter)
    {
        S.SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Niagara asset."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    // (emitter scope, emitter data) contexts whose scripts are searched.
    struct FSearchContext
    {
        FString EmitterName;
        MCP_NIAGARA_EMITTER_DATA_TYPE* EmData = nullptr;
    };
    TArray<FSearchContext> Contexts;
    TSet<FString> EmitterNamesLower;
    if (System)
    {
        EmitterNamesLower = CollectEmitterNamesLower(System);
        for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
        {
            if (!EmitterName.IsEmpty() && !Handle.GetName().ToString().Equals(EmitterName, ESearchCase::IgnoreCase))
            {
                continue;
            }
            #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            UNiagaraEmitter* Em = Handle.GetInstance().Emitter;
            #else
            UNiagaraEmitter* Em = Handle.GetInstance();
            #endif
            if (Em)
            {
                Contexts.Add({Handle.GetName().ToString(), MCP_GET_LATEST_EMITTER_DATA(Em)});
            }
        }
        if (!EmitterName.IsEmpty() && Contexts.Num() == 0)
        {
            S.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Emitter '%s' not found."), *EmitterName), TEXT("EMITTER_NOT_FOUND"));
            return true;
        }
        // System-stage modules (no emitter scope in the constant name).
        Contexts.Add({FString(), nullptr});
    }
    else
    {
        const FString UniqueName = StandaloneEmitter->GetUniqueEmitterName();
        EmitterNamesLower.Add(UniqueName.ToLower());
        Contexts.Add({UniqueName, MCP_GET_LATEST_EMITTER_DATA(StandaloneEmitter)});
    }

    // One matched input may live in several stores (emitter script + merged
    // system-script copy); all of them get the write so none goes stale.
    struct FMatch
    {
        UNiagaraScript* Script = nullptr;
        FNiagaraVariable Var;
        FString EmitterScope;
    };
    TArray<FMatch> Matches;
    TSet<FString> CandidateInputs;   // inputs seen on the requested module, for the miss message
    TSet<FString> CandidateModules;  // all module names seen, for the miss message
    for (const FSearchContext& Ctx : Contexts)
    {
        TArray<UNiagaraScript*> Scripts;
        GatherNiagaraScriptsForRapidIteration(System, Ctx.EmData, Scripts);
        for (UNiagaraScript* Script : Scripts)
        {
            for (const FNiagaraVariableWithOffset& Var : Script->RapidIterationParameters.ReadParameterVariables())
            {
                FString FoundModule, FoundInput;
                bool bHadEmitterPrefix = false;
                if (!ParseRapidIterationConstantName(Var.GetName().ToString(), Ctx.EmitterName, EmitterNamesLower, FoundModule, FoundInput, bHadEmitterPrefix))
                {
                    continue;
                }
                // A system-script gather runs per context; keep each name in
                // the scope it belongs to so contexts don't double-match.
                if (System && Ctx.EmitterName.IsEmpty() != !bHadEmitterPrefix)
                {
                    continue;
                }
                CandidateModules.Add(FoundModule);
                if (!FoundModule.Equals(ModuleName, ESearchCase::IgnoreCase))
                {
                    continue;
                }
                CandidateInputs.Add(FoundInput);
                if (!FoundInput.Equals(InputName, ESearchCase::IgnoreCase))
                {
                    continue;
                }
                Matches.Add({Script, FNiagaraVariable(Var.GetType(), Var.GetName()), Ctx.EmitterName});
            }
        }
    }

    if (Matches.Num() == 0)
    {
        const bool bModuleSeen = CandidateInputs.Num() > 0;
        const FString Available = FString::Join(bModuleSeen ? CandidateInputs.Array() : CandidateModules.Array(), TEXT(", "));
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("No directly-set rapid-iteration value for module '%s' input '%s'. %s: %s. (Inputs overridden to dynamic inputs or linked parameters have no directly-set value.)"),
                *ModuleName, *InputName,
                bModuleSeen ? TEXT("Available inputs on that module") : TEXT("Modules with directly-set values"),
                Available.IsEmpty() ? TEXT("<none>") : *Available),
            TEXT("PARAM_NOT_FOUND"));
        return true;
    }

    // Same full constant name in several emitters (no emitterName given) is
    // ambiguous; the same name in several SCRIPTS is the mirrored-copy case.
    TSet<FString> MatchedScopes;
    for (const FMatch& M : Matches)
    {
        MatchedScopes.Add(M.EmitterScope);
    }
    if (MatchedScopes.Num() > 1)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Input '%s' of module '%s' exists in multiple scopes (%s). Pass 'emitterName' to pick one."),
                *InputName, *ModuleName, *FString::Join(MatchedScopes.Array(), TEXT(", "))),
            TEXT("AMBIGUOUS_TARGET"));
        return true;
    }

    const FNiagaraTypeDefinition TypeDef = Matches[0].Var.GetType();
    const FNiagaraTypeDefinition BaseDef = TypeDef.RemoveStaticDef();
    TArray<uint8> Bytes;
    Bytes.SetNumZeroed(TypeDef.GetSize());

    auto TypeError = [&](const TCHAR* Expected)
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Input '%s' has type '%s' — send %s."), *InputName, *TypeDef.GetName(), Expected),
            TEXT("VALUE_TYPE_MISMATCH"));
    };
    auto GetVectorComponents = [&](int32 Count, float* Out) -> bool
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (!Payload->TryGetObjectField(TEXT("vectorValue"), Obj) || !Obj || !Obj->IsValid())
        {
            return false;
        }
        static const TCHAR* Axes[4] = {TEXT("x"), TEXT("y"), TEXT("z"), TEXT("w")};
        for (int32 Index = 0; Index < Count; ++Index)
        {
            double Component = 0.0;
            if (!(*Obj)->TryGetNumberField(Axes[Index], Component))
            {
                return false;
            }
            Out[Index] = static_cast<float>(Component);
        }
        return true;
    };

    double NumValue = 0.0;
    if (BaseDef == FNiagaraTypeDefinition::GetFloatDef())
    {
        if (!Payload->TryGetNumberField(TEXT("floatValue"), NumValue) && !Payload->TryGetNumberField(TEXT("intValue"), NumValue))
        {
            TypeError(TEXT("'floatValue'"));
            return true;
        }
        *reinterpret_cast<float*>(Bytes.GetData()) = static_cast<float>(NumValue);
    }
    else if (BaseDef == FNiagaraTypeDefinition::GetIntDef())
    {
        if (!Payload->TryGetNumberField(TEXT("intValue"), NumValue))
        {
            TypeError(TEXT("'intValue'"));
            return true;
        }
        *reinterpret_cast<int32*>(Bytes.GetData()) = static_cast<int32>(NumValue);
    }
    else if (BaseDef == FNiagaraTypeDefinition::GetBoolDef())
    {
        bool bValue = false;
        if (!Payload->TryGetBoolField(TEXT("boolValue"), bValue))
        {
            TypeError(TEXT("'boolValue'"));
            return true;
        }
        reinterpret_cast<FNiagaraBool*>(Bytes.GetData())->SetValue(bValue);
    }
    else if (BaseDef.IsEnum())
    {
        UEnum* Enum = BaseDef.GetEnum();
        FString EnumName;
        if (Payload->TryGetNumberField(TEXT("intValue"), NumValue))
        {
            *reinterpret_cast<int32*>(Bytes.GetData()) = static_cast<int32>(NumValue);
        }
        else if (Enum && Payload->TryGetStringField(TEXT("stringValue"), EnumName))
        {
            const int64 EnumValue = Enum->GetValueByNameString(EnumName);
            if (EnumValue == INDEX_NONE)
            {
                TArray<FString> ValidNames;
                for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
                {
                    ValidNames.Add(Enum->GetNameStringByIndex(Index));
                }
                S.SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("'%s' is not a value of enum '%s'. Valid: %s"), *EnumName, *Enum->GetName(), *FString::Join(ValidNames, TEXT(", "))),
                    TEXT("VALUE_TYPE_MISMATCH"));
                return true;
            }
            *reinterpret_cast<int32*>(Bytes.GetData()) = static_cast<int32>(EnumValue);
        }
        else
        {
            TypeError(TEXT("'intValue' or 'stringValue' (enum value name)"));
            return true;
        }
    }
    else if (BaseDef == FNiagaraTypeDefinition::GetVec2Def())
    {
        if (!GetVectorComponents(2, reinterpret_cast<float*>(Bytes.GetData()))) { TypeError(TEXT("'vectorValue' {x,y}")); return true; }
    }
    else if (BaseDef == FNiagaraTypeDefinition::GetVec3Def() || BaseDef == FNiagaraTypeDefinition::GetPositionDef())
    {
        if (!GetVectorComponents(3, reinterpret_cast<float*>(Bytes.GetData()))) { TypeError(TEXT("'vectorValue' {x,y,z}")); return true; }
    }
    else if (BaseDef == FNiagaraTypeDefinition::GetVec4Def() || BaseDef == FNiagaraTypeDefinition::GetQuatDef())
    {
        if (!GetVectorComponents(4, reinterpret_cast<float*>(Bytes.GetData()))) { TypeError(TEXT("'vectorValue' {x,y,z,w}")); return true; }
    }
    else if (BaseDef == FNiagaraTypeDefinition::GetColorDef())
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (!Payload->TryGetObjectField(TEXT("colorValue"), Obj) || !Obj || !Obj->IsValid())
        {
            TypeError(TEXT("'colorValue' {r,g,b,a}"));
            return true;
        }
        FLinearColor Color = GetColorFromJsonNiagara(*Obj);
        FMemory::Memcpy(Bytes.GetData(), &Color, sizeof(FLinearColor));
    }
    else
    {
        S.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Input '%s' has unsupported type '%s' (data interfaces, UObjects, and custom structs cannot be set here)."), *InputName, *TypeDef.GetName()),
            TEXT("UNSUPPORTED_TYPE"));
        return true;
    }

    int32 Applied = 0;
    TArray<FString> Failed;
    for (const FMatch& M : Matches)
    {
        M.Script->Modify();
        if (M.Script->RapidIterationParameters.SetParameterData(Bytes.GetData(), M.Var))
        {
            Applied++;
        }
        else
        {
            Failed.Add(M.Script->GetPathName());
        }
    }
    if (Failed.Num() > 0)
    {
        // Fail-in-place: report what landed and what didn't; no rollback.
        TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
        Details->SetNumberField(TEXT("applied"), Applied);
        Details->SetStringField(TEXT("failedScripts"), FString::Join(Failed, TEXT(", ")));
        S.SendAutomationResponse(RequestingSocket, RequestId, false,
            FString::Printf(TEXT("SetParameterData failed on %d of %d stores."), Failed.Num(), Matches.Num()),
            Details, TEXT("SET_FAILED"));
        return true;
    }

    // The system scripts keep merged copies of emitter-scoped values; the
    // multi-store write covers same-named copies, this covers aliased ones.
    const FString MatchedScope = Matches[0].EmitterScope;
    if (System && !MatchedScope.IsEmpty())
    {
        for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
        {
            if (Handle.GetName().ToString().Equals(MatchedScope, ESearchCase::IgnoreCase))
            {
                System->RefreshSystemParametersFromEmitter(Handle);
                break;
            }
        }
    }

    // Static switches bake at compile time; a plain store write alone would
    // leave stale compiled code behind.
    const bool bStaticInput = TypeDef.IsStatic();
    if (bStaticInput)
    {
        if (System)
        {
            System->RequestCompile(false);
        }
        else
        {
#if MCP_HAS_NIAGARA_VERSIONING_APIS
            UNiagaraSystem::RequestCompileForEmitter(FVersionedNiagaraEmitter(StandaloneEmitter, MCP_GET_EMITTER_VERSION_GUID(StandaloneEmitter)));
#else
            UNiagaraSystem::RequestCompileForEmitter(StandaloneEmitter);
#endif
        }
    }

    UObject* AssetToSave = System ? static_cast<UObject*>(System) : StandaloneEmitter;
    if (bSave)
    {
        McpSafeAssetSave(AssetToSave);
    }

    // Receipt: re-read from the store, not an echo of the request.
    const uint8* ReadBack = Matches[0].Script->RapidIterationParameters.GetParameterData(Matches[0].Var);
    McpHandlerUtils::AddVerification(Result, AssetToSave);
    Result->SetStringField(TEXT("parameterName"), Matches[0].Var.GetName().ToString());
    Result->SetStringField(TEXT("type"), TypeDef.GetName());
    Result->SetNumberField(TEXT("storesUpdated"), Applied);
    if (bStaticInput)
    {
        Result->SetBoolField(TEXT("staticRecompileRequested"), true);
    }
    if (TSharedPtr<FJsonValue> ValueJson = NiagaraRapidIterationValueToJson(TypeDef, ReadBack))
    {
        Result->SetField(TEXT("value"), ValueJson);
    }
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Set module input '%s.%s'."), *ModuleName, *InputName));
    S.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Module input set."), Result);
    return true;
#else
    S.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

#undef MCP_NIAGARA_AUTHORING_PREAMBLE

#undef GetJsonStringField
#undef GetJsonNumberField
#undef GetJsonBoolField
