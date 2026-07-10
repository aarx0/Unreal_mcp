// =============================================================================
// McpPropertyReflection.h
// =============================================================================
// Property reflection and JSON conversion utilities for MCP Automation Bridge.
//
// This file provides:
// - Export property values to JSON
// - Apply JSON values to properties
// - Type-safe property access helpers
// - Enum conversion utilities
//
// REFACTORING NOTES:
// - Extracted from McpAutomationBridgeHelpers.h for better organization
// - Reduces include dependency bloat for handlers that only need property access
// - Consolidates duplicated property conversion patterns across handlers
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"
#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/PropertyPortFlags.h"

// =============================================================================
// MCP Property Reflection Namespace
// =============================================================================
namespace McpPropertyReflection
{
    // =========================================================================
    // JSON Value Export (Property -> JSON)
    // =========================================================================

    /**
     * Convert a single Unreal property value from a container into a JSON value.
     * 
     * Supported property types:
     * - Strings (FStrProperty)
     * - Names (FNameProperty)
     * - Booleans (FBoolProperty)
     * - Numeric types (float, double, int32, int64, byte)
     * - Enums (returns name or numeric value)
     * - Object references (returns path string or null)
     * - Soft object/class references (returns soft path or null)
     * - Common structs (FVector, FRotator as arrays)
     * - Arrays (of supported inner types)
     * - Maps (with basic value types)
     * - Sets (of supported element types)
     *
     * Struct properties (other than Vector/Rotator) are exported as a nested JSON
     * object of their child properties; object references as their path string;
     * arrays of structs/objects recurse element-by-element. Recursion is bounded
     * by Depth (see ExportStructToJson) and falls back to ExportText at the cap.
     *
     * @param TargetContainer Pointer to the container holding the property value
     * @param Property The property definition to export
     * @param Depth Current recursion depth (callers pass 0; used internally)
     * @return JSON value representing the property, or null for unsupported types
     */
    MCPAUTOMATIONBRIDGE_API TSharedPtr<FJsonValue> ExportPropertyToJsonValue(
        void* TargetContainer,
        FProperty* Property,
        int32 Depth = 0);

    /**
     * Export all properties of a UObject to a JSON object.
     * @param Object The object to export
     * @param bIncludeTransient If true, include transient properties
     * @return JSON object with all property values
     */
    MCPAUTOMATIONBRIDGE_API TSharedPtr<FJsonObject> ExportObjectToJson(
        UObject* Object, 
        bool bIncludeTransient = false);

    /**
     * Export specific properties of a UObject to a JSON object.
     * @param Object The object to export
     * @param PropertyNames Names of properties to export
     * @return JSON object with specified property values
     */
    MCPAUTOMATIONBRIDGE_API TSharedPtr<FJsonObject> ExportPropertiesToJson(
        UObject* Object,
        const TArray<FName>& PropertyNames);

    // =========================================================================
    // JSON Value Import (JSON -> Property)
    // =========================================================================

    /**
     * Apply a JSON value to a reflected property on a target container.
     * 
     * JSON type is matched strictly to the property's storage kind — a JSON bool
     * only sets a bool, a JSON number only sets a numeric property. Mismatches fail
     * loud rather than coercing. Supports:
     * - Bool (from boolean)
     * - String/Name (from string)
     * - Numeric types, including sub-width ints (from number; out-of-range errors)
     * - Enums (from string name or numeric value)
     * - Object references (from path string)
     * - Soft references (from path string or null)
     * - Structs (Vector/Rotator from array, or struct from JSON object)
     * - Arrays of supported types
     *
     * @param TargetContainer Pointer to the container to modify
     * @param Property The property to set
     * @param ValueField The JSON value to apply
     * @param OutError Receives error message on failure
     * @param Depth Current recursion depth (callers pass 0; used internally)
     * @param OwnerForInstancing UObject to use as the Outer when re-instancing an
     *        Instanced subobject (CPF_InstancedReference) from a JSON object carrying
     *        "__class". Must be threaded from the owning asset so the new subobject
     *        serializes into the asset's package. nullptr disables re-instancing (the
     *        object form then errors, preserving prior behavior).
     * @return true if successful
     */
    MCPAUTOMATIONBRIDGE_API bool ApplyJsonValueToProperty(
        void* TargetContainer,
        FProperty* Property,
        const TSharedPtr<FJsonValue>& ValueField,
        FString& OutError,
        int32 Depth = 0,
        UObject* OwnerForInstancing = nullptr);

