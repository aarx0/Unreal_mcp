// =============================================================================
// McpAutomationBridge_EffectHandlers.cpp
// =============================================================================
// manage_effect member handlers. Dispatch lives in the FMcpCall classes
// (Private/MCP/Calls/McpCalls_ManageEffect.cpp); each HandleEffect* member
// here implements one advertised action. create_particle_trail/
// create_environment_effect/create_impact_effect/create_niagara_ribbon share
// CreateNiagaraEffect. The Niagara authoring actions are per-action
// HandleNiagara* members (NiagaraAuthoringHandlers.cpp) and the three graph
// actions go through HandleNiagaraGraphAction (NiagaraGraphHandlers.cpp).
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0-5.7: All handlers supported
// - Niagara system conditional includes via __has_include
// - Debug drawing APIs stable
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "DrawDebugHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_EffectHandlers.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#if __has_include("NiagaraActor.h")
#include "NiagaraActor.h"
#endif
#if __has_include("NiagaraComponent.h")
#include "NiagaraComponent.h"
#endif
#if __has_include("NiagaraSystem.h")
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#endif
#if __has_include("NiagaraEmitter.h")
#include "NiagaraEmitter.h"
#endif
#if __has_include("NiagaraScript.h")
#include "NiagaraScript.h"
#endif
#if __has_include("NiagaraDataInterface.h")
#include "NiagaraDataInterface.h"
#endif
#if __has_include("NiagaraSimulationStageBase.h")
#include "NiagaraSimulationStageBase.h"
#endif
#if __has_include("NiagaraRendererProperties.h")
#include "NiagaraRendererProperties.h"
#endif
#if __has_include("Engine/PointLight.h")
#include "Engine/PointLight.h"
#endif
#if __has_include("Engine/SpotLight.h")
#include "Engine/SpotLight.h"
#endif
#if __has_include("Engine/DirectionalLight.h")
#include "Engine/DirectionalLight.h"
#endif
#if __has_include("Engine/RectLight.h")
#include "Engine/RectLight.h"
#endif
#if __has_include("Components/LightComponent.h")
#include "Components/LightComponent.h"
#endif
#if __has_include("Components/PointLightComponent.h")
#include "Components/PointLightComponent.h"
#endif
#if __has_include("Components/SpotLightComponent.h")
#include "Components/SpotLightComponent.h"
#endif
#if __has_include("Components/RectLightComponent.h")
#include "Components/RectLightComponent.h"
#endif
#if __has_include("Components/DirectionalLightComponent.h")
#include "Components/DirectionalLightComponent.h"
#endif
// Volumetric Fog
#if __has_include("Engine/ExponentialHeightFog.h")
#include "Engine/ExponentialHeightFog.h"
#endif
#if __has_include("Components/ExponentialHeightFogComponent.h")
#include "Components/ExponentialHeightFogComponent.h"
#endif
// Cascade Particle Systems
#if __has_include("Particles/ParticleSystem.h")
#include "Particles/ParticleSystem.h"
#endif
#if __has_include("Particles/ParticleSystemComponent.h")
#include "Particles/ParticleSystemComponent.h"
#endif
#if __has_include("Particles/Emitter.h")
#include "Particles/Emitter.h"
#endif
#endif

namespace {
struct FMcpActiveDebugShape {
  FString ShapeType;
  FVector Location = FVector::ZeroVector;
  double ExpiresAtSeconds = 0.0;
};

// Shapes drawn through this handler; the engine expires them by duration, so
// the mirror prunes by the same clock. Game-thread only, like all handlers.
TArray<FMcpActiveDebugShape> GMcpActiveDebugShapes;

void McpPruneExpiredDebugShapes() {
  const double Now = FPlatformTime::Seconds();
  GMcpActiveDebugShapes.RemoveAll([Now](const FMcpActiveDebugShape &Shape) {
    return Shape.ExpiresAtSeconds <= Now;
  });
}

void McpRecordDebugShape(const FString &ShapeType, const FVector &Location,
                         float Duration) {
  McpPruneExpiredDebugShapes();
  FMcpActiveDebugShape Shape;
  Shape.ShapeType = ShapeType;
  Shape.Location = Location;
  Shape.ExpiresAtSeconds = FPlatformTime::Seconds() + Duration;
  GMcpActiveDebugShapes.Add(Shape);
}
} // namespace

