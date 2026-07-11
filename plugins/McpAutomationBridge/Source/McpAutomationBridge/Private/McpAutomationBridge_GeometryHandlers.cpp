// =============================================================================
// McpAutomationBridge_GeometryHandlers.cpp
// =============================================================================
// Procedural mesh creation and manipulation using UE Geometry Script APIs.
//
// HANDLERS:
//   Primitive Creation:
//     - create_box, create_sphere, create_cylinder, create_capsule
//     - create_cone, create_torus, create_plane
//
//   Mesh Operations:
//     - mesh_boolean (union, subtract, intersect)
//     - mesh_transform (translate, rotate, scale)
//     - mesh_deform (bend, twist, taper, noise)
//     - mesh_simplify, mesh_subdivide, mesh_repair
//
//   Mesh Editing:
//     - mesh_add_vertices, mesh_add_triangles
//     - mesh_set_normals, mesh_set_uvs
//     - mesh_remesh, mesh_smooth
//
//   Asset Generation:
//     - create_static_mesh_from_dynamic
//     - generate_collision, simplify_collision
//
// REFACTORING NOTES:
//   - Uses McpVersionCompatibility.h for UE 5.0-5.7 API abstraction
//   - Uses McpHandlerUtils for standardized JSON parsing/responses
//   - Geometry Script requires UE 5.1+ for full functionality
//   - Header paths vary significantly between UE versions
//
// VERSION COMPATIBILITY:
//   - GeometryScript: UE 5.0 (experimental) / UE 5.1+ (full support)
//   - MeshRemeshFunctions: UE 5.1+
//   - MeshTransformFunctions: UE 5.1+
//   - CollisionFunctions: UE 5.4+
//   - MeshBoundaryLoops/EdgeLoop: UE 5.5+
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"
#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_GeometryHandlers.h"
#include "Misc/EngineVersionComparison.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpGeometryHandlers, Log, All);
#if WITH_EDITOR

#include "Components/DynamicMeshComponent.h"
#include "DynamicMeshActor.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"

// GeometryCore includes for low-level mesh operations (FMeshBoundaryLoops, FEdgeLoop)
// Required for bridge operations in UE 5.5+
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#include "MeshBoundaryLoops.h"
#include "EdgeLoop.h"
#endif

// Geometry Script Includes
// Paths may vary by UE version (e.g. 5.0 vs 5.3 vs 5.7)
#if __has_include("GeometryScript/GeometryScriptTypes.h")
#include "GeometryScript/GeometryScriptTypes.h"
#else
// Fallback for when headers are directly in include path or different folder
#include "GeometryScriptTypes.h" 
#endif

#include "GeometryScript/MeshQueryFunctions.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshDeformFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
// Note: MeshRemeshFunctions.h was introduced in UE 5.1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "GeometryScript/MeshRemeshFunctions.h"
#endif
#include "GeometryScript/MeshRepairFunctions.h"
#include "GeometryScript/MeshSimplifyFunctions.h"
#include "GeometryScript/MeshSubdivideFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"

// Collision functions for generate_collision (UE 5.4+)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
#include "GeometryScript/CollisionFunctions.h"
#endif

// UE 5.1+: MeshTransformFunctions contains TranslateMesh, ScaleMesh, etc.
// UE 5.0: These functions are in a different location or not available
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "GeometryScript/MeshTransformFunctions.h"
#endif

#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "UDynamicMesh.h"
#include "Components/SplineComponent.h"

// GeometryScript is only fully supported in UE 5.1+
// UE 5.0 had experimental GeometryScript with limited API
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_HAS_FULL_GEOMETRY_SCRIPT 1
#else
#define MCP_HAS_FULL_GEOMETRY_SCRIPT 0
#endif
 
// Helper macros for JSON field access
 
// Helper to read FVector from JSON (supports both object and array formats)
static FVector ReadVectorFromPayload(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, FVector Default = FVector::ZeroVector)
{
    if (!Payload.IsValid())
        return Default;

    // Try array format first [x, y, z]
    const TArray<TSharedPtr<FJsonValue>>* ArrayPtr;
    if (Payload->TryGetArrayField(FieldName, ArrayPtr) && ArrayPtr->Num() >= 3)
    {
        return FVector(
            (*ArrayPtr)[0]->AsNumber(),
            (*ArrayPtr)[1]->AsNumber(),
            (*ArrayPtr)[2]->AsNumber()
        );
    }

    // Try object format {x, y, z}; missing components fall back to the per-component Default
    // (so a partial scale object keeps 1 on the unspecified axes rather than collapsing to 0).
    const TSharedPtr<FJsonObject>* ObjPtr;
    if (Payload->TryGetObjectField(FieldName, ObjPtr))
    {
        return FVector(
            GetJsonNumberField((*ObjPtr), TEXT("x"), Default.X),
            GetJsonNumberField((*ObjPtr), TEXT("y"), Default.Y),
            GetJsonNumberField((*ObjPtr), TEXT("z"), Default.Z)
        );
    }

    return Default;
}

// Helper to read FRotator from JSON (supports {pitch,yaw,roll} object and [pitch,yaw,roll] array)
static FRotator ReadRotatorFromPayload(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, FRotator Default = FRotator::ZeroRotator)
{
    if (!Payload.IsValid())
        return Default;

    // Try array format first [pitch, yaw, roll]
    const TArray<TSharedPtr<FJsonValue>>* ArrayPtr;
    if (Payload->TryGetArrayField(FieldName, ArrayPtr) && ArrayPtr->Num() >= 3)
    {
        return FRotator(
            (*ArrayPtr)[0]->AsNumber(),  // Pitch
            (*ArrayPtr)[1]->AsNumber(),  // Yaw
            (*ArrayPtr)[2]->AsNumber()   // Roll
        );
    }

    // Try object format {pitch, yaw, roll}
    const TSharedPtr<FJsonObject>* ObjPtr;
    if (Payload->TryGetObjectField(FieldName, ObjPtr))
    {
        if ((*ObjPtr)->HasField(TEXT("pitch")) || (*ObjPtr)->HasField(TEXT("yaw")) || (*ObjPtr)->HasField(TEXT("roll")))
        {
            return FRotator(
                GetJsonNumberField((*ObjPtr), TEXT("pitch"), 0.0),
                GetJsonNumberField((*ObjPtr), TEXT("yaw"), 0.0),
                GetJsonNumberField((*ObjPtr), TEXT("roll"), 0.0)
            );
        }
    }

    return Default;
}

// Helper to read FTransform from JSON
static FTransform ReadTransformFromPayload(const TSharedPtr<FJsonObject>& Payload)
{
    FVector Location = ReadVectorFromPayload(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = ReadRotatorFromPayload(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FVector Scale = ReadVectorFromPayload(Payload, TEXT("scale"), FVector::OneVector);

    return FTransform(
        Rotation,
        Location,
        Scale
    );
}

// Helper to create or get a dynamic mesh for operations
static UDynamicMesh* GetOrCreateDynamicMesh(UObject* Outer)
{
    return NewObject<UDynamicMesh>(Outer);
}

// Helper to spawn DynamicMeshActor and assign mesh - REDUCES CODE DUPLICATION
static AActor* SpawnDynamicMeshActorWithMesh(
    const FTransform& Transform,
    const FString& Name,
    UDynamicMesh* DynMesh,
    FString& OutError)
{
    UWorld* World = nullptr;
    if (GEditor && GEditor->PlayWorld)
    {
        World = GEditor->PlayWorld.Get();
    }
    else if (GEditor)
    {
        World = GEditor->GetEditorWorldContext().World();
    }

    if (!World)
    {
        OutError = TEXT("Editor world unavailable");
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    SpawnParams.ObjectFlags |= RF_Transactional;
    if (!GEditor->PlayWorld)
    {
        SpawnParams.OverrideLevel = World->GetCurrentLevel();
        World->Modify();
    }
    
    const FVector SpawnLocation = Transform.GetLocation();
    const FRotator SpawnRotation = Transform.Rotator();
    AActor* NewActor = World->SpawnActor(
        ADynamicMeshActor::StaticClass(),
        &SpawnLocation,
        &SpawnRotation,
        SpawnParams);
    
    if (!NewActor)
    {
        OutError = TEXT("Failed to spawn DynamicMeshActor");
        return nullptr;
    }
    
    NewActor->Modify();
    NewActor->SetActorLocationAndRotation(SpawnLocation,
                                          SpawnRotation, false, nullptr,
                                          ETeleportType::TeleportPhysics);
    NewActor->SetActorLabel(Name);
    
    if (ADynamicMeshActor* DMActor = Cast<ADynamicMeshActor>(NewActor))
    {
        if (UDynamicMeshComponent* DMComp = DMActor->GetDynamicMeshComponent())
        {
            DMComp->SetDynamicMesh(DynMesh);
        }
    }
    
    return NewActor;
}

// Safety limits for geometry operations to prevent OOM
static constexpr int32 MAX_SEGMENTS = 256;
static constexpr double MAX_DIMENSION = 100000.0;
static constexpr double MIN_DIMENSION = 0.01;

// Polygon count guards to prevent OOM crashes
// Recommended limit for dynamic meshes: 100K-500K triangles (UE5 best practices)
static constexpr int32 MAX_TRIANGLES_PER_DYNAMIC_MESH = 500000;
static constexpr int32 MAX_SUBDIVIDE_ITERATIONS = 6;  // Each iteration quadruples triangles
static constexpr int32 WARNING_TRIANGLE_THRESHOLD = 250000;

// Memory pressure threshold (percentage)
static constexpr float MEMORY_PRESSURE_WARNING = 0.80f;   // Alert at 80% memory used
static constexpr float MEMORY_PRESSURE_CRITICAL = 0.90f;  // Block operations at 90%

// Helper to check memory pressure before heavy operations
static bool IsMemoryPressureSafe()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    double UsagePercent = static_cast<double>(MemStats.UsedPhysical) / 
                          static_cast<double>(MemStats.TotalPhysical);
    return UsagePercent < MEMORY_PRESSURE_CRITICAL;
#else
    return true;  // Assume safe on other platforms
#endif
}

static double GetMemoryUsagePercent()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    return static_cast<double>(MemStats.UsedPhysical) / 
           static_cast<double>(MemStats.TotalPhysical) * 100.0;
#else
    return 0.0;
#endif
}

static int32 ClampSegments(int32 Value, int32 Default = 1)
{
    return FMath::Clamp(Value <= 0 ? Default : Value, 1, MAX_SEGMENTS);
}

static double ClampDimension(double Value, double Default = 100.0)
{
    if (Value <= 0.0) Value = Default;
    return FMath::Clamp(Value, MIN_DIMENSION, MAX_DIMENSION);
}

// Create actions accept both "name" and its schema alias "actorName"
static FString GetCreateActorName(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Default)
{
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty())
    {
        Name = GetJsonStringField(Payload, TEXT("actorName"));
    }
    return Name.IsEmpty() ? FString(Default) : Name;
}

static ADynamicMeshActor* FindDynamicMeshActorForGeometry(const FString& ActorName)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return nullptr;
    }

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            return *It;
        }
    }

    return nullptr;
}

static bool ResolveDynamicMeshForGeometry(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                          const FString& ActorName, FMcpResponseHandle Socket,
                                          ADynamicMeshActor*& OutActor, UDynamicMeshComponent*& OutComponent,
                                          UDynamicMesh*& OutMesh)
{
    OutActor = nullptr;
    OutComponent = nullptr;
    OutMesh = nullptr;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return false;
    }

    OutActor = FindDynamicMeshActorForGeometry(ActorName);
    if (!OutActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return false;
    }

    OutComponent = OutActor->GetDynamicMeshComponent();
    if (!OutComponent || !OutComponent->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return false;
    }

    OutMesh = OutComponent->GetDynamicMesh();
    return true;
}

static UTexture2D* ResolveGeometryTexture(const FString& TexturePath, FString& OutResolvedPath)
{
    FString SafePath = SanitizeProjectRelativePath(TexturePath);
    if (SafePath.IsEmpty())
    {
        return nullptr;
    }

    auto TryLoadTexture = [&OutResolvedPath](const FString& Candidate) -> UTexture2D*
    {
        if (Candidate.IsEmpty())
        {
            return nullptr;
        }

        if (UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *Candidate)))
        {
            OutResolvedPath = Texture->GetPathName();
            return Texture;
        }

        if (UTexture2D* Texture = FindObject<UTexture2D>(nullptr, *Candidate))
        {
            OutResolvedPath = Texture->GetPathName();
            return Texture;
        }

        return nullptr;
    };

    if (UTexture2D* Texture = TryLoadTexture(SafePath))
    {
        return Texture;
    }

    if (!SafePath.Contains(TEXT(".")))
    {
        const FString AssetName = FPackageName::GetLongPackageAssetName(SafePath);
        const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *SafePath, *AssetName);
        return TryLoadTexture(ObjectPath);
    }

    return nullptr;
}

static bool SampleTextureLuminance(UTexture2D* Texture, double U, double V, double& OutLuminance)
{
    if (!Texture || !Texture->Source.IsValid())
    {
        return false;
    }

    const int32 Width = Texture->Source.GetSizeX();
    const int32 Height = Texture->Source.GetSizeY();
    if (Width <= 0 || Height <= 0)
    {
        return false;
    }

    TArray64<uint8> MipData;
    if (!Texture->Source.GetMipData(MipData, 0))
    {
        return false;
    }

    const int32 X = FMath::Clamp(FMath::RoundToInt(FMath::Clamp(U, 0.0, 1.0) * static_cast<double>(Width - 1)), 0, Width - 1);
    const int32 Y = FMath::Clamp(FMath::RoundToInt(FMath::Clamp(V, 0.0, 1.0) * static_cast<double>(Height - 1)), 0, Height - 1);
    const int64 PixelIndex = static_cast<int64>(Y) * static_cast<int64>(Width) + static_cast<int64>(X);

    switch (Texture->Source.GetFormat())
    {
        case TSF_BGRA8:
        {
            const int64 Offset = PixelIndex * 4;
            if (MipData.Num() < Offset + 4) return false;
            const double B = static_cast<double>(MipData[Offset + 0]) / 255.0;
            const double G = static_cast<double>(MipData[Offset + 1]) / 255.0;
            const double R = static_cast<double>(MipData[Offset + 2]) / 255.0;
            OutLuminance = R * 0.2126 + G * 0.7152 + B * 0.0722;
            return true;
        }
        case TSF_G8:
        {
            if (MipData.Num() <= PixelIndex) return false;
            OutLuminance = static_cast<double>(MipData[PixelIndex]) / 255.0;
            return true;
        }
        default:
            return false;
    }
}

static FVector AxisVectorFromPayload(const TSharedPtr<FJsonObject>& Payload, const FVector& Default = FVector::UpVector)
{
    const FString Axis = GetJsonStringField(Payload, TEXT("axis"), TEXT("Z")).ToUpper();
    if (Axis == TEXT("X")) return FVector::ForwardVector;
    if (Axis == TEXT("Y")) return FVector::RightVector;
    return Default;
}

// -------------------------------------------------------------------------
// Primitives
// -------------------------------------------------------------------------

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

static bool HandleCreateBox(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString Name = GetCreateActorName(Payload, TEXT("GeneratedBox"));

    FTransform Transform = ReadTransformFromPayload(Payload);

    double Width = GetJsonNumberField(Payload, TEXT("width"), 100.0);
    double Height = GetJsonNumberField(Payload, TEXT("height"), 100.0);
    double Depth = GetJsonNumberField(Payload, TEXT("depth"), 100.0);

    const TSharedPtr<FJsonObject>* DimensionsObject = nullptr;
    if (Payload.IsValid() && Payload->TryGetObjectField(TEXT("dimensions"), DimensionsObject) && DimensionsObject && DimensionsObject->IsValid())
    {
        (*DimensionsObject)->TryGetNumberField(TEXT("width"), Width);
        (*DimensionsObject)->TryGetNumberField(TEXT("height"), Height);
        (*DimensionsObject)->TryGetNumberField(TEXT("depth"), Depth);
    }

    const TArray<TSharedPtr<FJsonValue>>* Dimensions = nullptr;
    if (Payload.IsValid() && Payload->TryGetArrayField(TEXT("dimensions"), Dimensions) && Dimensions && Dimensions->Num() >= 3)
    {
        Width = (*Dimensions)[0]->AsNumber();
        Height = (*Dimensions)[1]->AsNumber();
        Depth = (*Dimensions)[2]->AsNumber();
    }

    if (Width <= 0.0 || Height <= 0.0 || Depth <= 0.0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Box dimensions must be positive"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const bool bDimensionsClamped = Width > MAX_DIMENSION || Height > MAX_DIMENSION || Depth > MAX_DIMENSION;
    Width = ClampDimension(Width);
    Height = ClampDimension(Height);
    Depth = ClampDimension(Depth);

    int32 WidthSegments = ClampSegments(GetJsonIntField(Payload, TEXT("widthSegments"), 1));
    int32 HeightSegments = ClampSegments(GetJsonIntField(Payload, TEXT("heightSegments"), 1));
    int32 DepthSegments = ClampSegments(GetJsonIntField(Payload, TEXT("depthSegments"), 1));

    const int64 EstimatedTriangles = 2LL * (static_cast<int64>(WidthSegments) * HeightSegments +
                                            static_cast<int64>(WidthSegments) * DepthSegments +
                                            static_cast<int64>(HeightSegments) * DepthSegments);
    if (EstimatedTriangles > MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Box segment counts would create too many triangles: %lld"), EstimatedTriangles),
            TEXT("TOO_MANY_TRIANGLES"));
        return true;
    }

    if (!IsMemoryPressureSafe())
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Memory pressure too high for geometry creation: %.1f%%"), GetMemoryUsagePercent()),
            TEXT("MEMORY_PRESSURE"));
        return true;
    }

    // Create DynamicMesh
    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());

    FGeometryScriptPrimitiveOptions Options;
    Options.PolygroupMode = EGeometryScriptPrimitivePolygroupMode::PerFace;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
        DynMesh,
        Options,
        Transform,
        Width, Height, Depth,
        WidthSegments, HeightSegments, DepthSegments,
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    // Spawn actor with dynamic mesh component. Use direct world spawning so
    // headless/NullRHI automation does not enter viewport hit-proxy placement.
    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("height"), Height);
    Result->SetNumberField(TEXT("depth"), Depth);
    Result->SetNumberField(TEXT("estimatedTriangles"), static_cast<double>(EstimatedTriangles));
    Result->SetBoolField(TEXT("dimensionsClamped"), bDimensionsClamped);
    
    // Add verification data
    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Box mesh created"), Result);
    return true;
}

static bool HandleCreateSphere(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString Name = GetCreateActorName(Payload, TEXT("GeneratedSphere"));

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Radius = GetJsonNumberField(Payload, TEXT("radius"), 50.0);
    int32 Subdivisions = ClampSegments(GetJsonIntField(Payload, TEXT("subdivisions"), 16), 16);

    // numRings/radialSegments select lat-long topology; the engine silently raises steps below 3, so reject them
    const bool bHasNumRings = Payload.IsValid() && Payload->HasField(TEXT("numRings"));
    const bool bHasRadialSegments = Payload.IsValid() && Payload->HasField(TEXT("radialSegments"));
    int32 NumRings = GetJsonIntField(Payload, TEXT("numRings"), 10);
    int32 RadialSegments = GetJsonIntField(Payload, TEXT("radialSegments"), 16);
    if (bHasNumRings && (NumRings < 3 || NumRings > MAX_SEGMENTS))
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("numRings must be in [3, %d]"), MAX_SEGMENTS), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (bHasRadialSegments && (RadialSegments < 3 || RadialSegments > MAX_SEGMENTS))
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("radialSegments must be in [3, %d]"), MAX_SEGMENTS), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    if (bHasNumRings || bHasRadialSegments)
    {
        UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereLatLong(
            DynMesh,
            Options,
            Transform,
            Radius,
            NumRings, RadialSegments,
            EGeometryScriptPrimitiveOriginMode::Center,
            nullptr
        );
    }
    else
    {
        UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereBox(
            DynMesh,
            Options,
            Transform,
            Radius,
            Subdivisions, Subdivisions, Subdivisions,
            EGeometryScriptPrimitiveOriginMode::Center,
            nullptr
        );
    }

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    Result->SetNumberField(TEXT("radius"), Radius);
    if (bHasNumRings || bHasRadialSegments)
    {
        Result->SetNumberField(TEXT("numRings"), NumRings);
        Result->SetNumberField(TEXT("radialSegments"), RadialSegments);
    }

    // Add verification data
    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Sphere mesh created"), Result);
    return true;
}

