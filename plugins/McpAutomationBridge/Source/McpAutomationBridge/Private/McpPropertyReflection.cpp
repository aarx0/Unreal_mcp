// =============================================================================
// McpPropertyReflection.cpp
// =============================================================================
// Implementation of property reflection and JSON conversion utilities.
// =============================================================================

#include "McpPropertyReflection.h"
#include "McpVersionCompatibility.h"
#include "JsonObjectConverter.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/TextProperty.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

// =============================================================================
// JSON Value Export Implementation
// =============================================================================

namespace McpPropertyReflection
{

// Bounds recursion when deep-exporting/importing nested structs (and arrays of
// structs). Object references are emitted as path strings rather than recursing
// into the referenced object, and structs are value types that cannot contain
// themselves, so true cycles are impossible — this cap only guards against
// pathologically deep nesting and runaway output. At the cap, struct export
// falls back to a single ExportText string.
static constexpr int32 GMcpMaxReflectionDepth = 6;

TSharedPtr<FJsonValue> ExportPropertyToJsonValue(void* TargetContainer, FProperty* Property, int32 Depth)
{
    if (!TargetContainer || !Property)
    {
        return nullptr;
    }

    // Strings
    if (FStrProperty* Str = CastField<FStrProperty>(Property))
    {
        return MakeShared<FJsonValueString>(Str->GetPropertyValue_InContainer(TargetContainer));
    }

    // Names
    if (FNameProperty* NP = CastField<FNameProperty>(Property))
    {
        return MakeShared<FJsonValueString>(NP->GetPropertyValue_InContainer(TargetContainer).ToString());
    }

    // Booleans
    if (FBoolProperty* BP = CastField<FBoolProperty>(Property))
    {
        return MakeShared<FJsonValueBoolean>(BP->GetPropertyValue_InContainer(TargetContainer));
    }

    // Numeric types
    if (FFloatProperty* FP = CastField<FFloatProperty>(Property))
    {
        return MakeShared<FJsonValueNumber>(static_cast<double>(FP->GetPropertyValue_InContainer(TargetContainer)));
    }
    if (FDoubleProperty* DP = CastField<FDoubleProperty>(Property))
    {
        return MakeShared<FJsonValueNumber>(DP->GetPropertyValue_InContainer(TargetContainer));
    }
    if (FIntProperty* IP = CastField<FIntProperty>(Property))
    {
        return MakeShared<FJsonValueNumber>(static_cast<double>(IP->GetPropertyValue_InContainer(TargetContainer)));
    }
    if (FInt64Property* I64P = CastField<FInt64Property>(Property))
    {
        return MakeShared<FJsonValueNumber>(static_cast<double>(I64P->GetPropertyValue_InContainer(TargetContainer)));
    }

    // Byte property (may have enum)
    if (FByteProperty* BP = CastField<FByteProperty>(Property))
    {
        const uint8 ByteVal = BP->GetPropertyValue_InContainer(TargetContainer);
        if (UEnum* Enum = BP->Enum)
        {
            const FString EnumName = Enum->GetNameStringByValue(ByteVal);
            if (!EnumName.IsEmpty())
            {
                return MakeShared<FJsonValueString>(EnumName);
            }
        }
        return MakeShared<FJsonValueNumber>(static_cast<double>(ByteVal));
    }

    // Enum property (newer engine versions)
    if (FEnumProperty* EP = CastField<FEnumProperty>(Property))
    {
        if (UEnum* Enum = EP->GetEnum())
        {
            void* ValuePtr = EP->ContainerPtrToValuePtr<void>(TargetContainer);
            if (FNumericProperty* UnderlyingProp = EP->GetUnderlyingProperty())
            {
                const int64 EnumVal = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
                const FString EnumName = Enum->GetNameStringByValue(EnumVal);
                if (!EnumName.IsEmpty())
                {
                    return MakeShared<FJsonValueString>(EnumName);
                }
                return MakeShared<FJsonValueNumber>(static_cast<double>(EnumVal));
            }
        }
        return MakeShared<FJsonValueNumber>(0.0);
    }

    // Object references
    if (FObjectProperty* OP = CastField<FObjectProperty>(Property))
    {
        UObject* O = OP->GetObjectPropertyValue_InContainer(TargetContainer);
        if (O)
        {
            return MakeShared<FJsonValueString>(O->GetPathName());
        }
        return MakeShared<FJsonValueNull>();
    }

    // Soft object references
    if (FSoftObjectProperty* SOP = CastField<FSoftObjectProperty>(Property))
    {
        const void* ValuePtr = SOP->ContainerPtrToValuePtr<void>(TargetContainer);
        const FSoftObjectPtr* SoftObjPtr = static_cast<const FSoftObjectPtr*>(ValuePtr);
        if (SoftObjPtr && !SoftObjPtr->IsNull())
        {
            return MakeShared<FJsonValueString>(SoftObjPtr->ToSoftObjectPath().ToString());
        }
        return MakeShared<FJsonValueNull>();
    }

    // Soft class references
    if (FSoftClassProperty* SCP = CastField<FSoftClassProperty>(Property))
    {
        const void* ValuePtr = SCP->ContainerPtrToValuePtr<void>(TargetContainer);
        const FSoftObjectPtr* SoftClassPtr = static_cast<const FSoftObjectPtr*>(ValuePtr);
        if (SoftClassPtr && !SoftClassPtr->IsNull())
        {
            return MakeShared<FJsonValueString>(SoftClassPtr->ToSoftObjectPath().ToString());
        }
        return MakeShared<FJsonValueNull>();
    }

    // Structs: Vector and Rotator common cases
    if (FStructProperty* SP = CastField<FStructProperty>(Property))
    {
        const FString TypeName = SP->Struct ? SP->Struct->GetName() : FString();
        if (TypeName.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
        {
            const FVector* V = SP->ContainerPtrToValuePtr<FVector>(TargetContainer);
            return VectorToJsonValue(*V);
        }
        else if (TypeName.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase))
        {
            const FRotator* R = SP->ContainerPtrToValuePtr<FRotator>(TargetContainer);
            return RotatorToJsonValue(*R);
        }

        // Generic struct: recurse into the struct's child properties and emit a
        // nested JSON object (object references inside become path strings). Pass
        // the struct's own address as the container base for its children.
        if (Depth < GMcpMaxReflectionDepth)
        {
            TSharedPtr<FJsonObject> StructObj = ExportStructToJson(
                SP->ContainerPtrToValuePtr<void>(TargetContainer), SP->Struct, Depth);
            if (StructObj.IsValid() && StructObj->Values.Num() > 0)
            {
                return MakeShared<FJsonValueObject>(StructObj);
            }
        }

        // Fallback: export textual representation (depth cap reached, or a struct
        // with no reflected/exportable child properties).
        FString Exported;
        SP->Struct->ExportText(
            Exported,
            SP->ContainerPtrToValuePtr<void>(TargetContainer),
            nullptr, nullptr, 0, nullptr, true
        );
        return MakeShared<FJsonValueString>(Exported);
    }

    // Arrays
    if (FArrayProperty* AP = CastField<FArrayProperty>(Property))
    {
        return MakeShared<FJsonValueArray>(ExportArrayToJson(TargetContainer, AP, Depth));
    }

    // Maps
    if (FMapProperty* MP = CastField<FMapProperty>(Property))
    {
        TSharedPtr<FJsonObject> MapObj = MakeShared<FJsonObject>();
        FScriptMapHelper Helper(MP, MP->ContainerPtrToValuePtr<void>(TargetContainer));

        for (int32 i = 0; i < Helper.Num(); ++i)
        {
            if (!Helper.IsValidIndex(i))
            {
                continue;
            }

            const uint8* KeyPtr = Helper.GetKeyPtr(i);
            const uint8* ValuePtr = Helper.GetValuePtr(i);

            // Convert key to string
            FString KeyStr;
            FProperty* KeyProp = MP->KeyProp;
            if (FStrProperty* StrKey = CastField<FStrProperty>(KeyProp))
            {
                KeyStr = *reinterpret_cast<const FString*>(KeyPtr);
            }
            else if (FNameProperty* NameKey = CastField<FNameProperty>(KeyProp))
            {
                KeyStr = reinterpret_cast<const FName*>(KeyPtr)->ToString();
            }
            else if (FIntProperty* IntKey = CastField<FIntProperty>(KeyProp))
            {
                KeyStr = FString::FromInt(*reinterpret_cast<const int32*>(KeyPtr));
            }
            else
            {
                KeyStr = FString::Printf(TEXT("key_%d"), i);
            }

            // Convert value
            FProperty* ValueProp = MP->ValueProp;
            if (FStrProperty* StrVal = CastField<FStrProperty>(ValueProp))
            {
                MapObj->SetStringField(KeyStr, *reinterpret_cast<const FString*>(ValuePtr));
            }
            else if (FIntProperty* IntVal = CastField<FIntProperty>(ValueProp))
            {
                MapObj->SetNumberField(KeyStr, static_cast<double>(*reinterpret_cast<const int32*>(ValuePtr)));
            }
            else if (FFloatProperty* FloatVal = CastField<FFloatProperty>(ValueProp))
            {
                MapObj->SetNumberField(KeyStr, static_cast<double>(*reinterpret_cast<const float*>(ValuePtr)));
            }
            else if (FBoolProperty* BoolVal = CastField<FBoolProperty>(ValueProp))
            {
                MapObj->SetBoolField(KeyStr, (*reinterpret_cast<const uint8*>(ValuePtr)) != 0);
            }
            else
            {
                FString ValueStr;
                MCP_PROPERTY_EXPORT_TEXT(ValueProp, ValueStr, ValuePtr, nullptr, nullptr, PPF_None);
                MapObj->SetStringField(KeyStr, ValueStr);
            }
        }

        return MakeShared<FJsonValueObject>(MapObj);
    }

    // Sets
    if (FSetProperty* SP = CastField<FSetProperty>(Property))
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        FScriptSetHelper Helper(SP, SP->ContainerPtrToValuePtr<void>(TargetContainer));

        for (int32 i = 0; i < Helper.Num(); ++i)
        {
            if (!Helper.IsValidIndex(i))
            {
                continue;
            }

            const uint8* ElemPtr = Helper.GetElementPtr(i);
            FProperty* ElemProp = SP->ElementProp;

            if (FStrProperty* StrElem = CastField<FStrProperty>(ElemProp))
            {
                Out.Add(MakeShared<FJsonValueString>(*reinterpret_cast<const FString*>(ElemPtr)));
            }
            else if (FNameProperty* NameElem = CastField<FNameProperty>(ElemProp))
            {
                Out.Add(MakeShared<FJsonValueString>(reinterpret_cast<const FName*>(ElemPtr)->ToString()));
            }
            else if (FIntProperty* IntElem = CastField<FIntProperty>(ElemProp))
            {
                Out.Add(MakeShared<FJsonValueNumber>(static_cast<double>(*reinterpret_cast<const int32*>(ElemPtr))));
            }
            else if (FFloatProperty* FloatElem = CastField<FFloatProperty>(ElemProp))
            {
                Out.Add(MakeShared<FJsonValueNumber>(static_cast<double>(*reinterpret_cast<const float*>(ElemPtr))));
            }
            else
            {
                FString ElemStr;
                MCP_PROPERTY_EXPORT_TEXT(ElemProp, ElemStr, ElemPtr, nullptr, nullptr, PPF_None);
                Out.Add(MakeShared<FJsonValueString>(ElemStr));
            }
        }

        return MakeShared<FJsonValueArray>(Out);
    }