    /**
     * Apply multiple JSON values to properties of an object.
     * @param Object The object to modify
     * @param JsonValues Map of property names to JSON values
     * @param OutErrors Optional map to receive property-specific errors
     * @return Number of properties successfully set
     */
    MCPAUTOMATIONBRIDGE_API int32 ApplyJsonValuesToObject(
        UObject* Object,
        const TMap<FName, TSharedPtr<FJsonValue>>& JsonValues,
        TMap<FName, FString>* OutErrors = nullptr);

    // =========================================================================
    // Property Type Utilities
    // =========================================================================

    /**
     * Get a human-readable type name for a property.
     */
    MCPAUTOMATIONBRIDGE_API FString GetPropertyTypeName(FProperty* Property);

    /**
     * Get a property by name from an object's class.
     * @param Object The object to search
     * @param PropertyName The property name to find
     * @return The property, or nullptr if not found
     */
    inline FProperty* FindPropertyByName(UObject* Object, const FName& PropertyName)
    {
        if (!Object)
        {
            return nullptr;
        }
        
        UClass* Class = Object->GetClass();
        if (!Class)
        {
            return nullptr;
        }
        
        return Class->FindPropertyByName(PropertyName);
    }

    // =========================================================================
    // Discriminated typed-value helpers (typed-params migration)
    // =========================================================================

    /** A single typed value pulled from a discriminated-union payload. */
    struct FMcpTypedValue
    {
        FString Field;                 // e.g. "floatValue"
        FString Kind;                  // bool|int|float|string|color|vector|vector2|vector4|struct|array
        TSharedPtr<FJsonValue> Json;   // value, as the correct JSON type
        bool IsValid() const { return Json.IsValid(); }
    };

    enum class EMcpTypedValueParse : uint8 { Ok, None, Ambiguous };

    /**
     * Read exactly one typed value field from a payload:
     * boolValue / intValue / floatValue / stringValue / colorValue{r,g,b,a} /
     * vector2Value{x,y} / vectorValue{x,y,z} / vector4Value{x,y,z,w} /
     * structValue{...} / arrayValue[...]. Each field is a real schema type, so it
     * transmits correctly and the populated field names the caller's intended type.
     * structValue/arrayValue are the open escape for runtime-defined shapes
     * (structs / instanced {"__class",...} / maps and arrays); they are read as a
     * real JSON object/array ONLY -- a stringified container is a fail-loud bug, not
     * rescued here. ApplyJsonValueToProperty is the structural gate for the contents.
     * @return Ok (Out filled) on exactly one; None on zero; Ambiguous on >1
     *         (OutDetail lists the offending field names).
     */
    MCPAUTOMATIONBRIDGE_API EMcpTypedValueParse ReadDiscriminatedValue(
        const TSharedPtr<FJsonObject>& Payload,
        FMcpTypedValue& Out,
        FString& OutDetail);

    /**
     * Cross-check: does the property's real reflected type match the caller's
     * intended value kind (bool/int/float/string/color/vector/vector2/vector4/
     * struct/array)? A mismatch should fail loud rather than silently coerce.
     * The open kinds (struct/array) match any struct/object/map or array/set
     * property respectively — the importer is the real structural gate.
     */
    MCPAUTOMATIONBRIDGE_API bool PropertyMatchesValueKind(
        const FProperty* Property,
        const FString& Kind);

    // =========================================================================
    // Struct Utilities
    // =========================================================================