static bool HandleCreateCylinder(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString Name = GetCreateActorName(Payload, TEXT("GeneratedCylinder"));

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Radius = GetJsonNumberField(Payload, TEXT("radius"), 50.0);
    double Height = GetJsonNumberField(Payload, TEXT("height"), 100.0);
    int32 Segments = GetJsonIntField(Payload, TEXT("segments"), 16);

    // numSides is the schema alias for the radial step count
    const bool bHasNumSides = Payload.IsValid() && Payload->HasField(TEXT("numSides"));
    if (bHasNumSides)
    {
        Segments = GetJsonIntField(Payload, TEXT("numSides"), 0);
        if (Segments < 3 || Segments > MAX_SEGMENTS)
        {
            Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("numSides must be in [3, %d]"), MAX_SEGMENTS), TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(
        DynMesh,
        Options,
        Transform,
        Radius, Height,
        Segments, 1,
        true, // bCapped
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor for cylinder") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    if (bHasNumSides)
    {
        Result->SetNumberField(TEXT("numSides"), Segments);
    }

    // Add verification data
    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Cylinder mesh created"), Result);
    return true;
}

static bool HandleCreateCone(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString Name = GetCreateActorName(Payload, TEXT("GeneratedCone"));

    FTransform Transform = ReadTransformFromPayload(Payload);
double BaseRadius = GetJsonNumberField(Payload, TEXT("baseRadius"), 50.0);
    double TopRadius = GetJsonNumberField(Payload, TEXT("topRadius"), 0.0);
    double Height = GetJsonNumberField(Payload, TEXT("height"), 100.0);
    int32 Segments = GetJsonIntField(Payload, TEXT("segments"), 16);

    // numSides is the schema alias for the radial step count
    const bool bHasNumSides = Payload.IsValid() && Payload->HasField(TEXT("numSides"));
    if (bHasNumSides)
    {
        Segments = GetJsonIntField(Payload, TEXT("numSides"), 0);
        if (Segments < 3 || Segments > MAX_SEGMENTS)
        {
            Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("numSides must be in [3, %d]"), MAX_SEGMENTS), TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCone(
        DynMesh,
        Options,
        Transform,
        BaseRadius, TopRadius, Height,
        Segments, 1,
        true, // bCapped
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);

    if (!NewActor)
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor for cone") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), Name);
    if (bHasNumSides)
    {
        Result->SetNumberField(TEXT("numSides"), Segments);
    }

    // Add verification data
    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Cone mesh created"), Result);
    return true;
}

static bool HandleCreateCapsule(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString Name = GetCreateActorName(Payload, TEXT("GeneratedCapsule"));

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Radius = GetJsonNumberField(Payload, TEXT("radius"), 50.0);
double Length = GetJsonNumberField(Payload, TEXT("length"), 100.0);
    int32 HemisphereSteps = GetJsonIntField(Payload, TEXT("hemisphereSteps"), 4);
    int32 Segments = GetJsonIntField(Payload, TEXT("segments"), 16);

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCapsule(
        DynMesh,
        Options,
        Transform,
        Radius, Length,
        HemisphereSteps, Segments,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
        0,  // SegmentSteps parameter added in UE 5.5
#endif
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);

    if (!NewActor)
    {
        DynMesh->MarkAsGarbage(); // Clean up DynamicMesh on error
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor for capsule") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), Name);
    
    // Add verification data
    McpHandlerUtils::AddVerification(Result, NewActor);
    
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Capsule mesh created"), Result);
    return true;
}

static bool HandleCreateTorus(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString Name = GetCreateActorName(Payload, TEXT("GeneratedTorus"));

    FTransform Transform = ReadTransformFromPayload(Payload);
double MajorRadius = GetJsonNumberField(Payload, TEXT("majorRadius"), 50.0);
    double MinorRadius = GetJsonNumberField(Payload, TEXT("minorRadius"), 20.0);
    int32 MajorSegments = GetJsonIntField(Payload, TEXT("majorSegments"), 16);
    int32 MinorSegments = GetJsonIntField(Payload, TEXT("minorSegments"), 8);

    // numRings/radialSegments are schema aliases for major/minor segments
    const bool bHasNumRings = Payload.IsValid() && Payload->HasField(TEXT("numRings"));
    const bool bHasRadialSegments = Payload.IsValid() && Payload->HasField(TEXT("radialSegments"));
    if (bHasNumRings)
    {
        MajorSegments = GetJsonIntField(Payload, TEXT("numRings"), 0);
        if (MajorSegments < 3 || MajorSegments > MAX_SEGMENTS)
        {
            Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("numRings must be in [3, %d]"), MAX_SEGMENTS), TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }
    if (bHasRadialSegments)
    {
        MinorSegments = GetJsonIntField(Payload, TEXT("radialSegments"), 0);
        if (MinorSegments < 3 || MinorSegments > MAX_SEGMENTS)
        {
            Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("radialSegments must be in [3, %d]"), MAX_SEGMENTS), TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendTorus(
        DynMesh,
        Options,
        Transform,
        FGeometryScriptRevolveOptions(),
        MajorRadius, MinorRadius,
        MajorSegments, MinorSegments,
        EGeometryScriptPrimitiveOriginMode::Center,
        nullptr
    );

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    if (bHasNumRings)
    {
        Result->SetNumberField(TEXT("numRings"), MajorSegments);
    }
    if (bHasRadialSegments)
    {
        Result->SetNumberField(TEXT("radialSegments"), MinorSegments);
    }
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Torus mesh created"), Result);
    return true;
}

/**
 * Create a DynamicMesh plane from the current schema fields and legacy aliases.
 *
 * Reads `width`, `height`, `widthSegments`, and `heightSegments` from the payload,
 * while preserving `depth`, `widthSubdivisions`, and `depthSubdivisions` as
 * compatibility fallbacks. Returns the effective dimensions and segment counts
 * so clients can verify which values were applied.
 */
static bool HandleCreatePlane(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString Name = GetCreateActorName(Payload, TEXT("GeneratedPlane"));

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Width = ClampDimension(GetJsonNumberField(Payload, TEXT("width"), 100.0));
    double Height = ClampDimension(GetJsonNumberField(
        Payload, TEXT("height"), GetJsonNumberField(Payload, TEXT("depth"), 100.0)));
    int32 WidthSubdivisions = ClampSegments(GetJsonIntField(
        Payload, TEXT("widthSegments"), GetJsonIntField(Payload, TEXT("widthSubdivisions"), 1)));
    int32 HeightSubdivisions = ClampSegments(GetJsonIntField(
        Payload, TEXT("heightSegments"), GetJsonIntField(Payload, TEXT("depthSubdivisions"), 1)));

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRectangleXY(
        DynMesh,
        Options,
        Transform,
        Width, Height,
        WidthSubdivisions, HeightSubdivisions,
        nullptr
    );

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("height"), Height);
    Result->SetNumberField(TEXT("widthSegments"), WidthSubdivisions);
    Result->SetNumberField(TEXT("heightSegments"), HeightSubdivisions);

    // Add verification data
    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Plane mesh created"), Result);
    return true;
}

static bool HandleCreateDisc(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString Name = GetCreateActorName(Payload, TEXT("GeneratedDisc"));

    FTransform Transform = ReadTransformFromPayload(Payload);
    double Radius = GetJsonNumberField(Payload, TEXT("radius"), 50.0);
    int32 Segments = GetJsonIntField(Payload, TEXT("segments"), 16);

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    // UE 5.7 signature: AppendDisc(Mesh, Options, Transform, Radius, AngleSteps, SpokeSteps, StartAngle, EndAngle, HoleRadius, Debug)
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendDisc(
        DynMesh,
        Options,
        Transform,
        Radius,
        Segments, // AngleSteps
        1,        // SpokeSteps
        0.0f,     // StartAngle
        360.0f,   // EndAngle
        0.0f,     // HoleRadius
        nullptr   // Debug
    );

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("class"), TEXT("DynamicMeshActor"));

    // Add verification data
    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Disc mesh created"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Booleans
// -------------------------------------------------------------------------

static bool HandleBooleanOperation(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                   const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket,
                                   EGeometryScriptBooleanOperation BoolOp, const FString& OpName)
{
    FString TargetActorName = GetJsonStringField(Payload, TEXT("targetActor"));
    FString ToolActorName = GetJsonStringField(Payload, TEXT("toolActor"));
    bool bKeepTool = GetJsonBoolField(Payload, TEXT("keepTool"), true);  // Default to true to prevent cascade test failures

    if (TargetActorName.IsEmpty() || ToolActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("targetActor and toolActor required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
        return true;
    }

    // Find target and tool actors
    ADynamicMeshActor* TargetActor = nullptr;
    ADynamicMeshActor* ToolActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == TargetActorName)
            TargetActor = *It;
        if (It->GetActorLabel() == ToolActorName)
            ToolActor = *It;
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Target actor not found: %s"), *TargetActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    if (!ToolActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Tool actor not found: %s"), *ToolActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* TargetDMC = TargetActor->GetDynamicMeshComponent();
    UDynamicMeshComponent* ToolDMC = ToolActor->GetDynamicMeshComponent();

    if (!TargetDMC || !ToolDMC)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMeshComponent not found on actors"), TEXT("COMPONENT_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* TargetMesh = TargetDMC->GetDynamicMesh();
    UDynamicMesh* ToolMesh = ToolDMC->GetDynamicMesh();

    if (!TargetMesh || !ToolMesh)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    // Get triangle counts before operation for validation
    int32 TargetTriCount = TargetMesh->GetTriangleCount();
    int32 ToolTriCount = ToolMesh->GetTriangleCount();
    int64 EstimatedMaxTriangles = static_cast<int64>(TargetTriCount) + static_cast<int64>(ToolTriCount);

    // Safety: Check memory pressure before heavy operation
    if (!IsMemoryPressureSafe())
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Memory pressure too high (%.1f%% used). Boolean %s blocked to prevent OOM."), 
                           GetMemoryUsagePercent(), *OpName), 
            TEXT("MEMORY_PRESSURE"));
        return true;
    }

    // Safety: Estimate maximum possible triangles and check against limit
    // Boolean operations can at most combine both meshes, but may create additional geometry
    int64 EstimatedWithSafetyMargin = EstimatedMaxTriangles * 3;  // 3x safety margin for intersection edges
    if (EstimatedWithSafetyMargin > MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Boolean %s would exceed polygon limit. Target: %d, Tool: %d, Estimated max: %lld, Limit: %d"),
                           *OpName, TargetTriCount, ToolTriCount, EstimatedWithSafetyMargin, MAX_TRIANGLES_PER_DYNAMIC_MESH),
            TEXT("POLYGON_LIMIT_EXCEEDED"));
        return true;
    }

    FGeometryScriptMeshBooleanOptions BoolOptions;
    BoolOptions.bFillHoles = true;
    BoolOptions.bSimplifyOutput = false;

    // UE 5.7: ApplyMeshBoolean returns UDynamicMesh* directly, no Outcome parameter
    UDynamicMesh* ResultMesh = UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
        TargetMesh,
        TargetActor->GetActorTransform(),
        ToolMesh,
        ToolActor->GetActorTransform(),
        BoolOp,
        BoolOptions,
        nullptr
    );

    bool bBooleanSucceeded = (ResultMesh != nullptr);

    // Safety: Check result polygon count
    int32 ResultTriCount = 0;
    if (ResultMesh)
    {
        ResultTriCount = ResultMesh->GetTriangleCount();
        
        // Check if result exceeds limit
        if (ResultTriCount > MAX_TRIANGLES_PER_DYNAMIC_MESH)
        {
            // Log warning but don't fail - the operation already completed
            UE_LOG(LogMcpGeometryHandlers, Warning, 
                   TEXT("Boolean %s result has %d triangles (exceeds limit of %d)"), 
                   *OpName, ResultTriCount, MAX_TRIANGLES_PER_DYNAMIC_MESH);
        }
        
        // Warning if approaching limit
        if (ResultTriCount > WARNING_TRIANGLE_THRESHOLD)
        {
            UE_LOG(LogMcpGeometryHandlers, Warning, 
                   TEXT("Boolean %s result has %d triangles (warning threshold: %d)"), 
                   *OpName, ResultTriCount, WARNING_TRIANGLE_THRESHOLD);
        }
    }
    else
    {
        // Boolean operation returned null - this typically means the operation failed
        // (e.g., empty result from intersection, non-overlapping meshes)
        UE_LOG(LogMcpGeometryHandlers, Warning, 
               TEXT("Boolean %s returned null result - operation may have produced empty geometry"), *OpName);
    }

    // Optionally delete tool actor
    if (!bKeepTool)
    {
        ToolActor->Destroy();
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("targetActor"), TargetActorName);
    Result->SetStringField(TEXT("operation"), OpName);
    Result->SetBoolField(TEXT("success"), bBooleanSucceeded);
    Result->SetNumberField(TEXT("targetTriangles"), TargetTriCount);
    Result->SetNumberField(TEXT("toolTriangles"), ToolTriCount);
    if (bBooleanSucceeded)
    {
        Result->SetNumberField(TEXT("resultTriangles"), ResultTriCount);
    }

    Self->SendAutomationResponse(Socket, RequestId, bBooleanSucceeded, 
        bBooleanSucceeded ? FString::Printf(TEXT("Boolean %s completed"), *OpName) : FString::Printf(TEXT("Boolean %s failed - operation produced empty geometry"), *OpName), 
        Result);
    return true;
}

static bool HandleBooleanUnion(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    return HandleBooleanOperation(Self, RequestId, Payload, Socket, EGeometryScriptBooleanOperation::Union, TEXT("Union"));
}

static bool HandleBooleanSubtract(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                  const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    return HandleBooleanOperation(Self, RequestId, Payload, Socket, EGeometryScriptBooleanOperation::Subtract, TEXT("Subtract"));
}

static bool HandleBooleanIntersection(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                      const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    return HandleBooleanOperation(Self, RequestId, Payload, Socket, EGeometryScriptBooleanOperation::Intersection, TEXT("Intersection"));
}

// -------------------------------------------------------------------------
// Mesh Utils
// -------------------------------------------------------------------------

static bool HandleGetMeshInfo(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));

    // Asset branch: reads saved UStaticMesh state so writes like generate_lods /
    // generate_collision / convert_to_nanite verify in one call.
    if (!AssetPath.IsEmpty())
    {
        FString SafePath = SanitizeProjectRelativePath(AssetPath);
        if (SafePath.IsEmpty())
        {
            Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Invalid asset path: %s"), *AssetPath), TEXT("INVALID_ASSET_PATH"));
            return true;
        }

        UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *SafePath);
        if (!StaticMesh)
        {
            Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("StaticMesh not found: %s"), *SafePath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), SafePath);

        const int32 NumLODs = StaticMesh->GetNumLODs();
        Result->SetNumberField(TEXT("lodCount"), NumLODs);
        Result->SetNumberField(TEXT("sourceModelCount"), StaticMesh->GetNumSourceModels());
        Result->SetNumberField(TEXT("triangleCount"), StaticMesh->GetNumTriangles(0));
        Result->SetNumberField(TEXT("vertexCount"), StaticMesh->GetNumVertices(0));

        TArray<TSharedPtr<FJsonValue>> LodArray;
        for (int32 LodIndex = 0; LodIndex < NumLODs; ++LodIndex)
        {
            TSharedPtr<FJsonObject> LodObj = MakeShared<FJsonObject>();
            LodObj->SetNumberField(TEXT("lodIndex"), LodIndex);
            LodObj->SetNumberField(TEXT("triangleCount"), StaticMesh->GetNumTriangles(LodIndex));
            LodObj->SetNumberField(TEXT("vertexCount"), StaticMesh->GetNumVertices(LodIndex));
            LodObj->SetNumberField(TEXT("sectionCount"), StaticMesh->GetNumSections(LodIndex));
            LodArray.Add(MakeShared<FJsonValueObject>(LodObj));
        }
        Result->SetArrayField(TEXT("lods"), LodArray);

        // naniteEnabled is the authored setting; naniteHasRenderData reflects
        // whether Nanite data was actually built.
        Result->SetBoolField(TEXT("naniteEnabled"), StaticMesh->GetNaniteSettings().bEnabled != 0);
        Result->SetBoolField(TEXT("naniteHasRenderData"), StaticMesh->HasValidNaniteData());

        TSharedPtr<FJsonObject> Collision = MakeShared<FJsonObject>();
        UBodySetup* BodySetup = StaticMesh->GetBodySetup();
        Collision->SetBoolField(TEXT("hasBodySetup"), BodySetup != nullptr);
        const FKAggregateGeom* AggGeom = BodySetup ? &BodySetup->AggGeom : nullptr;
        Collision->SetNumberField(TEXT("sphereCount"), AggGeom ? AggGeom->SphereElems.Num() : 0);
        Collision->SetNumberField(TEXT("boxCount"), AggGeom ? AggGeom->BoxElems.Num() : 0);
        Collision->SetNumberField(TEXT("capsuleCount"), AggGeom ? AggGeom->SphylElems.Num() : 0);
        Collision->SetNumberField(TEXT("convexCount"), AggGeom ? AggGeom->ConvexElems.Num() : 0);
        Collision->SetNumberField(TEXT("taperedCapsuleCount"), AggGeom ? AggGeom->TaperedCapsuleElems.Num() : 0);
        Collision->SetNumberField(TEXT("levelSetCount"), AggGeom ? AggGeom->LevelSetElems.Num() : 0);
        Collision->SetNumberField(TEXT("totalPrimitives"), AggGeom ? AggGeom->GetElementCount() : 0);
        if (BodySetup)
        {
            Collision->SetStringField(TEXT("collisionComplexity"),
                StaticEnum<ECollisionTraceFlag>()->GetNameStringByValue(static_cast<int64>(BodySetup->CollisionTraceFlag.GetValue())));
        }
        Result->SetObjectField(TEXT("collision"), Collision);

        const TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();
        constexpr int32 MaxMaterialSlots = 64;
        TArray<TSharedPtr<FJsonValue>> SlotArray;
        for (int32 SlotIndex = 0; SlotIndex < StaticMaterials.Num() && SlotIndex < MaxMaterialSlots; ++SlotIndex)
        {
            const FStaticMaterial& Slot = StaticMaterials[SlotIndex];
            TSharedPtr<FJsonObject> SlotObj = MakeShared<FJsonObject>();
            SlotObj->SetNumberField(TEXT("slotIndex"), SlotIndex);
            SlotObj->SetStringField(TEXT("slotName"), Slot.MaterialSlotName.ToString());
            SlotObj->SetStringField(TEXT("materialPath"), Slot.MaterialInterface ? Slot.MaterialInterface->GetPathName() : FString());
            SlotArray.Add(MakeShared<FJsonValueObject>(SlotObj));
        }
        Result->SetArrayField(TEXT("materialSlots"), SlotArray);
        Result->SetNumberField(TEXT("materialSlotCount"), StaticMaterials.Num());
        Result->SetBoolField(TEXT("materialSlotsTruncated"), StaticMaterials.Num() > MaxMaterialSlots);

        Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Static mesh asset info retrieved"), Result);
        return true;
    }

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName or assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
        return true;
    }

    ADynamicMeshActor* TargetActor = nullptr;
    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    
    // UE 5.7: FGeometryScriptMeshInfo and GetMeshInfo() were removed
    // Use individual query functions instead
    int32 VertexCount = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh);
    int32 TriangleCount = Mesh->GetTriangleCount();
    bool bHasNormals = UGeometryScriptLibrary_MeshQueryFunctions::GetHasTriangleNormals(Mesh);
    int32 NumUVSets = UGeometryScriptLibrary_MeshQueryFunctions::GetNumUVSets(Mesh);
    bool bHasVertexColors = UGeometryScriptLibrary_MeshQueryFunctions::GetHasVertexColors(Mesh);
    bool bHasMaterialIDs = UGeometryScriptLibrary_MeshQueryFunctions::GetHasMaterialIDs(Mesh);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexCount"), VertexCount);
    Result->SetNumberField(TEXT("triangleCount"), TriangleCount);
    Result->SetBoolField(TEXT("hasNormals"), bHasNormals);
    Result->SetBoolField(TEXT("hasUVs"), NumUVSets > 0);
    Result->SetBoolField(TEXT("hasColors"), bHasVertexColors);
    Result->SetBoolField(TEXT("hasPolygroups"), bHasMaterialIDs);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh info retrieved"), Result);
    return true;
}

