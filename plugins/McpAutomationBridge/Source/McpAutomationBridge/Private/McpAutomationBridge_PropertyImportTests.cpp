// =============================================================================
// McpAutomationBridge_PropertyImportTests.cpp
// =============================================================================
// Behavioral spec for the unified JSON->FProperty importer
// (McpPropertyReflection::ApplyJsonValueToProperty) and its validator twin
// (PropertyMatchesValueKind / ReadDiscriminatedValue).
//
// Run headlessly with:
//   system_control { action: "run_tests", filter: "McpBridge.PropertyImport" }
// =============================================================================

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "McpAutomationBridge_PropertyImportTestTypes.h"
#include "McpPropertyReflection.h"
#include "Dom/JsonObject.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace McpPropertyImportTestUtil
{
    static UMcpPropImportTestObject* NewFixture()
    {
        return NewObject<UMcpPropImportTestObject>(GetTransientPackage());
    }

    static FProperty* Prop(const TCHAR* Name)
    {
        return UMcpPropImportTestObject::StaticClass()->FindPropertyByName(Name);
    }

    static TSharedPtr<FJsonValue> JBool(bool B) { return MakeShared<FJsonValueBoolean>(B); }
    static TSharedPtr<FJsonValue> JNum(double N) { return MakeShared<FJsonValueNumber>(N); }
    static TSharedPtr<FJsonValue> JStr(const FString& S) { return MakeShared<FJsonValueString>(S); }
    static TSharedPtr<FJsonValue> JArr(TArray<TSharedPtr<FJsonValue>> A) { return MakeShared<FJsonValueArray>(MoveTemp(A)); }
    static TSharedPtr<FJsonValue> JObj(TSharedPtr<FJsonObject> O) { return MakeShared<FJsonValueObject>(O); }
}

