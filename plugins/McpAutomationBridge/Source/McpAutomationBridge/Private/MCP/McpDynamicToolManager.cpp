#include "MCP/McpDynamicToolManager.h"
#include "MCP/McpToolRegistry.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpToolManager, Log, All);

// ─── Protected lists ────────────────────────────────────────────────────────

bool FMcpDynamicToolManager::IsProtectedTool(const FString& Name)
{
	return Name == TEXT("manage_tools") || Name == TEXT("inspect");
}

bool FMcpDynamicToolManager::IsProtectedCategory(const FString& Name)
{
	return Name == TEXT("core");
}

// ─── Initialize ─────────────────────────────────────────────────────────────

void FMcpDynamicToolManager::Initialize(const FMcpToolRegistry& Registry, bool bLoadAllTools)
{
	FScopeLock Lock(&StateMutex);
	ToolStates.Empty();
	CategoryStates.Empty();
	SessionOverrides.Empty();

	for (const FString& ToolName : Registry.GetToolNames())
	{
		FString Category = Registry.GetToolCategory(ToolName);

		bool bEnabled = bLoadAllTools || (Category == TEXT("core"));

		FToolState& TS = ToolStates.Add(ToolName);
		TS.Name = ToolName;
		TS.Category = Category;
		TS.bEnabled = bEnabled;

		FCategoryState& CS = CategoryStates.FindOrAdd(Category);
		if (CS.Name.IsEmpty())
		{
			CS.Name = Category;
			CS.bEnabled = bEnabled;
			CS.ToolCount = 0;
		}
		CS.ToolCount++;
	}

	UE_LOG(LogMcpToolManager, Log, TEXT("Initialized from registry with %d tools across %d categories"),
		ToolStates.Num(), CategoryStates.Num());
}

// ─── Query ──────────────────────────────────────────────────────────────────

const FMcpDynamicToolManager::FSessionOverrides* FMcpDynamicToolManager::FindSession_NoLock(
	const FString& SessionId) const
{
	return SessionId.IsEmpty() ? nullptr : SessionOverrides.Find(SessionId);
}

bool FMcpDynamicToolManager::IsCategoryEnabled_NoLock(
	const FString& Category, const FSessionOverrides* Session) const
{
	if (Session)
	{
		if (const bool* Override = Session->Categories.Find(Category))
		{
			return *Override;
		}
	}
	const FCategoryState* CS = CategoryStates.Find(Category);
	return !CS || CS->bEnabled;
}

bool FMcpDynamicToolManager::IsToolEnabled_NoLock(
	const FString& ToolName, const FSessionOverrides* Session) const
{
	const FToolState* TS = ToolStates.Find(ToolName);
	if (!TS) return false;

	bool bToolEnabled = TS->bEnabled;
	if (Session)
	{
		if (const bool* Override = Session->Tools.Find(ToolName))
		{
			bToolEnabled = *Override;
		}
	}
	return bToolEnabled && IsCategoryEnabled_NoLock(TS->Category, Session);
}

bool FMcpDynamicToolManager::IsToolEnabled(const FString& ToolName, const FString& SessionId) const
{
	FScopeLock Lock(&StateMutex);
	return IsToolEnabled_NoLock(ToolName, FindSession_NoLock(SessionId));
}