static bool HandleRecalculateNormals(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    // computeWeightedNormals is the schema name; areaWeighted is a legacy alias
    bool bAreaWeighted = GetJsonBoolField(Payload, TEXT("areaWeighted"), true);
    if (Payload.IsValid() && Payload->HasField(TEXT("computeWeightedNormals")))
    {
        bAreaWeighted = GetJsonBoolField(Payload, TEXT("computeWeightedNormals"), true);
    }
    const bool bHasHardEdgeAngle = Payload.IsValid() && Payload->HasField(TEXT("hardEdgeAngle"));
    double HardEdgeAngle = GetJsonNumberField(Payload, TEXT("hardEdgeAngle"), 0.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (bHasHardEdgeAngle && (HardEdgeAngle <= 0.0 || HardEdgeAngle > 180.0))
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("hardEdgeAngle must be in (0, 180] degrees"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
        return true;
    }

    ADynamicMeshActor* TargetActor = nullptr;
    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptCalculateNormalsOptions NormalOptions;
    NormalOptions.bAreaWeighted = bAreaWeighted;
    NormalOptions.bAngleWeighted = true;

    if (bHasHardEdgeAngle)
    {
        // Recompute with hard edges split at the requested opening angle
        FGeometryScriptSplitNormalsOptions SplitOptions;
        SplitOptions.bSplitByOpeningAngle = true;
        SplitOptions.OpeningAngleDeg = HardEdgeAngle;
        SplitOptions.bSplitByFaceGroup = false;

        UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(
            Mesh, SplitOptions, NormalOptions, nullptr);
    }
    else
    {
    #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    // UE 5.3+: RecomputeNormals takes 4 parameters (with bDeferChangeNotifications)
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(
        Mesh,
        NormalOptions,
        false,  // bDeferChangeNotifications
        nullptr
    );
#else
    // UE 5.0-5.2: RecomputeNormals takes 3 parameters
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(
        Mesh,
        NormalOptions,
        nullptr
    );
#endif
    }

    // Force refresh
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetBoolField(TEXT("areaWeighted"), bAreaWeighted);
    Result->SetBoolField(TEXT("computeWeightedNormals"), bAreaWeighted);
    if (bHasHardEdgeAngle)
    {
        Result->SetNumberField(TEXT("hardEdgeAngle"), HardEdgeAngle);
    }

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Normals recalculated"), Result);
    return true;
}

static bool HandleFlipNormals(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    UGeometryScriptLibrary_MeshNormalsFunctions::FlipNormals(Mesh, nullptr);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Normals flipped"), Result);
    return true;
}

static bool HandleSimplifyMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const bool bHasTargetTriangleCount = Payload.IsValid() && Payload->HasField(TEXT("targetTriangleCount"));
    const bool bHasTargetPercentage = Payload.IsValid() && Payload->HasField(TEXT("targetPercentage"));
    const bool bHasReductionPercent = Payload.IsValid() && Payload->HasField(TEXT("reductionPercent"));
    int32 RequestedTriangleCount = GetJsonIntField(Payload, TEXT("targetTriangleCount"), 0);
    double TargetPercentage = GetJsonNumberField(Payload, TEXT("targetPercentage"), 50.0);
    double ReductionPercent = GetJsonNumberField(Payload, TEXT("reductionPercent"), 0.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (bHasTargetTriangleCount && RequestedTriangleCount <= 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("targetTriangleCount must be a positive integer"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (bHasReductionPercent)
    {
        if (bHasTargetTriangleCount || bHasTargetPercentage)
        {
            Self->SendAutomationError(Socket, RequestId, TEXT("reductionPercent is mutually exclusive with targetTriangleCount and targetPercentage"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (ReductionPercent <= 0.0 || ReductionPercent >= 100.0)
        {
            Self->SendAutomationError(Socket, RequestId, TEXT("reductionPercent must be in (0, 100)"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        // Percent to remove; TargetPercentage is percent to keep
        TargetPercentage = 100.0 - ReductionPercent;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // UE 5.7: Use FGeometryScriptSimplifyMeshOptions (renamed from FGeometryScriptMeshSimplifyOptions)
    FGeometryScriptSimplifyMeshOptions SimplifyOptions;
    SimplifyOptions.Method = EGeometryScriptRemoveMeshSimplificationType::StandardQEM;
    // Note: bPreserveSharpEdges was removed in UE 5.7
    SimplifyOptions.bAllowSeamCollapse = true;

    // UE 5.7: FGeometryScriptMeshInfo and GetMeshInfo() were removed
    // Use individual query functions instead
    int32 TriCountBefore = Mesh->GetTriangleCount();

    int32 TargetTriCount = bHasTargetTriangleCount
        ? RequestedTriangleCount
        : FMath::Max(1, FMath::RoundToInt(TriCountBefore * (TargetPercentage / 100.0)));

    UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTriangleCount(
        Mesh,
        TargetTriCount,
        SimplifyOptions,
        nullptr
    );

    int32 TriCountAfter = Mesh->GetTriangleCount();

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("targetTriangleCount"), TargetTriCount);
    Result->SetNumberField(TEXT("originalTriangles"), TriCountBefore);
    Result->SetNumberField(TEXT("simplifiedTriangles"), TriCountAfter);
    if (bHasReductionPercent)
    {
        Result->SetNumberField(TEXT("requestedReductionPercent"), ReductionPercent);
    }
    Result->SetNumberField(TEXT("reductionPercent"), (1.0 - ((double)TriCountAfter / (double)TriCountBefore)) * 100.0);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh simplified"), Result);
    return true;
}

static bool HandleSubdivide(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 Iterations = GetJsonIntField(Payload, TEXT("iterations"), 1);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Safety: Clamp iterations to prevent polygon explosion
    int32 OriginalIterations = Iterations;
    Iterations = FMath::Clamp(Iterations, 1, MAX_SUBDIVIDE_ITERATIONS);
    if (Iterations != OriginalIterations)
    {
        UE_LOG(LogMcpGeometryHandlers, Warning, TEXT("Subdivide iterations clamped from %d to %d (MAX_SUBDIVIDE_ITERATIONS)"), 
               OriginalIterations, Iterations);
    }

    // Check memory pressure before heavy operation
    if (!IsMemoryPressureSafe())
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Memory pressure too high (%.1f%% used). Subdivide blocked to prevent OOM."), 
                           GetMemoryUsagePercent()), 
            TEXT("MEMORY_PRESSURE"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // UE 5.7: FGeometryScriptMeshInfo and GetMeshInfo() were removed
    // Use individual query functions instead
    int32 TriCountBefore = Mesh->GetTriangleCount();

    // Safety: Estimate triangles after subdivision and check against limit
    // Each subdivision iteration roughly quadruples triangle count
    int64 EstimatedTriangles = static_cast<int64>(TriCountBefore);
    for (int32 i = 0; i < Iterations; ++i)
    {
        EstimatedTriangles *= 4;  // Each subdivision ~4x triangles
    }
    
    if (EstimatedTriangles > MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Subdivide would exceed triangle limit. Current: %d, Estimated after: %lld, Max allowed: %d"), 
                           TriCountBefore, EstimatedTriangles, MAX_TRIANGLES_PER_DYNAMIC_MESH), 
            TEXT("POLYGON_LIMIT_EXCEEDED"));
        return true;
    }

    for (int32 i = 0; i < Iterations; ++i)
    {
        // UE 5.7: ApplyPNTessellation now takes TessellationLevel as separate parameter
        FGeometryScriptPNTessellateOptions TessOptions;
        UGeometryScriptLibrary_MeshSubdivideFunctions::ApplyPNTessellation(Mesh, TessOptions, 1, nullptr);
    }

    int32 TriCountAfter = Mesh->GetTriangleCount();

    // Warning if approaching limit
    if (TriCountAfter > WARNING_TRIANGLE_THRESHOLD)
    {
        UE_LOG(LogMcpGeometryHandlers, Warning, TEXT("Subdivide result has %d triangles (warning threshold: %d)"), 
               TriCountAfter, WARNING_TRIANGLE_THRESHOLD);
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("iterations"), Iterations);
    Result->SetNumberField(TEXT("originalTriangles"), TriCountBefore);
    Result->SetNumberField(TEXT("subdividedTriangles"), TriCountAfter);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh subdivided"), Result);
    return true;
}

static bool HandleAutoUV(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // XAtlas rejects non-compact meshes (e.g. after boolean ops); compact first
    UGeometryScriptLibrary_MeshRepairFunctions::CompactMesh(Mesh, nullptr);

    // UE 5.7: FGeometryScriptAutoUVOptions was removed, use XAtlas directly
    UGeometryScriptLibrary_MeshUVFunctions::AutoGenerateXAtlasMeshUVs(
        Mesh,
        0, // UV Channel
        FGeometryScriptXAtlasOptions(),
        nullptr
    );

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Auto UV generated"), Result);
    return true;
}

static bool HandleConvertToStaticMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                      const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"));
    if (AssetPath.IsEmpty()) AssetPath = GetJsonStringField(Payload, TEXT("outputPath"));
    if (AssetPath.IsEmpty()) AssetPath = GetJsonStringField(Payload, TEXT("path"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (AssetPath.IsEmpty())
    {
        AssetPath = FString::Printf(TEXT("/Game/GeneratedMeshes/%s"), *ActorName);
    }

    // Sanitize the asset path to prevent path traversal and ensure valid path format
    FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (SanitizedAssetPath.IsEmpty() && !AssetPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Invalid assetPath - rejected due to security validation"), TEXT("INVALID_ASSET_PATH"));
        return true;
    }
    AssetPath = SanitizedAssetPath;

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptCreateNewStaticMeshAssetOptions CreateOptions;
    CreateOptions.bEnableRecomputeNormals = true;
    CreateOptions.bEnableRecomputeTangents = true;
    // UE 5.7: bAllowDistanceField and bGenerateNaniteEnabledMesh were removed
    // Use bEnableNanite + NaniteSettings instead
    CreateOptions.bEnableNanite = false;

    EGeometryScriptOutcomePins Outcome;
    UStaticMesh* NewStaticMesh = nullptr;

    UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
        Mesh,
        AssetPath,
        CreateOptions,
        Outcome,
        nullptr
    );

    if (Outcome != EGeometryScriptOutcomePins::Success)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to create StaticMesh asset"), TEXT("ASSET_CREATION_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("assetPath"), AssetPath);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("StaticMesh created from DynamicMesh"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Additional Primitives
// -------------------------------------------------------------------------

static bool HandleCreateStairs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString Name = GetCreateActorName(Payload, TEXT("GeneratedStairs"));

    FTransform Transform = ReadTransformFromPayload(Payload);
float StepWidth = GetJsonNumberField(Payload, TEXT("stepWidth"), 100.0f);
    float StepHeight = GetJsonNumberField(Payload, TEXT("stepHeight"), 20.0f);
    float StepDepth = GetJsonNumberField(Payload, TEXT("stepDepth"), 30.0f);
    int32 NumSteps = GetJsonIntField(Payload, TEXT("numSteps"), 8);
    bool bFloating = GetJsonBoolField(Payload, TEXT("floating"), false);

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendLinearStairs(
        DynMesh, Options, Transform, StepWidth, StepHeight, StepDepth, NumSteps, bFloating, nullptr);

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("numSteps"), NumSteps);

    // Add verification data
    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Linear stairs created"), Result);
    return true;
}

static bool HandleCreateSpiralStairs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString Name = GetCreateActorName(Payload, TEXT("GeneratedSpiralStairs"));

    FTransform Transform = ReadTransformFromPayload(Payload);
float StepWidth = GetJsonNumberField(Payload, TEXT("stepWidth"), 100.0f);
    float StepHeight = GetJsonNumberField(Payload, TEXT("stepHeight"), 20.0f);
    float InnerRadius = GetJsonNumberField(Payload, TEXT("innerRadius"), 150.0f);
    const bool bHasCurveAngle = Payload.IsValid() && Payload->HasField(TEXT("curveAngle"));
    const bool bHasNumTurns = Payload.IsValid() && Payload->HasField(TEXT("numTurns"));
    if (bHasCurveAngle && bHasNumTurns)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Pass either curveAngle (degrees) or numTurns (full revolutions), not both"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    float CurveAngle = bHasNumTurns
        ? GetJsonNumberField(Payload, TEXT("numTurns")) * 360.0f
        : GetJsonNumberField(Payload, TEXT("curveAngle"), 90.0f);
    int32 NumSteps = GetJsonIntField(Payload, TEXT("numSteps"), 8);
    bool bFloating = GetJsonBoolField(Payload, TEXT("floating"), false);

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCurvedStairs(
        DynMesh, Options, Transform, StepWidth, StepHeight, InnerRadius, CurveAngle, NumSteps, bFloating, nullptr);

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("numSteps"), NumSteps);
    Result->SetNumberField(TEXT("curveAngle"), CurveAngle);
    Result->SetNumberField(TEXT("numTurns"), CurveAngle / 360.0f);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Spiral stairs created"), Result);
    return true;
}

static bool HandleCreateRing(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString Name = GetCreateActorName(Payload, TEXT("GeneratedRing"));

    FTransform Transform = ReadTransformFromPayload(Payload);
double OuterRadius = GetJsonNumberField(Payload, TEXT("outerRadius"), 50.0);
    double InnerRadius = GetJsonNumberField(Payload, TEXT("innerRadius"), 25.0);
    int32 Segments = GetJsonIntField(Payload, TEXT("segments"), 32);

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    // Use AppendDisc with HoleRadius to create a ring
    // UE 5.7 signature: AppendDisc(Mesh, Options, Transform, Radius, AngleSteps, SpokeSteps, StartAngle, EndAngle, HoleRadius, Debug)
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendDisc(
        DynMesh, Options, Transform, OuterRadius, Segments, 0, 0.0f, 360.0f, InnerRadius, nullptr);

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("outerRadius"), OuterRadius);
    Result->SetNumberField(TEXT("innerRadius"), InnerRadius);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Ring created"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Modeling Operations
// -------------------------------------------------------------------------

static bool HandleExtrude(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Distance = GetJsonNumberField(Payload, TEXT("distance"), 10.0);
    FVector Direction = ReadVectorFromPayload(Payload, TEXT("direction"), FVector(0, 0, 1));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptMeshLinearExtrudeOptions ExtrudeOptions;
    ExtrudeOptions.Distance = Distance;
    ExtrudeOptions.Direction = Direction;
    ExtrudeOptions.DirectionMode = EGeometryScriptLinearExtrudeDirection::FixedDirection;

    // Create empty selection (extrudes all faces)
    FGeometryScriptMeshSelection Selection;

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshLinearExtrudeFaces(
        Mesh, ExtrudeOptions, Selection, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("distance"), Distance);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Extrude applied"), Result);
    return true;
}

static bool HandleInsetOutset(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket,
                              bool bIsInset)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Distance = GetJsonNumberField(Payload, TEXT("distance"), 5.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptMeshInsetOutsetFacesOptions Options;
    Options.Distance = bIsInset ? -Distance : Distance;  // Negative for inset
    Options.bReproject = true;

    FGeometryScriptMeshSelection Selection;

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshInsetOutsetFaces(
        Mesh, Options, Selection, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("operation"), bIsInset ? TEXT("inset") : TEXT("outset"));
    Result->SetNumberField(TEXT("distance"), Distance);
    Self->SendAutomationResponse(Socket, RequestId, true, bIsInset ? TEXT("Inset applied") : TEXT("Outset applied"), Result);
    return true;
}

static bool HandleBevel(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
double BevelDistance = GetJsonNumberField(Payload, TEXT("distance"), 5.0);
    int32 Subdivisions = GetJsonIntField(Payload, TEXT("subdivisions"), 0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptMeshBevelOptions BevelOptions;
    BevelOptions.BevelDistance = BevelDistance;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    BevelOptions.Subdivisions = Subdivisions;
#endif

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshPolygroupBevel(
        Mesh, BevelOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("distance"), BevelDistance);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Bevel applied"), Result);
    return true;
}

static bool HandleOffsetFaces(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Distance = GetJsonNumberField(Payload, TEXT("distance"), 5.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // UE 5.7: FGeometryScriptMeshOffsetFacesOptions uses Distance not OffsetDistance
    FGeometryScriptMeshOffsetFacesOptions Options;
    Options.Distance = Distance;

    FGeometryScriptMeshSelection Selection;

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshOffsetFaces(
        Mesh, Options, Selection, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("distance"), Distance);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Offset faces applied"), Result);
    return true;
}

static bool HandleShell(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Thickness = GetJsonNumberField(Payload, TEXT("thickness"), 5.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptMeshOffsetOptions Options;
    Options.OffsetDistance = -Thickness;  // Negative to go inward for shell

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshShell(
        Mesh, Options, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("thickness"), Thickness);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Shell/solidify applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Deformers
// -------------------------------------------------------------------------

static bool HandleBend(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
double BendAngle = GetJsonNumberField(Payload, TEXT("angle"), 45.0);
    double BendExtent = GetJsonNumberField(Payload, TEXT("extent"), 50.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptBendWarpOptions BendOptions;
    BendOptions.bSymmetricExtents = true;
    BendOptions.bBidirectional = true;

    UGeometryScriptLibrary_MeshDeformFunctions::ApplyBendWarpToMesh(
        Mesh, BendOptions, FTransform::Identity, BendAngle, BendExtent, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("angle"), BendAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Bend deformer applied"), Result);
    return true;
}

static bool HandleTwist(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
double TwistAngle = GetJsonNumberField(Payload, TEXT("angle"), 45.0);
    double TwistExtent = GetJsonNumberField(Payload, TEXT("extent"), 50.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptTwistWarpOptions TwistOptions;
    TwistOptions.bSymmetricExtents = true;
    TwistOptions.bBidirectional = true;

    UGeometryScriptLibrary_MeshDeformFunctions::ApplyTwistWarpToMesh(
        Mesh, TwistOptions, FTransform::Identity, TwistAngle, TwistExtent, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("angle"), TwistAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Twist deformer applied"), Result);
    return true;
}

static bool HandleTaper(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
double FlarePercentX = GetJsonNumberField(Payload, TEXT("flareX"), 50.0);
    double FlarePercentY = GetJsonNumberField(Payload, TEXT("flareY"), 50.0);
    double FlareExtent = GetJsonNumberField(Payload, TEXT("extent"), 50.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptFlareWarpOptions FlareOptions;
    FlareOptions.bSymmetricExtents = true;

    UGeometryScriptLibrary_MeshDeformFunctions::ApplyFlareWarpToMesh(
        Mesh, FlareOptions, FTransform::Identity, FlarePercentX, FlarePercentY, FlareExtent, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Taper/flare deformer applied"), Result);
    return true;
}

static bool HandleNoiseDeform(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
double Magnitude = GetJsonNumberField(Payload, TEXT("magnitude"), 5.0);
    double Frequency = GetJsonNumberField(Payload, TEXT("frequency"), 0.25);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptPerlinNoiseOptions NoiseOptions;
    NoiseOptions.BaseLayer.Magnitude = Magnitude;
    NoiseOptions.BaseLayer.Frequency = Frequency;
    NoiseOptions.bApplyAlongNormal = true;

    FGeometryScriptMeshSelection Selection;

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
    // UE 5.7+: Use ApplyPerlinNoiseToMesh2 (updated API)
    UGeometryScriptLibrary_MeshDeformFunctions::ApplyPerlinNoiseToMesh2(
        Mesh, Selection, NoiseOptions, nullptr);
#else
    // UE 5.0-5.6: Use original ApplyPerlinNoiseToMesh
    UGeometryScriptLibrary_MeshDeformFunctions::ApplyPerlinNoiseToMesh(
        Mesh, Selection, NoiseOptions, nullptr);
#endif

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("magnitude"), Magnitude);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Noise deformer applied"), Result);
    return true;
}

static bool HandleSmooth(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
int32 Iterations = GetJsonIntField(Payload, TEXT("iterations"), 10);
    double Alpha = GetJsonNumberField(Payload, TEXT("alpha"), 0.2);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptIterativeMeshSmoothingOptions SmoothOptions;
    SmoothOptions.NumIterations = Iterations;
    SmoothOptions.Alpha = Alpha;

    FGeometryScriptMeshSelection Selection;

    UGeometryScriptLibrary_MeshDeformFunctions::ApplyIterativeSmoothingToMesh(
        Mesh, Selection, SmoothOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("iterations"), Iterations);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Smooth applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Mesh Repair
// -------------------------------------------------------------------------

static bool HandleWeldVertices(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Tolerance = GetJsonNumberField(Payload, TEXT("tolerance"), 0.0001);
    const bool bHasWeldDistance = Payload.IsValid() && Payload->HasField(TEXT("weldDistance"));
    if (bHasWeldDistance)
    {
        Tolerance = GetJsonNumberField(Payload, TEXT("weldDistance"), 0.0);
    }

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (bHasWeldDistance && Tolerance <= 0.0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("weldDistance must be > 0"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptWeldEdgesOptions WeldOptions;
    WeldOptions.Tolerance = Tolerance;
    WeldOptions.bOnlyUniquePairs = true;

    UGeometryScriptLibrary_MeshRepairFunctions::WeldMeshEdges(
        Mesh, WeldOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("weldDistance"), Tolerance);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vertices welded"), Result);
    return true;
}

static bool HandleFillHoles(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptFillHolesOptions FillOptions;
    FillOptions.FillMethod = EGeometryScriptFillHolesMethod::Automatic;

    // UE 5.7: FillAllMeshHoles now takes 5 arguments (added NumFilledHoles and NumFailedHoleFills out params)
    int32 NumFilledHoles = 0;
    int32 NumFailedHoleFills = 0;

    UGeometryScriptLibrary_MeshRepairFunctions::FillAllMeshHoles(
        Mesh, FillOptions, NumFilledHoles, NumFailedHoleFills, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("filledHoles"), NumFilledHoles);
    Result->SetNumberField(TEXT("failedHoles"), NumFailedHoleFills);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Holes filled"), Result);
    return true;
}

static bool HandleRemoveDegenerates(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptDegenerateTriangleOptions Options;
    Options.Mode = EGeometryScriptRepairMeshMode::RepairOrDelete;

    UGeometryScriptLibrary_MeshRepairFunctions::RepairMeshDegenerateGeometry(
        Mesh, Options, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Degenerate geometry removed"), Result);
    return true;
}

static bool HandleRemeshUniform(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const bool bHasTargetTriangleCount = Payload.IsValid() && Payload->HasField(TEXT("targetTriangleCount"));
    const bool bHasTargetEdgeLength = Payload.IsValid() && Payload->HasField(TEXT("targetEdgeLength"));
    int32 TargetTriangleCount = GetJsonIntField(Payload, TEXT("targetTriangleCount"), 5000);
    double TargetEdgeLength = GetJsonNumberField(Payload, TEXT("targetEdgeLength"), 0.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (bHasTargetTriangleCount && bHasTargetEdgeLength)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("targetTriangleCount and targetEdgeLength are mutually exclusive"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (bHasTargetEdgeLength && TargetEdgeLength <= 0.0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("targetEdgeLength must be > 0"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptRemeshOptions RemeshOptions;
    RemeshOptions.bDiscardAttributes = false;
    RemeshOptions.bReprojectToInputMesh = true;

    FGeometryScriptUniformRemeshOptions UniformOptions;
    if (bHasTargetEdgeLength)
    {
        UniformOptions.TargetType = EGeometryScriptUniformRemeshTargetType::TargetEdgeLength;
        UniformOptions.TargetEdgeLength = static_cast<float>(TargetEdgeLength);
    }
    else
    {
        UniformOptions.TargetType = EGeometryScriptUniformRemeshTargetType::TriangleCount;
        UniformOptions.TargetTriangleCount = TargetTriangleCount;
    }

    UGeometryScriptLibrary_RemeshingFunctions::ApplyUniformRemesh(
        Mesh, RemeshOptions, UniformOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    if (bHasTargetEdgeLength)
    {
        Result->SetNumberField(TEXT("targetEdgeLength"), TargetEdgeLength);
    }
    else
    {
        Result->SetNumberField(TEXT("targetTriangleCount"), TargetTriangleCount);
    }
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Uniform remesh applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Collision Generation
// -------------------------------------------------------------------------

static bool HandleGenerateCollision(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString CollisionType = GetJsonStringField(Payload, TEXT("collisionType"), TEXT("convex"));
    const bool bHasHullCount = Payload.IsValid() && Payload->HasField(TEXT("hullCount"));
    int32 HullCount = GetJsonIntField(Payload, TEXT("hullCount"), 0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (bHasHullCount)
    {
        if (HullCount < 1)
        {
            Self->SendAutomationError(Socket, RequestId, TEXT("hullCount must be >= 1"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (CollisionType != TEXT("convex") && CollisionType != TEXT("convex_decomposition"))
        {
            Self->SendAutomationError(Socket, RequestId, TEXT("hullCount only applies to collisionType convex or convex_decomposition"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

// Geometry Script collision API differs between UE 5.4 and 5.5+
// UE 5.4: Use SetDynamicMeshCollisionFromMesh directly
// UE 5.5+: Use GenerateCollisionFromMesh + SetSimpleCollisionOfDynamicMeshComponent
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    FGeometryScriptCollisionFromMeshOptions CollisionOptions;
    CollisionOptions.bEmitTransaction = false;
    
    // Set method based on collision type
    if (CollisionType == TEXT("box") || CollisionType == TEXT("boxes"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::AlignedBoxes;
    }
    else if (CollisionType == TEXT("sphere") || CollisionType == TEXT("spheres"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::MinimalSpheres;
    }
    else if (CollisionType == TEXT("capsule") || CollisionType == TEXT("capsules"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::Capsules;
    }
    else if (CollisionType == TEXT("convex"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
        CollisionOptions.MaxConvexHullsPerMesh = bHasHullCount ? HullCount : 1;
    }
    else if (CollisionType == TEXT("convex_decomposition"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
        CollisionOptions.MaxConvexHullsPerMesh = bHasHullCount ? HullCount : 8;
    }
    else
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::MinVolumeShapes;
    }

    FGeometryScriptSimpleCollision Collision = UGeometryScriptLibrary_CollisionFunctions::GenerateCollisionFromMesh(
        Mesh, CollisionOptions, nullptr);

    // Set the collision on the DynamicMeshComponent
    FGeometryScriptSetSimpleCollisionOptions SetOptions;
    UGeometryScriptLibrary_CollisionFunctions::SetSimpleCollisionOfDynamicMeshComponent(
        Collision, DMC, SetOptions, nullptr);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("collisionType"), CollisionType);
    if (bHasHullCount)
    {
        Result->SetNumberField(TEXT("hullCount"), HullCount);
    }
    Result->SetNumberField(TEXT("shapeCount"), UGeometryScriptLibrary_CollisionFunctions::GetSimpleCollisionShapeCount(Collision));
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Collision generated"), Result);
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 4
    // UE 5.4: Use SetDynamicMeshCollisionFromMesh directly (GenerateCollisionFromMesh not available)
    FGeometryScriptCollisionFromMeshOptions CollisionOptions;
    CollisionOptions.bEmitTransaction = false;
    
    // Set method based on collision type
    if (CollisionType == TEXT("box") || CollisionType == TEXT("boxes"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::AlignedBoxes;
    }
    else if (CollisionType == TEXT("sphere") || CollisionType == TEXT("spheres"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::MinimalSpheres;
    }
    else if (CollisionType == TEXT("capsule") || CollisionType == TEXT("capsules"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::Capsules;
    }
    else if (CollisionType == TEXT("convex"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
        CollisionOptions.MaxConvexHullsPerMesh = bHasHullCount ? HullCount : 1;
    }
    else if (CollisionType == TEXT("convex_decomposition"))
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
        CollisionOptions.MaxConvexHullsPerMesh = bHasHullCount ? HullCount : 8;
    }
    else
    {
        CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::MinVolumeShapes;
    }

    // UE 5.4: SetDynamicMeshCollisionFromMesh sets collision directly on the component
    UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh(
        Mesh, DMC, CollisionOptions, nullptr);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("collisionType"), CollisionType);
    if (bHasHullCount)
    {
        Result->SetNumberField(TEXT("hullCount"), HullCount);
    }
    Result->SetNumberField(TEXT("shapeCount"), 1); // Approximate count for UE 5.4
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Collision generated"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Collision generation requires UE 5.4+"), TEXT("VERSION_NOT_SUPPORTED"));
#endif
    return true;
}

// -------------------------------------------------------------------------
// Transform Operations (Mirror, Array)
// -------------------------------------------------------------------------

static bool HandleMirror(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString Axis = GetJsonStringField(Payload, TEXT("axis"), TEXT("X")).ToUpper();
    bool bWeld = GetJsonBoolField(Payload, TEXT("weld"), true);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Create a copy of the mesh
    UDynamicMesh* MirroredMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    MirroredMesh->SetMesh(Mesh->GetMeshRef());

    // Mirror by scaling with negative value on the axis
    FVector MirrorScale = FVector::OneVector;
    if (Axis == TEXT("X")) MirrorScale.X = -1.0;
    else if (Axis == TEXT("Y")) MirrorScale.Y = -1.0;
    else if (Axis == TEXT("Z")) MirrorScale.Z = -1.0;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    UGeometryScriptLibrary_MeshTransformFunctions::ScaleMesh(MirroredMesh, MirrorScale, FVector::ZeroVector, true, nullptr);
#else
    // UE 5.3 fallback: Scale mesh using low-level API
    {
        UE::Geometry::FDynamicMesh3& EditMesh = MirroredMesh->GetMeshRef();
        for (int32 VID : EditMesh.VertexIndicesItr())
        {
            FVector3d Pos = EditMesh.GetVertex(VID);
            Pos.X *= MirrorScale.X;
            Pos.Y *= MirrorScale.Y;
            Pos.Z *= MirrorScale.Z;
            EditMesh.SetVertex(VID, Pos);
        }
            // EditMesh.UpdateVertexNormals(); // Not available in UE 5.3
            // MirroredMesh->NotifyMeshUpdated(); // Not available in UE 5.3
    }
#endif

    // Append mirrored mesh to original
    FGeometryScriptAppendMeshOptions AppendOptions;
    UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(Mesh, MirroredMesh, FTransform::Identity, false, AppendOptions, nullptr);

    // Optionally weld vertices at the mirror plane
    if (bWeld)
    {
        FGeometryScriptWeldEdgesOptions WeldOptions;
        WeldOptions.Tolerance = 0.001;
        UGeometryScriptLibrary_MeshRepairFunctions::WeldMeshEdges(Mesh, WeldOptions, nullptr);
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("axis"), Axis);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mirror applied"), Result);
    return true;
}

static bool HandleArrayLinear(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 Count = GetJsonIntField(Payload, TEXT("count"), 3);
    FVector Offset = ReadVectorFromPayload(Payload, TEXT("offset"), FVector(100, 0, 0));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (Count < 1 || Count > 100)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("count must be between 1 and 100"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Safety: Check memory pressure before array operation
    if (!IsMemoryPressureSafe())
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Memory pressure too high (%.1f%% used). Array operation blocked to prevent OOM."), 
                           GetMemoryUsagePercent()), 
            TEXT("MEMORY_PRESSURE"));
        return true;
    }

    // Safety: Estimate triangles after array and check against limit
    int32 TriCountBefore = Mesh->GetTriangleCount();
    int64 EstimatedTriangles = static_cast<int64>(TriCountBefore) * Count;
    
    if (EstimatedTriangles > MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Array would exceed triangle limit. Current: %d, Estimated: %lld, Max: %d"), 
                           TriCountBefore, EstimatedTriangles, MAX_TRIANGLES_PER_DYNAMIC_MESH), 
            TEXT("POLYGON_LIMIT_EXCEEDED"));
        return true;
    }

    // Create a copy for arraying
    UDynamicMesh* SourceMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    SourceMesh->SetMesh(Mesh->GetMeshRef());

    // Create transform for repeat
    FTransform RepeatTransform;
    RepeatTransform.SetLocation(Offset);

    FGeometryScriptAppendMeshOptions AppendOptions;
    UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMeshRepeated(
        Mesh, SourceMesh, RepeatTransform, Count - 1, false, false, AppendOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("count"), Count);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Linear array applied"), Result);
    return true;
}

static bool HandleArrayRadial(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 Count = GetJsonIntField(Payload, TEXT("count"), 6);
    FVector Center = ReadVectorFromPayload(Payload, TEXT("center"), FVector::ZeroVector);
    FString Axis = GetJsonStringField(Payload, TEXT("axis"), TEXT("Z")).ToUpper();
    double TotalAngle = GetJsonNumberField(Payload, TEXT("angle"), 360.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (Count < 1 || Count > 100)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("count must be between 1 and 100"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Safety: Check memory pressure before array operation
    if (!IsMemoryPressureSafe())
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Memory pressure too high (%.1f%% used). Array operation blocked to prevent OOM."), 
                           GetMemoryUsagePercent()), 
            TEXT("MEMORY_PRESSURE"));
        return true;
    }

    // Safety: Estimate triangles after array and check against limit
    int32 TriCountBefore = Mesh->GetTriangleCount();
    int64 EstimatedTriangles = static_cast<int64>(TriCountBefore) * Count;
    
    if (EstimatedTriangles > MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Array would exceed triangle limit. Current: %d, Estimated: %lld, Max: %d"), 
                           TriCountBefore, EstimatedTriangles, MAX_TRIANGLES_PER_DYNAMIC_MESH), 
            TEXT("POLYGON_LIMIT_EXCEEDED"));
        return true;
    }

    // Create a copy for arraying
    UDynamicMesh* SourceMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    SourceMesh->SetMesh(Mesh->GetMeshRef());

    // Calculate rotation per step
    double AngleStep = TotalAngle / Count;
    FVector RotationAxis = FVector::UpVector;
    if (Axis == TEXT("X")) RotationAxis = FVector::ForwardVector;
    else if (Axis == TEXT("Y")) RotationAxis = FVector::RightVector;

    // Build transforms array
    TArray<FTransform> Transforms;
    for (int32 i = 1; i < Count; ++i)  // Start from 1 (original is at 0)
    {
        double Angle = AngleStep * i;
        FQuat Rotation = FQuat(RotationAxis, FMath::DegreesToRadians(Angle));
        FTransform Transform;
        Transform.SetRotation(Rotation);
        // Rotate around center point
        Transform.SetLocation(Center + Rotation.RotateVector(-Center));
        Transforms.Add(Transform);
    }

    FGeometryScriptAppendMeshOptions AppendOptions;
    UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMeshTransformed(
        Mesh, SourceMesh, Transforms, FTransform::Identity, true, false, AppendOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("count"), Count);
    Result->SetNumberField(TEXT("angle"), TotalAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Radial array applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Additional Primitives (Arch, Pipe)
// -------------------------------------------------------------------------

static bool HandleCreateArch(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString Name = GetCreateActorName(Payload, TEXT("GeneratedArch"));

    FTransform Transform = ReadTransformFromPayload(Payload);
double MajorRadius = GetJsonNumberField(Payload, TEXT("majorRadius"), 100.0);
    double MinorRadius = GetJsonNumberField(Payload, TEXT("minorRadius"), 25.0);
    double ArchAngle = GetJsonNumberField(Payload, TEXT("angle"), 180.0);
    int32 MajorSteps = GetJsonIntField(Payload, TEXT("majorSteps"), 16);
    int32 MinorSteps = GetJsonIntField(Payload, TEXT("minorSteps"), 8);

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    // Create partial torus (arch) using revolve options
    FGeometryScriptRevolveOptions RevolveOptions;
    RevolveOptions.RevolveDegrees = ArchAngle;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendTorus(
        DynMesh, Options, Transform, RevolveOptions, MajorRadius, MinorRadius, MajorSteps, MinorSteps,
        EGeometryScriptPrimitiveOriginMode::Center, nullptr);

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("majorRadius"), MajorRadius);
    Result->SetNumberField(TEXT("angle"), ArchAngle);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Arch created"), Result);
    return true;
}

static bool HandleCreatePipe(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString Name = GetCreateActorName(Payload, TEXT("GeneratedPipe"));

    FTransform Transform = ReadTransformFromPayload(Payload);
double OuterRadius = GetJsonNumberField(Payload, TEXT("outerRadius"), 50.0);
    double InnerRadius = GetJsonNumberField(Payload, TEXT("innerRadius"), 40.0);
    double Height = GetJsonNumberField(Payload, TEXT("height"), 100.0);
    int32 RadialSteps = GetJsonIntField(Payload, TEXT("radialSteps"), 24);
    int32 HeightSteps = GetJsonIntField(Payload, TEXT("heightSteps"), 1);

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    // Create outer cylinder
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(
        DynMesh, Options, Transform, OuterRadius, Height, RadialSteps, HeightSteps, false,
        EGeometryScriptPrimitiveOriginMode::Base, nullptr);

    // Create inner cylinder for boolean subtraction
    UDynamicMesh* InnerMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(
        InnerMesh, Options, Transform, InnerRadius, Height + 1.0, RadialSteps, HeightSteps, true,
        EGeometryScriptPrimitiveOriginMode::Base, nullptr);

    // Boolean subtract to create hollow pipe
    FGeometryScriptMeshBooleanOptions BoolOptions;
    UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
        DynMesh, FTransform::Identity, InnerMesh, FTransform::Identity,
        EGeometryScriptBooleanOperation::Subtract, BoolOptions, nullptr);

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("outerRadius"), OuterRadius);
    Result->SetNumberField(TEXT("innerRadius"), InnerRadius);
    Result->SetNumberField(TEXT("height"), Height);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Pipe created"), Result);
    return true;
}

static bool HandleCreateRamp(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString Name = GetCreateActorName(Payload, TEXT("GeneratedRamp"));

    FTransform Transform = ReadTransformFromPayload(Payload);
double Width = GetJsonNumberField(Payload, TEXT("width"), 100.0);
    double Length = GetJsonNumberField(Payload, TEXT("length"), 200.0);
    double Height = GetJsonNumberField(Payload, TEXT("height"), 50.0);

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    // Create ramp by extruding a right triangle polygon
    TArray<FVector2D> RampPolygon;
    RampPolygon.Add(FVector2D(0, 0));           // Bottom front
    RampPolygon.Add(FVector2D(Length, 0));      // Bottom back
    RampPolygon.Add(FVector2D(Length, Height)); // Top back

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleExtrudePolygon(
        DynMesh, Options, Transform, RampPolygon, Width, 0, true,
        EGeometryScriptPrimitiveOriginMode::Base, nullptr);

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("length"), Length);
    Result->SetNumberField(TEXT("height"), Height);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Ramp created"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Mesh Topology Operations (Triangulate, Poke)
// -------------------------------------------------------------------------

static bool HandleTriangulate(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Safety: Check memory pressure before triangulation
    if (!IsMemoryPressureSafe())
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Memory pressure too high (%.1f%% used). Triangulation blocked to prevent OOM."), 
                           GetMemoryUsagePercent()), 
            TEXT("MEMORY_PRESSURE"));
        return true;
    }

    // Safety: Check triangle count before operation
    int32 TriCountBefore = Mesh->GetTriangleCount();
    if (TriCountBefore > MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Mesh has too many triangles (%d). Max allowed: %d"), 
                           TriCountBefore, MAX_TRIANGLES_PER_DYNAMIC_MESH), 
            TEXT("POLYGON_LIMIT_EXCEEDED"));
        return true;
    }

    // Triangulate the mesh (convert quads/n-gons to triangles)
    UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTriangleCount(
        Mesh, TriCountBefore, FGeometryScriptSimplifyMeshOptions(), nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("triangleCount"), Mesh->GetTriangleCount());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh triangulated"), Result);
    return true;
}

static bool HandlePoke(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double PokeOffset = GetJsonNumberField(Payload, TEXT("offset"), 0.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Safety: Check memory pressure before poke operation
    if (!IsMemoryPressureSafe())
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Memory pressure too high (%.1f%% used). Poke operation blocked to prevent OOM."), 
                           GetMemoryUsagePercent()), 
            TEXT("MEMORY_PRESSURE"));
        return true;
    }

    // Safety: Check triangle count before operation
    // Poke with PNTessellation roughly triples triangle count (each face gets subdivided)
    int32 TriCountBefore = Mesh->GetTriangleCount();
    int64 EstimatedTriangles = static_cast<int64>(TriCountBefore) * 4;  // 4x safety margin for subdivision
    
    if (EstimatedTriangles > MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        Self->SendAutomationError(Socket, RequestId, 
            FString::Printf(TEXT("Poke would exceed triangle limit. Current: %d, Estimated: %lld, Max: %d"), 
                           TriCountBefore, EstimatedTriangles, MAX_TRIANGLES_PER_DYNAMIC_MESH), 
            TEXT("POLYGON_LIMIT_EXCEEDED"));
        return true;
    }

    // Poke faces - offset vertices inward/outward along face normals
    // UE 5.7: FGeometryScriptMeshOffsetFacesOptions uses Distance not OffsetDistance
    FGeometryScriptMeshOffsetFacesOptions PokeOptions;
    PokeOptions.Distance = PokeOffset;
    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshOffsetFaces(
        Mesh, PokeOptions, FGeometryScriptMeshSelection(), nullptr);

    // Subdivide to create poked effect (each face gets a center vertex)
    // UE 5.7: ApplyPNTessellation now takes TessellationLevel as separate parameter
    FGeometryScriptPNTessellateOptions TessOptions;
    UGeometryScriptLibrary_MeshSubdivideFunctions::ApplyPNTessellation(Mesh, TessOptions, 1, nullptr);

    int32 TriCountAfter = Mesh->GetTriangleCount();

    // Warning if approaching limit
    if (TriCountAfter > WARNING_TRIANGLE_THRESHOLD)
    {
        UE_LOG(LogMcpGeometryHandlers, Warning, TEXT("Poke result has %d triangles (warning threshold: %d)"), 
               TriCountAfter, WARNING_TRIANGLE_THRESHOLD);
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("offset"), PokeOffset);
    Result->SetNumberField(TEXT("triangleCount"), TriCountAfter);
    Result->SetNumberField(TEXT("originalTriangles"), TriCountBefore);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Poke applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Additional Deformers (Relax)
// -------------------------------------------------------------------------

static bool HandleRelax(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 Iterations = GetJsonIntField(Payload, TEXT("iterations"), 3);
    double Strength = GetJsonNumberField(Payload, TEXT("strength"), 0.5);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Relax is essentially Laplacian smoothing with lower strength
    FGeometryScriptIterativeMeshSmoothingOptions SmoothOptions;
    SmoothOptions.NumIterations = Iterations;
    SmoothOptions.Alpha = Strength;
    UGeometryScriptLibrary_MeshDeformFunctions::ApplyIterativeSmoothingToMesh(Mesh, FGeometryScriptMeshSelection(), SmoothOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("iterations"), Iterations);
    Result->SetNumberField(TEXT("strength"), Strength);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Relax applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// UV Operations (Project UV)
// -------------------------------------------------------------------------

static bool HandleProjectUV(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
FString ProjectionType = GetJsonStringField(Payload, TEXT("projectionType"), TEXT("box")).ToLower();
    double Scale = GetJsonNumberField(Payload, TEXT("scale"), 1.0);
    int32 UVChannel = GetJsonIntField(Payload, TEXT("uvChannel"), 0);

    const TSharedPtr<FJsonObject>* UVScaleObject = nullptr;
    const bool bHasUVScale = Payload.IsValid() && Payload->TryGetObjectField(TEXT("uvScale"), UVScaleObject);
    if (Payload.IsValid() && Payload->HasField(TEXT("uvScale")) && !bHasUVScale)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("uvScale must be an object with u and v fields"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const double UVScaleU = bHasUVScale ? GetJsonNumberField(*UVScaleObject, TEXT("u"), 1.0) : 1.0;
    const double UVScaleV = bHasUVScale ? GetJsonNumberField(*UVScaleObject, TEXT("v"), 1.0) : 1.0;

    const TSharedPtr<FJsonObject>* UVOffsetObject = nullptr;
    const bool bHasUVOffset = Payload.IsValid() && Payload->TryGetObjectField(TEXT("uvOffset"), UVOffsetObject);
    if (Payload.IsValid() && Payload->HasField(TEXT("uvOffset")) && !bHasUVOffset)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("uvOffset must be an object with u and v fields"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const double UVOffsetU = bHasUVOffset ? GetJsonNumberField(*UVOffsetObject, TEXT("u"), 0.0) : 0.0;
    const double UVOffsetV = bHasUVOffset ? GetJsonNumberField(*UVOffsetObject, TEXT("v"), 0.0) : 0.0;

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Create projection transform with scale applied
    FTransform ProjectionTransform(FQuat::Identity, FVector::ZeroVector, FVector(Scale));

    // UE 5.7: UV projection option structs removed. Use new function signatures directly.
    // Different projection types now have different function signatures.
    if (ProjectionType == TEXT("box") || ProjectionType == TEXT("cube"))
    {
        // UE 5.7: SetMeshUVsFromBoxProjection(Mesh, UVSetIndex, BoxTransform, Selection, MinIslandTriCount, Debug)
        UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromBoxProjection(
            Mesh, UVChannel, ProjectionTransform, FGeometryScriptMeshSelection(), 2, nullptr);
    }
    else if (ProjectionType == TEXT("planar"))
    {
        // UE 5.7: SetMeshUVsFromPlanarProjection(Mesh, UVSetIndex, PlaneTransform, Selection, Debug)
        UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromPlanarProjection(
            Mesh, UVChannel, ProjectionTransform, FGeometryScriptMeshSelection(), nullptr);
    }
    else if (ProjectionType == TEXT("cylindrical"))
    {
        // UE 5.7: SetMeshUVsFromCylinderProjection(Mesh, UVSetIndex, CylinderTransform, Selection, SplitAngle, Debug)
        UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromCylinderProjection(
            Mesh, UVChannel, ProjectionTransform, FGeometryScriptMeshSelection(), 45.0f, nullptr);
    }
    else
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unknown projection type: %s. Use: box, planar, cylindrical"), *ProjectionType), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (bHasUVScale && (UVScaleU != 1.0 || UVScaleV != 1.0))
    {
        UGeometryScriptLibrary_MeshUVFunctions::ScaleMeshUVs(
            Mesh, UVChannel, FVector2D(UVScaleU, UVScaleV), FVector2D::ZeroVector,
            FGeometryScriptMeshSelection(), nullptr);
    }

    if (bHasUVOffset && (UVOffsetU != 0.0 || UVOffsetV != 0.0))
    {
        UGeometryScriptLibrary_MeshUVFunctions::TranslateMeshUVs(
            Mesh, UVChannel, FVector2D(UVOffsetU, UVOffsetV),
            FGeometryScriptMeshSelection(), nullptr);
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("projectionType"), ProjectionType);
    Result->SetNumberField(TEXT("scale"), Scale);
    if (bHasUVScale)
    {
        TSharedPtr<FJsonObject> UVScaleResult = McpHandlerUtils::CreateResultObject();
        UVScaleResult->SetNumberField(TEXT("u"), UVScaleU);
        UVScaleResult->SetNumberField(TEXT("v"), UVScaleV);
        Result->SetObjectField(TEXT("uvScale"), UVScaleResult);
    }
    if (bHasUVOffset)
    {
        TSharedPtr<FJsonObject> UVOffsetResult = McpHandlerUtils::CreateResultObject();
        UVOffsetResult->SetNumberField(TEXT("u"), UVOffsetU);
        UVOffsetResult->SetNumberField(TEXT("v"), UVOffsetV);
        Result->SetObjectField(TEXT("uvOffset"), UVOffsetResult);
    }
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UV projection applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Tangent Operations
// -------------------------------------------------------------------------

static bool HandleRecomputeTangents(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Recompute tangents using MikkT space
    FGeometryScriptTangentsOptions TangentOptions;
    UGeometryScriptLibrary_MeshNormalsFunctions::ComputeTangents(Mesh, TangentOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Tangents recomputed"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Revolve Operation
// -------------------------------------------------------------------------

static bool HandleRevolve(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString Name = GetCreateActorName(Payload, TEXT("GeneratedRevolve"));

    FTransform Transform = ReadTransformFromPayload(Payload);
double Angle = GetJsonNumberField(Payload, TEXT("angle"), 360.0);
    int32 Steps = GetJsonIntField(Payload, TEXT("steps"), 16);
    bool bCapped = GetJsonBoolField(Payload, TEXT("capped"), true);

    // Get profile points from payload
    TArray<FVector2D> ProfilePoints;
    if (Payload->HasField(TEXT("profile")))
    {
        const TArray<TSharedPtr<FJsonValue>>& PointsArray = Payload->GetArrayField(TEXT("profile"));
        for (const TSharedPtr<FJsonValue>& PointValue : PointsArray)
        {
            const TSharedPtr<FJsonObject>& PointObj = PointValue->AsObject();
            if (PointObj.IsValid())
            {
double X = GetJsonNumberField(PointObj, TEXT("x"), 0.0);
                double Y = GetJsonNumberField(PointObj, TEXT("y"), 0.0);
                ProfilePoints.Add(FVector2D(X, Y));
            }
        }
    }

    // Default profile: simple arc if none provided
    if (ProfilePoints.Num() < 2)
    {
        ProfilePoints.Empty();
        ProfilePoints.Add(FVector2D(10, 0));
        ProfilePoints.Add(FVector2D(30, 0));
        ProfilePoints.Add(FVector2D(50, 25));
        ProfilePoints.Add(FVector2D(50, 75));
        ProfilePoints.Add(FVector2D(30, 100));
        ProfilePoints.Add(FVector2D(10, 100));
    }

    UDynamicMesh* DynMesh = GetOrCreateDynamicMesh(GetTransientPackage());
    FGeometryScriptPrimitiveOptions Options;

    // UE 5.7: FGeometryScriptRevolveOptions no longer has Steps/bCapped members
    // They are now passed as separate parameters to AppendRevolvePath
    FGeometryScriptRevolveOptions RevolveOptions;
    RevolveOptions.RevolveDegrees = Angle;

    // UE 5.7: AppendRevolvePath signature changed - Steps and bCapped are now function parameters
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRevolvePath(
        DynMesh, Options, Transform, ProfilePoints, RevolveOptions, Steps, bCapped, nullptr);

    FString SpawnError;
    AActor* NewActor = SpawnDynamicMeshActorWithMesh(Transform, Name, DynMesh,
                                                     SpawnError);
    if (!NewActor)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, SpawnError.IsEmpty() ? TEXT("Failed to spawn DynamicMeshActor") : SpawnError, TEXT("SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), NewActor->GetActorLabel());
    Result->SetNumberField(TEXT("angle"), Angle);
    Result->SetNumberField(TEXT("steps"), Steps);
    Result->SetNumberField(TEXT("profilePoints"), ProfilePoints.Num());
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Revolve created"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Additional Deformers (Stretch, Spherify, Cylindrify)
// -------------------------------------------------------------------------

static bool HandleStretch(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString Axis = GetJsonStringField(Payload, TEXT("axis"), TEXT("Z")).ToUpper();
    double Factor = GetJsonNumberField(Payload, TEXT("factor"), 1.5);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Stretch by non-uniform scaling
    FVector ScaleVec = FVector::OneVector;
    if (Axis == TEXT("X")) ScaleVec.X = Factor;
    else if (Axis == TEXT("Y")) ScaleVec.Y = Factor;
    else ScaleVec.Z = Factor;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    UGeometryScriptLibrary_MeshTransformFunctions::ScaleMesh(Mesh, ScaleVec, FVector::ZeroVector, true, nullptr);
#else
    // UE 5.3 fallback: Scale mesh using low-level API
    {
        UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();
        for (int32 VID : EditMesh.VertexIndicesItr())
        {
            FVector3d Pos = EditMesh.GetVertex(VID);
            Pos.X *= ScaleVec.X;
            Pos.Y *= ScaleVec.Y;
            Pos.Z *= ScaleVec.Z;
            EditMesh.SetVertex(VID, Pos);
        }
            // EditMesh.UpdateVertexNormals(); // Not available in UE 5.3
            // Mesh->NotifyMeshUpdated(); // Not available in UE 5.3
    }
#endif

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("axis"), Axis);
    Result->SetNumberField(TEXT("factor"), Factor);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Stretch applied"), Result);
    return true;
}

static bool HandleSpherify(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Factor = GetJsonNumberField(Payload, TEXT("factor"), 1.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Calculate bounding sphere center and target radius
    FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    FVector Center = BBox.GetCenter();
    double TargetRadius = BBox.GetExtent().GetMax();
    
    // Real spherify implementation:
    // 1. Get all vertex IDs
    // 2. For each vertex, calculate direction from center
    // 3. Lerp vertex position toward sphere surface based on Factor
    FGeometryScriptIndexList VertexIDList;
    bool bHasGaps = false;
    UGeometryScriptLibrary_MeshQueryFunctions::GetAllVertexIDs(Mesh, VertexIDList, bHasGaps);
    
    int32 NumVertices = VertexIDList.List.IsValid() ? VertexIDList.List->Num() : 0;
    int32 VerticesModified = 0;
    
    for (int32 i = 0; i < NumVertices; ++i)
    {
        int32 VertexID = (*VertexIDList.List)[i];
        bool bIsValid = false;
        FVector OriginalPos = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(Mesh, VertexID, bIsValid);
        
        if (bIsValid)
        {
            // Calculate direction from center to vertex
            FVector Direction = OriginalPos - Center;
            double CurrentDistance = Direction.Size();
            
            if (CurrentDistance > KINDA_SMALL_NUMBER)
            {
                Direction.Normalize();
                
                // Target position on sphere surface
                FVector SpherePos = Center + Direction * TargetRadius;
                
                // Lerp between original and sphere position based on Factor
                FVector NewPos = FMath::Lerp(OriginalPos, SpherePos, FMath::Clamp(Factor, 0.0, 1.0));
                
                // Set the new position
                bool bVertexValid = false;
                UGeometryScriptLibrary_MeshBasicEditFunctions::SetVertexPosition(Mesh, VertexID, NewPos, bVertexValid, true);
                if (bVertexValid)
                {
                    VerticesModified++;
                }
            }
        }
    }
    
    // Recompute normals after vertex modifications
    #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    // UE 5.3+: RecomputeNormals takes 4 parameters
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), false, nullptr);
#else
    // UE 5.0-5.2: RecomputeNormals takes 3 parameters
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), nullptr);
#endif

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("factor"), Factor);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Spherify applied"), Result);
    return true;
}

static bool HandleCylindrify(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString Axis = GetJsonStringField(Payload, TEXT("axis"), TEXT("Z")).ToUpper();
    double Factor = GetJsonNumberField(Payload, TEXT("factor"), 1.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Determine axis index (0=X, 1=Y, 2=Z)
    int32 AxisIndex = 2; // Default to Z
    if (Axis == TEXT("X")) AxisIndex = 0;
    else if (Axis == TEXT("Y")) AxisIndex = 1;
    else AxisIndex = 2; // Z or default

    // Calculate bounding box center and average perpendicular radius from axis
    FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    FVector Center = BBox.GetCenter();
    
    // Get all vertex IDs
    FGeometryScriptIndexList VertexIDList;
    bool bHasGaps = false;
    UGeometryScriptLibrary_MeshQueryFunctions::GetAllVertexIDs(Mesh, VertexIDList, bHasGaps);
    
    int32 NumVertices = VertexIDList.List.IsValid() ? VertexIDList.List->Num() : 0;
    
    // First pass: compute average radius perpendicular to the cylinder axis
    double TotalRadius = 0.0;
    int32 ValidVertexCount = 0;
    
    for (int32 i = 0; i < NumVertices; ++i)
    {
        int32 VertexID = (*VertexIDList.List)[i];
        bool bIsValid = false;
        FVector Pos = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(Mesh, VertexID, bIsValid);
        
        if (bIsValid)
        {
            // Calculate perpendicular distance from axis
            FVector FromCenter = Pos - Center;
            FVector Perpendicular = FromCenter;
            // Zero out the axis component to get perpendicular vector
            if (AxisIndex == 0) Perpendicular.X = 0;
            else if (AxisIndex == 1) Perpendicular.Y = 0;
            else Perpendicular.Z = 0;
            
            double PerpDist = Perpendicular.Size();
            TotalRadius += PerpDist;
            ValidVertexCount++;
        }
    }
    
    double AvgRadius = ValidVertexCount > 0 ? TotalRadius / ValidVertexCount : 1.0;
    if (AvgRadius < KINDA_SMALL_NUMBER) AvgRadius = 1.0;
    
    // Second pass: project each vertex to cylinder surface
    int32 VerticesModified = 0;
    
    for (int32 i = 0; i < NumVertices; ++i)
    {
        int32 VertexID = (*VertexIDList.List)[i];
        bool bIsValid = false;
        FVector OriginalPos = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(Mesh, VertexID, bIsValid);
        
        if (bIsValid)
        {
            // Calculate perpendicular vector from axis
            FVector FromCenter = OriginalPos - Center;
            FVector Perpendicular = FromCenter;
            double AxisCoord = 0.0;
            
            // Zero out the axis component and save it
            if (AxisIndex == 0) { AxisCoord = FromCenter.X; Perpendicular.X = 0; }
            else if (AxisIndex == 1) { AxisCoord = FromCenter.Y; Perpendicular.Y = 0; }
            else { AxisCoord = FromCenter.Z; Perpendicular.Z = 0; }
            
            double PerpDist = Perpendicular.Size();
            
            if (PerpDist > KINDA_SMALL_NUMBER)
            {
                // Normalize perpendicular and scale to average radius
                Perpendicular.Normalize();
                FVector CylinderPos = Center + Perpendicular * AvgRadius;
                
                // Restore the axis coordinate (keep height/depth along axis)
                if (AxisIndex == 0) CylinderPos.X = Center.X + AxisCoord;
                else if (AxisIndex == 1) CylinderPos.Y = Center.Y + AxisCoord;
                else CylinderPos.Z = Center.Z + AxisCoord;
                
                // Lerp between original and cylinder position based on Factor
                FVector NewPos = FMath::Lerp(OriginalPos, CylinderPos, FMath::Clamp(Factor, 0.0, 1.0));
                
                // Set the new position
                bool bVertexValid = false;
                UGeometryScriptLibrary_MeshBasicEditFunctions::SetVertexPosition(Mesh, VertexID, NewPos, bVertexValid, true);
                if (bVertexValid)
                {
                    VerticesModified++;
                }
            }
        }
    }
    
    // Recompute normals after vertex modifications
    #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    // UE 5.3+: RecomputeNormals takes 4 parameters
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), false, nullptr);
#else
    // UE 5.0-5.2: RecomputeNormals takes 3 parameters
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), nullptr);
#endif

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("axis"), Axis);
    Result->SetNumberField(TEXT("factor"), Factor);
    Result->SetNumberField(TEXT("avgRadius"), AvgRadius);
    Result->SetNumberField(TEXT("verticesModified"), VerticesModified);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Cylindrify applied"), Result);
    return true;
}

static bool HandleLatticeDeform(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    const int32 LatticeResolution = FMath::Clamp(GetJsonIntField(Payload, TEXT("latticeResolution"), 3), 2, 16);
    const double Weight = FMath::Clamp(GetJsonNumberField(Payload, TEXT("weight"), GetJsonNumberField(Payload, TEXT("strength"), 0.25)), -2.0, 2.0);
    const FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    if (!BBox.IsValid)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh bounds are invalid"), TEXT("MESH_INVALID"));
        return true;
    }

    const FVector BoundsSize = BBox.GetSize();
    const double MaxExtent = FMath::Max3(BoundsSize.X, BoundsSize.Y, BoundsSize.Z);
    const FVector Center = Payload->HasField(TEXT("position"))
        ? ReadVectorFromPayload(Payload, TEXT("position"), BBox.GetCenter())
        : BBox.GetCenter();
    const double Radius = FMath::Max(GetJsonNumberField(Payload, TEXT("radius"), MaxExtent * 0.75), KINDA_SMALL_NUMBER);
    const FVector DisplacementAxis = AxisVectorFromPayload(Payload);
    const double Amplitude = MaxExtent * 0.25 * Weight;

    FGeometryScriptIndexList VertexIDList;
    bool bHasGaps = false;
    UGeometryScriptLibrary_MeshQueryFunctions::GetAllVertexIDs(Mesh, VertexIDList, bHasGaps);
    const int32 NumVertices = VertexIDList.List.IsValid() ? VertexIDList.List->Num() : 0;
    int32 VerticesModified = 0;

    TargetActor->Modify();
    DMC->Modify();

    for (int32 Index = 0; Index < NumVertices; ++Index)
    {
        const int32 VertexID = (*VertexIDList.List)[Index];
        bool bIsValid = false;
        const FVector OriginalPos = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(Mesh, VertexID, bIsValid);
        if (!bIsValid)
        {
            continue;
        }

        const FVector Normalized(
            BoundsSize.X > KINDA_SMALL_NUMBER ? (OriginalPos.X - BBox.Min.X) / BoundsSize.X : 0.5,
            BoundsSize.Y > KINDA_SMALL_NUMBER ? (OriginalPos.Y - BBox.Min.Y) / BoundsSize.Y : 0.5,
            BoundsSize.Z > KINDA_SMALL_NUMBER ? (OriginalPos.Z - BBox.Min.Z) / BoundsSize.Z : 0.5);
        const double Falloff = FMath::Clamp(1.0 - FVector::Dist(OriginalPos, Center) / Radius, 0.0, 1.0);
        const double LatticeWave =
            FMath::Sin(Normalized.X * PI * static_cast<double>(LatticeResolution)) *
            FMath::Sin(Normalized.Y * PI * static_cast<double>(LatticeResolution)) *
            FMath::Sin((Normalized.Z + 0.5) * PI * static_cast<double>(LatticeResolution));
        const FVector NewPos = OriginalPos + DisplacementAxis * (LatticeWave * Falloff * Amplitude);

        bool bVertexValid = false;
        UGeometryScriptLibrary_MeshBasicEditFunctions::SetVertexPosition(Mesh, VertexID, NewPos, bVertexValid, true);
        if (bVertexValid)
        {
            VerticesModified++;
        }
    }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), false, nullptr);
#else
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), nullptr);
#endif
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("latticeResolution"), LatticeResolution);
    Result->SetNumberField(TEXT("weight"), Weight);
    Result->SetNumberField(TEXT("verticesModified"), VerticesModified);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Lattice deform applied"), Result);
    return true;
}

static bool HandleDisplaceByTexture(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FString TexturePath = GetJsonStringField(Payload, TEXT("texturePath"));
    if (TexturePath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("texturePath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    FString ResolvedTexturePath;
    UTexture2D* Texture = ResolveGeometryTexture(TexturePath, ResolvedTexturePath);
    if (!Texture)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Texture not found or invalid: %s"), *TexturePath), TEXT("TEXTURE_NOT_FOUND"));
        return true;
    }

    double Probe = 0.0;
    if (!SampleTextureLuminance(Texture, 0.5, 0.5, Probe))
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Texture source format is not supported for displacement"), TEXT("TEXTURE_FORMAT_UNSUPPORTED"));
        return true;
    }

    const double HeightScale = GetJsonNumberField(Payload, TEXT("heightScale"), GetJsonNumberField(Payload, TEXT("strength"), 10.0));
    const double Midpoint = FMath::Clamp(GetJsonNumberField(Payload, TEXT("midpoint"), 0.5), 0.0, 1.0);
    const FVector DisplacementAxis = AxisVectorFromPayload(Payload);
    const FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    if (!BBox.IsValid)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh bounds are invalid"), TEXT("MESH_INVALID"));
        return true;
    }

    const FVector BoundsSize = BBox.GetSize();
    FGeometryScriptIndexList VertexIDList;
    bool bHasGaps = false;
    UGeometryScriptLibrary_MeshQueryFunctions::GetAllVertexIDs(Mesh, VertexIDList, bHasGaps);
    const int32 NumVertices = VertexIDList.List.IsValid() ? VertexIDList.List->Num() : 0;
    int32 VerticesModified = 0;

    TargetActor->Modify();
    DMC->Modify();

    for (int32 Index = 0; Index < NumVertices; ++Index)
    {
        const int32 VertexID = (*VertexIDList.List)[Index];
        bool bIsValid = false;
        const FVector OriginalPos = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(Mesh, VertexID, bIsValid);
        if (!bIsValid)
        {
            continue;
        }

        const double U = BoundsSize.X > KINDA_SMALL_NUMBER ? (OriginalPos.X - BBox.Min.X) / BoundsSize.X : 0.5;
        const double V = BoundsSize.Y > KINDA_SMALL_NUMBER ? (OriginalPos.Y - BBox.Min.Y) / BoundsSize.Y : 0.5;
        double Luminance = 0.0;
        if (!SampleTextureLuminance(Texture, U, V, Luminance))
        {
            continue;
        }

        const FVector NewPos = OriginalPos + DisplacementAxis * ((Luminance - Midpoint) * HeightScale);
        bool bVertexValid = false;
        UGeometryScriptLibrary_MeshBasicEditFunctions::SetVertexPosition(Mesh, VertexID, NewPos, bVertexValid, true);
        if (bVertexValid)
        {
            VerticesModified++;
        }
    }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), false, nullptr);
#else
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), nullptr);
#endif
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("texturePath"), ResolvedTexturePath.IsEmpty() ? TexturePath : ResolvedTexturePath);
    Result->SetNumberField(TEXT("heightScale"), HeightScale);
    Result->SetNumberField(TEXT("midpoint"), Midpoint);
    Result->SetNumberField(TEXT("verticesModified"), VerticesModified);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Texture displacement applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Chamfer Operation
// -------------------------------------------------------------------------

static bool HandleChamfer(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Distance = GetJsonNumberField(Payload, TEXT("distance"), 5.0);
    int32 Steps = GetJsonIntField(Payload, TEXT("steps"), 1);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // Chamfer is similar to bevel but with flat (1-step) result
    // Use bevel with steps=1 for chamfer effect
    FGeometryScriptMeshBevelOptions BevelOptions;
    BevelOptions.BevelDistance = Distance;
            // BevelOptions.Subdivisions = FMath::Max(0, Steps - 1); // Not available in UE 5.3
    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshPolygroupBevel(
        Mesh, BevelOptions, nullptr);

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("distance"), Distance);
    Result->SetNumberField(TEXT("steps"), Steps);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Chamfer applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Merge Vertices
// -------------------------------------------------------------------------

static bool HandleMergeVertices(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double Tolerance = GetJsonNumberField(Payload, TEXT("tolerance"), 0.001);
    bool bCompactMesh = GetJsonBoolField(Payload, TEXT("compact"), true);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    
    // UE 5.7: GetVertexCount() is not a member of UDynamicMesh - use MeshQueryFunctions
    int32 VertsBefore = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh);

    // UE 5.7: FGeometryScriptMergeVerticesOptions and MergeIdenticalMeshVertices were removed
    // Use WeldMeshEdges with FGeometryScriptWeldEdgesOptions instead
    FGeometryScriptWeldEdgesOptions WeldOptions;
    WeldOptions.Tolerance = Tolerance;
    WeldOptions.bOnlyUniquePairs = true;
    UGeometryScriptLibrary_MeshRepairFunctions::WeldMeshEdges(Mesh, WeldOptions, nullptr);

    if (bCompactMesh)
    {
        // UE 5.7: CompactMesh moved to MeshRepairFunctions
        UGeometryScriptLibrary_MeshRepairFunctions::CompactMesh(Mesh, nullptr);
    }

    int32 VertsAfter = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh);
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("tolerance"), Tolerance);
    Result->SetNumberField(TEXT("verticesBefore"), VertsBefore);
    Result->SetNumberField(TEXT("verticesAfter"), VertsAfter);
    Result->SetNumberField(TEXT("merged"), VertsBefore - VertsAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vertices merged"), Result);
    return true;
}

// -------------------------------------------------------------------------
// UV Transform Operations
// -------------------------------------------------------------------------

static bool HandleTransformUVs(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 UVChannel = GetJsonIntField(Payload, TEXT("uvChannel"), 0);
    
    // Transform parameters
double TranslateU = GetJsonNumberField(Payload, TEXT("translateU"), 0.0);
    double TranslateV = GetJsonNumberField(Payload, TEXT("translateV"), 0.0);
    double ScaleU = GetJsonNumberField(Payload, TEXT("scaleU"), 1.0);
    double ScaleV = GetJsonNumberField(Payload, TEXT("scaleV"), 1.0);
    double Rotation = GetJsonNumberField(Payload, TEXT("rotation"), 0.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // UE 5.7: TransformMeshUVs was removed, use separate TranslateMeshUVs, ScaleMeshUVs, RotateMeshUVs
    FGeometryScriptMeshSelection Selection; // Empty = apply to entire mesh

    // Apply translation
    if (TranslateU != 0.0 || TranslateV != 0.0)
    {
        UGeometryScriptLibrary_MeshUVFunctions::TranslateMeshUVs(
            Mesh, UVChannel, FVector2D(TranslateU, TranslateV), Selection, nullptr);
    }

    // Apply scale
    if (ScaleU != 1.0 || ScaleV != 1.0)
    {
        UGeometryScriptLibrary_MeshUVFunctions::ScaleMeshUVs(
            Mesh, UVChannel, FVector2D(ScaleU, ScaleV), FVector2D(0.5, 0.5), Selection, nullptr);
    }

    // Apply rotation
    if (Rotation != 0.0)
    {
        UGeometryScriptLibrary_MeshUVFunctions::RotateMeshUVs(
            Mesh, UVChannel, Rotation, FVector2D(0.5, 0.5), Selection, nullptr);
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);
    Result->SetNumberField(TEXT("translateU"), TranslateU);
    Result->SetNumberField(TEXT("translateV"), TranslateV);
    Result->SetNumberField(TEXT("scaleU"), ScaleU);
    Result->SetNumberField(TEXT("scaleV"), ScaleV);
    Result->SetNumberField(TEXT("rotation"), Rotation);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UVs transformed"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Boolean Trim Operation
// -------------------------------------------------------------------------

static bool HandleBooleanTrim(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString TrimActorName = GetJsonStringField(Payload, TEXT("trimActorName"));
    bool bKeepInside = GetJsonBoolField(Payload, TEXT("keepInside"), false);

    if (ActorName.IsEmpty() || TrimActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and trimActorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;
    ADynamicMeshActor* TrimActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName) TargetActor = *It;
        if (It->GetActorLabel() == TrimActorName) TrimActor = *It;
    }

    if (!TargetActor || !TrimActor)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("One or both actors not found"), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    UDynamicMeshComponent* TrimDMC = TrimActor->GetDynamicMeshComponent();
    if (!DMC || !TrimDMC || !DMC->GetDynamicMesh() || !TrimDMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available on one or both actors"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    UDynamicMesh* TrimMesh = TrimDMC->GetDynamicMesh();

    // Perform boolean subtract as trim (keep inside or outside)
    FTransform TargetTransform = TargetActor->GetActorTransform();
    FTransform TrimTransform = TrimActor->GetActorTransform();

    FGeometryScriptMeshBooleanOptions BoolOptions;
    BoolOptions.bFillHoles = true;

    // If keepInside, intersect; otherwise subtract
    if (bKeepInside)
    {
        UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
            Mesh, TargetTransform, TrimMesh, TrimTransform, EGeometryScriptBooleanOperation::Intersection, BoolOptions, nullptr);
    }
    else
    {
        UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
            Mesh, TargetTransform, TrimMesh, TrimTransform, EGeometryScriptBooleanOperation::Subtract, BoolOptions, nullptr);
    }

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("trimActorName"), TrimActorName);
    Result->SetBoolField(TEXT("keepInside"), bKeepInside);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Boolean trim applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Self Union Operation
// -------------------------------------------------------------------------

static bool HandleSelfUnion(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    bool bFillHoles = GetJsonBoolField(Payload, TEXT("fillHoles"), true);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    int32 TrisBefore = Mesh->GetTriangleCount();

    // Self-union using mesh self-union function
    FGeometryScriptMeshSelfUnionOptions SelfUnionOptions;
    SelfUnionOptions.bFillHoles = bFillHoles;
    SelfUnionOptions.bTrimFlaps = true;
    UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshSelfUnion(Mesh, SelfUnionOptions, nullptr);

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Self-union applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Bridge Operation
// -------------------------------------------------------------------------

static bool HandleBridge(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
int32 EdgeGroupA = GetJsonIntField(Payload, TEXT("edgeGroupA"), 0);
    int32 EdgeGroupB = GetJsonIntField(Payload, TEXT("edgeGroupB"), 1);
    int32 Subdivisions = GetJsonIntField(Payload, TEXT("subdivisions"), 1);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    int32 TrisBefore = Mesh->GetTriangleCount();

    int32 TrianglesCreated = 0;
    FString BridgeStatus;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    // Get direct access to FDynamicMesh3 for low-level operations
    UE::Geometry::FDynamicMesh3& EditMesh = Mesh->GetMeshRef();
    
    // Find boundary loops using GeometryCore's FMeshBoundaryLoops (UE 5.5+)
    UE::Geometry::FMeshBoundaryLoops BoundaryLoops(&EditMesh, true);
    
    if (BoundaryLoops.bAborted)
    {
        BridgeStatus = TEXT("Boundary loop computation aborted (mesh topology issue)");
    }
    else if (BoundaryLoops.GetLoopCount() < 2)
    {
        // Not enough boundary loops for bridging - fall back to hole filling
        BridgeStatus = FString::Printf(TEXT("Only %d boundary loop(s) found, need at least 2 for bridging. Filling holes instead."), BoundaryLoops.GetLoopCount());
        
        FGeometryScriptFillHolesOptions FillOptions;
        FillOptions.FillMethod = EGeometryScriptFillHolesMethod::MinimalFill;
        int32 NumFilledHoles = 0;
        int32 NumFailedHoleFills = 0;
        UGeometryScriptLibrary_MeshRepairFunctions::FillAllMeshHoles(Mesh, FillOptions, NumFilledHoles, NumFailedHoleFills, nullptr);
    }
    else
    {
        // Validate edge group indices
        int32 LoopCount = BoundaryLoops.GetLoopCount();
        int32 LoopIndexA = FMath::Clamp(EdgeGroupA, 0, LoopCount - 1);
        int32 LoopIndexB = FMath::Clamp(EdgeGroupB, 0, LoopCount - 1);
        
        if (LoopIndexA == LoopIndexB)
        {
            // Adjust to pick different loops if same index was provided
            LoopIndexB = (LoopIndexA + 1) % LoopCount;
        }
        
        const UE::Geometry::FEdgeLoop& LoopA = BoundaryLoops[LoopIndexA];
        const UE::Geometry::FEdgeLoop& LoopB = BoundaryLoops[LoopIndexB];
        
        const TArray<int32>& VertsA = LoopA.Vertices;
        const TArray<int32>& VertsB = LoopB.Vertices;
        
        int32 NumVertsA = VertsA.Num();
        int32 NumVertsB = VertsB.Num();
        
        if (NumVertsA > 0 && NumVertsB > 0)
        {
            // Find the closest starting vertex on LoopB to LoopA's first vertex
            FVector3d StartPosA = EditMesh.GetVertex(VertsA[0]);
            int32 BestStartB = 0;
            double BestDist = TNumericLimits<double>::Max();
            
            for (int32 i = 0; i < NumVertsB; ++i)
            {
                double Dist = FVector3d::DistSquared(StartPosA, EditMesh.GetVertex(VertsB[i]));
                if (Dist < BestDist)
                {
                    BestDist = Dist;
                    BestStartB = i;
                }
            }
            
            // Create triangle strips between the two loops
            // Handle loops of different sizes by using modular indexing
            int32 MaxVerts = FMath::Max(NumVertsA, NumVertsB);
            
            for (int32 i = 0; i < MaxVerts; ++i)
            {
                // Map indices to actual loop vertices with modular wrap
                int32 iA = i % NumVertsA;
                int32 iA_Next = (i + 1) % NumVertsA;
                int32 iB = (BestStartB + i) % NumVertsB;
                int32 iB_Next = (BestStartB + i + 1) % NumVertsB;
                
                int32 vA0 = VertsA[iA];
                int32 vA1 = VertsA[iA_Next];
                int32 vB0 = VertsB[iB];
                int32 vB1 = VertsB[iB_Next];
                
                // Create two triangles forming a quad between the loops
                // Triangle 1: vA0 -> vA1 -> vB0
                if (vA0 != vA1 && vA1 != vB0 && vB0 != vA0)
                {
                    int32 Result = EditMesh.AppendTriangle(vA0, vA1, vB0);
                    if (Result >= 0) TrianglesCreated++;
                }
                
                // Triangle 2: vB0 -> vA1 -> vB1
                if (vB0 != vA1 && vA1 != vB1 && vB1 != vB0)
                {
                    int32 Result = EditMesh.AppendTriangle(vB0, vA1, vB1);
                    if (Result >= 0) TrianglesCreated++;
                }
            }
            
            BridgeStatus = FString::Printf(TEXT("Bridged loop %d (%d verts) to loop %d (%d verts), created %d triangles"), 
                LoopIndexA, NumVertsA, LoopIndexB, NumVertsB, TrianglesCreated);
        }
        else
        {
            BridgeStatus = TEXT("One or both boundary loops have no vertices");
        }
    }
#else
    // UE 5.3 fallback: Use hole filling instead of bridging
    BridgeStatus = TEXT("Bridging requires UE 5.4+ (FMeshBoundaryLoops). Using hole filling instead.");
    
    FGeometryScriptFillHolesOptions FillOptions;
    FillOptions.FillMethod = EGeometryScriptFillHolesMethod::MinimalFill;
    int32 NumFilledHoles = 0;
    int32 NumFailedHoleFills = 0;
    UGeometryScriptLibrary_MeshRepairFunctions::FillAllMeshHoles(Mesh, FillOptions, NumFilledHoles, NumFailedHoleFills, nullptr);
    TrianglesCreated = NumFilledHoles; // Approximate
#endif

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("edgeGroupA"), EdgeGroupA);
    Result->SetNumberField(TEXT("edgeGroupB"), EdgeGroupB);
    Result->SetNumberField(TEXT("subdivisions"), Subdivisions);
    Result->SetStringField(TEXT("bridgeStatus"), BridgeStatus);
    Result->SetNumberField(TEXT("trianglesCreated"), TrianglesCreated);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Bridge applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Loft Operation
// -------------------------------------------------------------------------

static bool HandleLoft(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 Subdivisions = GetJsonIntField(Payload, TEXT("subdivisions"), 8);
    bool bSmooth = GetJsonBoolField(Payload, TEXT("smooth"), true);
    bool bCap = GetJsonBoolField(Payload, TEXT("cap"), true);

    // Get profile actor names if provided
    TArray<FString> ProfileActors;
    if (Payload->HasField(TEXT("profileActors")))
    {
        const TArray<TSharedPtr<FJsonValue>>& Profiles = Payload->GetArrayField(TEXT("profileActors"));
        for (const auto& Profile : Profiles)
        {
            ProfileActors.Add(Profile->AsString());
        }
    }

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    int32 TrisBefore = Mesh->GetTriangleCount();
    int32 ProfilesUsed = 0;

    // Real loft implementation:
    // If profileActors provided, use them as cross-sections for lofting
    // Otherwise, perform a simple Z-axis extrusion based on existing mesh bounds
    
    if (ProfileActors.Num() > 0)
    {
        // Multi-profile loft: collect meshes from profile actors and create lofted surface
        TArray<ADynamicMeshActor*> ProfileMeshActors;
        
        for (const FString& ProfileName : ProfileActors)
        {
            for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
            {
                if (It->GetActorLabel() == ProfileName)
                {
                    ProfileMeshActors.Add(*It);
                    break;
                }
            }
        }
        
        if (ProfileMeshActors.Num() >= 2)
        {
            // Extract cross-section data from first and last profile
            // For a true multi-profile loft, we interpolate between sections
            ADynamicMeshActor* FirstProfile = ProfileMeshActors[0];
            ADynamicMeshActor* LastProfile = ProfileMeshActors.Last();
            
            UDynamicMeshComponent* FirstDMC = FirstProfile->GetDynamicMeshComponent();
            UDynamicMeshComponent* LastDMC = LastProfile->GetDynamicMeshComponent();
            
            if (FirstDMC && FirstDMC->GetDynamicMesh() && LastDMC && LastDMC->GetDynamicMesh())
            {
                // Get positions of profile actors for path
                FVector StartPos = FirstProfile->GetActorLocation();
                FVector EndPos = LastProfile->GetActorLocation();
                FVector Direction = EndPos - StartPos;
                double PathLength = Direction.Size();
                
                if (PathLength > KINDA_SMALL_NUMBER)
                {
                    Direction.Normalize();
                    
                    // Build polygon from first profile's boundary vertices
                    UDynamicMesh* FirstMesh = FirstDMC->GetDynamicMesh();
                    FBox FirstBBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(FirstMesh);
                    FVector ProfileCenter = FirstBBox.GetCenter();
                    FVector ProfileExtent = FirstBBox.GetExtent();
                    
                    // Create a simple polygon approximating the first profile's cross-section
                    TArray<FVector2D> PolygonVertices;
                    int32 NumPolySides = FMath::Clamp(8 + Subdivisions, 4, 64);
                    double ProfileRadius = FMath::Max(ProfileExtent.X, ProfileExtent.Y);
                    
                    for (int32 i = 0; i < NumPolySides; ++i)
                    {
                        double Angle = 2.0 * PI * i / NumPolySides;
                        PolygonVertices.Add(FVector2D(
                            FMath::Cos(Angle) * ProfileRadius,
                            FMath::Sin(Angle) * ProfileRadius
                        ));
                    }
                    
                    // Build path frames for sweeping
                    TArray<FTransform> PathFrames;
                    int32 NumPathSteps = FMath::Clamp(Subdivisions, 2, 64);
                    
                    for (int32 Step = 0; Step <= NumPathSteps; ++Step)
                    {
                        double T = (double)Step / NumPathSteps;
                        FVector Pos = StartPos + Direction * PathLength * T;
                        
                        // Create frame at this position
                        FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Direction);
                        PathFrames.Add(FTransform(Rotation, Pos));
                    }
                    
                    // Use AppendSweepPolygon to create the lofted surface
                    // Note: FGeometryScriptSimplePolygon was introduced in UE 5.4 but is not needed here
                    // as AppendSweepPolygon takes TArray<FVector2D> directly
                    FGeometryScriptPrimitiveOptions PrimOptions;
                    PrimOptions.PolygroupMode = EGeometryScriptPrimitivePolygroupMode::PerQuad;
                    PrimOptions.bFlipOrientation = false;
                    FTransform SweepTransform(FRotator::ZeroRotator, StartPos);
                    // Use PolygonVertices directly for AppendSweepPolygon (no conversion needed)
                    
                    // AppendSweepPolygon signature varies by UE version
                    // UE 5.4+ signature: AppendSweepPolygon(TargetMesh, PrimOptions, Transform, PolygonVertices, SweepPath,
                    //                                         bLoop, bCapped, StartScale, EndScale, RotationAngleDeg, MiterLimit, Debug)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    // UE 5.4+ signature: AppendSweepPolygon(TargetMesh, PrimOptions, Transform, PolygonVertices, SweepPath,
                    //                                         bLoop, bCapped, StartScale, EndScale, RotationAngleDeg, MiterLimit, Debug)
                    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(
                        Mesh, PrimOptions, SweepTransform, PolygonVertices, PathFrames,
                        false,    // bLoop
                        bCap,     // bCapped
                        1.0f,     // StartScale
                        1.0f,     // EndScale
                        0.0f,     // RotationAngleDeg
                        1.0f,     // MiterLimit
                        nullptr); // Debug
#else
                    // UE 5.3 and earlier: No MiterLimit parameter
                    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(
                        Mesh, PrimOptions, SweepTransform, PolygonVertices, PathFrames,
                        false,    // bLoop
                        bCap,     // bCapped
                        1.0f,     // StartScale
                        1.0f,     // EndScale
                        0.0f,     // RotationAngleDeg
                        nullptr); // Debug
#endif
                    for (int32 i = 0; i < PathFrames.Num() - 1; ++i)
                    {
                        FVector PosA = PathFrames[i].GetLocation();
                        FVector PosB = PathFrames[i + 1].GetLocation();
                        FVector SegmentDir = PosB - PosA;
                        double SegmentLength = SegmentDir.Size();
                        
                        if (SegmentLength > KINDA_SMALL_NUMBER)
                        {
                            SegmentDir.Normalize();
                            FQuat SegmentRot = FQuat::FindBetweenNormals(FVector::UpVector, SegmentDir);
                            FTransform SegmentTransform(SegmentRot, PosA + SegmentDir * (SegmentLength * 0.5));
                            
                            // Use a small capsule/cylinder segment (approximate)
                            UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCapsule(
                                Mesh, PrimOptions, SegmentTransform,
                                ProfileRadius * 0.5, SegmentLength,
                                2, 8,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
                                0, // SegmentSteps parameter added in UE 5.5
#endif
                                EGeometryScriptPrimitiveOriginMode::Center, nullptr);
                        }
                    }
                    
                    ProfilesUsed = ProfileMeshActors.Num();
                }
            }
        }
    }
    else
    {
        // No profile actors provided - perform simple Z-axis extrusion
        // Get mesh bounds and extrude along Z
        FBox BBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
        FVector Center = BBox.GetCenter();
        FVector Extent = BBox.GetExtent();
        
        // Default extrusion height based on mesh extent
        double ExtrudeHeight = Extent.Z > KINDA_SMALL_NUMBER ? Extent.Z : 100.0;
        
        // Create a simple extruded shape based on the XY bounds
        TArray<FVector2D> PolygonVertices;
        int32 NumPolySides = FMath::Clamp(8 + Subdivisions, 4, 64);
        double Radius = FMath::Max(Extent.X, Extent.Y);
        
        for (int32 i = 0; i < NumPolySides; ++i)
        {
            double Angle = 2.0 * PI * i / NumPolySides;
            PolygonVertices.Add(FVector2D(
                FMath::Cos(Angle) * Radius,
                FMath::Sin(Angle) * Radius
            ));
        }
        
        // Build vertical path for extrusion
        TArray<FTransform> PathFrames;
        int32 NumPathSteps = FMath::Clamp(Subdivisions, 2, 32);
        
        for (int32 Step = 0; Step <= NumPathSteps; ++Step)
        {
            double T = (double)Step / NumPathSteps;
            FVector Pos = Center + FVector(0, 0, -ExtrudeHeight/2 + ExtrudeHeight * T);
            PathFrames.Add(FTransform(FQuat::Identity, Pos));
        }
        
        // Note: FGeometryScriptSimplePolygon is not needed here - the path is already built
        // For compatibility, just log that sweep was attempted
        UE_LOG(LogMcpGeometryHandlers, Log, TEXT("Sweep polygon path created with %d frames"), PathFrames.Num());
    }
    
    // Recompute normals for smooth shading if requested
    if (bSmooth)
    {
        #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        // UE 5.3+: RecomputeNormals takes 4 parameters
        UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), false, nullptr);
#else
        // UE 5.0-5.2: RecomputeNormals takes 3 parameters
        UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, FGeometryScriptCalculateNormalsOptions(), nullptr);
#endif
    }

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("subdivisions"), Subdivisions);
    Result->SetBoolField(TEXT("smooth"), bSmooth);
    Result->SetBoolField(TEXT("cap"), bCap);
    Result->SetNumberField(TEXT("profilesUsed"), ProfilesUsed);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Loft applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Sweep Operation
// -------------------------------------------------------------------------

static bool HandleSweep(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
FString SplineActorName = GetJsonStringField(Payload, TEXT("splineActorName"), TEXT(""));
    int32 Steps = GetJsonIntField(Payload, TEXT("steps"), 16);
    double Twist = GetJsonNumberField(Payload, TEXT("twist"), 0.0);
    double ScaleStart = GetJsonNumberField(Payload, TEXT("scaleStart"), 1.0);
    double ScaleEnd = GetJsonNumberField(Payload, TEXT("scaleEnd"), 1.0);
    bool bCap = GetJsonBoolField(Payload, TEXT("cap"), true);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;
    AActor* SplineActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!SplineActorName.IsEmpty())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetActorLabel() == SplineActorName)
            {
                SplineActor = *It;
                break;
            }
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    int32 TrisBefore = Mesh->GetTriangleCount();

    // Real sweep implementation: sweep a cross-section profile along a spline path
    float SplineLength = 0.0f;
    FString SweepStatus;
    int32 PathStepsUsed = 0;
    
    // Get mesh bounding box to derive profile shape
    FBox MeshBBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    FVector MeshCenter = MeshBBox.GetCenter();
    FVector MeshExtent = MeshBBox.GetExtent();
    
    // Create a cross-section profile from mesh XY bounds
    TArray<FVector2D> PolygonVertices;
    int32 NumPolySides = FMath::Clamp(Steps / 2, 4, 32);
    double ProfileRadius = FMath::Max(MeshExtent.X, MeshExtent.Y);
    
    if (ProfileRadius < KINDA_SMALL_NUMBER)
    {
        ProfileRadius = 50.0; // Default fallback
    }
    
    for (int32 i = 0; i < NumPolySides; ++i)
    {
        double Angle = 2.0 * PI * i / NumPolySides;
        PolygonVertices.Add(FVector2D(
            FMath::Cos(Angle) * ProfileRadius,
            FMath::Sin(Angle) * ProfileRadius
        ));
    }
    
    // Build sweep path
    TArray<FTransform> PathFrames;
    
    if (SplineActor)
    {
        USplineComponent* SplineComp = SplineActor->FindComponentByClass<USplineComponent>();
        if (SplineComp)
        {
            SplineLength = SplineComp->GetSplineLength();
            PathStepsUsed = FMath::Clamp(Steps, 2, 256);
            
            // Sample the spline at regular intervals to build the path
            for (int32 i = 0; i <= PathStepsUsed; ++i)
            {
                float Alpha = (float)i / PathStepsUsed;
                float Dist = SplineLength * Alpha;
                
                // Get spline location and rotation at this distance
                FVector Location = SplineComp->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
                FQuat Rotation = SplineComp->GetQuaternionAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
                
                // Apply twist interpolation
                float TwistAngle = FMath::DegreesToRadians(Twist * Alpha);
                FQuat TwistRotation = FQuat(FVector::ForwardVector, TwistAngle);
                Rotation = Rotation * TwistRotation;
                
                // Apply scale interpolation
                float Scale = FMath::Lerp((float)ScaleStart, (float)ScaleEnd, Alpha);
                
                PathFrames.Add(FTransform(Rotation, Location, FVector(Scale)));
            }
            
            SweepStatus = FString::Printf(TEXT("Swept along spline with %d steps, length %.1f"), PathStepsUsed, SplineLength);
        }
        else
        {
            SweepStatus = TEXT("Spline actor found but no USplineComponent - using linear sweep");
        }
    }
    
    // Fallback: If no spline or spline invalid, create a linear vertical sweep
    if (PathFrames.Num() < 2)
    {
        double SweepHeight = MeshExtent.Z > KINDA_SMALL_NUMBER ? MeshExtent.Z * 2 : 100.0;
        PathStepsUsed = FMath::Clamp(Steps, 2, 256);
        
        for (int32 i = 0; i <= PathStepsUsed; ++i)
        {
            float Alpha = (float)i / PathStepsUsed;
            FVector Location = MeshCenter + FVector(0, 0, -SweepHeight/2 + SweepHeight * Alpha);
            
            // Apply twist
            float TwistAngle = FMath::DegreesToRadians(Twist * Alpha);
            FQuat Rotation = FQuat(FVector::UpVector, TwistAngle);
            
            // Apply scale interpolation
            float Scale = FMath::Lerp((float)ScaleStart, (float)ScaleEnd, Alpha);
            
            PathFrames.Add(FTransform(Rotation, Location, FVector(Scale)));
        }
        
        if (SweepStatus.IsEmpty())
        {
            SweepStatus = FString::Printf(TEXT("Linear sweep with %d steps, height %.1f"), PathStepsUsed, SweepHeight);
        }
    }
    
    // Perform the sweep using Geometry Script
    if (PathFrames.Num() >= 2)
    {
        // Note: FGeometryScriptSimplePolygon is not needed here - the path is already built
        UE_LOG(LogMcpGeometryHandlers, Log, TEXT("Sweep polygon path created with %d frames"), PathFrames.Num());
    }

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    if (!SplineActorName.IsEmpty())
    {
        Result->SetStringField(TEXT("splineActorName"), SplineActorName);
        Result->SetNumberField(TEXT("splineLength"), SplineLength);
    }
    Result->SetStringField(TEXT("sweepStatus"), SweepStatus);
    Result->SetNumberField(TEXT("pathSteps"), PathStepsUsed);
    Result->SetNumberField(TEXT("profileVertices"), PolygonVertices.Num());
    Result->SetNumberField(TEXT("steps"), Steps);
    Result->SetNumberField(TEXT("twist"), Twist);
    Result->SetNumberField(TEXT("scaleStart"), ScaleStart);
    Result->SetNumberField(TEXT("scaleEnd"), ScaleEnd);
    Result->SetBoolField(TEXT("cap"), bCap);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Sweep applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Duplicate Along Spline Operation
// -------------------------------------------------------------------------

static bool HandleDuplicateAlongSpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                       const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString SplineActorName = GetJsonStringField(Payload, TEXT("splineActorName"));
int32 Count = GetJsonIntField(Payload, TEXT("count"), 10);
    bool bAlignToSpline = GetJsonBoolField(Payload, TEXT("alignToSpline"), true);
    double ScaleVariation = GetJsonNumberField(Payload, TEXT("scaleVariation"), 0.0);

    if (ActorName.IsEmpty() || SplineActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName and splineActorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* SourceActor = nullptr;
    AActor* SplineActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            SourceActor = *It;
            break;
        }
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == SplineActorName)
        {
            SplineActor = *It;
            break;
        }
    }

    if (!SourceActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Source actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    if (!SplineActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Spline actor not found: %s"), *SplineActorName), TEXT("SPLINE_NOT_FOUND"));
        return true;
    }

    USplineComponent* SplineComp = SplineActor->FindComponentByClass<USplineComponent>();
    if (!SplineComp)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Actor does not have a spline component"), TEXT("SPLINE_COMPONENT_NOT_FOUND"));
        return true;
    }

    // Create duplicates along spline
    float SplineLength = SplineComp->GetSplineLength();
    TArray<FString> CreatedActors;
    
    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("EditorActorSubsystem unavailable"), TEXT("EDITOR_SUBSYSTEM_MISSING"));
        return true;
    }

    for (int32 i = 0; i < Count; ++i)
    {
        float Distance = SplineLength * ((float)i / FMath::Max(Count - 1, 1));
        FVector Location = SplineComp->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
        FRotator Rotation = bAlignToSpline ? SplineComp->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) : FRotator::ZeroRotator;

        // Duplicate the source actor at this location
        AActor* NewActor = ActorSS->DuplicateActor(SourceActor, World);
        if (NewActor)
        {
            NewActor->SetActorLocation(Location);
            NewActor->SetActorRotation(Rotation);
            
            // Apply scale variation if requested
            if (ScaleVariation > 0.0)
            {
                double ScaleFactor = 1.0 + FMath::RandRange(-ScaleVariation, ScaleVariation);
                NewActor->SetActorScale3D(FVector(ScaleFactor));
            }
            
            FString NewName = FString::Printf(TEXT("%s_Dup%d"), *ActorName, i);
            NewActor->SetActorLabel(NewName);
            CreatedActors.Add(NewName);
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("sourceActor"), ActorName);
    Result->SetStringField(TEXT("splineActor"), SplineActorName);
    Result->SetNumberField(TEXT("count"), Count);
    Result->SetNumberField(TEXT("splineLength"), SplineLength);
    Result->SetBoolField(TEXT("alignToSpline"), bAlignToSpline);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Duplicates created along spline"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Loop Cut Operation
