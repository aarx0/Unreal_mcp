// =============================================================================
// McpHandlerUtils.h
// =============================================================================
// Centralized utility functions and macros for MCP Automation Bridge handlers.
// 
// This file provides:
// - Standardized JSON parsing helpers
// - Common response building utilities
// - Action dispatch macros
// - Blueprint graph manipulation utilities
// - Pin type conversion helpers
//
// REFACTORING NOTES:
// - Functions extracted from McpAutomationBridge_BlueprintHandlers.cpp (900+ lines)
// - Response building standardized across all 56 handler files
// - JSON parsing patterns consolidated from duplicated implementations
// 
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

// Forward declarations
class UMcpAutomationBridgeSubsystem;
class FMcpBridgeWebSocket;

// =============================================================================
// MCP Namespace for Handler Utilities
// =============================================================================
namespace McpHandlerUtils
{
    // =========================================================================
    // JSON Response Building
    // =========================================================================
    
    /**
     * Build a standardized success response object.
     * @param Message Human-readable success message
     * @param Data Optional result data object
     * @return JSON object ready for SendAutomationResponse
     */
    inline TSharedPtr<FJsonObject> BuildSuccessResponse(
        const FString& Message,
        const TSharedPtr<FJsonObject>& Data = nullptr)
    {
        TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), Message);
        if (Data.IsValid())
        {
            Response->SetObjectField(TEXT("data"), Data);
        }
        return Response;
    }

    /**
     * Build a standardized error response object.
     * @param ErrorCode Short error code (e.g., "INVALID_PARAM", "NOT_FOUND")
     * @param Message Human-readable error message
     * @param Details Optional additional error details
     * @return JSON object ready for SendAutomationResponse
     */
    inline TSharedPtr<FJsonObject> BuildErrorResponse(
        const FString& ErrorCode,
        const FString& Message,
        const TSharedPtr<FJsonObject>& Details = nullptr)
    {
        TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), Message);
        Response->SetStringField(TEXT("code"), ErrorCode);
        if (Details.IsValid())
        {
            Response->SetObjectField(TEXT("details"), Details);
        }
        return Response;
    }

    // =========================================================================
    // JSON Field Extraction Helpers (with validation)
    // =========================================================================

    /**
     * Extract a required string field with validation and error response.
     * @param Payload The JSON payload to extract from
     * @param FieldName The field name to extract
     * @param OutValue Output value if found
     * @param OutError Error message if extraction failed
     * @return true if field was found and extracted
     */
    inline bool TryGetRequiredString(
        const TSharedPtr<FJsonObject>& Payload,
        const FString& FieldName,
        FString& OutValue,
        FString& OutError)
    {
        if (!Payload.IsValid())
        {
            OutError = FString::Printf(TEXT("Payload is null when extracting '%s'"), *FieldName);
            return false;
        }
        
        if (!Payload->TryGetStringField(FieldName, OutValue))
        {
            OutError = FString::Printf(TEXT("Missing required field '%s'"), *FieldName);
            return false;
        }
        
        if (OutValue.IsEmpty())
        {
            OutError = FString::Printf(TEXT("Field '%s' is empty"), *FieldName);
            return false;
        }
        
        return true;
    }

    /**
     * Extract an optional string field with default value.
     */
    inline FString GetOptionalString(
        const TSharedPtr<FJsonObject>& Payload,
        const FString& FieldName,
        const FString& DefaultValue = FString())
    {
        FString Value;
        if (Payload.IsValid() && Payload->TryGetStringField(FieldName, Value))
        {
            return Value;
        }
        return DefaultValue;
    }

    /**
     * Extract an optional integer field with default value.
     */
    inline int32 GetOptionalInt(
        const TSharedPtr<FJsonObject>& Payload,
        const FString& FieldName,
        int32 DefaultValue = 0)
    {
        int32 Value = DefaultValue;
        if (Payload.IsValid())
        {
            Payload->TryGetNumberField(FieldName, Value);
        }
        return Value;
    }

    /**
     * Extract an optional float/double field with default value.
     */
    inline double GetOptionalFloat(
        const TSharedPtr<FJsonObject>& Payload,
        const FString& FieldName,
        double DefaultValue = 0.0)
    {
        double Value = DefaultValue;
        if (Payload.IsValid())
        {
            Payload->TryGetNumberField(FieldName, Value);
        }
        return Value;
    }

    /**
     * Extract an optional boolean field with default value.
     */
    inline bool GetOptionalBool(
        const TSharedPtr<FJsonObject>& Payload,
        const FString& FieldName,
        bool DefaultValue = false)
    {
        bool Value = DefaultValue;
        if (Payload.IsValid())
        {
            Payload->TryGetBoolField(FieldName, Value);
        }
        return Value;
    }

    // =========================================================================
    // JSON Value Conversion
    // =========================================================================

    /**
     * Convert a JSON value to its string representation.
     * Handles all JSON types including objects and arrays.
     */
    MCPAUTOMATIONBRIDGE_API FString JsonValueToString(const TSharedPtr<FJsonValue>& Value);

    /**
     * Convert a FVector to a JSON object.
     */
    inline TSharedPtr<FJsonObject> VectorToJson(const FVector& Vector)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("x"), Vector.X);
        Obj->SetNumberField(TEXT("y"), Vector.Y);
        Obj->SetNumberField(TEXT("z"), Vector.Z);
        return Obj;
    }

    /**
     * Convert a FRotator to a JSON object.
     */
    inline TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Rotator)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("pitch"), Rotator.Pitch);
        Obj->SetNumberField(TEXT("yaw"), Rotator.Yaw);
        Obj->SetNumberField(TEXT("roll"), Rotator.Roll);
        return Obj;
    }

    /**
     * Parse a JSON object to FVector.
     */
    inline bool JsonToVector(const TSharedPtr<FJsonObject>& Obj, FVector& OutVector)
    {
        if (!Obj.IsValid())
        {
            return false;
        }
        
        double X = 0.0, Y = 0.0, Z = 0.0;
        Obj->TryGetNumberField(TEXT("x"), X);
        Obj->TryGetNumberField(TEXT("y"), Y);
        Obj->TryGetNumberField(TEXT("z"), Z);
        
        // Also try uppercase keys
        if (!Obj->HasField(TEXT("x")))
        {
            Obj->TryGetNumberField(TEXT("X"), X);
            Obj->TryGetNumberField(TEXT("Y"), Y);
            Obj->TryGetNumberField(TEXT("Z"), Z);
        }
        
        OutVector = FVector(X, Y, Z);
        return true;
    }

    /**
     * Parse a JSON object to FRotator.
     */
    inline bool JsonToRotator(const TSharedPtr<FJsonObject>& Obj, FRotator& OutRotator)
    {
        if (!Obj.IsValid())
        {
            return false;
        }
        
        double Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
        Obj->TryGetNumberField(TEXT("pitch"), Pitch);
        Obj->TryGetNumberField(TEXT("yaw"), Yaw);
        Obj->TryGetNumberField(TEXT("roll"), Roll);
        
        // Also try uppercase keys
        if (!Obj->HasField(TEXT("pitch")))
        {
            Obj->TryGetNumberField(TEXT("Pitch"), Pitch);
            Obj->TryGetNumberField(TEXT("Yaw"), Yaw);
            Obj->TryGetNumberField(TEXT("Roll"), Roll);
        }
        
        OutRotator = FRotator(Pitch, Yaw, Roll);
        return true;
    }


    // =========================================================================
    // Actor/Component Utilities
    // =========================================================================