// Active drawn shapes; the catalog of drawable types rides along under
// supportedShapes.
bool McpHandlers::Effect::HandleEffectListDebugShapes(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  TArray<TSharedPtr<FJsonValue>> SupportedShapes;
  SupportedShapes.Add(MakeShared<FJsonValueString>(TEXT("sphere")));
  SupportedShapes.Add(MakeShared<FJsonValueString>(TEXT("box")));
  SupportedShapes.Add(MakeShared<FJsonValueString>(TEXT("circle")));
  SupportedShapes.Add(MakeShared<FJsonValueString>(TEXT("line")));
  SupportedShapes.Add(MakeShared<FJsonValueString>(TEXT("point")));
  SupportedShapes.Add(MakeShared<FJsonValueString>(TEXT("coordinate")));
  SupportedShapes.Add(MakeShared<FJsonValueString>(TEXT("cylinder")));
  SupportedShapes.Add(MakeShared<FJsonValueString>(TEXT("cone")));
  SupportedShapes.Add(MakeShared<FJsonValueString>(TEXT("capsule")));
  SupportedShapes.Add(MakeShared<FJsonValueString>(TEXT("arrow")));
  SupportedShapes.Add(MakeShared<FJsonValueString>(TEXT("plane")));

  McpPruneExpiredDebugShapes();
  const double Now = FPlatformTime::Seconds();
  TArray<TSharedPtr<FJsonValue>> ActiveShapes;
  for (const FMcpActiveDebugShape &Shape : GMcpActiveDebugShapes) {
    TSharedPtr<FJsonObject> ShapeObj = MakeShared<FJsonObject>();
    ShapeObj->SetStringField(TEXT("shapeType"), Shape.ShapeType);
    ShapeObj->SetStringField(
        TEXT("location"),
        FString::Printf(TEXT("%.2f,%.2f,%.2f"), Shape.Location.X,
                        Shape.Location.Y, Shape.Location.Z));
    ShapeObj->SetNumberField(TEXT("secondsRemaining"),
                             Shape.ExpiresAtSeconds - Now);
    ActiveShapes.Add(MakeShared<FJsonValueObject>(ShapeObj));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetArrayField(TEXT("shapes"), ActiveShapes);
  Resp->SetNumberField(TEXT("count"), ActiveShapes.Num());
  Resp->SetArrayField(TEXT("supportedShapes"), SupportedShapes);
  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Active debug shapes"), Resp);
  return true;
}

bool McpHandlers::Effect::HandleEffectClearDebugShapes(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
#if WITH_EDITOR
  if (GEditor && GEditor->GetEditorWorldContext().World()) {
    FlushPersistentDebugLines(GEditor->GetEditorWorldContext().World());
    GMcpActiveDebugShapes.Empty();
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Debug shapes cleared"), Resp);
    return true;
  } else {
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor world not available"), nullptr,
                           TEXT("NO_WORLD"));
    return true;
  }
#else
  S.SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Debug shape clearing requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::Effect::HandleEffectDebugShape(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  // shapeType is required
  FString ShapeType = TEXT("sphere");
  Payload->TryGetStringField(TEXT("shapeType"), ShapeType);
  // Also accept 'shape' as alias
  if (ShapeType.Equals(TEXT("sphere"), ESearchCase::IgnoreCase) && Payload->HasField(TEXT("shape"))) {
    Payload->TryGetStringField(TEXT("shape"), ShapeType);
  }

  // Location is required for debug shapes
  if (!Payload->HasField(TEXT("location"))) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"), TEXT("location parameter is required for debug_shape"));
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Missing required parameter: location"), Resp,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Parse location
  FVector Loc(0, 0, 0);
  const TSharedPtr<FJsonValue> LocVal = Payload->TryGetField(TEXT("location"));
  if (LocVal.IsValid()) {
    if (LocVal->Type == EJson::Array) {
      const TArray<TSharedPtr<FJsonValue>> &Arr = LocVal->AsArray();
      if (Arr.Num() >= 3)
        Loc = FVector((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(), (float)Arr[2]->AsNumber());
    } else if (LocVal->Type == EJson::Object) {
      const TSharedPtr<FJsonObject> O = LocVal->AsObject();
      if (O.IsValid())
        Loc = FVector(
            (float)(O->HasField(TEXT("x")) ? GetJsonNumberField(O, TEXT("x")) : 0.0),
            (float)(O->HasField(TEXT("y")) ? GetJsonNumberField(O, TEXT("y")) : 0.0),
            (float)(O->HasField(TEXT("z")) ? GetJsonNumberField(O, TEXT("z")) : 0.0));
    }
  }

  // Duration (default: 5.0 seconds)
  const float Duration = Payload->HasField(TEXT("duration"))
                             ? (float)GetJsonNumberField(Payload, TEXT("duration"))
                             : 5.0f;

  // Size/Radius (default: 100.0)
  const float Size = Payload->HasField(TEXT("radius"))
                         ? (float)GetJsonNumberField(Payload, TEXT("radius"))
                         : (Payload->HasField(TEXT("size"))
                                ? (float)GetJsonNumberField(Payload, TEXT("size"))
                                : 100.0f);

  // Thickness for lines (default: 2.0)
  const float Thickness = Payload->HasField(TEXT("thickness"))
                              ? (float)GetJsonNumberField(Payload, TEXT("thickness"))
                              : 2.0f;

  // Color (default: white)
  TArray<double> ColorArr = {255, 255, 255, 255};
  const TArray<TSharedPtr<FJsonValue>> *ColorJsonArr = nullptr;
  if (Payload->TryGetArrayField(TEXT("color"), ColorJsonArr) && ColorJsonArr && ColorJsonArr->Num() >= 3) {
    ColorArr[0] = (*ColorJsonArr)[0]->AsNumber();
    ColorArr[1] = (*ColorJsonArr)[1]->AsNumber();
    ColorArr[2] = (*ColorJsonArr)[2]->AsNumber();
    if (ColorJsonArr->Num() >= 4) {
      ColorArr[3] = (*ColorJsonArr)[3]->AsNumber();
    }
  }

#if WITH_EDITOR
  if (!GEditor) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"), TEXT("Editor not available for debug drawing"));
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), Resp,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();
  if (!World) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"), TEXT("No world available for debug drawing"));
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No world available"), Resp,
                           TEXT("NO_WORLD"));
    return true;
  }

  const FColor DebugColor((uint8)ColorArr[0], (uint8)ColorArr[1], (uint8)ColorArr[2], (uint8)ColorArr[3]);
  const FString LowerShapeType = ShapeType.ToLower();

  if (LowerShapeType == TEXT("sphere")) {
    DrawDebugSphere(World, Loc, Size, 16, DebugColor, false, Duration, 0, Thickness);
  } else if (LowerShapeType == TEXT("box")) {
    DrawDebugBox(World, Loc, FVector(Size), FRotator::ZeroRotator.Quaternion(), DebugColor, false, Duration, 0, Thickness);
  } else if (LowerShapeType == TEXT("circle")) {
    DrawDebugCircle(World, Loc, Size, 32, DebugColor, false, Duration, 0, Thickness, FVector::UpVector);
  } else if (LowerShapeType == TEXT("line")) {
    FVector EndLoc = Loc + FVector(100, 0, 0);
    if (Payload->HasField(TEXT("endLocation"))) {
      const TSharedPtr<FJsonValue> EndVal = Payload->TryGetField(TEXT("endLocation"));
      if (EndVal.IsValid() && EndVal->Type == EJson::Array) {
        const TArray<TSharedPtr<FJsonValue>> &Arr = EndVal->AsArray();
        if (Arr.Num() >= 3)
          EndLoc = FVector((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(), (float)Arr[2]->AsNumber());
      } else if (EndVal.IsValid() && EndVal->Type == EJson::Object) {
        const TSharedPtr<FJsonObject> O = EndVal->AsObject();
        if (O.IsValid())
          EndLoc = FVector(
              (float)(O->HasField(TEXT("x")) ? GetJsonNumberField(O, TEXT("x")) : 0.0),
              (float)(O->HasField(TEXT("y")) ? GetJsonNumberField(O, TEXT("y")) : 0.0),
              (float)(O->HasField(TEXT("z")) ? GetJsonNumberField(O, TEXT("z")) : 0.0));
      }
    }
    DrawDebugLine(World, Loc, EndLoc, DebugColor, false, Duration, 0, Thickness);
  } else if (LowerShapeType == TEXT("point")) {
    DrawDebugPoint(World, Loc, Size, DebugColor, false, Duration);
  } else if (LowerShapeType == TEXT("arrow")) {
    FVector EndLoc = Loc + FVector(100, 0, 0);
    DrawDebugDirectionalArrow(World, Loc, EndLoc, Size > 0 ? Size : 10.0f, DebugColor, false, Duration, 0, Thickness);
  } else if (LowerShapeType == TEXT("capsule")) {
    DrawDebugCapsule(World, Loc, Size, Size, FQuat::Identity, DebugColor, false, Duration, 0, Thickness);
  } else if (LowerShapeType == TEXT("cylinder")) {
    DrawDebugCylinder(World, Loc, Loc + FVector(0, 0, Size * 2), Size, 16, DebugColor, false, Duration, 0, Thickness);
  } else if (LowerShapeType == TEXT("cone")) {
    DrawDebugCone(World, Loc, FVector::UpVector, Size * 2, FMath::DegreesToRadians(45.0f), FMath::DegreesToRadians(45.0f), 16, DebugColor, false, Duration, 0, Thickness);
  } else if (LowerShapeType == TEXT("coordinate")) {
    DrawDebugCoordinateSystem(World, Loc, FRotator::ZeroRotator, Size, false, Duration, 0, Thickness);
  } else if (LowerShapeType == TEXT("plane")) {
    DrawDebugBox(World, Loc, FVector(Size, Size, 1.0f), FQuat::Identity, DebugColor, false, Duration, 0, Thickness);
  } else {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"), FString::Printf(TEXT("Unsupported shape type: %s"), *ShapeType));
    Resp->SetStringField(TEXT("supportedShapes"), TEXT("sphere, box, circle, line, point, arrow, capsule, cylinder, cone, coordinate, plane"));
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Unsupported shape type"), Resp,
                           TEXT("UNSUPPORTED_SHAPE"));
    return true;
  }

  McpRecordDebugShape(LowerShapeType, Loc, Duration);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("shapeType"), ShapeType);
  Resp->SetStringField(TEXT("location"), FString::Printf(TEXT("%.2f,%.2f,%.2f"), Loc.X, Loc.Y, Loc.Z));
  Resp->SetNumberField(TEXT("duration"), Duration);
  S.SendAutomationResponse(Socket, RequestId, true, TEXT("Debug shape drawn"), Resp, FString());
  return true;