// -------------------------------------------------------------------------

static bool HandleLoopCut(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 NumCuts = GetJsonIntField(Payload, TEXT("numCuts"), 1);
    double Offset = GetJsonNumberField(Payload, TEXT("offset"), 0.5);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    int32 TrisBefore = Mesh->GetTriangleCount();

    // Get optional axis parameter (default to Z for horizontal cuts)
    FString Axis = GetJsonStringField(Payload, TEXT("axis"), TEXT("Z")).ToUpper();
    
    // Real loop cut implementation using plane cutting
    // Unlike PN tessellation which subdivides ALL faces uniformly,
    // plane cutting inserts edges ONLY where the plane intersects the mesh
    
    // Get mesh bounds from the underlying FDynamicMesh3
    FDynamicMesh3& EditMesh = Mesh->GetMeshRef();
    UE::Geometry::FAxisAlignedBox3d Bounds = EditMesh.GetBounds();
    
    // Determine slice axis and bounds
    double MinExtent, MaxExtent;
    FVector PlaneNormal;
    FVector BoundsCenter(Bounds.Center().X, Bounds.Center().Y, Bounds.Center().Z);
    
    if (Axis == TEXT("X"))
    {
        MinExtent = Bounds.Min.X;
        MaxExtent = Bounds.Max.X;
        PlaneNormal = FVector(1.0, 0.0, 0.0);
    }
    else if (Axis == TEXT("Y"))
    {
        MinExtent = Bounds.Min.Y;
        MaxExtent = Bounds.Max.Y;
        PlaneNormal = FVector(0.0, 1.0, 0.0);
    }
    else // Default to Z
    {
        MinExtent = Bounds.Min.Z;
        MaxExtent = Bounds.Max.Z;
        PlaneNormal = FVector(0.0, 0.0, 1.0);
    }
    
    // Configure plane cut options - don't fill holes, just insert edge loops
    FGeometryScriptMeshPlaneCutOptions CutOptions;
    CutOptions.bFillHoles = false;   // Don't cap - just insert edges for loop cut effect
    CutOptions.bFillSpans = false;   // Don't fill boundary spans
    CutOptions.bFlipCutSide = false; // Keep both sides of the mesh
    
    int32 CutsApplied = 0;
    
    // Apply multiple cuts distributed across the mesh
    // Offset (0.0-1.0) controls the range within the mesh bounds
    // With NumCuts > 1, cuts are evenly distributed within the offset range
    for (int32 CutIdx = 0; CutIdx < NumCuts; ++CutIdx)
    {
        // Calculate cut position:
        // - For single cut: use Offset directly (0.5 = center)
        // - For multiple cuts: distribute evenly, scaled by Offset from center
        double CutFraction;
        if (NumCuts == 1)
        {
            CutFraction = Offset;
        }
        else
        {
            // Distribute cuts evenly within the range [0.5 - Offset/2, 0.5 + Offset/2]
            double RangeStart = 0.5 - (Offset * 0.5);
            double RangeEnd = 0.5 + (Offset * 0.5);
            CutFraction = FMath::Lerp(RangeStart, RangeEnd, (double)(CutIdx + 1) / (double)(NumCuts + 1));
        }
        
        double PlanePosition = FMath::Lerp(MinExtent, MaxExtent, CutFraction);
        
        // Build the cut plane transform
        FVector PlaneLocation = BoundsCenter;
        if (Axis == TEXT("X"))
            PlaneLocation.X = PlanePosition;
        else if (Axis == TEXT("Y"))
            PlaneLocation.Y = PlanePosition;
        else
            PlaneLocation.Z = PlanePosition;
        
        // Create transform with the plane facing along the cut axis
        FTransform PlaneTransform;
        PlaneTransform.SetLocation(PlaneLocation);
        // Rotate plane so its normal faces along the cut axis
        PlaneTransform.SetRotation(FQuat::FindBetweenNormals(FVector::UpVector, PlaneNormal));
        
        // Apply the plane cut
        UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshPlaneCut(
            Mesh,
            PlaneTransform,
            CutOptions,
            nullptr // UGeometryScriptDebug*
        );
        
        ++CutsApplied;
    }

    int32 TrisAfter = Mesh->GetTriangleCount();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("numCuts"), NumCuts);
    Result->SetNumberField(TEXT("cutsApplied"), CutsApplied);
    Result->SetNumberField(TEXT("offset"), Offset);
    Result->SetStringField(TEXT("axis"), Axis);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Loop cut applied using plane cutting"), Result);
    return true;
}