TSet<FString> FMcpDynamicToolManager::GetEnabledToolNames(const FString& SessionId) const
{
	FScopeLock Lock(&StateMutex);
	const FSessionOverrides* Session = FindSession_NoLock(SessionId);
	TSet<FString> Result;
	for (const auto& Pair : ToolStates)
	{
		if (IsToolEnabled_NoLock(Pair.Key, Session))
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
}

bool FMcpDynamicToolManager::GetLastMutation(
	const FString& SessionId, FString& OutAction, double& OutSecondsAgo) const
{
	FScopeLock Lock(&StateMutex);
	const FSessionOverrides* Session = FindSession_NoLock(SessionId);
	if (!Session || Session->LastMutationTime == 0.0)
	{
		return false;
	}
	OutAction = Session->LastMutationAction;
	OutSecondsAgo = FPlatformTime::Seconds() - Session->LastMutationTime;
	return true;
}

void FMcpDynamicToolManager::RecordMutation_NoLock(
	FSessionOverrides& Session, const FString& Action, bool bChanged)
{
	if (bChanged)
	{
		Session.LastMutationAction = Action;
		Session.LastMutationTime = FPlatformTime::Seconds();
	}
}

void FMcpDynamicToolManager::DropSession(const FString& SessionId)
{
	if (SessionId.IsEmpty())
	{
		return;
	}
	FScopeLock Lock(&StateMutex);
	if (SessionOverrides.Remove(SessionId) > 0)
	{
		UE_LOG(LogMcpToolManager, Verbose, TEXT("Dropped tool overrides for expired session %s"), *SessionId);
	}
}

// ─── Action dispatch ────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMcpDynamicToolManager::HandleAction(
	const FString& Action, const TSharedPtr<FJsonObject>& Args, const FString& SessionId)
{
	// Read-only actions
	if (Action == TEXT("list_tools"))
	{
		FScopeLock Lock(&StateMutex);
		return ListTools(FindSession_NoLock(SessionId));
	}
	if (Action == TEXT("list_categories"))
	{
		FScopeLock Lock(&StateMutex);
		return ListCategories(FindSession_NoLock(SessionId));
	}
	if (Action == TEXT("get_status"))
	{
		FScopeLock Lock(&StateMutex);
		return GetStatus(FindSession_NoLock(SessionId));
	}

	// Mutating actions write this session's overlay only.
	if (SessionId.IsEmpty())
	{
		auto Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"),
			FString::Printf(TEXT("Action '%s' requires a session (enablement is per-session)"), *Action));
		return Err;
	}

	bool bChanged = false;
	TSharedPtr<FJsonObject> Result;

	if (Action == TEXT("reset"))
	{
		FScopeLock Lock(&StateMutex);
		return Reset(SessionId, bChanged);
	}

	// Everything below requires arguments.
	if (!Args.IsValid())
	{
		auto Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), FString::Printf(TEXT("Action '%s' requires arguments"), *Action));
		return Err;
	}

	if (Action == TEXT("enable_tools") || Action == TEXT("disable_tools"))
	{
		TArray<FString> Names;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Args->TryGetArrayField(TEXT("tools"), Arr) && Arr)
		{
			for (const auto& V : *Arr)
			{
				FString S;
				if (V->TryGetString(S)) Names.Add(S);
			}
		}
		FScopeLock Lock(&StateMutex);
		FSessionOverrides& Session = SessionOverrides.FindOrAdd(SessionId);
		Result = (Action == TEXT("enable_tools"))
			? EnableTools(Session, Names, bChanged)
			: DisableTools(Session, Names, bChanged);
		RecordMutation_NoLock(Session, Action, bChanged);
		return Result;
	}

	if (Action == TEXT("enable_category") || Action == TEXT("disable_category"))
	{
		FString Cat;
		Args->TryGetStringField(TEXT("category"), Cat);
		FScopeLock Lock(&StateMutex);
		FSessionOverrides& Session = SessionOverrides.FindOrAdd(SessionId);
		Result = (Action == TEXT("enable_category"))
			? EnableCategory(Session, Cat, bChanged)
			: DisableCategory(Session, Cat, bChanged);
		RecordMutation_NoLock(Session, Action, bChanged);
		return Result;
	}

	auto Err = MakeShared<FJsonObject>();
	Err->SetBoolField(TEXT("success"), false);
	Err->SetStringField(TEXT("error"),
		FString::Printf(TEXT("Unknown action: %s"), *Action));
	return Err;
}

// ─── Views ──────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMcpDynamicToolManager::ListTools(const FSessionOverrides* Session) const
{
	TArray<TSharedPtr<FJsonValue>> ToolsArr;
	for (const auto& Pair : ToolStates)
	{
		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Pair.Value.Name);
		Obj->SetBoolField(TEXT("enabled"), IsToolEnabled_NoLock(Pair.Key, Session));
		Obj->SetStringField(TEXT("category"), Pair.Value.Category);
		Obj->SetBoolField(TEXT("protected"), IsProtectedTool(Pair.Key));
		ToolsArr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("tools"), ToolsArr);
	Result->SetNumberField(TEXT("totalTools"), ToolStates.Num());
	Result->SetStringField(TEXT("scope"), TEXT("session"));
	return Result;
}

