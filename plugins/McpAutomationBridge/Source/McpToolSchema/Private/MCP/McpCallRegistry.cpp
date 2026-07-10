// Registry container only. FMcpCall::Execute lives downstream in the main
// module (McpCallExecute.cpp): it drives the subsystem + GEditor, which this
// upstream module cannot see.
#include "MCP/McpCallRegistry.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpCallRegistry, Log, All);

FMcpCallRegistry& FMcpCallRegistry::Get()
{
	static FMcpCallRegistry Registry;
	return Registry;
}

FString FMcpCallRegistry::MakeKey(const TCHAR* Tool, const TCHAR* Action)
{
	return FString(Tool) + TEXT(".") + Action;
}

void FMcpCallRegistry::RegisterCall(TUniquePtr<FMcpCall> Call)
{
	check(Call.IsValid());
	const FMcpCallDecl& Decl = Call->GetDecl();
	const FString Key = MakeKey(Decl.Tool, Decl.Action);
	if (CallsByKey.Contains(Key))
	{
		UE_LOG(LogMcpCallRegistry, Error,
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

TArray<FMcpCall*> FMcpCallRegistry::CallsForTool(const FString& Tool) const
{
	TArray<FMcpCall*> Result;
	for (const TPair<FString, TUniquePtr<FMcpCall>>& Pair : CallsByKey)
	{
		FMcpCall* Call = Pair.Value.Get();
		if (Call && Tool.Equals(Call->GetDecl().Tool, ESearchCase::IgnoreCase))
		{
			Result.Add(Call);
		}
	}
	return Result;
}
