#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "McpAutomationBridge_PropertyImportTestTypes.generated.h"

// Reflected fixture types for the property-import behavioral spec. UHT does not
// process .cpp files, so the tested UCLASS/USTRUCT/UENUM must live in a header;
// they are private to the module and used only by
// McpAutomationBridge_PropertyImportTests.cpp.

UENUM()
enum class EMcpPropImportTestEnum : uint8
{
    Alpha,
    Beta,
    Gamma
};

USTRUCT()
struct FMcpPropImportTestSub
{
    GENERATED_BODY()

    UPROPERTY()
    int32 A = 0;

    UPROPERTY()
    float B = 0.f;
};

// Dedicated fixture for the template-write propagation spec
// (CaptureArchetypeInstances / PropagateDefaultToInstances). Separate from
// UMcpPropImportTestObject so instance counts in those tests are not polluted
// by fixtures other import tests leave behind before GC.
UCLASS()
class UMcpPropagationTestObject : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY() int32 IntProp = 7;
    UPROPERTY() FMcpPropImportTestSub SubProp;
    UPROPERTY(Instanced) TObjectPtr<UObject> InstancedRef;
};

UCLASS()
class UMcpPropImportTestObject : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY() bool BoolProp = false;
    UPROPERTY() int32 IntProp = 0;
    UPROPERTY() int64 Int64Prop = 0;
    UPROPERTY() int16 Int16Prop = 0;
    UPROPERTY() int8 Int8Prop = 0;
    UPROPERTY() uint8 ByteProp = 0;
    UPROPERTY() uint16 UInt16Prop = 0;
    UPROPERTY() uint32 UInt32Prop = 0;
    UPROPERTY() uint64 UInt64Prop = 0;
    UPROPERTY() float FloatProp = 0.f;
    UPROPERTY() double DoubleProp = 0.0;
    UPROPERTY() FString StrProp;
    UPROPERTY() FName NameProp;
    UPROPERTY() EMcpPropImportTestEnum EnumProp = EMcpPropImportTestEnum::Alpha;
    UPROPERTY() FVector VectorProp = FVector::ZeroVector;
    UPROPERTY() FMcpPropImportTestSub SubProp;
    UPROPERTY() TArray<int32> IntArray;
    UPROPERTY() TObjectPtr<UObject> ObjectRef = nullptr;
    UPROPERTY() TSoftObjectPtr<UObject> SoftObjectRef;
};
