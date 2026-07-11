// =============================================================================
// McpPropertyReflection.cpp
// =============================================================================
// Implementation of property reflection and JSON conversion utilities.
// =============================================================================

#include "McpPropertyReflection.h"
#include "McpVersionCompatibility.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/TextProperty.h"

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

// Deep-export an instanced subobject (e.g. an Enhanced Input UInputTrigger/UInputModifier,
// or a montage's UAnimNotify) as a nested JSON object carrying its concrete class under
// "__class", so the subobject's OWN configuration survives a get_asset_properties read
// (and an importer can re-instance it). Only CPF_InstancedReference object values are
// routed here; plain asset references keep exporting as a path string. Depth-bounded via
// the shared cap, and transient/deprecated child fields are skipped to match
// ExportObjectToJson / ExportStructToJson.
TSharedPtr<FJsonObject> ExportInstancedObjectToJson(UObject* Subobject, int32 Depth)
{
    if (!Subobject)
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("__class"), Subobject->GetClass()->GetPathName());

    for (TFieldIterator<FProperty> It(Subobject->GetClass()); It; ++It)
    {
        FProperty* Child = *It;
        if (!Child || Child->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient))
        {
            continue;
        }
        TSharedPtr<FJsonValue> Value = ExportPropertyToJsonValue(Subobject, Child, Depth + 1);
        if (Value.IsValid())
        {
            Obj->SetField(Child->GetName(), Value);
        }
    }

    return Obj;
}

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

    // Text (FText) — emit the display string. Lossy of namespace/key (fine for
    // inspection and round-tripping the visible text); the import side restores via
    // ImportText (the generic string fallback). Previously FText was unhandled here, so
    // text properties were silently dropped from reads.
    if (FTextProperty* TP = CastField<FTextProperty>(Property))
    {
        return MakeShared<FJsonValueString>(TP->GetPropertyValue_InContainer(TargetContainer).ToString());
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

    // Object references. An instanced subobject (CPF_InstancedReference — e.g. an
    // input trigger/modifier, or a montage AnimNotify) is deep-exported so its own
    // config survives the read; a plain asset reference stays a path string.
    if (FObjectProperty* OP = CastField<FObjectProperty>(Property))
    {
        UObject* O = OP->GetObjectPropertyValue_InContainer(TargetContainer);
        if (!O)
        {
            return MakeShared<FJsonValueNull>();
        }
        if (OP->HasAnyPropertyFlags(CPF_InstancedReference) && Depth < GMcpMaxReflectionDepth)
        {
            TSharedPtr<FJsonObject> Inst = ExportInstancedObjectToJson(O, Depth);
            if (Inst.IsValid())
            {
                return MakeShared<FJsonValueObject>(Inst);
            }
        }
        return MakeShared<FJsonValueString>(O->GetPathName());
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
            uint8* PairPtr = Helper.GetPairPtr(i);
            FProperty* KeyProp = MP->KeyProp;
            FProperty* ValueProp = MP->ValueProp;

            // Key -> a string field name. Strings/names directly; any other key type via
            // ExportText so its value is preserved (previously non-str/name/int keys were
            // replaced by a positional "key_%d", silently losing the actual key).
            FString KeyStr;
            if (FStrProperty* StrKey = CastField<FStrProperty>(KeyProp))
            {
                KeyStr = *reinterpret_cast<const FString*>(KeyPtr);
            }
            else if (FNameProperty* NameKey = CastField<FNameProperty>(KeyProp))
            {
                KeyStr = reinterpret_cast<const FName*>(KeyPtr)->ToString();
            }
            else
            {
                MCP_PROPERTY_EXPORT_TEXT(KeyProp, KeyStr, KeyPtr, nullptr, nullptr, PPF_None);
            }

            // Value -> delegate to the per-property exporter so EVERY value type
            // (float/double/int64/byte/name/enum/struct/object/instanced/nested) gets the
            // right JSON type, instead of a partial if-else that dropped double/int64/etc.
            // to an ExportText string. KeyProp/ValueProp offsets are relative to the pair
            // base, so pass GetPairPtr (the *_InContainer accessors re-add ValueProp's
            // offset to reach the value; GetValuePtr would mis-offset).
            TSharedPtr<FJsonValue> ValueJson = ExportPropertyToJsonValue(PairPtr, ValueProp, Depth + 1);
            if (ValueJson.IsValid())
            {
                MapObj->SetField(KeyStr, ValueJson);
            }
            else
            {
                FString ValueStr;
                MCP_PROPERTY_EXPORT_TEXT(ValueProp, ValueStr, Helper.GetValuePtr(i), nullptr, nullptr, PPF_None);
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

            // Element offset is 0 relative to GetElementPtr, so it's a valid container
            // base. Delegate to the per-property exporter so EVERY element type
            // (float/double/int64/byte/name/enum/struct/object/instanced/nested) gets the
            // right JSON type, instead of a partial if-else that dropped double/int64/etc.
            uint8* ElemPtr = Helper.GetElementPtr(i);
            TSharedPtr<FJsonValue> ElemJson = ExportPropertyToJsonValue(ElemPtr, SP->ElementProp, Depth + 1);
            if (ElemJson.IsValid())
            {
                Out.Add(ElemJson);
            }
            else
            {
                FString ElemStr;
                MCP_PROPERTY_EXPORT_TEXT(SP->ElementProp, ElemStr, ElemPtr, nullptr, nullptr, PPF_None);
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
        // The object IS the owner for any Instanced subobjects re-created underneath it.
        if (McpPropertyReflection::ApplyJsonValueToProperty(Object, Property, Pair.Value, Error, 0, Object))
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

bool ImportJsonToArray(void* Container, FArrayProperty* ArrayProp, const TArray<TSharedPtr<FJsonValue>>& JsonArray, FString& OutError, int32 Depth, UObject* OwnerForInstancing)
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
        if (!McpPropertyReflection::ApplyJsonValueToProperty(ElemPtr, Inner, JsonVal, PropError, Depth + 1, OwnerForInstancing))
        {
            OutError = FString::Printf(TEXT("Failed to set array element %d: %s"), i, *PropError);
            return false;
        }
    }

    return true;
}

// Case-tolerant struct-field lookup for the field-by-field struct importer.
// Exact FName match first (our own export emits exact GetName() keys), then a
// case-insensitive sweep so hand-written JSON with different casing still maps.
static FProperty* FindStructChildProperty(const UScriptStruct* Struct, const FString& FieldName)
{
    if (!Struct)
    {
        return nullptr;
    }
    if (FProperty* Exact = Struct->FindPropertyByName(FName(*FieldName)))
    {
        return Exact;
    }
    for (TFieldIterator<FProperty> It(Struct); It; ++It)
    {
        if ((*It)->GetName().Equals(FieldName, ESearchCase::IgnoreCase))
        {
            return *It;
        }
    }
    return nullptr;
}

static const TCHAR* JsonTypeToString(EJson Type)
{
    switch (Type)
    {
    case EJson::None:    return TEXT("none");
    case EJson::Null:    return TEXT("null");
    case EJson::String:  return TEXT("string");
    case EJson::Number:  return TEXT("number");
    case EJson::Boolean: return TEXT("bool");
    case EJson::Array:   return TEXT("array");
    case EJson::Object:  return TEXT("object");
    }
    return TEXT("unknown");
}

// Apply a JSON number to a fixed-width integer property. Number only (a string is
// not a number), and a value outside the type's representable range is an error,
// never a silent narrowing.
template <typename FPropertyType, typename CppType>
static bool ApplyBoundedIntProperty(FPropertyType* Prop, void* Container,
                                    const TSharedPtr<FJsonValue>& ValueField, FString& OutError)
{
    if (ValueField->Type != EJson::Number)
    {
        OutError = FString::Printf(TEXT("Expected number for %s '%s', got %s"),
                                   *Prop->GetClass()->GetName(), *Prop->GetName(),
                                   JsonTypeToString(ValueField->Type));
        return false;
    }
    const double Num = ValueField->AsNumber();
    if (Num < static_cast<double>(TNumericLimits<CppType>::Min()) ||
        Num > static_cast<double>(TNumericLimits<CppType>::Max()))
    {
        OutError = FString::Printf(TEXT("Value %g out of range for %s '%s'"),
                                   Num, *Prop->GetClass()->GetName(), *Prop->GetName());
        return false;
    }
    Prop->SetPropertyValue_InContainer(Container, static_cast<CppType>(Num));
    return true;
}

bool ApplyJsonValueToProperty(void* TargetContainer, FProperty* Property, const TSharedPtr<FJsonValue>& ValueField, FString& OutError, int32 Depth, UObject* OwnerForInstancing)
{
    OutError.Empty();
    if (!TargetContainer || !Property || !ValueField.IsValid())
    {
        OutError = TEXT("Invalid target/property/value");
        return false;
    }

    // Bool
    if (FBoolProperty* BP = CastField<FBoolProperty>(Property))
    {
        if (ValueField->Type != EJson::Boolean)
        {
            OutError = FString::Printf(TEXT("Expected bool for bool property '%s', got %s"),
                                       *Property->GetName(), JsonTypeToString(ValueField->Type));
            return false;
        }
        BP->SetPropertyValue_InContainer(TargetContainer, ValueField->AsBool());
        return true;
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
        if (ValueField->Type != EJson::Number)
        {
            OutError = FString::Printf(TEXT("Expected number for float property '%s', got %s"),
                                       *Property->GetName(), JsonTypeToString(ValueField->Type));
            return false;
        }
        FP->SetPropertyValue_InContainer(TargetContainer, static_cast<float>(ValueField->AsNumber()));
        return true;
    }

    // Double
    if (FDoubleProperty* DP = CastField<FDoubleProperty>(Property))
    {
        if (ValueField->Type != EJson::Number)
        {
            OutError = FString::Printf(TEXT("Expected number for double property '%s', got %s"),
                                       *Property->GetName(), JsonTypeToString(ValueField->Type));
            return false;
        }
        DP->SetPropertyValue_InContainer(TargetContainer, ValueField->AsNumber());
        return true;
    }

    // Int32
    if (FIntProperty* IP = CastField<FIntProperty>(Property))
    {
        return ApplyBoundedIntProperty<FIntProperty, int32>(IP, TargetContainer, ValueField, OutError);
    }

    // Int64
    if (FInt64Property* I64P = CastField<FInt64Property>(Property))
    {
        return ApplyBoundedIntProperty<FInt64Property, int64>(I64P, TargetContainer, ValueField, OutError);
    }

    // Sub-width integer types. PropertyMatchesValueKind advertises these under the
    // 'int' kind, so the importer must apply them too or a validated intValue would
    // fall through to the generic tail and fail.
    if (FInt8Property* I8P = CastField<FInt8Property>(Property))
    {
        return ApplyBoundedIntProperty<FInt8Property, int8>(I8P, TargetContainer, ValueField, OutError);
    }
    if (FInt16Property* I16P = CastField<FInt16Property>(Property))
    {
        return ApplyBoundedIntProperty<FInt16Property, int16>(I16P, TargetContainer, ValueField, OutError);
    }
    if (FUInt16Property* U16P = CastField<FUInt16Property>(Property))
    {
        return ApplyBoundedIntProperty<FUInt16Property, uint16>(U16P, TargetContainer, ValueField, OutError);
    }
    if (FUInt32Property* U32P = CastField<FUInt32Property>(Property))
    {
        return ApplyBoundedIntProperty<FUInt32Property, uint32>(U32P, TargetContainer, ValueField, OutError);
    }
    if (FUInt64Property* U64P = CastField<FUInt64Property>(Property))
    {
        return ApplyBoundedIntProperty<FUInt64Property, uint64>(U64P, TargetContainer, ValueField, OutError);
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
        return ApplyBoundedIntProperty<FByteProperty, uint8>(BP, TargetContainer, ValueField, OutError);
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
        // Instanced subobject re-instancing: a JSON object carrying "__class" (as emitted
        // by the deep-export path) rebuilds the subobject fresh, Outered to
        // OwnerForInstancing so it serializes into the owning asset's package. Restricted
        // to CPF_InstancedReference properties — a plain asset reference expects a path
        // string — and requires a threaded owner.
        if (ValueField->Type == EJson::Object && OP->HasAnyPropertyFlags(CPF_InstancedReference))
        {
            const TSharedPtr<FJsonObject>& Obj = ValueField->AsObject();
            FString ClassPath;
            if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("__class"), ClassPath) || ClassPath.IsEmpty())
            {
                OutError = TEXT("Instanced object value requires a \"__class\" field");
                return false;
            }
            if (!OwnerForInstancing)
            {
                OutError = TEXT("Cannot re-instance subobject: no owner threaded for the NewObject Outer");
                return false;
            }
            UClass* SubClass = LoadObject<UClass>(nullptr, *ClassPath);
            if (!SubClass)
            {
                OutError = FString::Printf(TEXT("Failed to resolve instanced subobject class '%s'"), *ClassPath);
                return false;
            }
            if (OP->PropertyClass && !SubClass->IsChildOf(OP->PropertyClass))
            {
                OutError = FString::Printf(TEXT("Class '%s' is not a '%s'"), *SubClass->GetName(), *OP->PropertyClass->GetName());
                return false;
            }
            UObject* NewInst = NewObject<UObject>(OwnerForInstancing, SubClass, NAME_None, RF_Transactional);
            if (!NewInst)
            {
                OutError = FString::Printf(TEXT("NewObject failed for class '%s'"), *ClassPath);
                return false;
            }
            // Apply every present field that resolves to a property, then assign the
            // instance so the fields that did land are kept. Any child-apply failure is
            // aggregated and propagated: a partial apply is reported as a failure rather
            // than silently swallowed. The new instance is itself the owner for any
            // nested instanced grandchildren.
            TArray<FString> ChildFailures;
            for (const auto& Field : Obj->Values)
            {
                if (Field.Key.Equals(TEXT("__class"), ESearchCase::IgnoreCase))
                {
                    continue;
                }
                FProperty* ChildProp = SubClass->FindPropertyByName(FName(*Field.Key));
                if (!ChildProp)
                {
                    ChildFailures.Add(FString::Printf(TEXT("%s: no such property on '%s'"), *Field.Key, *SubClass->GetName()));
                    continue;
                }
                FString ChildErr;
                if (!ApplyJsonValueToProperty(NewInst, ChildProp, Field.Value, ChildErr, Depth + 1, NewInst))
                {
                    ChildFailures.Add(FString::Printf(TEXT("%s: %s"), *Field.Key, *ChildErr));
                }
            }
            OP->SetObjectPropertyValue_InContainer(TargetContainer, NewInst);
            if (ChildFailures.Num() > 0)
            {
                OutError = FString::Printf(TEXT("Re-instanced subobject '%s' but %d field(s) failed to apply: %s"),
                    *ClassPath, ChildFailures.Num(), *FString::Join(ChildFailures, TEXT("; ")));
                return false;
            }
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
            const TSharedPtr<FJsonObject> StructObj = ValueField->AsObject();

            // Import field-by-field through this importer rather than
            // JsonObjectToUStruct, which silently DROPS unknown fields. Every JSON
            // key must resolve to a real struct field, so a typo'd or stale name
            // fails loud instead of vanishing; each value recurses, so the check
            // holds at every depth. This path is also the only one that handles
            // instanced re-instancing (owner threaded, e.g. an InputMappingContext
            // whose Mappings[] -> Modifiers[] hold {"__class"} subobjects) and it
            // matches the shape ExportStructToJson emits. A JSON null means "field
            // omitted" (JsonObjectToUStruct's behaviour) — skip it, don't error.
            for (const auto& Field : StructObj->Values)
            {
                if (!Field.Value.IsValid() || Field.Value->Type == EJson::Null)
                {
                    continue;
                }
                FProperty* Child = FindStructChildProperty(SP->Struct, Field.Key);
                if (!Child)
                {
                    OutError = FString::Printf(TEXT("Struct '%s' has no field '%s'"), *TypeName, *Field.Key);
                    return false;
                }
                FString ChildErr;
                if (!ApplyJsonValueToProperty(StructPtr, Child, Field.Value, ChildErr, Depth + 1, OwnerForInstancing))
                {
                    OutError = FString::Printf(TEXT("Struct '%s' field '%s': %s"), *TypeName, *Field.Key, *ChildErr);
                    return false;
                }
            }
            return true;
        }

        if (SP->Struct && ValueField->Type == EJson::String)
        {
            const FString Txt = ValueField->AsString();
            // A struct delivered as a stringified JSON blob no longer reaches
            // here: every object-valued param is schema-typed and the transport
            // rejects a stringified object/array before dispatch. A string at a
            // struct property is a UE-syntax textual form ("(X=1,Y=2,Z=3)", FGuid
            // hex, FKey name) — import it directly; anything else is a genuine
            // error, surfaced rather than rescued.
            const TCHAR* ImportResult = nullptr;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            ImportResult = Property->ImportText_Direct(*Txt, StructPtr, nullptr, PPF_None, nullptr);
#else
            ImportResult = Property->ImportText(*Txt, StructPtr, PPF_None, nullptr);
#endif
            if (ImportResult)
            {
                return true;
            }
            OutError = FString::Printf(TEXT("String value for struct '%s' is not an importable UE text form"), *TypeName);
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
        // A stringified array no longer reaches here: array params are schema-
        // typed and the transport rejects a non-array before dispatch. Anything
        // that isn't a JSON array is a genuine error.
        if (ValueField->Type != EJson::Array)
        {
            OutError = TEXT("Expected a JSON array for array property");
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
            if (!ApplyJsonValueToProperty(ElemPtr, AP->Inner, Src[i], InnerErr, Depth + 1, OwnerForInstancing))
            {
                OutError = FString::Printf(TEXT("Failed to set array element %d: %s"), i, *InnerErr);
                return false;
            }
        }
        return true;
    }

    // Maps: a JSON object { "<key>": <value>, ... }. Keys come from the object's
    // field names (always strings, converted to the key property's type); values
    // go through this importer, so struct/object/instanced values work. Mirrors
    // the map export in ExportPropertyToJsonValue.
    if (FMapProperty* MP = CastField<FMapProperty>(Property))
    {
        // A map delivered as a stringified JSON object no longer reaches here:
        // object-valued params are schema-typed and the transport rejects a
        // stringified object before dispatch. Only a real JSON object is valid.
        if (ValueField->Type != EJson::Object)
        {
            OutError = TEXT("Expected a JSON object for map property");
            return false;
        }
        const TSharedPtr<FJsonObject> MapObj = ValueField->AsObject();
        FScriptMapHelper Helper(MP, MP->ContainerPtrToValuePtr<void>(TargetContainer));
        Helper.EmptyValues();
        for (const auto& Pair : MapObj->Values)
        {
            const int32 Index = Helper.AddDefaultValue_Invalid_NeedsRehash();
            // KeyProp and ValueProp offsets are relative to the PAIR base (key at
            // offset 0, value at the map layout's value-offset), so the pair pointer is
            // the correct container base for both. Passing GetValuePtr (already
            // pair+valueOffset) would double-offset the value via the *_InContainer
            // accessors and silently drop it.
            uint8* PairPtr = Helper.GetPairPtr(Index);
            const TSharedPtr<FJsonValue> KeyJson = MakeShared<FJsonValueString>(Pair.Key);
            FString KeyErr;
            if (!ApplyJsonValueToProperty(PairPtr, MP->KeyProp, KeyJson, KeyErr, Depth + 1, OwnerForInstancing))
            {
                OutError = FString::Printf(TEXT("Map key '%s': %s"), *Pair.Key, *KeyErr);
                // The just-added index is unhashed; Rehash() (NOT RemoveAt on that
                // index) restores the container to a valid state before bailing.
                Helper.Rehash();
                return false;
            }
            FString ValErr;
            if (!ApplyJsonValueToProperty(PairPtr, MP->ValueProp, Pair.Value, ValErr, Depth + 1, OwnerForInstancing))
            {
                OutError = FString::Printf(TEXT("Map value for '%s': %s"), *Pair.Key, *ValErr);
                Helper.Rehash();
                return false;
            }
        }
        Helper.Rehash();
        return true;
    }

    // Sets: a JSON array [ <elem>, ... ]. Elements go through this importer.
    // Mirrors the set export in ExportPropertyToJsonValue.
    if (FSetProperty* SetP = CastField<FSetProperty>(Property))
    {
        // A set delivered as a stringified JSON array no longer reaches here:
        // array-valued params are schema-typed and the transport rejects a
        // non-array before dispatch. Only a real JSON array is valid.
        if (ValueField->Type != EJson::Array)
        {
            OutError = TEXT("Expected a JSON array for set property");
            return false;
        }
        const TArray<TSharedPtr<FJsonValue>>& Elems = ValueField->AsArray();
        FScriptSetHelper Helper(SetP, SetP->ContainerPtrToValuePtr<void>(TargetContainer));
        Helper.EmptyElements();
        for (const TSharedPtr<FJsonValue>& Elem : Elems)
        {
            const int32 Index = Helper.AddDefaultValue_Invalid_NeedsRehash();
            FString ElemErr;
            if (!ApplyJsonValueToProperty(Helper.GetElementPtr(Index), SetP->ElementProp, Elem, ElemErr, Depth + 1, OwnerForInstancing))
            {
                OutError = FString::Printf(TEXT("Set element: %s"), *ElemErr);
                // The just-added index is unhashed; Rehash() (NOT RemoveAt on that
                // index) restores the container to a valid state before bailing.
                Helper.Rehash();
                return false;
            }
        }
        Helper.Rehash();
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

// =============================================================================
// Discriminated typed-value helpers (typed-params migration)
// =============================================================================

EMcpTypedValueParse ReadDiscriminatedValue(const TSharedPtr<FJsonObject>& Payload,
                                           FMcpTypedValue& Out, FString& OutDetail)
{
    TArray<FMcpTypedValue> Present;
    bool B = false;
    double N = 0.0;
    FString S;
    const TSharedPtr<FJsonObject>* Obj = nullptr;
    if (Payload->TryGetBoolField(TEXT("boolValue"), B))
        Present.Add({TEXT("boolValue"), TEXT("bool"), MakeShared<FJsonValueBoolean>(B)});
    if (Payload->TryGetNumberField(TEXT("intValue"), N))
        Present.Add({TEXT("intValue"), TEXT("int"), MakeShared<FJsonValueNumber>(N)});
    if (Payload->TryGetNumberField(TEXT("floatValue"), N))
        Present.Add({TEXT("floatValue"), TEXT("float"), MakeShared<FJsonValueNumber>(N)});
    if (Payload->TryGetStringField(TEXT("stringValue"), S))
        Present.Add({TEXT("stringValue"), TEXT("string"), MakeShared<FJsonValueString>(S)});
    if (Payload->TryGetObjectField(TEXT("colorValue"), Obj) && Obj && Obj->IsValid())
    {
        // Omitted alpha imports as opaque (1.0), not the zero-init 0 -- a bare
        // {r,g,b} must not land a fully transparent color.
        TSharedPtr<FJsonObject> Color = MakeShared<FJsonObject>(**Obj);
        if (!Color->HasField(TEXT("a")) && !Color->HasField(TEXT("A")))
            Color->SetNumberField(TEXT("a"), 1.0);
        Present.Add({TEXT("colorValue"), TEXT("color"), MakeShared<FJsonValueObject>(Color)});
    }
    if (Payload->TryGetObjectField(TEXT("vector2Value"), Obj) && Obj && Obj->IsValid())
        Present.Add({TEXT("vector2Value"), TEXT("vector2"), MakeShared<FJsonValueObject>(*Obj)});
    if (Payload->TryGetObjectField(TEXT("vectorValue"), Obj) && Obj && Obj->IsValid())
        Present.Add({TEXT("vectorValue"), TEXT("vector"), MakeShared<FJsonValueObject>(*Obj)});
    if (Payload->TryGetObjectField(TEXT("vector4Value"), Obj) && Obj && Obj->IsValid())
        Present.Add({TEXT("vector4Value"), TEXT("vector4"), MakeShared<FJsonValueObject>(*Obj)});

    // Open escapes for runtime-defined shapes (structs/maps/instanced, arrays).
    // Declared type:object / type:array so they transmit as REAL containers. A
    // stringified escape is a fail-loud bug, not something to rescue here -- no
    // content-sniffing of strings for hidden JSON. The importer is the structural
    // gate for what's inside.
    if (Payload->TryGetObjectField(TEXT("structValue"), Obj) && Obj && Obj->IsValid())
        Present.Add({TEXT("structValue"), TEXT("struct"), MakeShared<FJsonValueObject>(*Obj)});
    const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
    if (Payload->TryGetArrayField(TEXT("arrayValue"), Arr) && Arr)
        Present.Add({TEXT("arrayValue"), TEXT("array"), MakeShared<FJsonValueArray>(*Arr)});

    if (Present.Num() == 0)
        return EMcpTypedValueParse::None;
    if (Present.Num() > 1)
    {
        TArray<FString> Names;
        for (const FMcpTypedValue& V : Present)
            Names.Add(V.Field);
        OutDetail = FString::Join(Names, TEXT(", "));
        return EMcpTypedValueParse::Ambiguous;
    }
    Out = Present[0];
    return EMcpTypedValueParse::Ok;
}

bool PropertyMatchesValueKind(const FProperty* P, const FString& Kind)
{
    if (Kind == TEXT("bool"))
        return CastField<FBoolProperty>(P) != nullptr;
    if (Kind == TEXT("int"))
        return CastField<FIntProperty>(P) || CastField<FInt64Property>(P) ||
               CastField<FInt16Property>(P) || CastField<FInt8Property>(P) ||
               CastField<FUInt16Property>(P) || CastField<FUInt32Property>(P) ||
               CastField<FUInt64Property>(P) || CastField<FByteProperty>(P);
    if (Kind == TEXT("float"))
        return CastField<FFloatProperty>(P) || CastField<FDoubleProperty>(P);
    if (Kind == TEXT("string"))
        return CastField<FStrProperty>(P) || CastField<FNameProperty>(P) ||
               CastField<FTextProperty>(P) || CastField<FEnumProperty>(P) ||
               CastField<FByteProperty>(P) || CastField<FObjectProperty>(P) ||
               CastField<FSoftObjectProperty>(P) || CastField<FSoftClassProperty>(P) ||
               CastField<FClassProperty>(P);
    if (Kind == TEXT("vector"))
    {
        const FStructProperty* SP = CastField<FStructProperty>(P);
        return SP && SP->Struct == TBaseStructure<FVector>::Get();
    }
    if (Kind == TEXT("vector2"))
    {
        const FStructProperty* SP = CastField<FStructProperty>(P);
        return SP && SP->Struct == TBaseStructure<FVector2D>::Get();
    }
    if (Kind == TEXT("vector4"))
    {
        const FStructProperty* SP = CastField<FStructProperty>(P);
        return SP && SP->Struct == TBaseStructure<FVector4>::Get();
    }
    if (Kind == TEXT("color"))
    {
        const FStructProperty* SP = CastField<FStructProperty>(P);
        return SP && (SP->Struct == TBaseStructure<FLinearColor>::Get() ||
                      SP->Struct == TBaseStructure<FColor>::Get());
    }
    // Open escapes: match the container families and let the importer be the real
    // structural gate (a struct/map value at a float still fails loud here).
    if (Kind == TEXT("struct"))
        return CastField<FStructProperty>(P) || CastField<FObjectProperty>(P) ||
               CastField<FMapProperty>(P);
    if (Kind == TEXT("array"))
        return CastField<FArrayProperty>(P) || CastField<FSetProperty>(P);
    return false;
}

} // namespace McpPropertyReflection