#else
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), false);
  Resp->SetStringField(TEXT("error"), TEXT("Debug shape drawing requires editor build"));
  S.SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Debug shape drawing not available in non-editor build"), Resp,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool McpHandlers::Effect::HandleEffectParticle(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString Preset;
  Payload->TryGetStringField(TEXT("preset"), Preset);
  if (Preset.IsEmpty()) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(
        TEXT("error"),
        TEXT("preset parameter required for particle spawning"));
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Preset path required"), Resp,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Location and optional rotation/scale
  FVector Loc(0, 0, 0);
  if (Payload->HasField(TEXT("location"))) {
    const TSharedPtr<FJsonValue> LocVal =
        Payload->TryGetField(TEXT("location"));
    if (LocVal.IsValid()) {
      if (LocVal->Type == EJson::Array) {
        const TArray<TSharedPtr<FJsonValue>> &Arr = LocVal->AsArray();
        if (Arr.Num() >= 3)
          Loc =
              FVector((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(),
                      (float)Arr[2]->AsNumber());
      } else if (LocVal->Type == EJson::Object) {
        const TSharedPtr<FJsonObject> O = LocVal->AsObject();
        if (O.IsValid())
          Loc = FVector(
              (float)(O->HasField(TEXT("x")) ? GetJsonNumberField(O, TEXT("x"))
                                             : 0.0),
              (float)(O->HasField(TEXT("y")) ? GetJsonNumberField(O, TEXT("y"))
                                             : 0.0),
              (float)(O->HasField(TEXT("z")) ? GetJsonNumberField(O, TEXT("z"))
                                             : 0.0));
      }
    }
  }

  // Rotation may be an array
  TArray<double> RotArr = {0, 0, 0};
  const TArray<TSharedPtr<FJsonValue>> *RA = nullptr;
  if (Payload->TryGetArrayField(TEXT("rotation"), RA) && RA &&
      RA->Num() >= 3) {
    RotArr[0] = (*RA)[0]->AsNumber();
    RotArr[1] = (*RA)[1]->AsNumber();
    RotArr[2] = (*RA)[2]->AsNumber();
  }

  // Scale may be an array or a single numeric value
  TArray<double> ScaleArr = {1, 1, 1};
  const TArray<TSharedPtr<FJsonValue>> *ScaleJsonArr = nullptr;
  if (Payload->TryGetArrayField(TEXT("scale"), ScaleJsonArr) &&
      ScaleJsonArr && ScaleJsonArr->Num() >= 3) {
    ScaleArr[0] = (*ScaleJsonArr)[0]->AsNumber();
    ScaleArr[1] = (*ScaleJsonArr)[1]->AsNumber();
    ScaleArr[2] = (*ScaleJsonArr)[2]->AsNumber();
  } else if (Payload->TryGetNumberField(TEXT("scale"), ScaleArr[0])) {
    ScaleArr[1] = ScaleArr[2] = ScaleArr[0];
  }

  const bool bAutoDestroy =
      Payload->HasField(TEXT("autoDestroy"))
          ? GetJsonBoolField(Payload, TEXT("autoDestroy"))
          : false;

  // Duration (default: 5.0 seconds)
  const float Duration =
      Payload->HasField(TEXT("duration"))
          ? (float)GetJsonNumberField(Payload, TEXT("duration"))
          : 5.0f;

  // Size/Radius (default: 100.0)
  const float Size = Payload->HasField(TEXT("size"))
                         ? (float)GetJsonNumberField(Payload, TEXT("size"))
                         : 100.0f;

  // Thickness for lines (default: 2.0)
  const float Thickness =
      Payload->HasField(TEXT("thickness"))
          ? (float)GetJsonNumberField(Payload, TEXT("thickness"))
          : 2.0f;

  // Extract Color and ShapeType for debug drawing
  TArray<double> ColorArr = {255, 255, 255, 255};
  const TArray<TSharedPtr<FJsonValue>> *ColorJsonArr = nullptr;
  if (Payload->TryGetArrayField(TEXT("color"), ColorJsonArr) &&
      ColorJsonArr && ColorJsonArr->Num() >= 3) {
    ColorArr[0] = (*ColorJsonArr)[0]->AsNumber();
    ColorArr[1] = (*ColorJsonArr)[1]->AsNumber();
    ColorArr[2] = (*ColorJsonArr)[2]->AsNumber();
    if (ColorJsonArr->Num() >= 4) {
      ColorArr[3] = (*ColorJsonArr)[3]->AsNumber();
    }
  }

  FString ShapeType = TEXT("sphere");
  Payload->TryGetStringField(TEXT("shapeType"), ShapeType);

#if WITH_EDITOR
  if (!GEditor) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"),
                         TEXT("Editor not available for debug drawing"));
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), Resp,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Get the current world for debug drawing
  UWorld *World = GEditor->GetEditorWorldContext().World();
  if (!World) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"),
                         TEXT("No world available for debug drawing"));
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No world available"), Resp,
                           TEXT("NO_WORLD"));
    return true;
  }

  const FColor DebugColor((uint8)ColorArr[0], (uint8)ColorArr[1],
                          (uint8)ColorArr[2], (uint8)ColorArr[3]);
  const FString LowerShapeType = ShapeType.ToLower();

  if (LowerShapeType == TEXT("sphere")) {
    DrawDebugSphere(World, Loc, Size, 16, DebugColor, false, Duration, 0,
                    Thickness);
  } else if (LowerShapeType == TEXT("box")) {
    FVector BoxSize = FVector(Size);
    if (Payload->HasField(TEXT("boxSize"))) {
      const TArray<TSharedPtr<FJsonValue>> *BoxSizeArr = nullptr;
      if (Payload->TryGetArrayField(TEXT("boxSize"), BoxSizeArr) &&
          BoxSizeArr && BoxSizeArr->Num() >= 3) {
        BoxSize = FVector((float)(*BoxSizeArr)[0]->AsNumber(),
                          (float)(*BoxSizeArr)[1]->AsNumber(),
                          (float)(*BoxSizeArr)[2]->AsNumber());
      }
    }
    DrawDebugBox(World, Loc, BoxSize, FRotator::ZeroRotator.Quaternion(),
                 DebugColor, false, Duration, 0, Thickness);
  } else if (LowerShapeType == TEXT("circle")) {
    DrawDebugCircle(World, Loc, Size, 32, DebugColor, false, Duration, 0,
                    Thickness, FVector::UpVector);
  } else if (LowerShapeType == TEXT("line")) {
    FVector EndLoc = Loc + FVector(100, 0, 0);
    if (Payload->HasField(TEXT("endLocation"))) {
      const TSharedPtr<FJsonValue> EndVal =
          Payload->TryGetField(TEXT("endLocation"));
      if (EndVal.IsValid()) {
        if (EndVal->Type == EJson::Array) {
          const TArray<TSharedPtr<FJsonValue>> &Arr = EndVal->AsArray();
          if (Arr.Num() >= 3)
            EndLoc = FVector((float)Arr[0]->AsNumber(),
                             (float)Arr[1]->AsNumber(),
                             (float)Arr[2]->AsNumber());
        } else if (EndVal->Type == EJson::Object) {
          const TSharedPtr<FJsonObject> O = EndVal->AsObject();
          if (O.IsValid())
            EndLoc = FVector((float)(O->HasField(TEXT("x"))
                                         ? GetJsonNumberField(O, TEXT("x"))
                                         : 0.0),
                             (float)(O->HasField(TEXT("y"))
                                         ? GetJsonNumberField(O, TEXT("y"))
                                         : 0.0),
                             (float)(O->HasField(TEXT("z"))
                                         ? GetJsonNumberField(O, TEXT("z"))
                                         : 0.0));
        }
      }
    }
    DrawDebugLine(World, Loc, EndLoc, DebugColor, false, Duration, 0,
                  Thickness);
  } else if (LowerShapeType == TEXT("point")) {
    DrawDebugPoint(World, Loc, Size, DebugColor, false, Duration);
  } else if (LowerShapeType == TEXT("coordinate")) {
    FRotator Rot = FRotator::ZeroRotator;
    if (Payload->HasField(TEXT("rotation"))) {
      const TArray<TSharedPtr<FJsonValue>> *RotJsonArr = nullptr;
      if (Payload->TryGetArrayField(TEXT("rotation"), RotJsonArr) &&
          RotJsonArr && RotJsonArr->Num() >= 3) {
        Rot = FRotator((float)(*RotJsonArr)[0]->AsNumber(),
                       (float)(*RotJsonArr)[1]->AsNumber(),
                       (float)(*RotJsonArr)[2]->AsNumber());
      }
    }
    DrawDebugCoordinateSystem(World, Loc, Rot, Size, false, Duration, 0,
                              Thickness);
  } else if (LowerShapeType == TEXT("cylinder")) {
    FVector EndLoc = Loc + FVector(0, 0, 100);
    if (Payload->HasField(TEXT("endLocation"))) {
      const TSharedPtr<FJsonValue> EndVal =
          Payload->TryGetField(TEXT("endLocation"));
      if (EndVal.IsValid()) {
        if (EndVal->Type == EJson::Array) {
          const TArray<TSharedPtr<FJsonValue>> &Arr = EndVal->AsArray();
          if (Arr.Num() >= 3)
            EndLoc = FVector((float)Arr[0]->AsNumber(),
                             (float)Arr[1]->AsNumber(),
                             (float)Arr[2]->AsNumber());
        } else if (EndVal->Type == EJson::Object) {
          const TSharedPtr<FJsonObject> O = EndVal->AsObject();
          if (O.IsValid())
            EndLoc = FVector((float)(O->HasField(TEXT("x"))
                                         ? GetJsonNumberField(O, TEXT("x"))
                                         : 0.0),
                             (float)(O->HasField(TEXT("y"))
                                         ? GetJsonNumberField(O, TEXT("y"))
                                         : 0.0),
                             (float)(O->HasField(TEXT("z"))
                                         ? GetJsonNumberField(O, TEXT("z"))
                                         : 0.0));
        }
      }
    }
    DrawDebugCylinder(World, Loc, EndLoc, Size, 16, DebugColor, false,
                      Duration, 0, Thickness);
  } else if (LowerShapeType == TEXT("cone")) {
    FVector Direction = FVector::UpVector;
    if (Payload->HasField(TEXT("direction"))) {
      const TSharedPtr<FJsonValue> DirVal =
          Payload->TryGetField(TEXT("direction"));
      if (DirVal.IsValid()) {
        if (DirVal->Type == EJson::Array) {
          const TArray<TSharedPtr<FJsonValue>> &Arr = DirVal->AsArray();
          if (Arr.Num() >= 3)
            Direction = FVector((float)Arr[0]->AsNumber(),
                                (float)Arr[1]->AsNumber(),
                                (float)Arr[2]->AsNumber());
        } else if (DirVal->Type == EJson::Object) {
          const TSharedPtr<FJsonObject> O = DirVal->AsObject();
          if (O.IsValid())
            Direction = FVector((float)(O->HasField(TEXT("x"))
                                            ? GetJsonNumberField(O, TEXT("x"))
                                            : 0.0),
                                (float)(O->HasField(TEXT("y"))
                                            ? GetJsonNumberField(O, TEXT("y"))
                                            : 0.0),
                                (float)(O->HasField(TEXT("z"))
                                            ? GetJsonNumberField(O, TEXT("z"))
                                            : 0.0));
        }
      }
    }
    float Length = 100.0f;
    if (Payload->HasField(TEXT("length"))) {
      Length = (float)GetJsonNumberField(Payload, TEXT("length"));
    }
    // Default to a 45 degree cone if not specified
    float AngleWidth = FMath::DegreesToRadians(45.0f);
    float AngleHeight = FMath::DegreesToRadians(45.0f);

    if (Payload->HasField(TEXT("angle"))) {
      float AngleDeg = (float)GetJsonNumberField(Payload, TEXT("angle"));
      AngleWidth = AngleHeight = FMath::DegreesToRadians(AngleDeg);
    }

    DrawDebugCone(World, Loc, Direction, Length, AngleWidth, AngleHeight,
                  16, DebugColor, false, Duration, 0, Thickness);
  } else if (LowerShapeType == TEXT("capsule")) {
    FQuat Rot = FQuat::Identity;
    if (Payload->HasField(TEXT("rotation"))) {
      const TArray<TSharedPtr<FJsonValue>> *RotJsonArr = nullptr;
      if (Payload->TryGetArrayField(TEXT("rotation"), RotJsonArr) &&
          RotJsonArr && RotJsonArr->Num() >= 3) {
        Rot = FRotator((float)(*RotJsonArr)[0]->AsNumber(),
                       (float)(*RotJsonArr)[1]->AsNumber(),
                       (float)(*RotJsonArr)[2]->AsNumber())
                  .Quaternion();
      }
    }
    float HalfHeight = Size; // Default if not specified
    if (Payload->HasField(TEXT("halfHeight"))) {
      HalfHeight = (float)GetJsonNumberField(Payload, TEXT("halfHeight"));
    }
    DrawDebugCapsule(World, Loc, HalfHeight, Size, Rot, DebugColor, false,
                     Duration, 0, Thickness);
  } else if (LowerShapeType == TEXT("arrow")) {
    FVector EndLoc = Loc + FVector(100, 0, 0);
    if (Payload->HasField(TEXT("endLocation"))) {
      // ... parsing logic same as line ...
      const TSharedPtr<FJsonValue> EndVal =
          Payload->TryGetField(TEXT("endLocation"));
      if (EndVal.IsValid()) {
        if (EndVal->Type == EJson::Array) {
          const TArray<TSharedPtr<FJsonValue>> &Arr = EndVal->AsArray();
          if (Arr.Num() >= 3)
            EndLoc = FVector((float)Arr[0]->AsNumber(),
                             (float)Arr[1]->AsNumber(),
                             (float)Arr[2]->AsNumber());
        } else if (EndVal->Type == EJson::Object) {
          const TSharedPtr<FJsonObject> O = EndVal->AsObject();
          if (O.IsValid())
            EndLoc = FVector((float)(O->HasField(TEXT("x"))
                                         ? GetJsonNumberField(O, TEXT("x"))
                                         : 0.0),
                             (float)(O->HasField(TEXT("y"))
                                         ? GetJsonNumberField(O, TEXT("y"))
                                         : 0.0),
                             (float)(O->HasField(TEXT("z"))
                                         ? GetJsonNumberField(O, TEXT("z"))
                                         : 0.0));
        }
      }
    }
    float ArrowSize = Size > 0 ? Size : 10.0f;
    DrawDebugDirectionalArrow(World, Loc, EndLoc, ArrowSize, DebugColor,
                              false, Duration, 0, Thickness);
  } else if (LowerShapeType == TEXT("plane")) {
    // Draw a simple plane using a box with 0 height or DrawDebugSolidPlane
    // if available but DrawDebugBox is safer for wireframe Using Box with
    // minimal Z thickness
    FVector BoxSize = FVector(Size, Size, 1.0f);
    if (Payload->HasField(TEXT("boxSize"))) {
      // ... parsing ...
    }
    FQuat Rot = FQuat::Identity;
    if (Payload->HasField(TEXT("rotation"))) {
      const TArray<TSharedPtr<FJsonValue>> *RotJsonArr = nullptr;
      if (Payload->TryGetArrayField(TEXT("rotation"), RotJsonArr) &&
          RotJsonArr && RotJsonArr->Num() >= 3) {
        Rot = FRotator((float)(*RotJsonArr)[0]->AsNumber(),
                       (float)(*RotJsonArr)[1]->AsNumber(),
                       (float)(*RotJsonArr)[2]->AsNumber())
                  .Quaternion();
      }
    }
    DrawDebugBox(World, Loc, BoxSize, Rot, DebugColor, false, Duration, 0,
                 Thickness);
  } else {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Unsupported shape type: %s"), *ShapeType));
    Resp->SetStringField(
        TEXT("supportedShapes"),
        TEXT("sphere, box, circle, line, point, coordinate, cylinder, "
             "cone, capsule, arrow, plane"));
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Unsupported shape type"), Resp,
                           TEXT("UNSUPPORTED_SHAPE"));
    return true;
  }

  McpRecordDebugShape(LowerShapeType, Loc, Duration);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("shapeType"), ShapeType);
  Resp->SetStringField(
      TEXT("location"),
      FString::Printf(TEXT("%.2f,%.2f,%.2f"), Loc.X, Loc.Y, Loc.Z));
  Resp->SetNumberField(TEXT("duration"), Duration);
  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Debug shape drawn"), Resp, FString());
  return true;