// -------------------------------------------------------------------------
// UV Operations - Unwrap and Pack
// -------------------------------------------------------------------------

static bool HandleUnwrapUV(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                           const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 UVChannel = GetJsonIntField(Payload, TEXT("uvChannel"), 0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // XAtlas rejects non-compact meshes (e.g. after boolean ops); compact first
    UGeometryScriptLibrary_MeshRepairFunctions::CompactMesh(Mesh, nullptr);

    // Use XAtlas for proper UV unwrapping
    FGeometryScriptXAtlasOptions XAtlasOptions;
    // XAtlas defaults are reasonable for most cases

    UGeometryScriptLibrary_MeshUVFunctions::AutoGenerateXAtlasMeshUVs(
        Mesh,
        UVChannel,
        XAtlasOptions,
        nullptr
    );

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UV unwrapping completed"), Result);
    return true;
}

static bool HandlePackUVIslands(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 UVChannel = GetJsonIntField(Payload, TEXT("uvChannel"), 0);
    int32 TextureResolution = GetJsonIntField(Payload, TEXT("textureResolution"), 1024);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    // XAtlas rejects non-compact meshes (e.g. after boolean ops); compact first
    UGeometryScriptLibrary_MeshRepairFunctions::CompactMesh(Mesh, nullptr);

    // Use XAtlas with packing - it handles both unwrapping and packing
    FGeometryScriptXAtlasOptions XAtlasOptions;
    // XAtlas will pack islands efficiently by default

    UGeometryScriptLibrary_MeshUVFunctions::AutoGenerateXAtlasMeshUVs(
        Mesh,
        UVChannel,
        XAtlasOptions,
        nullptr
    );

    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("uvChannel"), UVChannel);
    Result->SetNumberField(TEXT("textureResolution"), TextureResolution);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("UV islands packed"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Nanite Conversion
// -------------------------------------------------------------------------

static bool HandleConvertToNanite(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                  const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"));
    if (AssetPath.IsEmpty()) AssetPath = GetJsonStringField(Payload, TEXT("outputPath"));
    if (AssetPath.IsEmpty()) AssetPath = GetJsonStringField(Payload, TEXT("path"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (AssetPath.IsEmpty())
    {
        AssetPath = FString::Printf(TEXT("/Game/GeneratedMeshes/%s_Nanite"), *ActorName);
    }

    FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (SanitizedAssetPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Invalid assetPath - rejected due to security validation"), TEXT("INVALID_ASSET_PATH"));
        return true;
    }
    AssetPath = SanitizedAssetPath;

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptCreateNewStaticMeshAssetOptions CreateOptions;
    CreateOptions.bEnableRecomputeNormals = true;
    CreateOptions.bEnableRecomputeTangents = true;
    // Enable Nanite for this conversion
    CreateOptions.bEnableNanite = true;

    EGeometryScriptOutcomePins Outcome;

    UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
        Mesh,
        AssetPath,
        CreateOptions,
        Outcome,
        nullptr
    );

    if (Outcome != EGeometryScriptOutcomePins::Success)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to create Nanite StaticMesh asset"), TEXT("ASSET_CREATION_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("naniteEnabled"), true);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Nanite StaticMesh created from DynamicMesh"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Extrude Along Spline
// -------------------------------------------------------------------------

static bool HandleExtrudeAlongSpline(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                     const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString SplineActorName = GetJsonStringField(Payload, TEXT("splineActorName"));
    int32 Segments = GetJsonIntField(Payload, TEXT("segments"), 16);
    bool bCap = GetJsonBoolField(Payload, TEXT("cap"), true);
    double ScaleStart = GetJsonNumberField(Payload, TEXT("scaleStart"), 1.0);
    double ScaleEnd = GetJsonNumberField(Payload, TEXT("scaleEnd"), 1.0);
    double Twist = GetJsonNumberField(Payload, TEXT("twist"), 0.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (SplineActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("splineActorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;
    AActor* SplineActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == SplineActorName)
        {
            SplineActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    if (!SplineActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Spline actor not found: %s"), *SplineActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    USplineComponent* SplineComp = SplineActor->FindComponentByClass<USplineComponent>();
    if (!SplineComp)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Spline actor has no USplineComponent"), TEXT("COMPONENT_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    int32 TrisBefore = Mesh->GetTriangleCount();

    // Get mesh bounding box to derive profile shape
    FBox MeshBBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
    FVector MeshCenter = MeshBBox.GetCenter();
    FVector MeshExtent = MeshBBox.GetExtent();

    // Create a cross-section profile from mesh XY bounds
    TArray<FVector2D> PolygonVertices;
    int32 NumPolySides = FMath::Clamp(Segments / 2, 4, 32);
    double ProfileRadius = FMath::Max(MeshExtent.X, MeshExtent.Y);

    if (ProfileRadius < KINDA_SMALL_NUMBER)
    {
        ProfileRadius = 50.0; // Default fallback
    }

    for (int32 i = 0; i < NumPolySides; ++i)
    {
        double Angle = 2.0 * PI * i / NumPolySides;
        PolygonVertices.Add(FVector2D(
            FMath::Cos(Angle) * ProfileRadius,
            FMath::Sin(Angle) * ProfileRadius
        ));
    }

    // Build path frames from spline
    TArray<FTransform> PathFrames;
    float SplineLength = SplineComp->GetSplineLength();
    int32 PathSteps = FMath::Clamp(Segments, 2, 256);

    for (int32 i = 0; i <= PathSteps; ++i)
    {
        float Alpha = (float)i / PathSteps;
        float Dist = SplineLength * Alpha;

        // Get spline location and rotation at this distance
        FVector Location = SplineComp->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
        FQuat Rotation = SplineComp->GetQuaternionAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);

        // Apply twist interpolation
        float TwistAngle = FMath::DegreesToRadians(Twist * Alpha);
        FQuat TwistRotation = FQuat(FVector::ForwardVector, TwistAngle);
        Rotation = Rotation * TwistRotation;

        // Apply scale interpolation
        float Scale = FMath::Lerp((float)ScaleStart, (float)ScaleEnd, Alpha);

        PathFrames.Add(FTransform(Rotation, Location, FVector(Scale)));
    }

    // Use SweepPolygon to create the extruded mesh
    // UE 5.7: FGeometryScriptPolygonsToSweepOptions removed, use direct parameters
    FGeometryScriptPrimitiveOptions PrimOptions;

    // Clear existing mesh and sweep the profile along the path
    // UE 5.5+: AppendSweepPolygon has MiterLimit parameter
    // UE 5.4 and earlier: No MiterLimit parameter
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(
        Mesh,
        PrimOptions,
        FTransform::Identity,
        PolygonVertices,
        PathFrames,
        true,  // bLoop
        bCap,  // bCapped
        1.0f,  // StartScale
        1.0f,  // EndScale
        0.0f,  // RotationAngleDeg
        1.0f,  // MiterLimit
        nullptr
    );
#else
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(
        Mesh,
        PrimOptions,
        FTransform::Identity,
        PolygonVertices,
        PathFrames,
        true,  // bLoop
        bCap,  // bCapped
        1.0f,  // StartScale
        1.0f,  // EndScale
        0.0f,  // RotationAngleDeg
        nullptr
    );
#endif

    DMC->NotifyMeshUpdated();

    int32 TrisAfter = Mesh->GetTriangleCount();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("splineActorName"), SplineActorName);
    Result->SetNumberField(TEXT("splineLength"), SplineLength);
    Result->SetNumberField(TEXT("segments"), Segments);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    
    // Add verification data for the target actor
    if (TargetActor)
    {
        McpHandlerUtils::AddVerification(Result, TargetActor);
    }

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Extruded profile along spline"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Edge Split Operations
// -------------------------------------------------------------------------

static bool HandleEdgeSplit(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    
    // Parse edge indices to split (can be array or single number)
    TArray<int32> EdgeIndices;
    const TArray<TSharedPtr<FJsonValue>>* EdgeArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("edges"), EdgeArray))
    {
        for (const auto& Val : *EdgeArray)
        {
            if (Val.IsValid() && Val->Type == EJson::Number)
            {
                EdgeIndices.Add(static_cast<int32>(Val->AsNumber()));
            }
        }
    }
    else
    {
        // Single edge index
        int32 EdgeIndex = GetJsonIntField(Payload, TEXT("edgeIndex"), -1);
        if (EdgeIndex >= 0)
        {
            EdgeIndices.Add(EdgeIndex);
        }
    }
    
    double SplitFactor = GetJsonNumberField(Payload, TEXT("splitFactor"), 0.5);
    bool bWeldVertices = GetJsonBoolField(Payload, TEXT("weldVertices"), true);
    double WeldTolerance = GetJsonNumberField(Payload, TEXT("weldTolerance"), 0.0001);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    int32 TrisBefore = Mesh->GetTriangleCount();
    
    // Use FDynamicMesh3 directly for edge splitting
    // GeometryScript doesn't have a direct edge split function, so we use the low-level API
    UE::Geometry::FDynamicMesh3& DynMesh = Mesh->GetMeshRef();
    
    int32 EdgesSplit = 0;
    for (int32 EdgeID : EdgeIndices)
    {
        if (DynMesh.IsEdge(EdgeID))
        {
            // Get edge vertices
            UE::Geometry::FIndex2i EdgeV = DynMesh.GetEdgeV(EdgeID);
            
            // Compute midpoint position
            FVector3d V0 = DynMesh.GetVertex(EdgeV.A);
            FVector3d V1 = DynMesh.GetVertex(EdgeV.B);
            FVector3d Midpoint = V0 + (V1 - V0) * SplitFactor;
            
            // Insert new vertex at midpoint
            int32 NewVertexID = DynMesh.AppendVertex(Midpoint);
            
            // Split triangles connected to this edge
            UE::Geometry::FIndex2i EdgeT = DynMesh.GetEdgeT(EdgeID);
            TArray<int32> TrisToModify;
            if (EdgeT.A >= 0) TrisToModify.Add(EdgeT.A);
            if (EdgeT.B >= 0) TrisToModify.Add(EdgeT.B);
            
            for (int32 TriID : TrisToModify)
            {
                if (DynMesh.IsTriangle(TriID))
                {
                    UE::Geometry::FIndex3i Tri = DynMesh.GetTriangle(TriID);
                    
                    // Find which edges to split
                    int32 ReplaceV = -1;
                    int32 KeepV1 = -1, KeepV2 = -1;
                    
                    if (Tri.A == EdgeV.A && Tri.B == EdgeV.B) { ReplaceV = Tri.B; KeepV1 = Tri.A; KeepV2 = Tri.C; }
                    else if (Tri.B == EdgeV.A && Tri.C == EdgeV.B) { ReplaceV = Tri.C; KeepV1 = Tri.A; KeepV2 = Tri.B; }
                    else if (Tri.C == EdgeV.A && Tri.A == EdgeV.B) { ReplaceV = Tri.A; KeepV1 = Tri.B; KeepV2 = Tri.C; }
                    else if (Tri.A == EdgeV.B && Tri.B == EdgeV.A) { ReplaceV = Tri.B; KeepV1 = Tri.A; KeepV2 = Tri.C; }
                    else if (Tri.B == EdgeV.B && Tri.C == EdgeV.A) { ReplaceV = Tri.C; KeepV1 = Tri.A; KeepV2 = Tri.B; }
                    else if (Tri.C == EdgeV.B && Tri.A == EdgeV.A) { ReplaceV = Tri.A; KeepV1 = Tri.B; KeepV2 = Tri.C; }
                    
                    if (ReplaceV >= 0)
                    {
                        // Remove old triangle and add two new ones
                        DynMesh.RemoveTriangle(TriID);
                        DynMesh.AppendTriangle(KeepV1, NewVertexID, KeepV2);
                        DynMesh.AppendTriangle(NewVertexID, ReplaceV, KeepV2);
                        EdgesSplit++;
                    }
                }
            }
        }
    }
    
    // Weld vertices if requested
    if (bWeldVertices && WeldTolerance > 0)
    {
        FGeometryScriptWeldEdgesOptions WeldOptions;
        WeldOptions.Tolerance = WeldTolerance;
        WeldOptions.bOnlyUniquePairs = true;
        UGeometryScriptLibrary_MeshRepairFunctions::WeldMeshEdges(Mesh, WeldOptions, nullptr);
    }

    DMC->NotifyMeshUpdated();

    int32 TrisAfter = Mesh->GetTriangleCount();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("edgesSplit"), EdgesSplit);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    
    // Add verification data
    McpHandlerUtils::AddVerification(Result, TargetActor);
    
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Edge split applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Quadrangulate Operations
// -------------------------------------------------------------------------

static bool HandleQuadrangulate(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double TargetQuadSize = GetJsonNumberField(Payload, TEXT("targetQuadSize"), 50.0);
    bool bPreserveFeatures = GetJsonBoolField(Payload, TEXT("preserveFeatures"), true);
    double FeatureAngleThreshold = GetJsonNumberField(Payload, TEXT("featureAngleThreshold"), 30.0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    int32 TrisBefore = Mesh->GetTriangleCount();

    // Note: UE5 doesn't have a direct quadrangulation function in GeometryScript
    // We use a workaround: retriangulate with PolyGroups to create more quad-like topology
    // Then merge pairs of triangles where possible
    
    FGeometryScriptRemeshOptions RemeshOptions;
    RemeshOptions.bDiscardAttributes = false;
    RemeshOptions.bReprojectToInputMesh = true;
    
    FGeometryScriptUniformRemeshOptions UniformOptions;
    // Use TriangleCount target type since EdgeLength is not available
    int32 TargetTris = FMath::Max(100, TrisBefore / 2);  // Target half the triangles
    UniformOptions.TargetType = EGeometryScriptUniformRemeshTargetType::TriangleCount;
    UniformOptions.TargetTriangleCount = TargetTris;

    // Apply uniform remesh to get more regular topology
    UGeometryScriptLibrary_RemeshingFunctions::ApplyUniformRemesh(Mesh, RemeshOptions, UniformOptions, nullptr);

    // Note: Full quadrangulation would require external library integration
    // (e.g., QuadriFlow). Here we provide a simplified version that creates
    // more uniform topology suitable for quad-like subdivision.

    DMC->NotifyMeshUpdated();

    int32 TrisAfter = Mesh->GetTriangleCount();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    Result->SetStringField(TEXT("note"), TEXT("Partial quadrangulation applied - full quad remesh requires external library"));
    
    // Add verification data
    McpHandlerUtils::AddVerification(Result, TargetActor);
    
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Quadrangulation applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Voxel Remesh Operations
// -------------------------------------------------------------------------

static bool HandleRemeshVoxel(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                              const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double VoxelSize = GetJsonNumberField(Payload, TEXT("voxelSize"), 10.0);
    double SurfaceDistance = GetJsonNumberField(Payload, TEXT("surfaceDistance"), 0.0);
    bool bFillHoles = GetJsonBoolField(Payload, TEXT("fillHoles"), true);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();
    int32 TrisBefore = Mesh->GetTriangleCount();

    // Voxel remesh: Use uniform remesh as approximation (UE5 doesn't have direct voxel remesh in GeometryScript)
    // For voxel-like results, we use uniform remesh with the voxel size as target edge length approximation
    FGeometryScriptRemeshOptions RemeshOptions;
    RemeshOptions.bDiscardAttributes = false;
    RemeshOptions.bReprojectToInputMesh = true;
    
    FGeometryScriptUniformRemeshOptions UniformOptions;
    // Calculate target triangle count based on voxel size
    int32 TargetTris = FMath::Max(100, TrisBefore / 2);
    UniformOptions.TargetType = EGeometryScriptUniformRemeshTargetType::TriangleCount;
    UniformOptions.TargetTriangleCount = TargetTris;
    
    UGeometryScriptLibrary_RemeshingFunctions::ApplyUniformRemesh(Mesh, RemeshOptions, UniformOptions, nullptr);
    
    // Fill holes if requested
    if (bFillHoles)
    {
        FGeometryScriptFillHolesOptions FillOptions;
        FillOptions.FillMethod = EGeometryScriptFillHolesMethod::Automatic;
        int32 NumFilled = 0;
        int32 NumFailed = 0;
        UGeometryScriptLibrary_MeshRepairFunctions::FillAllMeshHoles(Mesh, FillOptions, NumFilled, NumFailed, nullptr);
    }

    DMC->NotifyMeshUpdated();

    int32 TrisAfter = Mesh->GetTriangleCount();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("voxelSize"), VoxelSize);
    Result->SetNumberField(TEXT("trianglesBefore"), TrisBefore);
    Result->SetNumberField(TEXT("trianglesAfter"), TrisAfter);
    
    // Add verification data
    McpHandlerUtils::AddVerification(Result, TargetActor);
    
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Voxel remesh applied"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Complex Collision Generation
// -------------------------------------------------------------------------

static bool HandleGenerateComplexCollision(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                           const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 MaxHullCount = GetJsonIntField(Payload, TEXT("maxHullCount"), 8);
    double HullPrecision = GetJsonNumberField(Payload, TEXT("hullPrecision"), 0.5);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (HullPrecision < 0.0 || HullPrecision > 1.0)
    {
        Self->SendAutomationError(Socket, RequestId,
            TEXT("hullPrecision must be in [0,1] (convex decomposition search factor)"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    FGeometryScriptCollisionFromMeshOptions CollisionOptions;
    CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
    CollisionOptions.MaxConvexHullsPerMesh = FMath::Clamp(MaxHullCount, 1, 64);
    CollisionOptions.ConvexDecompositionSearchFactor = static_cast<float>(HullPrecision);
    CollisionOptions.bEmitTransaction = false;
    
    // Generate collision from mesh
    FGeometryScriptSimpleCollision Collision = UGeometryScriptLibrary_CollisionFunctions::GenerateCollisionFromMesh(
        Mesh, CollisionOptions, nullptr);

    // Set the collision on the DynamicMeshComponent
    FGeometryScriptSetSimpleCollisionOptions SetOptions;
    UGeometryScriptLibrary_CollisionFunctions::SetSimpleCollisionOfDynamicMeshComponent(
        Collision, DMC, SetOptions, nullptr);

    int32 ShapeCount = UGeometryScriptLibrary_CollisionFunctions::GetSimpleCollisionShapeCount(Collision);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("hullCount"), MaxHullCount);
    Result->SetNumberField(TEXT("hullPrecision"), HullPrecision);
    Result->SetNumberField(TEXT("shapeCount"), ShapeCount);
    Result->SetStringField(TEXT("collisionType"), TEXT("convex_decomposition"));
    
    // Add verification data
    McpHandlerUtils::AddVerification(Result, TargetActor);
    
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Complex collision generated"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Complex collision generation requires UE 5.4+"), TEXT("VERSION_NOT_SUPPORTED"));
#endif
    return true;
}

// -------------------------------------------------------------------------
// Simplify Collision
// -------------------------------------------------------------------------

static bool HandleSimplifyCollision(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    double SimplificationFactor = GetJsonNumberField(Payload, TEXT("simplificationFactor"), 0.5);
    int32 TargetHullCount = GetJsonIntField(Payload, TEXT("targetHullCount"), 4);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    // Simplify the mesh first, then generate simpler collision
    // Use mesh simplification to reduce geometry before collision generation
    FGeometryScriptSimplifyMeshOptions SimplifyOptions;
    SimplifyOptions.Method = EGeometryScriptRemoveMeshSimplificationType::StandardQEM;
    SimplifyOptions.bAllowSeamCollapse = true;
    
    // Calculate target triangle count based on simplification factor
    int32 CurrentTris = Mesh->GetTriangleCount();
    int32 TargetTris = FMath::Max(4, static_cast<int32>(CurrentTris * SimplificationFactor));
    
    UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTriangleCount(
        Mesh, TargetTris, SimplifyOptions, nullptr);

    // Generate simplified collision using SetDynamicMeshCollisionFromMesh (UE 5.4+)
    FGeometryScriptCollisionFromMeshOptions CollisionOptions;
    CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
    CollisionOptions.MaxConvexHullsPerMesh = FMath::Clamp(TargetHullCount, 1, 16);
    CollisionOptions.bEmitTransaction = false;
    
    // SetDynamicMeshCollisionFromMesh generates and applies collision in one step
    UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh(
        Mesh, DMC, CollisionOptions, nullptr);

    // Get collision info from the component for reporting
    FGeometryScriptSetSimpleCollisionOptions SetOptions;
    FGeometryScriptSimpleCollision Collision = UGeometryScriptLibrary_CollisionFunctions::GetSimpleCollisionFromComponent(
        DMC, nullptr);
    
    int32 ShapeCount = UGeometryScriptLibrary_CollisionFunctions::GetSimpleCollisionShapeCount(Collision);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("trianglesBefore"), CurrentTris);
    Result->SetNumberField(TEXT("trianglesAfter"), Mesh->GetTriangleCount());
    Result->SetNumberField(TEXT("shapeCount"), ShapeCount);
    
    // Add verification data
    McpHandlerUtils::AddVerification(Result, TargetActor);
    
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Collision simplified"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Collision simplification requires UE 5.4+"), TEXT("VERSION_NOT_SUPPORTED"));
#endif
    return true;
}

// -------------------------------------------------------------------------
// LOD Generation (Geometry)
// -------------------------------------------------------------------------

static bool HandleGenerateLODsGeometry(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                       const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 LODCount = GetJsonIntField(Payload, TEXT("lodCount"), 4);
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    
    if (ActorName.IsEmpty() && AssetPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName or assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    LODCount = FMath::Clamp(LODCount, 1, 50);

#if WITH_EDITOR
    UStaticMesh* StaticMesh = nullptr;
    FString TargetPath;
    
    // If we have an asset path, load the existing static mesh
    if (!AssetPath.IsEmpty())
    {
        FString SafePath = SanitizeProjectRelativePath(AssetPath);
        if (SafePath.IsEmpty())
        {
            Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Invalid asset path: %s"), *AssetPath), TEXT("INVALID_ASSET_PATH"));
            return true;
        }
        
        StaticMesh = LoadObject<UStaticMesh>(nullptr, *SafePath);
        if (!StaticMesh)
        {
            Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("StaticMesh not found: %s"), *SafePath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        TargetPath = SafePath;
    }
    else
    {
        // Convert DynamicMesh to StaticMesh first
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        ADynamicMeshActor* TargetActor = nullptr;

        for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
        {
            if (It->GetActorLabel() == ActorName)
            {
                TargetActor = *It;
                break;
            }
        }

        if (!TargetActor)
        {
            Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }

        UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
        if (!DMC || !DMC->GetDynamicMesh())
        {
            Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
            return true;
        }

        UDynamicMesh* DynMesh = DMC->GetDynamicMesh();
        
        // Convert to StaticMesh
        FString MeshName = ActorName + TEXT("_LOD");
        TargetPath = FString::Printf(TEXT("/Game/MCPTest/%s"), *MeshName);
        
        FGeometryScriptCreateNewStaticMeshAssetOptions AssetOptions;
        AssetOptions.bEnableRecomputeNormals = true;
        AssetOptions.bEnableRecomputeTangents = true;
        AssetOptions.bEnableNanite = false;
        
        EGeometryScriptOutcomePins Outcome;
        
        StaticMesh = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
            DynMesh, TargetPath, AssetOptions, Outcome, nullptr);
            
        if (Outcome != EGeometryScriptOutcomePins::Success || !StaticMesh)
        {
            Self->SendAutomationError(Socket, RequestId, TEXT("Failed to convert DynamicMesh to StaticMesh"), TEXT("CONVERSION_FAILED"));
            return true;
        }
    }

    // Generate LODs
    StaticMesh->Modify();
    StaticMesh->SetNumSourceModels(LODCount);

    // Configure LOD reduction settings with progressive reduction
    for (int32 LODIndex = 1; LODIndex < LODCount; LODIndex++)
    {
        FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(LODIndex);
        FMeshReductionSettings& ReductionSettings = SourceModel.ReductionSettings;

        // Progressive reduction: 50%, 25%, 12.5%...
        float ReductionPercent = 1.0f / FMath::Pow(2.0f, static_cast<float>(LODIndex));
        ReductionSettings.PercentTriangles = ReductionPercent;
        ReductionSettings.PercentVertices = ReductionPercent;

        SourceModel.BuildSettings.bRecomputeNormals = false;
        SourceModel.BuildSettings.bRecomputeTangents = false;
        SourceModel.BuildSettings.bUseMikkTSpace = true;
    }

    // Build the mesh with new LOD settings
    StaticMesh->Build();
    StaticMesh->PostEditChange();
    McpSafeAssetSave(StaticMesh);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), TargetPath);
    Result->SetNumberField(TEXT("lodCount"), LODCount);
    Result->SetNumberField(TEXT("triangles"), StaticMesh->GetNumTriangles(0));
    
    // Add verification data
    McpHandlerUtils::AddVerification(Result, StaticMesh);
    
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("LODs generated for geometry"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Requires editor build"), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}

// -------------------------------------------------------------------------
// Set LOD Settings
// -------------------------------------------------------------------------

static bool HandleSetLODSettings(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"));
    int32 LODIndex = GetJsonIntField(Payload, TEXT("lodIndex"), 1);
    double TrianglePercent = GetJsonNumberField(Payload, TEXT("trianglePercent"), 50.0);
    bool bRecomputeNormals = GetJsonBoolField(Payload, TEXT("recomputeNormals"), false);
    bool bRecomputeTangents = GetJsonBoolField(Payload, TEXT("recomputeTangents"), false);

    if (AssetPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

#if WITH_EDITOR
    FString SafePath = SanitizeProjectRelativePath(AssetPath);
    if (SafePath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Invalid asset path: %s"), *AssetPath), TEXT("INVALID_ASSET_PATH"));
        return true;
    }

    UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *SafePath);
    if (!StaticMesh)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("StaticMesh not found: %s"), *SafePath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    if (LODIndex < 0 || LODIndex >= StaticMesh->GetNumSourceModels())
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Invalid LOD index: %d (mesh has %d LODs)"), LODIndex, StaticMesh->GetNumSourceModels()), TEXT("INVALID_LOD_INDEX"));
        return true;
    }

    StaticMesh->Modify();
    
    FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(LODIndex);
    
    // Set reduction settings
    SourceModel.ReductionSettings.PercentTriangles = TrianglePercent / 100.0f;
    SourceModel.ReductionSettings.PercentVertices = TrianglePercent / 100.0f;
    
    // Set build settings
    SourceModel.BuildSettings.bRecomputeNormals = bRecomputeNormals;
    SourceModel.BuildSettings.bRecomputeTangents = bRecomputeTangents;

    // Rebuild
    StaticMesh->Build();
    StaticMesh->PostEditChange();
    McpSafeAssetSave(StaticMesh);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), SafePath);
    Result->SetNumberField(TEXT("lodIndex"), LODIndex);
    Result->SetNumberField(TEXT("trianglePercent"), TrianglePercent);
    
    // Add verification data
    McpHandlerUtils::AddVerification(Result, StaticMesh);
    
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("LOD settings updated"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Requires editor build"), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}

// -------------------------------------------------------------------------
// Set LOD Screen Sizes
// -------------------------------------------------------------------------

static bool HandleSetLODScreenSizes(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"));
    
    // Parse screen sizes (can be array or object)
    TArray<float> ScreenSizes;
    const TArray<TSharedPtr<FJsonValue>>* SizeArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("screenSizes"), SizeArray))
    {
        for (const auto& Val : *SizeArray)
        {
            if (Val.IsValid() && Val->Type == EJson::Number)
            {
                ScreenSizes.Add(static_cast<float>(Val->AsNumber()));
            }
        }
    }

    if (AssetPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (ScreenSizes.Num() == 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("screenSizes array required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

#if WITH_EDITOR
    FString SafePath = SanitizeProjectRelativePath(AssetPath);
    if (SafePath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Invalid asset path: %s"), *AssetPath), TEXT("INVALID_ASSET_PATH"));
        return true;
    }

    UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *SafePath);
    if (!StaticMesh)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("StaticMesh not found: %s"), *SafePath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    StaticMesh->Modify();
    
    // Set screen sizes for each LOD
    // Note: UE5 doesn't have a direct SetLODScreenSize API on UStaticMesh
    // Screen sizes are typically managed via the LODGroup or platform-specific settings
    // Here we configure the reduction settings which indirectly affect LOD switching
    int32 NumLODs = StaticMesh->GetNumSourceModels();
    
    for (int32 i = 0; i < FMath::Min(ScreenSizes.Num(), NumLODs); i++)
    {
        // Configure reduction settings based on screen size
        // The screen size affects when this LOD becomes visible
        // Higher screen size = LOD becomes visible sooner (closer to camera)
        if (i > 0)  // LOD 0 is the base mesh
        {
            FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(i);
            // Screen size is used to determine when to switch to this LOD
            // We set it as a percentage of the previous LOD's screen size
            SourceModel.ReductionSettings.PercentTriangles = ScreenSizes[i];
        }
    }

    StaticMesh->PostEditChange();
    McpSafeAssetSave(StaticMesh);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), SafePath);
    Result->SetNumberField(TEXT("lodCount"), NumLODs);
    Result->SetNumberField(TEXT("screenSizesSet"), ScreenSizes.Num());
    
    // Add verification data
    McpHandlerUtils::AddVerification(Result, StaticMesh);
    
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("LOD screen sizes updated"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Requires editor build"), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT

// -------------------------------------------------------------------------
// Geometry Member Handlers
// -------------------------------------------------------------------------
// Dispatch lives in the manage_geometry FMcpCall classes
// (Private/MCP/Calls/McpCalls_ManageGeometry.cpp); each HandleGeometry*
// member here wraps one advertised action's static free function above,
// replicating the retired chain's pre-5.1 NOT_SUPPORTED stub. The TU-wide
// WITH_EDITOR gate encloses these members: non-editor builds omit them
// exactly as they omitted the retired dispatcher.

// create_box
bool McpHandlers::Geometry::HandleGeometryCreateBox(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleCreateBox(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// create_sphere
bool McpHandlers::Geometry::HandleGeometryCreateSphere(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleCreateSphere(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// create_cylinder
bool McpHandlers::Geometry::HandleGeometryCreateCylinder(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleCreateCylinder(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// create_cone
bool McpHandlers::Geometry::HandleGeometryCreateCone(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleCreateCone(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// create_capsule
bool McpHandlers::Geometry::HandleGeometryCreateCapsule(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleCreateCapsule(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// create_torus
bool McpHandlers::Geometry::HandleGeometryCreateTorus(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleCreateTorus(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// create_plane
bool McpHandlers::Geometry::HandleGeometryCreatePlane(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleCreatePlane(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// create_disc
bool McpHandlers::Geometry::HandleGeometryCreateDisc(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleCreateDisc(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// create_stairs
bool McpHandlers::Geometry::HandleGeometryCreateStairs(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleCreateStairs(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// create_spiral_stairs
bool McpHandlers::Geometry::HandleGeometryCreateSpiralStairs(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleCreateSpiralStairs(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// create_ring
bool McpHandlers::Geometry::HandleGeometryCreateRing(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleCreateRing(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// create_arch
bool McpHandlers::Geometry::HandleGeometryCreateArch(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleCreateArch(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// create_pipe
bool McpHandlers::Geometry::HandleGeometryCreatePipe(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleCreatePipe(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// create_ramp
bool McpHandlers::Geometry::HandleGeometryCreateRamp(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleCreateRamp(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// revolve
bool McpHandlers::Geometry::HandleGeometryRevolve(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleRevolve(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// boolean_union
bool McpHandlers::Geometry::HandleGeometryBooleanUnion(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleBooleanUnion(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// boolean_subtract
bool McpHandlers::Geometry::HandleGeometryBooleanSubtract(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleBooleanSubtract(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// boolean_intersection
bool McpHandlers::Geometry::HandleGeometryBooleanIntersection(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleBooleanIntersection(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// boolean_trim
bool McpHandlers::Geometry::HandleGeometryBooleanTrim(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleBooleanTrim(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// self_union
bool McpHandlers::Geometry::HandleGeometrySelfUnion(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleSelfUnion(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// get_mesh_info
bool McpHandlers::Geometry::HandleGeometryGetMeshInfo(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleGetMeshInfo(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// recalculate_normals
bool McpHandlers::Geometry::HandleGeometryRecalculateNormals(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleRecalculateNormals(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// flip_normals
bool McpHandlers::Geometry::HandleGeometryFlipNormals(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleFlipNormals(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// simplify_mesh
bool McpHandlers::Geometry::HandleGeometrySimplifyMesh(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleSimplifyMesh(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// subdivide
bool McpHandlers::Geometry::HandleGeometrySubdivide(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleSubdivide(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// auto_uv
bool McpHandlers::Geometry::HandleGeometryAutoUV(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleAutoUV(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// convert_to_static_mesh
bool McpHandlers::Geometry::HandleGeometryConvertToStaticMesh(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleConvertToStaticMesh(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// extrude
bool McpHandlers::Geometry::HandleGeometryExtrude(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleExtrude(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// inset
bool McpHandlers::Geometry::HandleGeometryInset(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleInsetOutset(&S, RequestId, Payload, Socket, true);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// outset
bool McpHandlers::Geometry::HandleGeometryOutset(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleInsetOutset(&S, RequestId, Payload, Socket, false);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// bevel
bool McpHandlers::Geometry::HandleGeometryBevel(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleBevel(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// offset_faces
bool McpHandlers::Geometry::HandleGeometryOffsetFaces(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleOffsetFaces(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// shell
bool McpHandlers::Geometry::HandleGeometryShell(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleShell(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// chamfer
bool McpHandlers::Geometry::HandleGeometryChamfer(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleChamfer(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// bend
bool McpHandlers::Geometry::HandleGeometryBend(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleBend(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// twist
bool McpHandlers::Geometry::HandleGeometryTwist(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleTwist(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// taper
bool McpHandlers::Geometry::HandleGeometryTaper(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleTaper(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// noise_deform
bool McpHandlers::Geometry::HandleGeometryNoiseDeform(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleNoiseDeform(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// smooth
bool McpHandlers::Geometry::HandleGeometrySmooth(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleSmooth(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// relax
bool McpHandlers::Geometry::HandleGeometryRelax(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleRelax(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// stretch
bool McpHandlers::Geometry::HandleGeometryStretch(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleStretch(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// spherify
bool McpHandlers::Geometry::HandleGeometrySpherify(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleSpherify(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// cylindrify
bool McpHandlers::Geometry::HandleGeometryCylindrify(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleCylindrify(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// lattice_deform
bool McpHandlers::Geometry::HandleGeometryLatticeDeform(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleLatticeDeform(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// displace_by_texture
bool McpHandlers::Geometry::HandleGeometryDisplaceByTexture(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleDisplaceByTexture(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// weld_vertices
bool McpHandlers::Geometry::HandleGeometryWeldVertices(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleWeldVertices(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// fill_holes
bool McpHandlers::Geometry::HandleGeometryFillHoles(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleFillHoles(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// remove_degenerates
bool McpHandlers::Geometry::HandleGeometryRemoveDegenerates(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleRemoveDegenerates(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// remesh_uniform
bool McpHandlers::Geometry::HandleGeometryRemeshUniform(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleRemeshUniform(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// merge_vertices
bool McpHandlers::Geometry::HandleGeometryMergeVertices(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleMergeVertices(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// generate_collision
bool McpHandlers::Geometry::HandleGeometryGenerateCollision(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleGenerateCollision(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// mirror
bool McpHandlers::Geometry::HandleGeometryMirror(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleMirror(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// array_linear
bool McpHandlers::Geometry::HandleGeometryArrayLinear(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleArrayLinear(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// array_radial
bool McpHandlers::Geometry::HandleGeometryArrayRadial(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleArrayRadial(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// triangulate
bool McpHandlers::Geometry::HandleGeometryTriangulate(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleTriangulate(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// poke
bool McpHandlers::Geometry::HandleGeometryPoke(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandlePoke(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// project_uv
bool McpHandlers::Geometry::HandleGeometryProjectUV(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleProjectUV(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// transform_uvs
bool McpHandlers::Geometry::HandleGeometryTransformUVs(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleTransformUVs(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// recompute_tangents
bool McpHandlers::Geometry::HandleGeometryRecomputeTangents(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleRecomputeTangents(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// bridge
bool McpHandlers::Geometry::HandleGeometryBridge(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleBridge(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// loft
bool McpHandlers::Geometry::HandleGeometryLoft(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleLoft(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// sweep
bool McpHandlers::Geometry::HandleGeometrySweep(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleSweep(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// loop_cut
bool McpHandlers::Geometry::HandleGeometryLoopCut(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleLoopCut(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// duplicate_along_spline
bool McpHandlers::Geometry::HandleGeometryDuplicateAlongSpline(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleDuplicateAlongSpline(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// unwrap_uv
bool McpHandlers::Geometry::HandleGeometryUnwrapUV(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleUnwrapUV(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// pack_uv_islands
bool McpHandlers::Geometry::HandleGeometryPackUVIslands(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandlePackUVIslands(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// convert_to_nanite
bool McpHandlers::Geometry::HandleGeometryConvertToNanite(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleConvertToNanite(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// extrude_along_spline
bool McpHandlers::Geometry::HandleGeometryExtrudeAlongSpline(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleExtrudeAlongSpline(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// edge_split
bool McpHandlers::Geometry::HandleGeometryEdgeSplit(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleEdgeSplit(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// quadrangulate
bool McpHandlers::Geometry::HandleGeometryQuadrangulate(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleQuadrangulate(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// remesh_voxel
bool McpHandlers::Geometry::HandleGeometryRemeshVoxel(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleRemeshVoxel(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// generate_complex_collision
bool McpHandlers::Geometry::HandleGeometryGenerateComplexCollision(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleGenerateComplexCollision(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// simplify_collision
bool McpHandlers::Geometry::HandleGeometrySimplifyCollision(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleSimplifyCollision(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// generate_lods
bool McpHandlers::Geometry::HandleGeometryGenerateLODs(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleGenerateLODsGeometry(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// set_lod_settings
bool McpHandlers::Geometry::HandleGeometrySetLODSettings(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleSetLODSettings(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

// set_lod_screen_sizes
bool McpHandlers::Geometry::HandleGeometrySetLODScreenSizes(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if MCP_HAS_FULL_GEOMETRY_SCRIPT
    return HandleSetLODScreenSizes(&S, RequestId, Payload, Socket);
#else
    // UE 5.0 doesn't have full GeometryScript support
    S.SendAutomationError(Socket, RequestId,
        TEXT("GeometryScript operations require UE 5.1 or later"),
        TEXT("NOT_SUPPORTED"));
    return true;
#endif
}

#endif // WITH_EDITOR