// -----------------------------------------------------------------------------
// ACCEPT: correctly-typed JSON lands and the property holds the value.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpPropertyImportAccept, "McpBridge.PropertyImport.Accept",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPropertyImportAccept::RunTest(const FString& Parameters)
{
    using namespace McpPropertyReflection;
    using namespace McpPropertyImportTestUtil;

    UMcpPropImportTestObject* Obj = NewFixture();
    TestNotNull(TEXT("fixture"), Obj);
    if (!Obj) return false;

    FString Err;

    TestTrue(TEXT("bool accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("BoolProp")), JBool(true), Err));
    TestTrue(TEXT("bool value"), Obj->BoolProp);

    TestTrue(TEXT("int accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("IntProp")), JNum(123), Err));
    TestEqual(TEXT("int value"), Obj->IntProp, 123);

    TestTrue(TEXT("int64 accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("Int64Prop")), JNum(9000000000.0), Err));
    TestEqual(TEXT("int64 value"), Obj->Int64Prop, static_cast<int64>(9000000000LL));

    TestTrue(TEXT("int16 accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("Int16Prop")), JNum(1000), Err));
    TestEqual(TEXT("int16 value"), static_cast<int32>(Obj->Int16Prop), 1000);

    TestTrue(TEXT("int8 accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("Int8Prop")), JNum(-5), Err));
    TestEqual(TEXT("int8 value"), static_cast<int32>(Obj->Int8Prop), -5);

    TestTrue(TEXT("byte accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("ByteProp")), JNum(200), Err));
    TestEqual(TEXT("byte value"), static_cast<int32>(Obj->ByteProp), 200);

    TestTrue(TEXT("uint16 accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("UInt16Prop")), JNum(60000), Err));
    TestEqual(TEXT("uint16 value"), static_cast<int32>(Obj->UInt16Prop), 60000);

    TestTrue(TEXT("uint32 accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("UInt32Prop")), JNum(3000000000.0), Err));
    TestEqual(TEXT("uint32 value"), static_cast<int64>(Obj->UInt32Prop), static_cast<int64>(3000000000LL));

    TestTrue(TEXT("uint64 accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("UInt64Prop")), JNum(5000000000.0), Err));
    TestEqual(TEXT("uint64 value"), static_cast<int64>(Obj->UInt64Prop), static_cast<int64>(5000000000LL));

    TestTrue(TEXT("float accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("FloatProp")), JNum(3.5), Err));
    TestEqual(TEXT("float value"), Obj->FloatProp, 3.5f);

    TestTrue(TEXT("double accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("DoubleProp")), JNum(2.25), Err));
    TestEqual(TEXT("double value"), Obj->DoubleProp, 2.25);

    TestTrue(TEXT("string accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("StrProp")), JStr(TEXT("hi")), Err));
    TestEqual(TEXT("string value"), Obj->StrProp, FString(TEXT("hi")));

    TestTrue(TEXT("name accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("NameProp")), JStr(TEXT("Bone")), Err));
    TestTrue(TEXT("name value"), Obj->NameProp == FName(TEXT("Bone")));

    TestTrue(TEXT("enum accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("EnumProp")), JStr(TEXT("Beta")), Err));
    TestTrue(TEXT("enum value"), Obj->EnumProp == EMcpPropImportTestEnum::Beta);

    {
        TArray<TSharedPtr<FJsonValue>> A = { JNum(1), JNum(2), JNum(3) };
        TestTrue(TEXT("vector accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("VectorProp")), JArr(A), Err));
        TestTrue(TEXT("vector value"), Obj->VectorProp.Equals(FVector(1, 2, 3)));
    }

    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetField(TEXT("A"), JNum(7));
        O->SetField(TEXT("B"), JNum(1.5));
        TestTrue(TEXT("struct accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("SubProp")), JObj(O), Err));
        TestEqual(TEXT("struct A"), Obj->SubProp.A, 7);
        TestEqual(TEXT("struct B"), Obj->SubProp.B, 1.5f);
    }

    {
        TArray<TSharedPtr<FJsonValue>> A = { JNum(10), JNum(20), JNum(30) };
        TestTrue(TEXT("array accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("IntArray")), JArr(A), Err));
        TestEqual(TEXT("array num"), Obj->IntArray.Num(), 3);
        if (Obj->IntArray.Num() == 3)
        {
            TestEqual(TEXT("array[0]"), Obj->IntArray[0], 10);
            TestEqual(TEXT("array[2]"), Obj->IntArray[2], 30);
        }
    }

    {
        // Our own class is guaranteed resident, so its path always resolves.
        const FString Path = UMcpPropImportTestObject::StaticClass()->GetPathName();
        TestTrue(TEXT("object accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("ObjectRef")), JStr(Path), Err));
        TestTrue(TEXT("object value"), Obj->ObjectRef == UMcpPropImportTestObject::StaticClass());
    }

    {
        // Soft refs store the path without loading, so any well-formed path lands.
        TestTrue(TEXT("soft object accept"), ApplyJsonValueToProperty(Obj, Prop(TEXT("SoftObjectRef")), JStr(TEXT("/Game/Foo/Bar.Bar")), Err));
        TestEqual(TEXT("soft object value"), Obj->SoftObjectRef.ToSoftObjectPath().ToString(), FString(TEXT("/Game/Foo/Bar.Bar")));
    }

    return true;
}

// -----------------------------------------------------------------------------
// REJECT: wrong-typed JSON fails loud and the property keeps its prior value.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpPropertyImportReject, "McpBridge.PropertyImport.Reject",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPropertyImportReject::RunTest(const FString& Parameters)
{
    using namespace McpPropertyReflection;
    using namespace McpPropertyImportTestUtil;

    UMcpPropImportTestObject* Obj = NewFixture();
    if (!Obj) { AddError(TEXT("fixture null")); return false; }

    auto Reject = [&](const TCHAR* Label, const TCHAR* PropName, const TSharedPtr<FJsonValue>& V)
    {
        FString Err;
        const bool bOk = ApplyJsonValueToProperty(Obj, Prop(PropName), V, Err);
        TestFalse(FString::Printf(TEXT("%s returns false"), Label), bOk);
        TestFalse(FString::Printf(TEXT("%s error non-empty"), Label), Err.IsEmpty());
    };

    Obj->BoolProp = true;
    Reject(TEXT("bool<-number"), TEXT("BoolProp"), JNum(1));
    TestTrue(TEXT("bool<-number retains prior"), Obj->BoolProp);
    Reject(TEXT("bool<-string"), TEXT("BoolProp"), JStr(TEXT("true")));
    TestTrue(TEXT("bool<-string retains prior"), Obj->BoolProp);

    Obj->FloatProp = 9.f;
    Reject(TEXT("float<-string"), TEXT("FloatProp"), JStr(TEXT("3.14")));
    TestEqual(TEXT("float retains prior"), Obj->FloatProp, 9.f);

    Obj->IntProp = 77;
    Reject(TEXT("int<-string"), TEXT("IntProp"), JStr(TEXT("5")));
    TestEqual(TEXT("int retains prior"), Obj->IntProp, 77);

    Obj->Int16Prop = 123;
    Reject(TEXT("int16 over"), TEXT("Int16Prop"), JNum(40000));
    TestEqual(TEXT("int16 over retains prior"), static_cast<int32>(Obj->Int16Prop), 123);
    Reject(TEXT("int16 under"), TEXT("Int16Prop"), JNum(-40000));
    TestEqual(TEXT("int16 under retains prior"), static_cast<int32>(Obj->Int16Prop), 123);

    Obj->Int8Prop = 10;
    Reject(TEXT("int8 over"), TEXT("Int8Prop"), JNum(200));
    TestEqual(TEXT("int8 retains prior"), static_cast<int32>(Obj->Int8Prop), 10);

    Obj->ByteProp = 5;
    Reject(TEXT("byte over"), TEXT("ByteProp"), JNum(300));
    TestEqual(TEXT("byte over retains prior"), static_cast<int32>(Obj->ByteProp), 5);
    Reject(TEXT("byte under"), TEXT("ByteProp"), JNum(-1));
    TestEqual(TEXT("byte under retains prior"), static_cast<int32>(Obj->ByteProp), 5);

    Obj->UInt16Prop = 42;
    Reject(TEXT("uint16 over"), TEXT("UInt16Prop"), JNum(70000));
    TestEqual(TEXT("uint16 over retains prior"), static_cast<int32>(Obj->UInt16Prop), 42);
    Reject(TEXT("uint16 negative"), TEXT("UInt16Prop"), JNum(-1));
    TestEqual(TEXT("uint16 neg retains prior"), static_cast<int32>(Obj->UInt16Prop), 42);

    Obj->UInt32Prop = 7;
    Reject(TEXT("uint32 negative"), TEXT("UInt32Prop"), JNum(-1));
    TestEqual(TEXT("uint32 neg retains prior"), static_cast<int64>(Obj->UInt32Prop), static_cast<int64>(7));
    Reject(TEXT("uint32 over"), TEXT("UInt32Prop"), JNum(5000000000.0));
    TestEqual(TEXT("uint32 over retains prior"), static_cast<int64>(Obj->UInt32Prop), static_cast<int64>(7));

    Obj->EnumProp = EMcpPropImportTestEnum::Gamma;
    Reject(TEXT("enum unknown"), TEXT("EnumProp"), JStr(TEXT("Nonexistent")));
    TestTrue(TEXT("enum retains prior"), Obj->EnumProp == EMcpPropImportTestEnum::Gamma);

    Obj->SubProp.A = 55;
    Obj->SubProp.B = 4.f;
    {
        // A lone unknown field fails before any field is written, so the struct is
        // untouched. (The importer is not transactional for mixed known/unknown
        // fields — see project note "Partial-handling: no rollback".)
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetField(TEXT("Bogus"), JNum(2));
        Reject(TEXT("struct unknown field"), TEXT("SubProp"), JObj(O));
        TestEqual(TEXT("struct A retains prior"), Obj->SubProp.A, 55);
        TestEqual(TEXT("struct B retains prior"), Obj->SubProp.B, 4.f);
    }

    Reject(TEXT("object<-number"), TEXT("ObjectRef"), JNum(5));

    return true;
}

// -----------------------------------------------------------------------------
// Validator/importer agreement for the sub-int types, and that a validated
// intValue on FInt16Property both validates AND applies.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpPropertyImportKindAgreement, "McpBridge.PropertyImport.KindAgreement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPropertyImportKindAgreement::RunTest(const FString& Parameters)
{
    using namespace McpPropertyReflection;
    using namespace McpPropertyImportTestUtil;

    // UHT must have mapped each member to the property class the kind table and
    // importer branches assume; otherwise the two would silently disagree.
    TestNotNull(TEXT("Int16 is FInt16Property"), CastField<FInt16Property>(Prop(TEXT("Int16Prop"))));
    TestNotNull(TEXT("Int8 is FInt8Property"), CastField<FInt8Property>(Prop(TEXT("Int8Prop"))));
    TestNotNull(TEXT("UInt16 is FUInt16Property"), CastField<FUInt16Property>(Prop(TEXT("UInt16Prop"))));
    TestNotNull(TEXT("UInt32 is FUInt32Property"), CastField<FUInt32Property>(Prop(TEXT("UInt32Prop"))));
    TestNotNull(TEXT("UInt64 is FUInt64Property"), CastField<FUInt64Property>(Prop(TEXT("UInt64Prop"))));
    TestNotNull(TEXT("Byte is FByteProperty"), CastField<FByteProperty>(Prop(TEXT("ByteProp"))));
    TestNotNull(TEXT("Enum is FEnumProperty"), CastField<FEnumProperty>(Prop(TEXT("EnumProp"))));

    const TCHAR* IntNames[] = { TEXT("IntProp"), TEXT("Int64Prop"), TEXT("Int16Prop"), TEXT("Int8Prop"),
                                TEXT("UInt16Prop"), TEXT("UInt32Prop"), TEXT("UInt64Prop"), TEXT("ByteProp") };
    for (const TCHAR* Name : IntNames)
    {
        TestTrue(FString::Printf(TEXT("%s matches kind int"), Name),
                 PropertyMatchesValueKind(Prop(Name), TEXT("int")));
    }

    UMcpPropImportTestObject* Obj = NewFixture();
    if (!Obj) { AddError(TEXT("fixture null")); return false; }

    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetNumberField(TEXT("intValue"), 1234);
    FMcpTypedValue Typed;
    FString Detail;
    TestTrue(TEXT("intValue parses Ok"),
             ReadDiscriminatedValue(Payload, Typed, Detail) == EMcpTypedValueParse::Ok);
    TestEqual(TEXT("kind is int"), Typed.Kind, FString(TEXT("int")));
    TestTrue(TEXT("int16 validates against int kind"),
             PropertyMatchesValueKind(Prop(TEXT("Int16Prop")), Typed.Kind));

    FString Err;
    TestTrue(TEXT("int16 applies the validated value"),
             ApplyJsonValueToProperty(Obj, Prop(TEXT("Int16Prop")), Typed.Json, Err));
    TestEqual(TEXT("int16 applied value"), static_cast<int32>(Obj->Int16Prop), 1234);

    return true;
}

// -----------------------------------------------------------------------------
// Object-reference-by-string is the sanctioned typed contract: the 'string' kind
// matches object/soft-object properties, and the importer resolves a path.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpPropertyImportObjectPath, "McpBridge.PropertyImport.ObjectPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPropertyImportObjectPath::RunTest(const FString& Parameters)
{
    using namespace McpPropertyReflection;
    using namespace McpPropertyImportTestUtil;

    FProperty* ObjProp = Prop(TEXT("ObjectRef"));

    TestTrue(TEXT("string kind matches object property"), PropertyMatchesValueKind(ObjProp, TEXT("string")));
    TestTrue(TEXT("struct kind still matches object property"), PropertyMatchesValueKind(ObjProp, TEXT("struct")));
    TestFalse(TEXT("int kind rejects object property"), PropertyMatchesValueKind(ObjProp, TEXT("int")));
    TestFalse(TEXT("float kind rejects object property"), PropertyMatchesValueKind(ObjProp, TEXT("float")));
    TestTrue(TEXT("string kind matches soft object property"),
             PropertyMatchesValueKind(Prop(TEXT("SoftObjectRef")), TEXT("string")));

    UMcpPropImportTestObject* Obj = NewFixture();
    if (!Obj) { AddError(TEXT("fixture null")); return false; }
    FString Err;

    const FString GoodPath = UMcpPropImportTestObject::StaticClass()->GetPathName();
    TestTrue(TEXT("object path resolves"), ApplyJsonValueToProperty(Obj, ObjProp, JStr(GoodPath), Err));
    TestTrue(TEXT("object ref set"), Obj->ObjectRef == UMcpPropImportTestObject::StaticClass());

    Obj->ObjectRef = UMcpPropImportTestObject::StaticClass();
    const bool bBad = ApplyJsonValueToProperty(Obj, ObjProp, JStr(TEXT("/Script/Engine.ThisClassDoesNotExist_McpTest")), Err);
    TestFalse(TEXT("unresolved object path returns false"), bBad);
    TestFalse(TEXT("unresolved object path error non-empty"), Err.IsEmpty());
    TestTrue(TEXT("unresolved object path retains prior"), Obj->ObjectRef == UMcpPropImportTestObject::StaticClass());

    return true;
}

// =============================================================================
// Template-write propagation spec (CaptureArchetypeInstances /
// PropagateDefaultToInstances / AddPropagationReport).
// Fixtures use UMcpPropagationTestObject's CDO as the template; instances are
// MarkAsGarbage()d at test end so re-runs in the same session see none of them.
// =============================================================================

namespace McpPropagationTestUtil
{
    static UMcpPropagationTestObject* NewInstance()
    {
        return NewObject<UMcpPropagationTestObject>(GetTransientPackage());
    }

    static FProperty* Prop(const TCHAR* Name)
    {
        return UMcpPropagationTestObject::StaticClass()->FindPropertyByName(Name);
    }
}

// -----------------------------------------------------------------------------
// A non-template owner produces no sweep and no report field.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpPropagationNonTemplate, "McpBridge.PropertyImport.Propagation.NonTemplate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPropagationNonTemplate::RunTest(const FString& Parameters)
{
    using namespace McpPropertyReflection;
    using namespace McpPropagationTestUtil;

    UMcpPropagationTestObject* Instance = NewInstance();
    if (!Instance) { AddError(TEXT("fixture null")); return false; }

    FMcpDefaultPropagation State = CaptureArchetypeInstances(Instance, Prop(TEXT("IntProp")));
    TestFalse(TEXT("instance is not a template target"), State.bTemplateTarget);

    PropagateDefaultToInstances(State);
    TestEqual(TEXT("nothing updated"), State.InstancesUpdated, 0);

    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    AddPropagationReport(Response, State);
    TestFalse(TEXT("no report field for non-template writes"),
              Response->HasField(TEXT("defaultPropagation")));

    Instance->MarkAsGarbage();
    return true;
}

// -----------------------------------------------------------------------------
// The CDO sweep: instances still on the old default follow the new one;
// an instance that diverged keeps its override and is counted, not touched.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpPropagationCdoSweep, "McpBridge.PropertyImport.Propagation.CdoSweep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPropagationCdoSweep::RunTest(const FString& Parameters)
{
    using namespace McpPropertyReflection;
    using namespace McpPropagationTestUtil;
    using McpPropertyImportTestUtil::JNum;

    UMcpPropagationTestObject* CDO = GetMutableDefault<UMcpPropagationTestObject>();
    const int32 OriginalDefault = CDO->IntProp;

    UMcpPropagationTestObject* Follows1 = NewInstance();
    UMcpPropagationTestObject* Follows2 = NewInstance();
    UMcpPropagationTestObject* Overridden = NewInstance();
    if (!Follows1 || !Follows2 || !Overridden) { AddError(TEXT("fixture null")); return false; }
    Overridden->IntProp = 42;

    FMcpDefaultPropagation State = CaptureArchetypeInstances(CDO, Prop(TEXT("IntProp")));
    TestTrue(TEXT("CDO is a template target"), State.bTemplateTarget);

    FString Err;
    TestTrue(TEXT("CDO write applies"),
             ApplyJsonValueToProperty(CDO, Prop(TEXT("IntProp")), JNum(99), Err));
    PropagateDefaultToInstances(State);

    TestEqual(TEXT("matching instance 1 follows"), Follows1->IntProp, 99);
    TestEqual(TEXT("matching instance 2 follows"), Follows2->IntProp, 99);
    TestEqual(TEXT("overridden instance untouched"), Overridden->IntProp, 42);
    TestEqual(TEXT("updated count"), State.InstancesUpdated, 2);
    TestEqual(TEXT("leftOverridden count"), State.InstancesLeftOverridden, 1);
    TestEqual(TEXT("skipped count"), State.InstancesSkipped, 0);

    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    AddPropagationReport(Response, State);
    const TSharedPtr<FJsonObject>* Report = nullptr;
    if (TestTrue(TEXT("report field present"),
                 Response->TryGetObjectField(TEXT("defaultPropagation"), Report)))
    {
        TestEqual(TEXT("report instancesUpdated"),
                  static_cast<int32>((*Report)->GetNumberField(TEXT("instancesUpdated"))), 2);
        TestEqual(TEXT("report instancesLeftOverridden"),
                  static_cast<int32>((*Report)->GetNumberField(TEXT("instancesLeftOverridden"))), 1);
    }

    CDO->IntProp = OriginalDefault;
    Follows1->MarkAsGarbage();
    Follows2->MarkAsGarbage();
    Overridden->MarkAsGarbage();
    return true;
}

// -----------------------------------------------------------------------------
// Struct roots match and copy whole: divergence anywhere inside the root
// struct protects the entire instance value.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpPropagationStructRoot, "McpBridge.PropertyImport.Propagation.StructRoot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPropagationStructRoot::RunTest(const FString& Parameters)
{
    using namespace McpPropertyReflection;
    using namespace McpPropagationTestUtil;

    UMcpPropagationTestObject* CDO = GetMutableDefault<UMcpPropagationTestObject>();
    const FMcpPropImportTestSub OriginalDefault = CDO->SubProp;

    UMcpPropagationTestObject* Follows = NewInstance();
    UMcpPropagationTestObject* Overridden = NewInstance();
    if (!Follows || !Overridden) { AddError(TEXT("fixture null")); return false; }
    Overridden->SubProp.A = 5;

    FMcpDefaultPropagation State = CaptureArchetypeInstances(CDO, Prop(TEXT("SubProp")));

    // Direct member write on the CDO stands in for a nested-path import
    // ("SubProp.B"): propagation only sees the root struct property.
    CDO->SubProp.B = 2.5f;
    PropagateDefaultToInstances(State);

    TestEqual(TEXT("matching instance follows whole struct"), Follows->SubProp.B, 2.5f);
    TestEqual(TEXT("diverged instance keeps its A"), Overridden->SubProp.A, 5);
    TestEqual(TEXT("diverged instance keeps its B"), Overridden->SubProp.B, 0.f);
    TestEqual(TEXT("updated count"), State.InstancesUpdated, 1);
    TestEqual(TEXT("leftOverridden count"), State.InstancesLeftOverridden, 1);

    CDO->SubProp = OriginalDefault;
    Follows->MarkAsGarbage();
    Overridden->MarkAsGarbage();
    return true;
}

// -----------------------------------------------------------------------------
// Instanced-subobject properties are never binary-copied into instances:
// counted as skipped and reported, values untouched.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpPropagationInstancedSkip, "McpBridge.PropertyImport.Propagation.InstancedSkip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPropagationInstancedSkip::RunTest(const FString& Parameters)
{
    using namespace McpPropertyReflection;
    using namespace McpPropagationTestUtil;

    UMcpPropagationTestObject* CDO = GetMutableDefault<UMcpPropagationTestObject>();
    UMcpPropagationTestObject* Instance = NewInstance();
    if (!Instance) { AddError(TEXT("fixture null")); return false; }

    FMcpDefaultPropagation State = CaptureArchetypeInstances(CDO, Prop(TEXT("InstancedRef")));
    TestTrue(TEXT("CDO is a template target"), State.bTemplateTarget);
    TestEqual(TEXT("instance counted as skipped"), State.InstancesSkipped, 1);

    PropagateDefaultToInstances(State);
    TestEqual(TEXT("nothing updated"), State.InstancesUpdated, 0);

    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    AddPropagationReport(Response, State);
    const TSharedPtr<FJsonObject>* Report = nullptr;
    if (TestTrue(TEXT("report field present"),
                 Response->TryGetObjectField(TEXT("defaultPropagation"), Report)))
    {
        TestEqual(TEXT("report skipped count"),
                  static_cast<int32>((*Report)->GetNumberField(
                      TEXT("instancesSkippedInstancedProperty"))), 1);
    }

    Instance->MarkAsGarbage();
    return true;
}

#endif // WITH_AUTOMATION_TESTS