#else
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), false);
  Resp->SetStringField(TEXT("error"),
                       TEXT("Debug shape drawing requires editor build"));
  Resp->SetStringField(TEXT("shapeType"), ShapeType);
  S.SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("Debug shape drawing not available in non-editor build"), Resp,
      TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool McpHandlers::Effect::HandleEffectSetNiagaraParameter(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString SystemName;
  Payload->TryGetStringField(TEXT("systemName"), SystemName);
  FString ParameterName;
  Payload->TryGetStringField(TEXT("parameterName"), ParameterName);
  FString ParameterType;
  Payload->TryGetStringField(TEXT("parameterType"), ParameterType);
  if (ParameterName.IsEmpty()) {
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("parameterName required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (ParameterType.IsEmpty())
    ParameterType = TEXT("Float");

  UE_LOG(
      LogMcpAutomationBridgeSubsystem, Verbose,
      TEXT("SetNiagaraParameter: Looking for actor '%s' to set param '%s'"),
      *SystemName, *ParameterName);

#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS) {
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("EditorActorSubsystem not available"),
                           nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
    return true;
  }

  const FName ParamName(*ParameterName);

  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  bool bApplied = false;

  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("SetNiagaraParameter: Looking for actor '%s'"), *SystemName);

  bool bActorFound = false;
  bool bComponentFound = false;

  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    if (!Actor->GetActorLabel().Equals(SystemName, ESearchCase::IgnoreCase))
      continue;

    bActorFound = true;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("SetNiagaraParameter: Found actor '%s'"), *SystemName);
    UNiagaraComponent *NiComp =
        Actor->FindComponentByClass<UNiagaraComponent>();
    if (!NiComp) {
      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Warning,
          TEXT("SetNiagaraParameter: Actor '%s' has no NiagaraComponent"),
          *SystemName);
      // Keep looking? No, actor label is unique-ish. But let's
      // assume unique.
      // But maybe we should break if we found the actor but no component?
      bComponentFound = false;
      break;
    }
    bComponentFound = true;

    if (ParameterType.Equals(TEXT("Float"), ESearchCase::IgnoreCase)) {
      double NumberValue = 0.0;
      if (Payload->TryGetNumberField(TEXT("floatValue"), NumberValue)) {
        NiComp->SetVariableFloat(ParamName, static_cast<float>(NumberValue));
        bApplied = true;
      }
    } else if (ParameterType.Equals(TEXT("Vector"),
                                    ESearchCase::IgnoreCase)) {
      const TSharedPtr<FJsonObject> *ObjValue = nullptr;
      if (Payload->TryGetObjectField(TEXT("vectorValue"), ObjValue) &&
          ObjValue && (*ObjValue).IsValid()) {
        double VX = 0, VY = 0, VZ = 0;
        (*ObjValue)->TryGetNumberField(TEXT("x"), VX);
        (*ObjValue)->TryGetNumberField(TEXT("y"), VY);
        (*ObjValue)->TryGetNumberField(TEXT("z"), VZ);
        NiComp->SetVariableVec3(ParamName,
                                FVector((float)VX, (float)VY, (float)VZ));
        bApplied = true;
      }
    } else if (ParameterType.Equals(TEXT("Color"),
                                    ESearchCase::IgnoreCase)) {
      const TSharedPtr<FJsonObject> *ObjValue = nullptr;
      if (Payload->TryGetObjectField(TEXT("colorValue"), ObjValue) &&
          ObjValue && (*ObjValue).IsValid()) {
        double R = 0, G = 0, B = 0, A = 1;
        (*ObjValue)->TryGetNumberField(TEXT("r"), R);
        (*ObjValue)->TryGetNumberField(TEXT("g"), G);
        (*ObjValue)->TryGetNumberField(TEXT("b"), B);
        (*ObjValue)->TryGetNumberField(TEXT("a"), A);
        NiComp->SetVariableLinearColor(
            ParamName, FLinearColor((float)R, (float)G, (float)B, (float)A));
        bApplied = true;
      }
    } else if (ParameterType.Equals(TEXT("Bool"),
                                    ESearchCase::IgnoreCase)) {
      bool bValue = false;
      if (Payload->TryGetBoolField(TEXT("boolValue"), bValue)) {
        NiComp->SetVariableBool(ParamName, bValue);
        bApplied = true;
      }
    }

    // If we found the actor and component but failed to apply, we stop
    // searching.
    break;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bApplied);
  Resp->SetBoolField(TEXT("applied"), bApplied);
  Resp->SetStringField(TEXT("actorName"), SystemName);
  Resp->SetStringField(TEXT("parameterName"), ParameterName);
  Resp->SetStringField(TEXT("parameterType"), ParameterType);

  if (bApplied) {
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Niagara parameter set"), Resp, FString());
  } else {
    FString ErrMsg = TEXT("Niagara parameter not applied");
    FString ErrCode = TEXT("SET_NIAGARA_PARAM_FAILED");

    if (!bActorFound) {
      ErrMsg = FString::Printf(TEXT("Actor '%s' not found"), *SystemName);
      ErrCode = TEXT("ACTOR_NOT_FOUND");
    } else if (!bComponentFound) {
      ErrMsg = FString::Printf(TEXT("Actor '%s' has no Niagara component"),
                               *SystemName);
      ErrCode = TEXT("COMPONENT_NOT_FOUND");
    } else {
      // Check common failure reasons
      // Invalid Type?
      if (!ParameterType.Equals(TEXT("Float"), ESearchCase::IgnoreCase) &&
          !ParameterType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase) &&
          !ParameterType.Equals(TEXT("Color"), ESearchCase::IgnoreCase) &&
          !ParameterType.Equals(TEXT("Bool"), ESearchCase::IgnoreCase)) {
        ErrMsg = FString::Printf(TEXT("Invalid parameter type: %s"),
                                 *ParameterType);
        ErrCode = TEXT("INVALID_ARGUMENT");
      }
    }

    S.SendAutomationResponse(Socket, RequestId, false, ErrMsg, Resp,
                           ErrCode);
  }
  return true;