    /**
     * Convert a Vector to a JSON array [x, y, z].
     */
    inline TSharedPtr<FJsonValue> VectorToJsonValue(const FVector& Vector)
    {
        TArray<TSharedPtr<FJsonValue>> Arr;
        Arr.Add(MakeShared<FJsonValueNumber>(Vector.X));
        Arr.Add(MakeShared<FJsonValueNumber>(Vector.Y));
        Arr.Add(MakeShared<FJsonValueNumber>(Vector.Z));
        return MakeShared<FJsonValueArray>(Arr);
    }

    /**
     * Convert a JSON array to a Vector.
     * @param JsonArray The JSON array [x, y, z]
     * @param OutVector The output vector
     * @return true if conversion succeeded
     */
    inline bool JsonArrayToVector(const TArray<TSharedPtr<FJsonValue>>& JsonArray, FVector& OutVector)
    {
        if (JsonArray.Num() < 3)
        {
            return false;
        }
        
        OutVector.X = JsonArray[0]->AsNumber();
        OutVector.Y = JsonArray[1]->AsNumber();
        OutVector.Z = JsonArray[2]->AsNumber();
        return true;
    }

    /**
     * Convert a JSON object to a Vector.
     * @param JsonObject The JSON object {x, y, z}
     * @param OutVector The output vector
     * @return true if conversion succeeded
     */
    inline bool JsonToVector(const TSharedPtr<FJsonObject>& JsonObject, FVector& OutVector)
    {
        if (!JsonObject.IsValid())
        {
            return false;
        }
        
        double X = 0.0, Y = 0.0, Z = 0.0;
        if (!JsonObject->TryGetNumberField(TEXT("x"), X))
        {
            JsonObject->TryGetNumberField(TEXT("X"), X);
        }
        if (!JsonObject->TryGetNumberField(TEXT("y"), Y))
        {
            JsonObject->TryGetNumberField(TEXT("Y"), Y);
        }
        if (!JsonObject->TryGetNumberField(TEXT("z"), Z))
        {
            JsonObject->TryGetNumberField(TEXT("Z"), Z);
        }

        OutVector = FVector(X, Y, Z);
        return true;
    }

    /**
     * Convert a Vector to a JSON object {x, y, z}.
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
     * Convert a Rotator to a JSON array [pitch, yaw, roll].
     */
    inline TSharedPtr<FJsonValue> RotatorToJsonValue(const FRotator& Rotator)
    {
        TArray<TSharedPtr<FJsonValue>> Arr;
        Arr.Add(MakeShared<FJsonValueNumber>(Rotator.Pitch));
        Arr.Add(MakeShared<FJsonValueNumber>(Rotator.Yaw));
        Arr.Add(MakeShared<FJsonValueNumber>(Rotator.Roll));
        return MakeShared<FJsonValueArray>(Arr);
    }

    /**
     * Convert a JSON array to a Rotator.
     */
    inline bool JsonArrayToRotator(const TArray<TSharedPtr<FJsonValue>>& JsonArray, FRotator& OutRotator)
    {
        if (JsonArray.Num() < 3)
        {
            return false;
        }
        
        OutRotator.Pitch = JsonArray[0]->AsNumber();
        OutRotator.Yaw = JsonArray[1]->AsNumber();
        OutRotator.Roll = JsonArray[2]->AsNumber();
        return true;
    }

    /**
     * Convert a JSON object to a Rotator.
     * @param JsonObject The JSON object {pitch, yaw, roll}
     * @param OutRotator The output rotator
     * @return true if conversion succeeded
     */
    inline bool JsonToRotator(const TSharedPtr<FJsonObject>& JsonObject, FRotator& OutRotator)
    {
        if (!JsonObject.IsValid())
        {
            return false;
        }
        
        double Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
        if (!JsonObject->TryGetNumberField(TEXT("pitch"), Pitch))
        {
            JsonObject->TryGetNumberField(TEXT("Pitch"), Pitch);
        }
        if (!JsonObject->TryGetNumberField(TEXT("yaw"), Yaw))
        {
            JsonObject->TryGetNumberField(TEXT("Yaw"), Yaw);
        }
        if (!JsonObject->TryGetNumberField(TEXT("roll"), Roll))
        {
            JsonObject->TryGetNumberField(TEXT("Roll"), Roll);
        }

        OutRotator = FRotator(Pitch, Yaw, Roll);
        return true;
    }