    return nullptr;
}

TSharedPtr<FJsonObject> ExportObjectToJson(UObject* Object, bool bIncludeTransient)
{
    if (!Object)
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    UClass* Class = Object->GetClass();

    for (TFieldIterator<FProperty> It(Class); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property)
        {
            continue;
        }

        // Skip transient properties unless requested
        if (!bIncludeTransient && Property->HasAnyPropertyFlags(CPF_Transient))
        {
            continue;
        }

        // Skip deprecated properties
        if (Property->HasAnyPropertyFlags(CPF_Deprecated))
        {
            continue;
        }

        TSharedPtr<FJsonValue> Value = McpPropertyReflection::ExportPropertyToJsonValue(Object, Property);
        if (Value.IsValid())
        {
            Result->SetField(Property->GetName(), Value);
        }
    }

    return Result;
}

TSharedPtr<FJsonObject> ExportPropertiesToJson(UObject* Object, const TArray<FName>& PropertyNames)
{
    if (!Object)
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    UClass* Class = Object->GetClass();

    for (const FName& PropName : PropertyNames)
    {
        FProperty* Property = Class->FindPropertyByName(PropName);
        if (!Property)
        {
            continue;
        }

        TSharedPtr<FJsonValue> Value = McpPropertyReflection::ExportPropertyToJsonValue(Object, Property);
        if (Value.IsValid())
        {
            Result->SetField(Property->GetName(), Value);
        }
    }

    return Result;
}