#else
  S.SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("set_niagara_parameter requires editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::Effect::HandleEffectActivateNiagara(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString SystemName;
  Payload->TryGetStringField(TEXT("systemName"), SystemName);
  bool bReset = Payload->HasField(TEXT("reset"))
                    ? GetJsonBoolField(Payload, TEXT("reset"))
                    : true;

  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("ActivateNiagara: Looking for actor '%s'"), *SystemName);

#if WITH_EDITOR
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  bool bFound = false;
  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    if (!Actor->GetActorLabel().Equals(SystemName, ESearchCase::IgnoreCase))
      continue;

    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("ActivateNiagara: Found actor '%s'"), *SystemName);
    UNiagaraComponent *NiComp =
        Actor->FindComponentByClass<UNiagaraComponent>();
    if (!NiComp)
      continue;

    NiComp->Activate(bReset);
    bFound = true;
    break;
  }
  if (bFound) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("active"), true);
    for (AActor *FoundActor : AllActors) {
      if (FoundActor && FoundActor->GetActorLabel().Equals(SystemName, ESearchCase::IgnoreCase)) {
        McpHandlerUtils::AddVerification(Resp, FoundActor);
        break;
      }
    }
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Niagara system activated."), Resp);
  } else
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Niagara system not found."), nullptr,
                           TEXT("SYSTEM_NOT_FOUND"));
  return true;
#else
  S.SendAutomationResponse(Socket, RequestId, false,
                         TEXT("activate_niagara requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::Effect::HandleEffectDeactivateNiagara(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString SystemName;
  Payload->TryGetStringField(TEXT("systemName"), SystemName);
  if (SystemName.IsEmpty())
    Payload->TryGetStringField(TEXT("actorName"), SystemName);

#if WITH_EDITOR
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  bool bFound = false;
  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    if (!Actor->GetActorLabel().Equals(SystemName, ESearchCase::IgnoreCase))
      continue;

    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("DeactivateNiagara: Found actor '%s'"), *SystemName);
    UNiagaraComponent *NiComp =
        Actor->FindComponentByClass<UNiagaraComponent>();
    if (!NiComp)
      continue;

    NiComp->Deactivate();
    bFound = true;
    break;
  }
  if (bFound) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), SystemName);
    Resp->SetBoolField(TEXT("active"), false);
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Niagara system deactivated."), Resp);
  } else
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Niagara system not found."), nullptr,
                           TEXT("SYSTEM_NOT_FOUND"));
  return true;
#else
  S.SendAutomationResponse(Socket, RequestId, false,
                         TEXT("deactivate_niagara requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::Effect::HandleEffectAdvanceSimulation(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString SystemName;
  Payload->TryGetStringField(TEXT("systemName"), SystemName);
  if (SystemName.IsEmpty())
    Payload->TryGetStringField(TEXT("actorName"), SystemName);

  double DeltaTime = 0.1;
  Payload->TryGetNumberField(TEXT("deltaTime"), DeltaTime);
  int32 Steps = 1;
  Payload->TryGetNumberField(TEXT("steps"), Steps);

#if WITH_EDITOR
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  bool bFound = false;
  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    if (!Actor->GetActorLabel().Equals(SystemName, ESearchCase::IgnoreCase))
      continue;

    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("AdvanceSimulation: Found actor '%s'"), *SystemName);
    UNiagaraComponent *NiComp =
        Actor->FindComponentByClass<UNiagaraComponent>();
    if (!NiComp)
      continue;

    for (int i = 0; i < Steps; i++) {
      NiComp->AdvanceSimulation(Steps, DeltaTime);
    }
    bFound = true;
    break;
  }
  if (bFound) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), SystemName);
    Resp->SetNumberField(TEXT("steps"), Steps);
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Niagara simulation advanced."), Resp);
  } else
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Niagara system not found."), nullptr,
                           TEXT("SYSTEM_NOT_FOUND"));
  return true;