#if WITH_EDITOR
    /**
     * Find an actor by name in the current world.
     * @param ActorName Actor name to search for
     * @param bExactMatch If true, requires exact name match; otherwise partial match
     * @return Found actor or nullptr
     */
    MCPAUTOMATIONBRIDGE_API class AActor* FindActorByName(const FString& ActorName, bool bExactMatch = true);

    /**
     * Find a component by name on an actor.
     * @param Actor The actor to search
     * @param ComponentName Component name to search for
     * @return Found component or nullptr
     */
    MCPAUTOMATIONBRIDGE_API class UActorComponent* FindActorComponentByName(
        class AActor* Actor, 
        const FString& ComponentName);
#endif

    // =========================================================================
    // Object and Property Resolution Helpers
    // =========================================================================
    
#if WITH_EDITOR
    /**
     * Resolve an object from various path formats.
     * Supports: Actor names, component paths (Actor.Component), asset paths (/Game/...)
     * @param ObjectPath The path to resolve
     * @param OutResolvedPath Optional output for the normalized path
     * @return The resolved UObject, or nullptr if not found
     */
    MCPAUTOMATIONBRIDGE_API UObject* ResolveObjectFromPath(
        const FString& ObjectPath,
        FString* OutResolvedPath = nullptr);
#endif
    
    /**
     * Result struct for property resolution.
     */
    struct FPropertyResolveResult
    {
        FProperty* Property = nullptr;
        void* Container = nullptr;
        FString Error;
        bool IsValid() const { return Property != nullptr && Container != nullptr; }
    };
    
    /**
     * Resolve a property from an object, handling both simple and nested paths.
     * @param Object The object to resolve the property on
     * @param PropertyName The property name (can be "Property" or "Component.Property")
     * @return Result containing Property, Container, and any error message
     */
    MCPAUTOMATIONBRIDGE_API FPropertyResolveResult ResolveProperty(
        UObject* Object,
        const FString& PropertyName);

    // =========================================================================
    // Standard Response Builders (reduces MakeShared<FJsonObject> duplication)
    // =========================================================================
    
    /**
     * Create a result object with common fields.
     * Pattern: Result->SetStringField(TEXT("name"), Name);
     */
    inline TSharedPtr<FJsonObject> CreateResultObject()
    {
        return MakeShared<FJsonObject>();
    }
    
    /**
     * Create a result object with a single string field.
     */
    inline TSharedPtr<FJsonObject> CreateResultObject(const FString& Key, const FString& Value)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(Key, Value);
        return Result;
    }
    
    /**
     * Add common verification fields to a result object.
     * Determines actor vs asset automatically.
     */
    MCPAUTOMATIONBRIDGE_API void AddVerification(TSharedPtr<FJsonObject>& Result, UObject* Object);
}