// Additional implementations would follow for ApplyJsonValueToProperty, etc.
// The full implementation is in the previous code block - truncated for brevity
// but the key functions are implemented above.

int32 ApplyJsonValuesToObject(UObject* Object, const TMap<FName, TSharedPtr<FJsonValue>>& JsonValues, TMap<FName, FString>* OutErrors)
{
    if (!Object)
    {
        return 0;
    }

    int32 SuccessCount = 0;
    UClass* Class = Object->GetClass();

    for (const auto& Pair : JsonValues)
    {
        FProperty* Property = Class->FindPropertyByName(Pair.Key);
        if (!Property)
        {
            if (OutErrors)
            {
                OutErrors->Add(Pair.Key, TEXT("Property not found"));
            }
            continue;
        }

        FString Error;
        if (McpPropertyReflection::ApplyJsonValueToProperty(Object, Property, Pair.Value, Error))
        {
            SuccessCount++;
        }
        else if (OutErrors)
        {
            OutErrors->Add(Pair.Key, Error);
        }
    }

    return SuccessCount;
}

int32 ApplyJsonObjectToObject(UObject* Object, const TSharedPtr<FJsonObject>& JsonObject, TMap<FName, FString>* OutErrors)
{
    if (!Object || !JsonObject.IsValid())
    {
        return 0;
    }

    TMap<FName, TSharedPtr<FJsonValue>> Values;
    for (const auto& Pair : JsonObject->Values)
    {
        Values.Add(FName(*Pair.Key), Pair.Value);
    }

    return ApplyJsonValuesToObject(Object, Values, OutErrors);
}

