// McpSchemaBuilder.h — Fluent builder for MCP tool inputSchema JSON

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * Fluent builder for JSON Schema objects used as MCP tool inputSchema.
 *
 * Usage:
 *   FMcpSchemaBuilder()
 *       .StringEnum(TEXT("action"), {TEXT("spawn"), TEXT("delete")}, TEXT("Action to perform"))
 *       .String(TEXT("name"), TEXT("Actor name"))
 *       .Object(TEXT("location"), TEXT("3D location"), [](FMcpSchemaBuilder& S) {
 *           S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
 *       })
 *       .Required({TEXT("action")})
 *       .Build();
 */
class MCPTOOLSCHEMA_API FMcpSchemaBuilder
{
public:
	FMcpSchemaBuilder();

	/** String property. */
	FMcpSchemaBuilder& String(const FString& Name, const FString& Description);

	/** String property constrained to an enum of values. */
	FMcpSchemaBuilder& StringEnum(const FString& Name, const TArray<FString>& Values,
		const FString& Description);

	/** String property fixed to a single constant value (JSON Schema `const`).
	 *  The discriminator in per-action oneOf branches (Phase 3 per-action schemas). */
	FMcpSchemaBuilder& StringConst(const FString& Name, const FString& ConstValue);

	/** Number property. */
	FMcpSchemaBuilder& Number(const FString& Name, const FString& Description = FString());

	/** Boolean property. */
	FMcpSchemaBuilder& Bool(const FString& Name, const FString& Description = FString());

	/** Integer property. */
	FMcpSchemaBuilder& Integer(const FString& Name, const FString& Description = FString());

	/** Object property with optional nested sub-schema. */
	FMcpSchemaBuilder& Object(const FString& Name, const FString& Description,
		TFunction<void(FMcpSchemaBuilder&)> SubBuilder = nullptr);

	/** Array property with item type (default: "string"). */
	FMcpSchemaBuilder& Array(const FString& Name, const FString& Description,
		const FString& ItemType = TEXT("string"));

	/** Array of objects property with item schema. */
	FMcpSchemaBuilder& ArrayOfObjects(const FString& Name, const FString& Description,
		TFunction<void(FMcpSchemaBuilder&)> ItemBuilder = nullptr);

	/** Declare required property names. Can be called multiple times. */
	FMcpSchemaBuilder& Required(const TArray<FString>& Names);

	/** Drop all accumulated required names (properties are kept). The
	 *  schema-from-decls fold uses this: per-action required lives in each
	 *  action's derived decl, not the flat tool-level 'required' — which would
	 *  otherwise force one action's required param onto every action. */
	FMcpSchemaBuilder& ClearRequired();

	/** Declare a required any-of group: at least one of these param names must be
	 *  present in the payload (presence only; the handler owns value checks).
	 *  Validation-only side-channel — recorded in builder state and read back by
	 *  McpDeriveDecl() into the action's decl; Build() writes NOTHING for it, so
	 *  the published schema is unchanged. Members are top-level params of this
	 *  fragment. */
	FMcpSchemaBuilder& RequiredAnyOf(const TArray<FString>& GroupMemberNames);

	/** Build the final inputSchema JSON object. */
	TSharedPtr<FJsonObject> Build() const;

	/** Any-of groups recorded by RequiredAnyOf(), for McpDeriveDecl() to fold into
	 *  the validation decl. Deliberately absent from Build()'s output. */
	const TArray<TArray<FString>>& GetRequiredGroups() const { return RequiredGroups; }

private:
	TSharedPtr<FJsonObject> Properties;
	TArray<FString> RequiredFields;
	TArray<TArray<FString>> RequiredGroups;

	void AddProperty(const FString& Name, const TSharedPtr<FJsonObject>& PropSchema);
};