#else
  S.SendAutomationResponse(Socket, RequestId, false,
                         TEXT("advance_simulation requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::Effect::HandleEffectCreateDynamicLight(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  // Validate required parameters - location is mandatory for meaningful light creation
  if (!Payload->HasField(TEXT("location"))) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"), TEXT("location parameter is required for create_dynamic_light"));
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Missing required parameter: location"), Resp,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString LightName;
  Payload->TryGetStringField(TEXT("lightName"), LightName);
  FString LightType;
  Payload->TryGetStringField(TEXT("lightType"), LightType);
  if (LightType.IsEmpty())
    LightType = TEXT("Point");

  // location
  FVector Loc(0, 0, 0);
  if (Payload->HasField(TEXT("location"))) {
    const TSharedPtr<FJsonValue> LocVal =
        Payload->TryGetField(TEXT("location"));
    if (LocVal.IsValid()) {
      if (LocVal->Type == EJson::Array) {
        const TArray<TSharedPtr<FJsonValue>> &Arr = LocVal->AsArray();
        if (Arr.Num() >= 3)
          Loc =
              FVector((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(),
                      (float)Arr[2]->AsNumber());
      } else if (LocVal->Type == EJson::Object) {
        const TSharedPtr<FJsonObject> O = LocVal->AsObject();
        if (O.IsValid())
          Loc = FVector(
              (float)(O->HasField(TEXT("x")) ? GetJsonNumberField(O, TEXT("x"))
                                             : 0.0),
              (float)(O->HasField(TEXT("y")) ? GetJsonNumberField(O, TEXT("y"))
                                             : 0.0),
              (float)(O->HasField(TEXT("z")) ? GetJsonNumberField(O, TEXT("z"))
                                             : 0.0));
      }
    }
  }

  double Intensity = 0.0;
  Payload->TryGetNumberField(TEXT("intensity"), Intensity);
  // color can be array or object
  bool bHasColor = false;
  double Cr = 1.0, Cg = 1.0, Cb = 1.0, Ca = 1.0;
  if (Payload->HasField(TEXT("color"))) {
    const TArray<TSharedPtr<FJsonValue>> *ColArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("color"), ColArr) && ColArr &&
        ColArr->Num() >= 3) {
      bHasColor = true;
      Cr = (*ColArr)[0]->AsNumber();
      Cg = (*ColArr)[1]->AsNumber();
      Cb = (*ColArr)[2]->AsNumber();
      Ca = (ColArr->Num() > 3) ? (*ColArr)[3]->AsNumber() : 1.0;
    } else {
      const TSharedPtr<FJsonObject> *CO = nullptr;
      if (Payload->TryGetObjectField(TEXT("color"), CO) && CO &&
          (*CO).IsValid()) {
        bHasColor = true;
        Cr = (*CO)->HasField(TEXT("r")) ? GetJsonNumberField(*CO, TEXT("r"))
                                        : Cr;
        Cg = (*CO)->HasField(TEXT("g")) ? GetJsonNumberField(*CO, TEXT("g"))
                                        : Cg;
        Cb = (*CO)->HasField(TEXT("b")) ? GetJsonNumberField(*CO, TEXT("b"))
                                        : Cb;
        Ca = (*CO)->HasField(TEXT("a")) ? GetJsonNumberField(*CO, TEXT("a"))
                                        : Ca;
      }
    }
  }

  // pulse param optional
  bool bPulseEnabled = false;
  double PulseFreq = 1.0;
  if (Payload->HasField(TEXT("pulse"))) {
    const TSharedPtr<FJsonObject> *P = nullptr;
    if (Payload->TryGetObjectField(TEXT("pulse"), P) && P &&
        (*P).IsValid()) {
      (*P)->TryGetBoolField(TEXT("enabled"), bPulseEnabled);
      (*P)->TryGetNumberField(TEXT("frequency"), PulseFreq);
    }
  }

#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (!GEditor) {
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS) {
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("EditorActorSubsystem not available"),
                           nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
    return true;
  }

  UClass *ChosenClass = APointLight::StaticClass();
  UClass *CompClass = UPointLightComponent::StaticClass();
  FString LT = LightType.ToLower();
  if (LT == TEXT("spot") || LT == TEXT("spotlight")) {
    ChosenClass = ASpotLight::StaticClass();
    CompClass = USpotLightComponent::StaticClass();
  } else if (LT == TEXT("directional") || LT == TEXT("directionallight")) {
    ChosenClass = ADirectionalLight::StaticClass();
    CompClass = UDirectionalLightComponent::StaticClass();
  } else if (LT == TEXT("rect") || LT == TEXT("rectlight")) {
    ChosenClass = ARectLight::StaticClass();
    CompClass = URectLightComponent::StaticClass();
  }

  AActor *Spawned = SpawnActorInActiveWorld<AActor>(ChosenClass, Loc,
                                                    FRotator::ZeroRotator);
  if (!Spawned) {
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to spawn light actor"), nullptr,
                           TEXT("CREATE_DYNAMIC_LIGHT_FAILED"));
    return true;
  }

  UActorComponent *C = Spawned->GetComponentByClass(CompClass);
  if (C) {
    if (ULightComponent *LC = Cast<ULightComponent>(C)) {
      LC->SetIntensity(static_cast<float>(Intensity));
      if (bHasColor) {
        LC->SetLightColor(
            FLinearColor(static_cast<float>(Cr), static_cast<float>(Cg),
                         static_cast<float>(Cb), static_cast<float>(Ca)));
      }
    }
  }

  if (!LightName.IsEmpty()) {
    Spawned->SetActorLabel(LightName);
  }
  if (bPulseEnabled) {
    Spawned->Tags.Add(
        FName(*FString::Printf(TEXT("MCP_PULSE:%g"), PulseFreq)));
  }

  McpHandlerUtils::AddVerification(Resp, Spawned);
  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Dynamic light created"), Resp, FString());
  return true;
#else
  S.SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("create_dynamic_light requires editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// Removes actors whose label starts with the provided filter (editor-only).
bool McpHandlers::Effect::HandleEffectCleanup(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString Filter;
  Payload->TryGetStringField(TEXT("filter"), Filter);
  if (Filter.IsEmpty()) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetNumberField(TEXT("removed"), 0);
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Cleanup skipped (empty filter)"), Resp,
                           FString());
    return true;
  }
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS) {
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("EditorActorSubsystem not available"),
                           nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
    return true;
  }
  TArray<AActor *> Actors = ActorSS->GetAllLevelActors();
  TArray<FString> Removed;
  for (AActor *A : Actors) {
    if (!A)
      continue;
    FString Label = A->GetActorLabel();
    if (Label.IsEmpty())
      continue;
    if (!Label.StartsWith(Filter, ESearchCase::IgnoreCase))
      continue;
    bool bDel = ActorSS->DestroyActor(A);
    if (bDel)
      Removed.Add(Label);
  }
  TArray<TSharedPtr<FJsonValue>> Arr;
  for (const FString &Name : Removed)
    Arr.Add(MakeShared<FJsonValueString>(Name));
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetArrayField(TEXT("removedActors"), Arr);
  Resp->SetNumberField(TEXT("removed"), Removed.Num());
  S.SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Cleanup completed (removed=%d)"),
                      Removed.Num()),
      Resp, FString());
  return true;