FString GetPropertyTypeName(FProperty* Property)
{
    if (!Property)
    {
        return TEXT("Unknown");
    }

    if (Property->IsA<FStrProperty>()) return TEXT("String");
    if (Property->IsA<FNameProperty>()) return TEXT("Name");
    if (Property->IsA<FBoolProperty>()) return TEXT("Bool");
    if (Property->IsA<FFloatProperty>()) return TEXT("Float");
    if (Property->IsA<FDoubleProperty>()) return TEXT("Double");
    if (Property->IsA<FIntProperty>()) return TEXT("Int");
    if (Property->IsA<FInt64Property>()) return TEXT("Int64");
    if (Property->IsA<FByteProperty>())
    {
        FByteProperty* BP = CastField<FByteProperty>(Property);
        if (BP->Enum) return FString::Printf(TEXT("Enum(%s)"), *BP->Enum->GetName());
        return TEXT("Byte");
    }
    if (Property->IsA<FEnumProperty>())
    {
        FEnumProperty* EP = CastField<FEnumProperty>(Property);
        if (EP->GetEnum()) return FString::Printf(TEXT("Enum(%s)"), *EP->GetEnum()->GetName());
        return TEXT("Enum");
    }
    if (Property->IsA<FObjectProperty>()) return TEXT("Object");
    if (Property->IsA<FSoftObjectProperty>()) return TEXT("SoftObject");
    if (Property->IsA<FSoftClassProperty>()) return TEXT("SoftClass");
    if (Property->IsA<FStructProperty>())
    {
        FStructProperty* SP = CastField<FStructProperty>(Property);
        if (SP->Struct) return FString::Printf(TEXT("Struct(%s)"), *SP->Struct->GetName());
        return TEXT("Struct");
    }
    if (Property->IsA<FArrayProperty>()) return TEXT("Array");
    if (Property->IsA<FMapProperty>()) return TEXT("Map");
    if (Property->IsA<FSetProperty>()) return TEXT("Set");
    if (Property->IsA<FTextProperty>()) return TEXT("Text");

    return Property->GetClass()->GetName();
}

bool IsPropertyTypeSupported(FProperty* Property)
{
    if (!Property) return false;
    
    return Property->IsA<FStrProperty>() ||
           Property->IsA<FNameProperty>() ||
           Property->IsA<FBoolProperty>() ||
           Property->IsA<FFloatProperty>() ||
           Property->IsA<FDoubleProperty>() ||
           Property->IsA<FIntProperty>() ||
           Property->IsA<FInt64Property>() ||
           Property->IsA<FByteProperty>() ||
           Property->IsA<FEnumProperty>() ||
           Property->IsA<FObjectProperty>() ||
           Property->IsA<FSoftObjectProperty>() ||
           Property->IsA<FSoftClassProperty>() ||
           Property->IsA<FStructProperty>() ||
           Property->IsA<FArrayProperty>() ||
           Property->IsA<FMapProperty>() ||
           Property->IsA<FSetProperty>();
}

FString GetPropertyValueAsString(UObject* Object, FProperty* Property)
{
    if (!Object || !Property) return FString();

    FString Result;
    MCP_PROPERTY_EXPORT_TEXT(Property, Result, Property->ContainerPtrToValuePtr<void>(Object), nullptr, nullptr, PPF_None);
    return Result;
}

bool SetPropertyValueFromString(UObject* Object, FProperty* Property, const FString& ValueString, FString* OutError)
{
    if (!Object || !Property)
    {
        if (OutError) *OutError = TEXT("Invalid object or property");
        return false;
    }

    void* Container = Property->ContainerPtrToValuePtr<void>(Object);
    if (!Container)
    {
        if (OutError) *OutError = TEXT("Failed to get property container");
        return false;
    }

    // UE 5.1+ uses ImportText_Direct instead of ImportText
    const TCHAR* Result = nullptr;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Result = Property->ImportText_Direct(*ValueString, Container, nullptr, PPF_None, nullptr);
#else
    // UE 5.0: ImportText takes (Buffer, Data, PortFlags, OwnerObject, ErrorText)
    Result = Property->ImportText(*ValueString, Container, PPF_None, nullptr);
#endif
    if (!Result)
    {
        if (OutError) *OutError = FString::Printf(TEXT("Failed to import value '%s' for property '%s'"), *ValueString, *Property->GetName());
        return false;
    }

    return true;
}

TArray<FString> GetEnumValueNames(UEnum* Enum)
{
    TArray<FString> Names;
    if (!Enum) return Names;

    for (int32 i = 0; i < Enum->NumEnums(); ++i)
    {
        if (Enum->HasMetaData(TEXT("Hidden"), i) || Enum->HasMetaData(TEXT("Deprecated"), i))
        {
            continue;
        }
        Names.Add(Enum->GetNameStringByIndex(i));
    }

    return Names;
}

FString EnumValueToName(UEnum* Enum, int64 Value)
{
    if (!Enum) return FString();
    return Enum->GetNameStringByValue(Value);
}

bool EnumNameToValue(UEnum* Enum, const FString& Name, int64& OutValue)
{
    if (!Enum) return false;

    OutValue = Enum->GetValueByNameString(Name);
    if (OutValue != INDEX_NONE) return true;

    FString FullName = Enum->GenerateFullEnumName(*Name);
    OutValue = Enum->GetValueByName(FName(*FullName));
    return OutValue != INDEX_NONE;
}

int32 GetArrayPropertyCount(void* Container, FArrayProperty* ArrayProp)
{
    if (!Container || !ArrayProp) return 0;

    FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Container));
    return Helper.Num();
}