TSharedPtr<FJsonObject> FMcpDynamicToolManager::ListCategories(const FSessionOverrides* Session) const
{
	TArray<TSharedPtr<FJsonValue>> CatsArr;
	for (const auto& Pair : CategoryStates)
	{
		int32 EnabledCount = 0;
		for (const auto& ToolPair : ToolStates)
		{
			if (ToolPair.Value.Category == Pair.Key && IsToolEnabled_NoLock(ToolPair.Key, Session))
			{
				EnabledCount++;
			}
		}
		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Pair.Value.Name);
		Obj->SetBoolField(TEXT("enabled"), IsCategoryEnabled_NoLock(Pair.Key, Session));
		Obj->SetNumberField(TEXT("toolCount"), Pair.Value.ToolCount);
		Obj->SetNumberField(TEXT("enabledCount"), EnabledCount);
		CatsArr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("categories"), CatsArr);
	Result->SetStringField(TEXT("scope"), TEXT("session"));
	return Result;
}

TSharedPtr<FJsonObject> FMcpDynamicToolManager::GetStatus(const FSessionOverrides* Session) const
{
	int32 EnabledCount = 0;
	for (const auto& Pair : ToolStates)
	{
		if (IsToolEnabled_NoLock(Pair.Key, Session)) EnabledCount++;
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("totalTools"), ToolStates.Num());
	Result->SetNumberField(TEXT("enabledTools"), EnabledCount);
	Result->SetNumberField(TEXT("disabledTools"), ToolStates.Num() - EnabledCount);
	Result->SetStringField(TEXT("scope"), TEXT("session"));
	Result->SetNumberField(TEXT("sessionOverrideCount"),
		Session ? Session->Tools.Num() + Session->Categories.Num() : 0);

	TArray<TSharedPtr<FJsonValue>> CatsArr;
	for (const auto& Pair : CategoryStates)
	{
		int32 CatEnabled = 0;
		for (const auto& ToolPair : ToolStates)
		{
			if (ToolPair.Value.Category == Pair.Key && IsToolEnabled_NoLock(ToolPair.Key, Session))
			{
				CatEnabled++;
			}
		}
		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Pair.Value.Name);
		Obj->SetBoolField(TEXT("enabled"), IsCategoryEnabled_NoLock(Pair.Key, Session));
		Obj->SetNumberField(TEXT("toolCount"), Pair.Value.ToolCount);
		Obj->SetNumberField(TEXT("enabledCount"), CatEnabled);
		CatsArr.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Result->SetArrayField(TEXT("categories"), CatsArr);
	return Result;
}

// ─── Mutations (per-session overlay) ────────────────────────────────────────

TSharedPtr<FJsonObject> FMcpDynamicToolManager::EnableTools(
	FSessionOverrides& Session, const TArray<FString>& ToolNames, bool& bOutChanged)
{
	TArray<TSharedPtr<FJsonValue>> Enabled;
	TArray<TSharedPtr<FJsonValue>> NotFound;
	bOutChanged = false;

	for (const FString& Name : ToolNames)
	{
		if (!ToolStates.Contains(Name))
		{
			NotFound.Add(MakeShared<FJsonValueString>(Name));
			continue;
		}
		if (!IsToolEnabled_NoLock(Name, &Session))
		{
			bOutChanged = true;
		}
		Session.Tools.Add(Name, true);
		// A tool-level enable must not stay masked by a session category disable.
		const FString& Category = ToolStates[Name].Category;
		if (!IsCategoryEnabled_NoLock(Category, &Session))
		{
			Session.Categories.Add(Category, true);
		}
		Enabled.Add(MakeShared<FJsonValueString>(Name));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("enabled"), Enabled);
	Result->SetArrayField(TEXT("notFound"), NotFound);
	return Result;
}

TSharedPtr<FJsonObject> FMcpDynamicToolManager::DisableTools(
	FSessionOverrides& Session, const TArray<FString>& ToolNames, bool& bOutChanged)
{
	TArray<TSharedPtr<FJsonValue>> Disabled;
	TArray<TSharedPtr<FJsonValue>> NotFound;
	TArray<TSharedPtr<FJsonValue>> Protected;
	bOutChanged = false;

	for (const FString& Name : ToolNames)
	{
		if (IsProtectedTool(Name))
		{
			Protected.Add(MakeShared<FJsonValueString>(Name));
			continue;
		}
		if (!ToolStates.Contains(Name))
		{
			NotFound.Add(MakeShared<FJsonValueString>(Name));
			continue;
		}
		if (IsToolEnabled_NoLock(Name, &Session))
		{
			bOutChanged = true;
		}
		Session.Tools.Add(Name, false);
		Disabled.Add(MakeShared<FJsonValueString>(Name));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("disabled"), Disabled);
	Result->SetArrayField(TEXT("notFound"), NotFound);
	Result->SetArrayField(TEXT("protected"), Protected);
	return Result;
}