    /**
     * Convert a Rotator to a JSON object {pitch, yaw, roll}.
     */
    inline TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Rotator)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("pitch"), Rotator.Pitch);
        Obj->SetNumberField(TEXT("yaw"), Rotator.Yaw);
        Obj->SetNumberField(TEXT("roll"), Rotator.Roll);
        return Obj;
    }

    // =========================================================================
    // Container Property Utilities
    // =========================================================================

    /**
     * Check if a property is an array type.
     */
    inline bool IsArrayProperty(FProperty* Property)
    {
        return Property != nullptr && Property->IsA<FArrayProperty>();
    }

    /**
     * Check if a property is a map type.
     */
    inline bool IsMapProperty(FProperty* Property)
    {
        return Property != nullptr && Property->IsA<FMapProperty>();
    }

    /**
     * Check if a property is a set type.
     */
    inline bool IsSetProperty(FProperty* Property)
    {
        return Property != nullptr && Property->IsA<FSetProperty>();
    }

    /**
     * Get the inner element property of an array property.
     */
    inline FProperty* GetArrayInnerProperty(FArrayProperty* ArrayProp)
    {
        return ArrayProp ? ArrayProp->Inner : nullptr;
    }

    /**
     * Get the element count of an array property.
     */
    MCPAUTOMATIONBRIDGE_API int32 GetArrayPropertyCount(void* Container, FArrayProperty* ArrayProp);

    /**
     * Export an array property to a JSON array. Struct, object-reference and
     * other non-primitive inner types recurse via ExportPropertyToJsonValue
     * (the array Inner has offset 0, so each element pointer is a valid
     * container base), falling back to ExportText only when that can't structure
     * the element.
     */
    MCPAUTOMATIONBRIDGE_API TArray<TSharedPtr<FJsonValue>> ExportArrayToJson(
        void* Container,
        FArrayProperty* ArrayProp,
        int32 Depth = 0);

    /**
     * Import a JSON array into an array property (replaces existing elements).
     * Struct and object-reference inners are supported via ApplyJsonValueToProperty.
     */
    MCPAUTOMATIONBRIDGE_API bool ImportJsonToArray(
        void* Container,
        FArrayProperty* ArrayProp,
        const TArray<TSharedPtr<FJsonValue>>& JsonArray,
        FString& OutError,
        int32 Depth = 0,
        UObject* OwnerForInstancing = nullptr);

    /**
     * Export a struct instance as a JSON object of its child properties (reusing
     * the per-property export path, so nested structs, object references and
     * arrays-of-structs are handled recursively). Object references become path
     * strings. Returns null at the recursion-depth cap or for a null struct, so
     * callers can fall back to a textual export.
     *
     * @param StructPtr Pointer to the struct instance (the container base for its
     *                  child property offsets — do NOT pass an already-offset ptr)
     * @param Struct    The struct layout
     * @param Depth     Current recursion depth (the struct's own level)
     */
    MCPAUTOMATIONBRIDGE_API TSharedPtr<FJsonObject> ExportStructToJson(
        void* StructPtr,
        const UScriptStruct* Struct,
        int32 Depth = 0);

    /**
     * Deep-export an Instanced subobject (CPF_InstancedReference — e.g. an input
     * trigger/modifier, a montage AnimNotify) as a nested JSON object carrying its
     * concrete class under "__class" plus its own properties. Used by the export path
     * for instanced object values.
     * @param Subobject The instanced subobject instance
     * @param Depth     Current recursion depth (bounded by the shared cap)
     */
    MCPAUTOMATIONBRIDGE_API TSharedPtr<FJsonObject> ExportInstancedObjectToJson(
        UObject* Subobject,
        int32 Depth = 0);

} // namespace McpPropertyReflection
