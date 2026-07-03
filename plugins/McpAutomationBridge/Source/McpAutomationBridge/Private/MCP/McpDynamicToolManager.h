#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpToolRegistry;

/**
 * Manages MCP tool visibility at runtime.
 *
 * Enablement is PER-SESSION: Initialize() builds the immutable default state
 * (the template every session starts from); manage_tools mutations write a
 * per-session overlay keyed by Mcp-Session-Id, so one client trimming its
 * tool set can no longer disable tools under concurrent sessions. reset (or
 * reconnecting) drops the session's overlay.
 */
class FMcpDynamicToolManager
{
public:
	/** Initialize the default state from the tool registry. */
	void Initialize(const FMcpToolRegistry& Registry, bool bLoadAllTools = false);

	/** Effective enablement for a session (tool AND category must be enabled).
	 *  An empty SessionId queries the defaults. */
	bool IsToolEnabled(const FString& ToolName, const FString& SessionId = FString()) const;

	/** Effective enabled tool names for a session (empty = defaults). */
	TSet<FString> GetEnabledToolNames(const FString& SessionId = FString()) const;

	/**
	 * Last mutating manage_tools action THIS session performed — a disabled
	 * tool can only be the same session's earlier doing. Returns false if the
	 * session never mutated its overlay.
	 */
	bool GetLastMutation(const FString& SessionId, FString& OutAction, double& OutSecondsAgo) const;

	/** Dispatch a manage_tools action against the session's overlay. */
	TSharedPtr<FJsonObject> HandleAction(const FString& Action,
		const TSharedPtr<FJsonObject>& Args, const FString& SessionId);

	/** Discard a session's overlay (session expired or DELETE /mcp). */
	void DropSession(const FString& SessionId);

private:
	struct FToolState
	{
		FString Name;
		FString Category;
		bool bEnabled = true;
	};

	struct FCategoryState
	{
		FString Name;
		bool bEnabled = true;
		int32 ToolCount = 0;
	};

	/** Per-session enablement overrides; absent keys fall back to defaults. */
	struct FSessionOverrides
	{
		TMap<FString, bool> Tools;
		TMap<FString, bool> Categories;
		FString LastMutationAction;
		double LastMutationTime = 0.0;
	};

	/** Defaults — written only by Initialize(). */
	TMap<FString, FToolState> ToolStates;
	TMap<FString, FCategoryState> CategoryStates;

	TMap<FString, FSessionOverrides> SessionOverrides;

	/** Protects ToolStates, CategoryStates, SessionOverrides. */
	mutable FCriticalSection StateMutex;

	// Lock-free impls — caller must hold StateMutex. Session may be null (defaults).
	const FSessionOverrides* FindSession_NoLock(const FString& SessionId) const;
	bool IsToolEnabled_NoLock(const FString& ToolName, const FSessionOverrides* Session) const;
	bool IsCategoryEnabled_NoLock(const FString& Category, const FSessionOverrides* Session) const;
	void RecordMutation_NoLock(FSessionOverrides& Session, const FString& Action, bool bChanged);

	// Actions — caller must hold StateMutex.
	TSharedPtr<FJsonObject> ListTools(const FSessionOverrides* Session) const;
	TSharedPtr<FJsonObject> ListCategories(const FSessionOverrides* Session) const;
	TSharedPtr<FJsonObject> GetStatus(const FSessionOverrides* Session) const;
	TSharedPtr<FJsonObject> EnableTools(FSessionOverrides& Session, const TArray<FString>& ToolNames, bool& bOutChanged);
	TSharedPtr<FJsonObject> DisableTools(FSessionOverrides& Session, const TArray<FString>& ToolNames, bool& bOutChanged);
	TSharedPtr<FJsonObject> EnableCategory(FSessionOverrides& Session, const FString& Category, bool& bOutChanged);
	TSharedPtr<FJsonObject> DisableCategory(FSessionOverrides& Session, const FString& Category, bool& bOutChanged);
	TSharedPtr<FJsonObject> Reset(const FString& SessionId, bool& bOutChanged);

	static bool IsProtectedTool(const FString& Name);
	static bool IsProtectedCategory(const FString& Name);
};
