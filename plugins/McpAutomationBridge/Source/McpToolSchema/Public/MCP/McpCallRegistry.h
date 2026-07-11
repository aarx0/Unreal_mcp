// Per-action call registry — the server's single source of truth for
// "here's what I know how to do" (see docs/action-declarations.md).
//
// Every (tool, action) pair is an FMcpCall subclass that owns its declaration
// AND implementation (co-located, Chromium-ExtensionFunction-style): the
// params the action reads, which are required, and its flags. Everything else
// derives from it: transport validation rejects params an action doesn't
// declare and missing required params; the published schema folds every
// call's AppendSchema fragment; the decl lint
// (tests/schema/action-decl-lint.ps1) checks handler source against it both
// directions. New actions MUST be classes.
#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "MCP/McpResponseHandle.h"

class FMcpSchemaBuilder;

enum class EMcpParamKind : uint8
{
	String,
	Number,
	Bool,
	Object,
	Array,
	Any,   // multiple shapes accepted (e.g. string-or-object)
};

enum class EMcpCallFlags : uint8
{
	None = 0,
	// Mutates editor/asset state (feeds the apply-receipt work).
	Mutating = 1 << 0,
	// Needs a live GEditor; Execute() rejects (EDITOR_NOT_AVAILABLE /
	// NOT_IMPLEMENTED outside editor builds) before Run() sees the call.
	RequiresEditor = 1 << 2,
};
ENUM_CLASS_FLAGS(EMcpCallFlags)

struct FMcpParamDecl
{
	const TCHAR* Name = nullptr;
	EMcpParamKind Kind = EMcpParamKind::Any;
	bool bRequired = false;

	// Nested kind schema (recursive-kind validation), derived from the fragment's
	// authored sub-schema. For an Object param: the declared member schemas (each a
	// FMcpParamDecl with its own Name/Kind, recursively). For an Array param:
	// exactly one entry describing the element schema (Name = nullptr). Empty for
	// scalars and for structured params authored without a sub-schema. bRequired is
	// unused on nested nodes — nested-required is deliberately not enforced. The
	// pointed-to arrays live in McpDeriveDecl's grow-only pools (process lifetime).
	TConstArrayView<FMcpParamDecl> Members;
	TConstArrayView<FMcpParamDecl> Element;
};

struct FMcpCallDecl
{
	const TCHAR* Tool = nullptr;
	const TCHAR* Action = nullptr;
	TConstArrayView<FMcpParamDecl> Params;
	EMcpCallFlags Flags = EMcpCallFlags::None;

	// Required any-of groups: each inner view is a group of param names, at least
	// one of which must be present in the payload. Validation-only — authored via
	// FMcpSchemaBuilder::RequiredAnyOf() and never emitted into the published JSON.
	// Backed by McpDeriveDecl's grow-only pools (process lifetime).
	TConstArrayView<TConstArrayView<const TCHAR*>> RequiredGroups;
};

/**
 * Base class for every published action. Subclasses co-locate their
 * declaration and implementation. Execute() is the shared pipeline (envelope
 * checks -> Run; the apply-receipt grows here).
 */
class FMcpCall
{
public:
	virtual ~FMcpCall() = default;
	virtual const FMcpCallDecl& GetDecl() const = 0;

	/**
	 * Author this action's schema fragment (schema-from-decls). Migrated classes
	 * emit their params here; the published facade schema folds every call's
	 * fragment, and McpDeriveDecl() reads the validation decl back out of the
	 * same fragment — so schema and decl can never drift. Default no-op leaves
	 * un-migrated (P_*-array) classes unaffected.
	 */
	virtual void AppendSchema(FMcpSchemaBuilder& Builder) const {}

	/** Shared pipeline. Returns true when the call was consumed (a response was sent). */
	bool Execute(class UMcpAutomationBridgeSubsystem& Subsystem,
	             const FString& RequestId,
	             const TSharedPtr<class FJsonObject>& Payload,
	             FMcpResponseHandle ResponseHandle);

protected:
	virtual bool Run(class UMcpAutomationBridgeSubsystem& Subsystem,
	                 const FString& RequestId,
	                 const TSharedPtr<class FJsonObject>& Payload,
	                 FMcpResponseHandle ResponseHandle) = 0;
};

class MCPTOOLSCHEMA_API FMcpCallRegistry
{
public:
	// Exported singleton: copies deleted so dllexport does not instantiate
	// copy ops over the non-copyable TUniquePtr map.
	FMcpCallRegistry() = default;
	FMcpCallRegistry(const FMcpCallRegistry&) = delete;
	FMcpCallRegistry& operator=(const FMcpCallRegistry&) = delete;

	static FMcpCallRegistry& Get();

	/** Register a classed action; its decl comes from the instance. Duplicate (tool, action) is a boot error. */
	void RegisterCall(TUniquePtr<FMcpCall> Call);

	/** nullptr when the pair is unknown (transport then falls back to UNKNOWN_ACTION handling). */
	const FMcpCallDecl* FindDecl(const FString& Tool, const FString& Action) const;

	/** nullptr when the pair is unknown. */
	FMcpCall* FindCall(const FString& Tool, const FString& Action) const;

	/** Every classed call belonging to Tool (case-insensitive), for schema folding. */
	TArray<FMcpCall*> CallsForTool(const FString& Tool) const;

	int32 NumDecls() const { return DeclsByKey.Num(); }

private:
	static FString MakeKey(const TCHAR* Tool, const TCHAR* Action);
	TMap<FString, const FMcpCallDecl*> DeclsByKey;
	TMap<FString, TUniquePtr<FMcpCall>> CallsByKey;
};

/**
 * Derive a lean validation decl from an authored schema fragment. Runs SchemaFn
 * into a builder, then reads the param kinds/required-set back out of the built
 * JSON. The returned reference is owned by grow-only static pools and lives for
 * the process; callers cache it in a function-local static. (schema-from-decls)
 */
MCPTOOLSCHEMA_API const FMcpCallDecl& McpDeriveDecl(const TCHAR* Tool, const TCHAR* Action,
                                  EMcpCallFlags Flags, void (*SchemaFn)(FMcpSchemaBuilder&));