#else
  S.SendAutomationResponse(Socket, RequestId, false,
                         TEXT("cleanup requires editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// Spawn Niagara system in-level as a NiagaraActor (editor-only)
bool McpHandlers::Effect::HandleEffectSpawnNiagara(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString SystemPath;
  Payload->TryGetStringField(TEXT("systemPath"), SystemPath);
  if (SystemPath.IsEmpty()) {
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("systemPath required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Guard against non-existent assets to prevent LoadPackage warnings
  if (!UEditorAssetLibrary::DoesAssetExist(SystemPath)) {
    S.SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Niagara system asset not found: %s"),
                        *SystemPath),
        nullptr, TEXT("SYSTEM_NOT_FOUND"));
    return true;
  }

  // Location and optional rotation/scale
  FVector Loc(0, 0, 0);
  if (Payload->HasField(TEXT("location"))) {
    const TSharedPtr<FJsonValue> LocVal =
        Payload->TryGetField(TEXT("location"));
    if (LocVal.IsValid()) {
      if (LocVal->Type == EJson::Array) {
        const TArray<TSharedPtr<FJsonValue>> &Arr = LocVal->AsArray();
        if (Arr.Num() >= 3)
          Loc = FVector((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(),
                        (float)Arr[2]->AsNumber());
      } else if (LocVal->Type == EJson::Object) {
        const TSharedPtr<FJsonObject> O = LocVal->AsObject();
        if (O.IsValid())
          Loc = FVector(
              (float)(O->HasField(TEXT("x")) ? GetJsonNumberField(O, TEXT("x"))
                                             : 0.0),
              (float)(O->HasField(TEXT("y")) ? GetJsonNumberField(O, TEXT("y"))
                                             : 0.0),
              (float)(O->HasField(TEXT("z")) ? GetJsonNumberField(O, TEXT("z"))
                                             : 0.0));
      }
    }
  }

  // Rotation may be an array
  TArray<double> RotArr = {0, 0, 0};
  const TArray<TSharedPtr<FJsonValue>> *RA = nullptr;
  if (Payload->TryGetArrayField(TEXT("rotation"), RA) && RA &&
      RA->Num() >= 3) {
    RotArr[0] = (*RA)[0]->AsNumber();
    RotArr[1] = (*RA)[1]->AsNumber();
    RotArr[2] = (*RA)[2]->AsNumber();
  }

  // Scale may be an array or a single numeric value
  TArray<double> ScaleArr = {1, 1, 1};
  const TArray<TSharedPtr<FJsonValue>> *ScaleJsonArr = nullptr;
  if (Payload->TryGetArrayField(TEXT("scale"), ScaleJsonArr) &&
      ScaleJsonArr && ScaleJsonArr->Num() >= 3) {
    ScaleArr[0] = (*ScaleJsonArr)[0]->AsNumber();
    ScaleArr[1] = (*ScaleJsonArr)[1]->AsNumber();
    ScaleArr[2] = (*ScaleJsonArr)[2]->AsNumber();
  } else if (Payload->TryGetNumberField(TEXT("scale"), ScaleArr[0])) {
    ScaleArr[1] = ScaleArr[2] = ScaleArr[0];
  }

  const bool bAutoDestroy =
      Payload->HasField(TEXT("autoDestroy"))
          ? GetJsonBoolField(Payload, TEXT("autoDestroy"))
          : false;
  FString AttachToActor;
  Payload->TryGetStringField(TEXT("attachToActor"), AttachToActor);

#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS) {
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("EditorActorSubsystem not available"),
                           nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
    return true;
  }

  UObject *NiagObj = UEditorAssetLibrary::LoadAsset(SystemPath);
  if (!NiagObj) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"),
                         TEXT("Niagara system asset not found"));
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Niagara system not found"), Resp,
                           TEXT("SYSTEM_NOT_FOUND"));
    return true;
  }

  const FRotator SpawnRot(static_cast<float>(RotArr[0]),
                          static_cast<float>(RotArr[1]),
                          static_cast<float>(RotArr[2]));
  AActor *Spawned = SpawnActorInActiveWorld<AActor>(
      ANiagaraActor::StaticClass(), Loc, SpawnRot);
  if (!Spawned) {
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to spawn NiagaraActor"), nullptr,
                           TEXT("SPAWN_FAILED"));
    return true;
  }

  UNiagaraComponent *NiComp =
      Spawned->FindComponentByClass<UNiagaraComponent>();
  if (NiComp && NiagObj->IsA<UNiagaraSystem>()) {
    NiComp->SetAsset(Cast<UNiagaraSystem>(NiagObj));
    NiComp->SetWorldScale3D(FVector(ScaleArr[0], ScaleArr[1], ScaleArr[2]));
    NiComp->Activate(true); // Set to true
  }

  if (!AttachToActor.IsEmpty()) {
    AActor *Parent = nullptr;
    TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
    for (AActor *A : AllActors) {
      if (A &&
          A->GetActorLabel().Equals(AttachToActor, ESearchCase::IgnoreCase)) {
        Parent = A;
        break;
      }
    }
    if (Parent) {
      Spawned->AttachToActor(Parent,
                             FAttachmentTransformRules::KeepWorldTransform);
    }
  }

  // Set actor label
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  if (Name.IsEmpty())
    Payload->TryGetStringField(TEXT("actorName"), Name);

  if (!Name.IsEmpty()) {
    Spawned->SetActorLabel(Name);
  } else {
    Spawned->SetActorLabel(FString::Printf(
        TEXT("Niagara_%lld"), FDateTime::Now().ToUnixTimestamp()));
  }


  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Resp, Spawned);
  S.SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Niagara spawned"), Resp, FString());
  return true;
#else
  S.SendAutomationResponse(Socket, RequestId, false,
                         TEXT("spawn_niagara requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::Effect::HandleEffectCreateVolumetricFog(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  // Create volumetric fog using AExponentialHeightFog
#if WITH_EDITOR
  if (!GEditor) {
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS) {
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("EditorActorSubsystem not available"),
                           nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
    return true;
  }

  // Parse location
  FVector Loc(0, 0, 0);
  if (Payload->HasField(TEXT("location"))) {
    const TSharedPtr<FJsonValue> LocVal = Payload->TryGetField(TEXT("location"));
    if (LocVal.IsValid() && LocVal->Type == EJson::Array) {
      const TArray<TSharedPtr<FJsonValue>> &Arr = LocVal->AsArray();
      if (Arr.Num() >= 3)
        Loc = FVector((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(), (float)Arr[2]->AsNumber());
    } else if (LocVal.IsValid() && LocVal->Type == EJson::Object) {
      const TSharedPtr<FJsonObject> O = LocVal->AsObject();
      if (O.IsValid())
        Loc = FVector(
            (float)(O->HasField(TEXT("x")) ? GetJsonNumberField(O, TEXT("x")) : 0.0),
            (float)(O->HasField(TEXT("y")) ? GetJsonNumberField(O, TEXT("y")) : 0.0),
            (float)(O->HasField(TEXT("z")) ? GetJsonNumberField(O, TEXT("z")) : 0.0));
    }
  }

  double Density = 0.05;
  Payload->TryGetNumberField(TEXT("density"), Density);
  double Scattering = 0.5;
  Payload->TryGetNumberField(TEXT("scattering"), Scattering);
  double Extinction = 0.5;
  Payload->TryGetNumberField(TEXT("extinction"), Extinction);

#if __has_include("Engine/ExponentialHeightFog.h")
  AActor *Spawned = SpawnActorInActiveWorld<AActor>(
      AExponentialHeightFog::StaticClass(), Loc, FRotator::ZeroRotator);
  if (Spawned) {
    UExponentialHeightFogComponent *FogComp = Spawned->FindComponentByClass<UExponentialHeightFogComponent>();
    if (FogComp) {
      FogComp->SetFogDensity(static_cast<float>(Density));
      // Enable volumetric fog
#if ENGINE_MAJOR_VERSION == 5
      FogComp->SetVolumetricFog(true);
      FogComp->SetVolumetricFogScatteringDistribution(static_cast<float>(Scattering));
      FogComp->SetVolumetricFogExtinctionScale(static_cast<float>(Extinction));
#endif
    }
    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);
    if (!Name.IsEmpty()) {
      Spawned->SetActorLabel(Name);
    } else {
      Spawned->SetActorLabel(FString::Printf(TEXT("VolumetricFog_%lld"), FDateTime::Now().ToUnixTimestamp()));
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), Spawned->GetActorLabel());
    Resp->SetStringField(TEXT("effectType"), TEXT("volumetric_fog"));
    McpHandlerUtils::AddVerification(Resp, Spawned);
    S.SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Volumetric fog created"), Resp, FString());
    return true;
  }
  S.SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Failed to spawn volumetric fog actor"), nullptr,
                         TEXT("SPAWN_FAILED"));
  return true;
#else
  S.SendAutomationResponse(Socket, RequestId, false,
                         TEXT("ExponentialHeightFog not available in this build"),
                         nullptr, TEXT("NOT_AVAILABLE"));
  return true;
