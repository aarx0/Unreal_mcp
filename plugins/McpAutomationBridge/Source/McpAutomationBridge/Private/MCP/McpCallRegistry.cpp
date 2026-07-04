#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSubsystem.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

bool FMcpCall::Execute(UMcpAutomationBridgeSubsystem& Subsystem,
                       const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload,
                       FMcpResponseHandle ResponseHandle)
{
	const FMcpCallDecl& Decl = GetDecl();
	if (!Payload.IsValid())
	{
		Subsystem.SendAutomationError(ResponseHandle, RequestId,
			FString::Printf(TEXT("%s payload missing."), Decl.Tool),
			TEXT("INVALID_PAYLOAD"));
		return true;
	}
	if (EnumHasAnyFlags(Decl.Flags, EMcpCallFlags::RequiresEditor))
	{
#if WITH_EDITOR
		if (!GEditor)
		{
			Subsystem.SendAutomationError(ResponseHandle, RequestId,
				TEXT("Editor not available"), TEXT("EDITOR_NOT_AVAILABLE"));
			return true;
		}
#else
		Subsystem.SendAutomationError(ResponseHandle, RequestId,
			FString::Printf(TEXT("%s.%s requires an editor build."), Decl.Tool, Decl.Action),
			TEXT("NOT_IMPLEMENTED"));
		return true;
#endif
	}
	return Run(Subsystem, RequestId, Payload, ResponseHandle);
}

FMcpCallRegistry& FMcpCallRegistry::Get()
{
	static FMcpCallRegistry Registry;
	return Registry;
}

FString FMcpCallRegistry::MakeKey(const TCHAR* Tool, const TCHAR* Action)
{
	return FString(Tool) + TEXT(".") + Action;
}

void FMcpCallRegistry::RegisterDecls(TConstArrayView<FMcpCallDecl> Decls)
{
	for (const FMcpCallDecl& Decl : Decls)
	{
		const FString Key = MakeKey(Decl.Tool, Decl.Action);
		if (DeclsByKey.Contains(Key))
		{
			// Loud-registration convention: a duplicate is a programming error,
			// not something to resolve silently.
			UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
				TEXT("McpCallRegistry: duplicate declaration for '%s' — second registration ignored"), *Key);
			ensureMsgf(false, TEXT("Duplicate MCP call declaration: %s"), *Key);
			continue;
		}
		DeclsByKey.Add(Key, &Decl);
	}
}

void FMcpCallRegistry::RegisterCall(TUniquePtr<FMcpCall> Call)
{
	check(Call.IsValid());
	const FMcpCallDecl& Decl = Call->GetDecl();
	const FString Key = MakeKey(Decl.Tool, Decl.Action);
	if (CallsByKey.Contains(Key))
	{
		UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
			TEXT("McpCallRegistry: duplicate call class for '%s' — second registration ignored"), *Key);
		ensureMsgf(false, TEXT("Duplicate MCP call class: %s"), *Key);
		return;
	}
	DeclsByKey.Add(Key, &Decl);
	CallsByKey.Add(Key, MoveTemp(Call));
}

const FMcpCallDecl* FMcpCallRegistry::FindDecl(const FString& Tool, const FString& Action) const
{
	const FMcpCallDecl* const* Found = DeclsByKey.Find(Tool + TEXT(".") + Action);
	return Found ? *Found : nullptr;
}

FMcpCall* FMcpCallRegistry::FindCall(const FString& Tool, const FString& Action) const
{
	const TUniquePtr<FMcpCall>* Found = CallsByKey.Find(Tool + TEXT(".") + Action);
	return Found ? Found->Get() : nullptr;
}