TArray<TSharedPtr<FJsonValue>> ExportArrayToJson(void* Container, FArrayProperty* ArrayProp, int32 Depth)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    if (!Container || !ArrayProp) return Out;

    FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Container));

    for (int32 i = 0; i < Helper.Num(); ++i)
    {
        void* ElemPtr = Helper.GetRawPtr(i);
        FProperty* Inner = ArrayProp->Inner;

        if (FStrProperty* StrInner = CastField<FStrProperty>(Inner))
        {
            Out.Add(MakeShared<FJsonValueString>(*reinterpret_cast<FString*>(ElemPtr)));
        }
        else if (FNameProperty* NameInner = CastField<FNameProperty>(Inner))
        {
            Out.Add(MakeShared<FJsonValueString>(reinterpret_cast<FName*>(ElemPtr)->ToString()));
        }
        else if (FBoolProperty* BoolInner = CastField<FBoolProperty>(Inner))
        {
            Out.Add(MakeShared<FJsonValueBoolean>((*reinterpret_cast<uint8*>(ElemPtr)) != 0));
        }
        else if (FFloatProperty* FInner = CastField<FFloatProperty>(Inner))
        {
            Out.Add(MakeShared<FJsonValueNumber>(static_cast<double>(*reinterpret_cast<float*>(ElemPtr))));
        }
        else if (FDoubleProperty* DInner = CastField<FDoubleProperty>(Inner))
        {
            Out.Add(MakeShared<FJsonValueNumber>(*reinterpret_cast<double*>(ElemPtr)));
        }
        else if (FIntProperty* IInner = CastField<FIntProperty>(Inner))
        {
            Out.Add(MakeShared<FJsonValueNumber>(static_cast<double>(*reinterpret_cast<int32*>(ElemPtr))));
        }
        else if (FInt64Property* I64Inner = CastField<FInt64Property>(Inner))
        {
            Out.Add(MakeShared<FJsonValueNumber>(static_cast<double>(*reinterpret_cast<int64*>(ElemPtr))));
        }
        else if (FByteProperty* BInner = CastField<FByteProperty>(Inner))
        {
            uint8 ByteVal = *reinterpret_cast<uint8*>(ElemPtr);
            if (UEnum* Enum = BInner->Enum)
            {
                FString EnumName = Enum->GetNameStringByValue(ByteVal);
                if (!EnumName.IsEmpty())
                {
                    Out.Add(MakeShared<FJsonValueString>(EnumName));
                    continue;
                }
            }
            Out.Add(MakeShared<FJsonValueNumber>(static_cast<double>(ByteVal)));
        }
        else
        {
            // Struct / object-reference / soft-ref / nested-container inners: recurse
            // through the per-property exporter. The array Inner has offset 0, so the
            // raw element pointer is itself a valid container base for it. Fall back
            // to ExportText only when that can't produce a structured value.
            TSharedPtr<FJsonValue> Value = ExportPropertyToJsonValue(ElemPtr, Inner, Depth + 1);
            if (Value.IsValid())
            {
                Out.Add(Value);
            }
            else
            {
                FString ElemStr;
                MCP_PROPERTY_EXPORT_TEXT(Inner, ElemStr, ElemPtr, nullptr, nullptr, PPF_None);
                Out.Add(MakeShared<FJsonValueString>(ElemStr));
            }
        }
    }

    return Out;
}

TSharedPtr<FJsonObject> ExportStructToJson(void* StructPtr, const UScriptStruct* Struct, int32 Depth)
{
    if (!StructPtr || !Struct || Depth >= GMcpMaxReflectionDepth)
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    for (TFieldIterator<FProperty> It(Struct); It; ++It)
    {
        FProperty* Child = *It;
        // Mirror ExportObjectToJson: omit deprecated and transient (non-serialized,
        // runtime-rebuilt) child fields so nested struct output stays consistent
        // with the object-level filtering and doesn't leak volatile state.
        if (!Child || Child->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient))
        {
            continue;
        }

        // The child's offset is relative to the struct base, so StructPtr is the
        // correct container base to hand the per-property exporter (its
        // *_InContainer accessors re-add the offset). Increment Depth: this struct
        // is one level deeper than its parent.
        TSharedPtr<FJsonValue> Value = ExportPropertyToJsonValue(StructPtr, Child, Depth + 1);
        if (Value.IsValid())
        {
            Obj->SetField(Child->GetName(), Value);
        }
    }

    return Obj;
}

bool ImportJsonToArray(void* Container, FArrayProperty* ArrayProp, const TArray<TSharedPtr<FJsonValue>>& JsonArray, FString& OutError, int32 Depth)
{
    if (!Container || !ArrayProp)
    {
        OutError = TEXT("Invalid container or array property");
        return false;
    }

    FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Container));
    Helper.EmptyValues();
    Helper.Resize(JsonArray.Num());

    FProperty* Inner = ArrayProp->Inner;

    for (int32 i = 0; i < JsonArray.Num(); ++i)
    {
        const TSharedPtr<FJsonValue>& JsonVal = JsonArray[i];
        // Resize() above already default-constructs every element, so no extra
        // InitializeValue is needed (a second init would re-construct over live
        // memory — harmless for the empty defaults here, but redundant).
        uint8* ElemPtr = Helper.GetRawPtr(i);

        // The array Inner has offset 0, so ElemPtr is a valid container base for it.
        // ApplyJsonValueToProperty now handles struct / object-reference inners, so
        // arrays of structs (e.g. input mappings) round-trip through here.
        FString PropError;
        if (!McpPropertyReflection::ApplyJsonValueToProperty(ElemPtr, Inner, JsonVal, PropError, Depth + 1))
        {
            OutError = FString::Printf(TEXT("Failed to set array element %d: %s"), i, *PropError);
            return false;
        }
    }

    return true;
}