#endif
#else
  S.SendAutomationResponse(Socket, RequestId, false,
                         TEXT("create_volumetric_fog requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::Effect::HandleEffectCreateParticleTrail(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  // Create a particle trail using Cascade particles or Niagara if available
  // If systemPath is provided, use Niagara; otherwise create a simple trail
  FString SystemPath;
  Payload->TryGetStringField(TEXT("systemPath"), SystemPath);
  if (SystemPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("emitter"), SystemPath);
  }

  if (!SystemPath.IsEmpty()) {
    // Use the provided system path
    return CreateNiagaraEffect(S, RequestId, Payload, Socket,
                               TEXT("create_particle_trail"), SystemPath);
  }

#if WITH_EDITOR
  // Create a simple trail without an asset - spawn a NiagaraActor with default settings
  if (!GEditor) {
    S.SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  FVector Loc(0, 0, 0);
  if (Payload->HasField(TEXT("location"))) {
    const TSharedPtr<FJsonValue> LocVal = Payload->TryGetField(TEXT("location"));
    if (LocVal.IsValid() && LocVal->Type == EJson::Array) {
      const TArray<TSharedPtr<FJsonValue>> &Arr = LocVal->AsArray();
      if (Arr.Num() >= 3)
        Loc = FVector((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(), (float)Arr[2]->AsNumber());
    }
  }

  // For procedural trail without asset, inform user that systemPath is needed
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), false);
  Resp->SetStringField(TEXT("error"), TEXT("systemPath or emitter parameter is required for particle trail creation. Please provide a valid Niagara system asset path."));
  S.SendAutomationResponse(Socket, RequestId, false,
                         TEXT("systemPath required for particle trail"), Resp,
                         TEXT("INVALID_ARGUMENT"));
  return true;
#else
  S.SendAutomationResponse(Socket, RequestId, false,
                         TEXT("create_particle_trail requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool McpHandlers::Effect::HandleEffectCreateEnvironmentEffect(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  // Create environment effect - requires a Niagara system asset
  FString SystemPath;
  Payload->TryGetStringField(TEXT("systemPath"), SystemPath);

  if (!SystemPath.IsEmpty()) {
    return CreateNiagaraEffect(S, RequestId, Payload, Socket,
                               TEXT("create_environment_effect"), SystemPath);
  }

  // Without systemPath, inform user
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), false);
  Resp->SetStringField(TEXT("error"), TEXT("systemPath parameter is required for environment effect creation. Please provide a valid Niagara system asset path."));
  S.SendAutomationResponse(Socket, RequestId, false,
                         TEXT("systemPath required for environment effect"), Resp,
                         TEXT("INVALID_ARGUMENT"));
  return true;
}

bool McpHandlers::Effect::HandleEffectCreateImpactEffect(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  // Create impact effect - requires a Niagara system asset
  FString SystemPath;
  Payload->TryGetStringField(TEXT("systemPath"), SystemPath);

  if (!SystemPath.IsEmpty()) {
    return CreateNiagaraEffect(S, RequestId, Payload, Socket,
                               TEXT("create_impact_effect"), SystemPath);
  }

  // Without systemPath, inform user
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), false);
  Resp->SetStringField(TEXT("error"), TEXT("systemPath parameter is required for impact effect creation. Please provide a valid Niagara system asset path."));
  S.SendAutomationResponse(Socket, RequestId, false,
                         TEXT("systemPath required for impact effect"), Resp,
                         TEXT("INVALID_ARGUMENT"));
  return true;
}

bool McpHandlers::Effect::HandleEffectCreateNiagaraRibbon(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  // Require systemPath
  return CreateNiagaraEffect(S, RequestId, Payload, Socket,
                             TEXT("create_niagara_ribbon"), FString());
}

// Helper function to create Niagara effects with default systems
bool McpHandlers::Effect::CreateNiagaraEffect(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket, const FString &EffectName,
    const FString &DefaultSystemPath) {
#if WITH_EDITOR
  if (!GEditor) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"), TEXT("Editor not available"));
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Editor not available"), Resp,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"),
                         TEXT("EditorActorSubsystem not available"));
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("EditorActorSubsystem not available"), Resp,
                           TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
    return true;
  }

  // Get custom system path or use default
  FString SystemPath = DefaultSystemPath;
  Payload->TryGetStringField(TEXT("systemPath"), SystemPath);

  if (SystemPath.IsEmpty()) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("systemPath is required for %s. Please provide a "
                             "valid asset path (e.g. /Game/Effects/MySystem)"),
                        *EffectName));
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("systemPath required"), Resp,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Location
  FVector Loc(0, 0, 0);
  if (Payload->HasField(TEXT("location"))) {
    const TSharedPtr<FJsonValue> LocVal =
        Payload->TryGetField(TEXT("location"));
    if (LocVal.IsValid()) {
      if (LocVal->Type == EJson::Array) {
        const TArray<TSharedPtr<FJsonValue>> &Arr = LocVal->AsArray();
        if (Arr.Num() >= 3)
          Loc = FVector((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(),
                        (float)Arr[2]->AsNumber());
      } else if (LocVal->Type == EJson::Object) {
        const TSharedPtr<FJsonObject> O = LocVal->AsObject();
        if (O.IsValid())
          Loc = FVector(
              (float)(O->HasField(TEXT("x")) ? GetJsonNumberField(O, TEXT("x"))
                                             : 0.0),
              (float)(O->HasField(TEXT("y")) ? GetJsonNumberField(O, TEXT("y"))
                                             : 0.0),
              (float)(O->HasField(TEXT("z")) ? GetJsonNumberField(O, TEXT("z"))
                                             : 0.0));
      }
    }
  }

  // Load the Niagara system
  if (!UEditorAssetLibrary::DoesAssetExist(SystemPath)) {
    S.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        FString::Printf(TEXT("Niagara system asset not found: %s"),
                        *SystemPath),
        nullptr, TEXT("SYSTEM_NOT_FOUND"));
    return true;
  }

  UObject *NiagObj = UEditorAssetLibrary::LoadAsset(SystemPath);
  if (!NiagObj) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"), TEXT("Niagara system asset not found"));
    Resp->SetStringField(TEXT("systemPath"), SystemPath);
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Niagara system not found"), Resp,
                           TEXT("SYSTEM_NOT_FOUND"));
    return true;
  }

  // Spawn the actor
  AActor *Spawned = SpawnActorInActiveWorld<AActor>(
      ANiagaraActor::StaticClass(), Loc, FRotator::ZeroRotator);
  if (!Spawned) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"), TEXT("Failed to spawn Niagara actor"));
    S.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Failed to spawn Niagara actor"), Resp,
                           TEXT("SPAWN_FAILED"));
    return true;
  }

  // Configure the Niagara component
  UNiagaraComponent *NiComp =
      Spawned->FindComponentByClass<UNiagaraComponent>();
  if (NiComp && NiagObj->IsA<UNiagaraSystem>()) {
    NiComp->SetAsset(Cast<UNiagaraSystem>(NiagObj));
    NiComp->Activate(true);
  }

  // Set actor label
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  if (Name.IsEmpty())
    Payload->TryGetStringField(TEXT("actorName"), Name);

  if (!Name.IsEmpty()) {
    Spawned->SetActorLabel(Name);
  } else {
    Spawned->SetActorLabel(FString::Printf(
        TEXT("%s_%lld"), *EffectName.Replace(TEXT("create_"), TEXT("")),
        FDateTime::Now().ToUnixTimestamp()));
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("CreateNiagaraEffect: Spawned actor '%s' (ID: %u)"),
         *Spawned->GetActorLabel(), Spawned->GetUniqueID());

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("effectType"), EffectName);
  Resp->SetStringField(TEXT("systemPath"), SystemPath);
  Resp->SetStringField(TEXT("actorName"), Spawned->GetActorLabel());
  Resp->SetNumberField(TEXT("actorId"), Spawned->GetUniqueID());
  S.SendAutomationResponse(
      RequestingSocket, RequestId, true,
      FString::Printf(TEXT("%s created successfully"), *EffectName), Resp,
      FString());
  return true;
#else
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), false);
  Resp->SetStringField(TEXT("error"),
                       TEXT("Effect creation requires editor build"));
  S.SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Effect creation not available in non-editor build"), Resp,
      TEXT("NOT_AVAILABLE"));
  return true;
#endif
}