// =============================================================================
// Blueprint Graph Utilities (Editor-only)
// =============================================================================
#if WITH_EDITOR && MCP_HAS_EDGRAPH_SCHEMA_K2

#include "EdGraphSchema_K2.h"

namespace McpBlueprintUtils
{
    // -------------------------------------------------------------------------
    // Pin Finding Utilities
    // -------------------------------------------------------------------------

    /**
     * Find an execution pin on a node by direction.
     */
    MCPAUTOMATIONBRIDGE_API UEdGraphPin* FindExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction);

    /**
     * Find a data pin on a node by direction and optionally by name.
     */
    MCPAUTOMATIONBRIDGE_API UEdGraphPin* FindDataPin(UEdGraphNode* Node, EEdGraphPinDirection Direction, const FName& PreferredName = NAME_None);

    /**
     * Find the preferred event execution output pin in a graph.
     * Prefers custom events, falls back to first available event.
     */
    MCPAUTOMATIONBRIDGE_API UEdGraphPin* FindPreferredEventExec(UEdGraph* Graph);

    // -------------------------------------------------------------------------
    // Pin Type Conversion
    // -------------------------------------------------------------------------

    /**
     * Convert a string type name to an FEdGraphPinType.
     * Supports: float, int, int64, bool, string, name, text, byte, vector, rotator, transform, object, class
     * Also resolves class/struct/enum paths.
     */
    MCPAUTOMATIONBRIDGE_API FEdGraphPinType MakePinType(const FString& TypeName);

    /**
     * Convert an FEdGraphPinType to a human-readable string.
     * Handles containers (Array, Set, Map) and complex types.
     */
    MCPAUTOMATIONBRIDGE_API FString DescribePinType(const FEdGraphPinType& PinType);

    // -------------------------------------------------------------------------
    // Node Creation Utilities
    // -------------------------------------------------------------------------

    /**
     * Create a variable getter node in a graph.
     */
    MCPAUTOMATIONBRIDGE_API UK2Node_VariableGet* CreateVariableGetter(UEdGraph* Graph, const FMemberReference& VarRef, float NodePosX, float NodePosY);

    /**
     * Log a pin connection failure for debugging.
     */
    MCPAUTOMATIONBRIDGE_API void LogConnectionFailure(const TCHAR* Context, UEdGraphPin* SourcePin, UEdGraphPin* TargetPin, const FPinConnectionResponse& Response);

    // -------------------------------------------------------------------------
    // Blueprint Introspection
    // -------------------------------------------------------------------------

    /**
     * Collect all variables from a blueprint as JSON.
     */
    MCPAUTOMATIONBRIDGE_API TArray<TSharedPtr<FJsonValue>> CollectBlueprintVariables(UBlueprint* Blueprint);

    /**
     * Collect all functions from a blueprint as JSON.
     */
    MCPAUTOMATIONBRIDGE_API TArray<TSharedPtr<FJsonValue>> CollectBlueprintFunctions(UBlueprint* Blueprint);
}

#endif // WITH_EDITOR && MCP_HAS_EDGRAPH_SCHEMA_K2