// Full ApplyJsonValueToProperty implementation - this is a critical function
// The implementation continues with all the type handling from McpAutomationBridgeHelpers.h

bool ApplyJsonValueToProperty(void* TargetContainer, FProperty* Property, const TSharedPtr<FJsonValue>& ValueField, FString& OutError, int32 Depth)
{
    // Standalone property importer used by reflection callers during the helper refactor.

    OutError.Empty();
    if (!TargetContainer || !Property || !ValueField.IsValid())
    {
        OutError = TEXT("Invalid target/property/value");
        return false;
    }

    // Bool
    if (FBoolProperty* BP = CastField<FBoolProperty>(Property))
    {
        if (ValueField->Type == EJson::Boolean)
        {
            BP->SetPropertyValue_InContainer(TargetContainer, ValueField->AsBool());
            return true;
        }
        if (ValueField->Type == EJson::Number)
        {
            BP->SetPropertyValue_InContainer(TargetContainer, ValueField->AsNumber() != 0.0);
            return true;
        }
        if (ValueField->Type == EJson::String)
        {
            BP->SetPropertyValue_InContainer(TargetContainer, ValueField->AsString().Equals(TEXT("true"), ESearchCase::IgnoreCase));
            return true;
        }
        OutError = TEXT("Unsupported JSON type for bool property");
        return false;
    }

    // String
    if (FStrProperty* SP = CastField<FStrProperty>(Property))
    {
        if (ValueField->Type == EJson::String)
        {
            SP->SetPropertyValue_InContainer(TargetContainer, ValueField->AsString());
            return true;
        }
        OutError = TEXT("Expected string for string property");
        return false;
    }

    // Name
    if (FNameProperty* NP = CastField<FNameProperty>(Property))
    {
        if (ValueField->Type == EJson::String)
        {
            NP->SetPropertyValue_InContainer(TargetContainer, FName(*ValueField->AsString()));
            return true;
        }
        OutError = TEXT("Expected string for name property");
        return false;
    }

    // Float
    if (FFloatProperty* FP = CastField<FFloatProperty>(Property))
    {
        double Val = 0.0;
        if (ValueField->Type == EJson::Number) Val = ValueField->AsNumber();
        else if (ValueField->Type == EJson::String) Val = FCString::Atod(*ValueField->AsString());
        else { OutError = TEXT("Unsupported JSON type for float property"); return false; }
        FP->SetPropertyValue_InContainer(TargetContainer, static_cast<float>(Val));
        return true;
    }

    // Double
    if (FDoubleProperty* DP = CastField<FDoubleProperty>(Property))
    {
        double Val = 0.0;
        if (ValueField->Type == EJson::Number) Val = ValueField->AsNumber();
        else if (ValueField->Type == EJson::String) Val = FCString::Atod(*ValueField->AsString());
        else { OutError = TEXT("Unsupported JSON type for double property"); return false; }
        DP->SetPropertyValue_InContainer(TargetContainer, Val);
        return true;
    }

    // Int32
    if (FIntProperty* IP = CastField<FIntProperty>(Property))
    {
        int64 Val = 0;
        if (ValueField->Type == EJson::Number) Val = static_cast<int64>(ValueField->AsNumber());
        else if (ValueField->Type == EJson::String) Val = FCString::Atoi64(*ValueField->AsString());
        else { OutError = TEXT("Unsupported JSON type for int property"); return false; }
        IP->SetPropertyValue_InContainer(TargetContainer, static_cast<int32>(Val));
        return true;
    }

    // Int64
    if (FInt64Property* I64P = CastField<FInt64Property>(Property))
    {
        int64 Val = 0;
        if (ValueField->Type == EJson::Number) Val = static_cast<int64>(ValueField->AsNumber());
        else if (ValueField->Type == EJson::String) Val = FCString::Atoi64(*ValueField->AsString());
        else { OutError = TEXT("Unsupported JSON type for int64 property"); return false; }
        I64P->SetPropertyValue_InContainer(TargetContainer, Val);
        return true;
    }

    // Byte (may be an enum)
    if (FByteProperty* BP = CastField<FByteProperty>(Property))
    {
        if (UEnum* Enum = BP->Enum)
        {
            if (ValueField->Type == EJson::String)
            {
                const FString InStr = ValueField->AsString();
                int64 EnumVal = Enum->GetValueByNameString(InStr);
                if (EnumVal == INDEX_NONE)
                {
                    EnumVal = Enum->GetValueByName(FName(*Enum->GenerateFullEnumName(*InStr)));
                }
                if (EnumVal == INDEX_NONE)
                {
                    OutError = FString::Printf(TEXT("Invalid enum value '%s' for enum '%s'"), *InStr, *Enum->GetName());
                    return false;
                }
                BP->SetPropertyValue_InContainer(TargetContainer, static_cast<uint8>(EnumVal));
                return true;
            }
            if (ValueField->Type == EJson::Number)
            {
                const int64 Val = static_cast<int64>(ValueField->AsNumber());
                if (!Enum->IsValidEnumValue(Val))
                {
                    OutError = FString::Printf(TEXT("Numeric value %lld is not valid for enum '%s'"), Val, *Enum->GetName());
                    return false;
                }
                BP->SetPropertyValue_InContainer(TargetContainer, static_cast<uint8>(Val));
                return true;
            }
            OutError = TEXT("Enum property requires string or number");
            return false;
        }
        int64 Val = 0;
        if (ValueField->Type == EJson::Number) Val = static_cast<int64>(ValueField->AsNumber());
        else if (ValueField->Type == EJson::String) Val = FCString::Atoi64(*ValueField->AsString());
        else { OutError = TEXT("Unsupported JSON type for byte property"); return false; }
        BP->SetPropertyValue_InContainer(TargetContainer, static_cast<uint8>(Val));
        return true;
    }

    // Enum property (newer engine versions)
    if (FEnumProperty* EP = CastField<FEnumProperty>(Property))
    {
        UEnum* Enum = EP->GetEnum();
        FNumericProperty* UnderlyingProp = EP->GetUnderlyingProperty();
        if (Enum && UnderlyingProp)
        {
            void* ValuePtr = EP->ContainerPtrToValuePtr<void>(TargetContainer);
            if (ValueField->Type == EJson::String)
            {
                const FString InStr = ValueField->AsString();
                int64 EnumVal = Enum->GetValueByNameString(InStr);
                if (EnumVal == INDEX_NONE)
                {
                    EnumVal = Enum->GetValueByName(FName(*Enum->GenerateFullEnumName(*InStr)));
                }
                if (EnumVal == INDEX_NONE)
                {
                    OutError = FString::Printf(TEXT("Invalid enum value '%s' for enum '%s'"), *InStr, *Enum->GetName());
                    return false;
                }
                UnderlyingProp->SetIntPropertyValue(ValuePtr, EnumVal);
                return true;
            }
            if (ValueField->Type == EJson::Number)
            {
                const int64 Val = static_cast<int64>(ValueField->AsNumber());
                if (!Enum->IsValidEnumValue(Val))
                {
                    OutError = FString::Printf(TEXT("Numeric value %lld is not valid for enum '%s'"), Val, *Enum->GetName());
                    return false;
                }
                UnderlyingProp->SetIntPropertyValue(ValuePtr, Val);
                return true;
            }
            OutError = TEXT("Enum property requires string or number");
            return false;
        }
        OutError = TEXT("Enum property has no valid enum definition");
        return false;
    }

    // Object reference (by path string; null or empty/"None" clears it)
    if (FObjectProperty* OP = CastField<FObjectProperty>(Property))
    {
        if (ValueField->Type == EJson::Null)
        {
            OP->SetObjectPropertyValue_InContainer(TargetContainer, nullptr);
            return true;
        }
        if (ValueField->Type == EJson::String)
        {
            const FString Path = ValueField->AsString();
            UObject* Res = nullptr;
            if (!Path.IsEmpty() && !Path.Equals(TEXT("None"), ESearchCase::IgnoreCase))
            {
                Res = LoadObject<UObject>(nullptr, *Path);
                if (!Res && !Path.Contains(TEXT(".")))
                {
                    Res = StaticLoadObject(UObject::StaticClass(), nullptr, *Path);
                }
                if (!Res)
                {
                    OutError = FString::Printf(TEXT("Failed to load object at path: %s"), *Path);
                    return false;
                }
            }
            OP->SetObjectPropertyValue_InContainer(TargetContainer, Res);
            return true;
        }
        OutError = TEXT("Unsupported JSON type for object property");
        return false;
    }

    // Soft object reference (FSoftObjectPtr)
    if (FSoftObjectProperty* SOP = CastField<FSoftObjectProperty>(Property))
    {
        void* ValuePtr = SOP->ContainerPtrToValuePtr<void>(TargetContainer);
        FSoftObjectPtr* SoftObjPtr = static_cast<FSoftObjectPtr*>(ValuePtr);
        if (!SoftObjPtr)
        {
            OutError = TEXT("Failed to access soft object property");
            return false;
        }
        if (ValueField->Type == EJson::Null || (ValueField->Type == EJson::String && ValueField->AsString().IsEmpty()))
        {
            *SoftObjPtr = FSoftObjectPtr();
            return true;
        }
        if (ValueField->Type == EJson::String)
        {
            *SoftObjPtr = FSoftObjectPath(ValueField->AsString());
            return true;
        }
        OutError = TEXT("Soft object property requires string path or null");
        return false;
    }

    // Soft class reference (FSoftClassPtr)
    if (FSoftClassProperty* SCP = CastField<FSoftClassProperty>(Property))
    {
        void* ValuePtr = SCP->ContainerPtrToValuePtr<void>(TargetContainer);
        FSoftObjectPtr* SoftClassPtr = static_cast<FSoftObjectPtr*>(ValuePtr);
        if (!SoftClassPtr)
        {
            OutError = TEXT("Failed to access soft class property");
            return false;
        }
        if (ValueField->Type == EJson::Null || (ValueField->Type == EJson::String && ValueField->AsString().IsEmpty()))
        {
            *SoftClassPtr = FSoftObjectPtr();
            return true;
        }
        if (ValueField->Type == EJson::String)
        {
            *SoftClassPtr = FSoftObjectPath(ValueField->AsString());
            return true;
        }
        OutError = TEXT("Soft class property requires string path or null");
        return false;
    }

    // Structs: Vector/Rotator from a numeric array, otherwise via the engine's
    // JSON<->struct converter (handles nested structs, FKey, object refs-by-path).
    if (FStructProperty* SP = CastField<FStructProperty>(Property))
    {
        const FString TypeName = SP->Struct ? SP->Struct->GetName() : FString();
        void* StructPtr = SP->ContainerPtrToValuePtr<void>(TargetContainer);

        if (ValueField->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& Arr = ValueField->AsArray();
            if (TypeName.Equals(TEXT("Vector"), ESearchCase::IgnoreCase) && Arr.Num() >= 3 && SP->Struct)
            {
                FVector V(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
                SP->Struct->CopyScriptStruct(StructPtr, &V);
                return true;
            }
            if (TypeName.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase) && Arr.Num() >= 3 && SP->Struct)
            {
                FRotator R(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
                SP->Struct->CopyScriptStruct(StructPtr, &R);
                return true;
            }
            OutError = FString::Printf(TEXT("Array form is only supported for Vector/Rotator (got struct '%s')"), *TypeName);
            return false;
        }

        if (SP->Struct && ValueField->Type == EJson::Object && ValueField->AsObject().IsValid())
        {
            if (FJsonObjectConverter::JsonObjectToUStruct(ValueField->AsObject().ToSharedRef(), SP->Struct, StructPtr, 0, 0))
            {
                return true;
            }
            OutError = FString::Printf(TEXT("Failed to convert JSON object into struct '%s'"), *TypeName);
            return false;
        }

        if (SP->Struct && ValueField->Type == EJson::String)
        {
            // Accept a JSON string that itself contains a JSON object describing the struct.
            const FString Txt = ValueField->AsString();
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Txt);
            TSharedPtr<FJsonObject> ParsedObj;
            if (FJsonSerializer::Deserialize(Reader, ParsedObj) && ParsedObj.IsValid())
            {
                if (FJsonObjectConverter::JsonObjectToUStruct(ParsedObj.ToSharedRef(), SP->Struct, StructPtr, 0, 0))
                {
                    return true;
                }
            }
            OutError = FString::Printf(TEXT("Failed to parse string value into struct '%s'"), *TypeName);
            return false;
        }

        OutError = TEXT("Unsupported JSON type for struct property");
        return false;
    }

    // Arrays: replace contents element-by-element. The Inner has offset 0, so each
    // element pointer is a valid container base — this recursion therefore handles
    // primitive, struct and object-reference inner types uniformly.
    if (FArrayProperty* AP = CastField<FArrayProperty>(Property))
    {
        if (ValueField->Type != EJson::Array)
        {
            OutError = TEXT("Expected array for array property");
            return false;
        }
        const TArray<TSharedPtr<FJsonValue>>& Src = ValueField->AsArray();
        FScriptArrayHelper Helper(AP, AP->ContainerPtrToValuePtr<void>(TargetContainer));
        Helper.EmptyValues();
        Helper.Resize(Src.Num());
        for (int32 i = 0; i < Src.Num(); ++i)
        {
            uint8* ElemPtr = Helper.GetRawPtr(i);
            FString InnerErr;
            if (!ApplyJsonValueToProperty(ElemPtr, AP->Inner, Src[i], InnerErr, Depth + 1))
            {
                OutError = FString::Printf(TEXT("Failed to set array element %d: %s"), i, *InnerErr);
                return false;
            }
        }
        return true;
    }

    // Generic fallback: accept a JSON string for any other property type via the
    // engine's textual import (covers e.g. FText and other text-importable types).
    if (ValueField->Type == EJson::String)
    {
        void* ValuePtr = Property->ContainerPtrToValuePtr<void>(TargetContainer);
        if (ValuePtr)
        {
            const TCHAR* Result = nullptr;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            Result = Property->ImportText_Direct(*ValueField->AsString(), ValuePtr, nullptr, PPF_None, nullptr);
#else
            Result = Property->ImportText(*ValueField->AsString(), ValuePtr, PPF_None, nullptr);
#endif
            if (Result)
            {
                return true;
            }
        }
    }

    OutError = FString::Printf(TEXT("Unsupported property type: %s"), *Property->GetClass()->GetName());
    return false;
}

} // namespace McpPropertyReflection
