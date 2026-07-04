// Per-action call registry — the server's single source of truth for
// "here's what I know how to do" (see docs/action-declarations.md).
//
// Every (tool, action) pair registers a declaration: the params the action
// reads, which are required, and its flags. Everything else derives from it:
// transport validation rejects params an action doesn't declare and missing
// required params; the published schema will derive from it (Stage 3); the
// decl lint (tests/schema/action-decl-lint.ps1) checks handler source against
// it both directions.
//
// Two backing kinds during the strangler-fig migration:
//   - SHIM: declaration only; dispatch still flows through the legacy family
//     handler's action branch. The overwhelming majority today.
//   - CLASS: an FMcpCall subclass owns declaration AND implementation
//     (co-located, Chromium-ExtensionFunction-style). New actions MUST be
//     classes; families convert opportunistically, deleting their legacy
//     branch in the same commit.
#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "McpAutomationBridgeGlobals.h"

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
	// Declaration could not be verified against a reachable handler branch
	// (dead action, unreachable dispatch). Validation SKIPS these — loudly,
	// via the lint — instead of rejecting on made-up truth.
	UnverifiedDecl = 1 << 1,
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
};

struct FMcpCallDecl
{
	const TCHAR* Tool = nullptr;
	const TCHAR* Action = nullptr;
	TConstArrayView<FMcpParamDecl> Params;
	EMcpCallFlags Flags = EMcpCallFlags::None;
};

/**
 * Base class for fully-migrated actions (the CLASS backing kind).
 * Subclasses co-locate their declaration and implementation. Execute() is the
 * shared pipeline (envelope checks -> Run; the apply-receipt grows here);
 * Run() mirrors the legacy family handlers so a branch converts mechanically.
 */
class FMcpCall
{
public:
	virtual ~FMcpCall() = default;
	virtual const FMcpCallDecl& GetDecl() const = 0;

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

class FMcpCallRegistry
{
public:
	static FMcpCallRegistry& Get();

	/** Register a family's declarations (SHIM backing). Duplicate (tool, action) is a boot error. */
	void RegisterDecls(TConstArrayView<FMcpCallDecl> Decls);

	/** Register a classed action (CLASS backing); its decl comes from the instance. */
	void RegisterCall(TUniquePtr<FMcpCall> Call);

	/** nullptr when the pair is unknown (transport then falls back to UNKNOWN_ACTION handling). */
	const FMcpCallDecl* FindDecl(const FString& Tool, const FString& Action) const;

	/** nullptr for shimmed actions — dispatch stays with the legacy family handler. */
	FMcpCall* FindCall(const FString& Tool, const FString& Action) const;

	int32 NumDecls() const { return DeclsByKey.Num(); }

private:
	static FString MakeKey(const TCHAR* Tool, const TCHAR* Action);
	TMap<FString, const FMcpCallDecl*> DeclsByKey;
	TMap<FString, TUniquePtr<FMcpCall>> CallsByKey;
};