TSharedPtr<FJsonObject> FMcpDynamicToolManager::EnableCategory(
	FSessionOverrides& Session, const FString& Category, bool& bOutChanged)
{
	TArray<TSharedPtr<FJsonValue>> Enabled;
	bOutChanged = false;

	if (Category != TEXT("all") && !CategoryStates.Contains(Category))
	{
		auto Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"),
			FString::Printf(TEXT("Category '%s' not found"), *Category));
		return Err;
	}

	for (const auto& CatPair : CategoryStates)
	{
		if (Category != TEXT("all") && CatPair.Key != Category)
		{
			continue;
		}
		if (!IsCategoryEnabled_NoLock(CatPair.Key, &Session))
		{
			bOutChanged = true;
		}
		Session.Categories.Add(CatPair.Key, true);
	}
	for (const auto& ToolPair : ToolStates)
	{
		if (Category != TEXT("all") && ToolPair.Value.Category != Category)
		{
			continue;
		}
		if (!IsToolEnabled_NoLock(ToolPair.Key, &Session))
		{
			bOutChanged = true;
			Enabled.Add(MakeShared<FJsonValueString>(ToolPair.Key));
		}
		Session.Tools.Add(ToolPair.Key, true);
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("category"), Category);
	Result->SetArrayField(TEXT("enabled"), Enabled);
	return Result;
}

TSharedPtr<FJsonObject> FMcpDynamicToolManager::DisableCategory(
	FSessionOverrides& Session, const FString& Category, bool& bOutChanged)
{
	TArray<TSharedPtr<FJsonValue>> Disabled;
	TArray<TSharedPtr<FJsonValue>> Protected;
	bOutChanged = false;

	if (Category != TEXT("all"))
	{
		if (!CategoryStates.Contains(Category))
		{
			auto Err = MakeShared<FJsonObject>();
			Err->SetBoolField(TEXT("success"), false);
			Err->SetStringField(TEXT("error"),
				FString::Printf(TEXT("Category '%s' not found"), *Category));
			return Err;
		}
		if (IsProtectedCategory(Category))
		{
			auto Err = MakeShared<FJsonObject>();
			Err->SetBoolField(TEXT("success"), false);
			Err->SetStringField(TEXT("error"),
				FString::Printf(TEXT("Category '%s' is protected and cannot be disabled"), *Category));
			return Err;
		}
	}

	for (const auto& CatPair : CategoryStates)
	{
		if (Category != TEXT("all") && CatPair.Key != Category)
		{
			continue;
		}
		if (IsProtectedCategory(CatPair.Key))
		{
			continue;
		}
		if (IsCategoryEnabled_NoLock(CatPair.Key, &Session))
		{
			bOutChanged = true;
		}
		Session.Categories.Add(CatPair.Key, false);
	}
	for (const auto& ToolPair : ToolStates)
	{
		if (Category != TEXT("all") && ToolPair.Value.Category != Category)
		{
			continue;
		}
		if (IsProtectedTool(ToolPair.Key) || IsProtectedCategory(ToolPair.Value.Category))
		{
			Protected.Add(MakeShared<FJsonValueString>(ToolPair.Key));
			continue;
		}
		if (IsToolEnabled_NoLock(ToolPair.Key, &Session))
		{
			bOutChanged = true;
			Disabled.Add(MakeShared<FJsonValueString>(ToolPair.Key));
		}
		Session.Tools.Add(ToolPair.Key, false);
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("category"), Category);
	Result->SetArrayField(TEXT("disabled"), Disabled);
	Result->SetArrayField(TEXT("protected"), Protected);
	return Result;
}

TSharedPtr<FJsonObject> FMcpDynamicToolManager::Reset(const FString& SessionId, bool& bOutChanged)
{
	const FSessionOverrides* Session = SessionOverrides.Find(SessionId);
	const int32 OverrideCount = Session ? Session->Tools.Num() + Session->Categories.Num() : 0;
	bOutChanged = SessionOverrides.Remove(SessionId) > 0 && OverrideCount > 0;

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("changed"), OverrideCount);
	Result->SetStringField(TEXT("message"),
		FString::Printf(TEXT("Session overrides cleared (%d dropped); back to server defaults."), OverrideCount));
	return Result;
}
