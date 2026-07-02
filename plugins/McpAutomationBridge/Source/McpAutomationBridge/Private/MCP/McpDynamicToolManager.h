#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpToolRegistry;

/**
 * Manages MCP tool visibility at runtime.
 * Port of src/tools/dynamic-tool-manager.ts.
 */
class FMcpDynamicToolManager
{
public:
	/** Initialize from tool registry (self-describing C++ tool classes). */
	void Initialize(const FMcpToolRegistry& Registry, bool bLoadAllTools = false);

	/** Check if a tool is enabled (tool AND category must be enabled). */
	bool IsToolEnabled(const FString& ToolName) const;

	/** Get set of all currently enabled tool names. */
	TSet<FString> GetEnabledToolNames() const;

	/**
	 * Last mutating manage_tools action that changed enablement (state is shared
	 * across all sessions, so any session's toggle explains another session's
	 * TOOL_DISABLED). Returns false if no mutation has happened yet.
	 */
	bool GetLastMutation(FString& OutAction, double& OutSecondsAgo) const;

	/** Dispatch a manage_tools action. Returns JSON result for the response. */
	TSharedPtr<FJsonObject> HandleAction(const FString& Action,
		const TSharedPtr<FJsonObject>& Args);

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
		int32 EnabledCount = 0;
	};

	TMap<FString, FToolState> ToolStates;
	TMap<FString, FCategoryState> CategoryStates;

	/** Snapshot of initial enabled state from Initialize(), keyed by tool name. */
	TMap<FString, bool> InitialToolEnabled;
	TMap<FString, bool> InitialCategoryEnabled;

	FString LastMutationAction;
	double LastMutationTime = 0.0;

	/** Protects ToolStates, CategoryStates, InitialToolEnabled,
	 *  InitialCategoryEnabled, LastMutationAction, LastMutationTime. */
	mutable FCriticalSection StateMutex;

	/** Lock-free impl — caller must hold StateMutex. */
	bool IsToolEnabled_NoLock(const FString& ToolName) const;

	/** Record a state-changing action — caller must hold StateMutex. */
	void RecordMutation_NoLock(const FString& Action, bool bChanged);

	// Actions
	TSharedPtr<FJsonObject> ListTools();
	TSharedPtr<FJsonObject> ListCategories();
	TSharedPtr<FJsonObject> EnableTools(const TArray<FString>& ToolNames, bool& bOutChanged);
	TSharedPtr<FJsonObject> DisableTools(const TArray<FString>& ToolNames, bool& bOutChanged);
	TSharedPtr<FJsonObject> EnableCategory(const FString& Category, bool& bOutChanged);
	TSharedPtr<FJsonObject> DisableCategory(const FString& Category, bool& bOutChanged);
	TSharedPtr<FJsonObject> GetStatus();
	TSharedPtr<FJsonObject> Reset(bool& bOutChanged);

	static bool IsProtectedTool(const FString& Name);
	static bool IsProtectedCategory(const FString& Name);
};
