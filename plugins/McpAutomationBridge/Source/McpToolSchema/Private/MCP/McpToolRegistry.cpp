// McpToolRegistry.cpp — Singleton registry for self-describing MCP tool definitions

#include "McpVersionCompatibility.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpToolDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpToolRegistry, Log, All);

const TSet<FString>& FMcpToolRegistry::GetCanonicalToolNames()
{
	static const TSet<FString> CanonicalToolNames = {
		TEXT("manage_asset"),
		TEXT("manage_blueprint"),
		TEXT("control_actor"),
		TEXT("control_editor"),
		TEXT("manage_level"),
		TEXT("build_environment"),
		TEXT("animation_physics"),
		TEXT("system_control"),
		TEXT("manage_sequence"),
		TEXT("inspect"),
		TEXT("manage_audio"),
		TEXT("manage_geometry"),
		TEXT("manage_effect"),
		TEXT("manage_gas"),
		TEXT("manage_character"),
		TEXT("manage_combat"),
		TEXT("manage_ai"),
		TEXT("manage_inventory"),
		TEXT("manage_interaction"),
		TEXT("manage_networking"),
		TEXT("manage_input"),
		TEXT("manage_level_structure")
	};

	return CanonicalToolNames;
}

FMcpToolRegistry& FMcpToolRegistry::Get()
{
	static FMcpToolRegistry Instance;
	return Instance;
}

void FMcpToolRegistry::Register(FMcpToolDefinition* Tool)
{
	if (!ensureMsgf(Tool, TEXT("McpToolRegistry: Register called with null tool")))
	{
		return;
	}

	FScopeLock Lock(&CacheMutex);
	const FString Name = Tool->GetName();
	if (!GetCanonicalToolNames().Contains(Name))
	{
		UE_LOG(LogMcpToolRegistry, Error,
			TEXT("Tool '%s' rejected: not in the canonical tool-name allowlist. "
				"Add it to FMcpToolRegistry::GetCanonicalToolNames or it will never appear in tools/list."),
			*Name);
		ensureMsgf(false, TEXT("Non-canonical MCP tool name '%s'"), *Name);
		return;
	}

	if (FMcpToolDefinition* const* Existing = ToolsByName.Find(Name))
	{
		// Re-registration of the same static instance is benign (unity builds reloading);
		// a different instance under the same name means two tool .cpps collide.
		if (*Existing != Tool)
		{
			UE_LOG(LogMcpToolRegistry, Error,
				TEXT("Tool name '%s' registered by two different tool definitions; keeping the first."),
				*Name);
			ensureMsgf(false, TEXT("Duplicate MCP tool name '%s'"), *Name);
		}
		return;
	}

	Tools.Add(Tool);
	ToolsByName.Add(Name, Tool);
	bCacheValid = false;
}

FMcpToolDefinition* FMcpToolRegistry::FindTool(const FString& Name) const
{
	if (const auto* Found = ToolsByName.Find(Name))
	{
		return *Found;
	}
	return nullptr;
}

TSet<FString> FMcpToolRegistry::GetToolNames() const
{
	TSet<FString> Names;
	Names.Reserve(Tools.Num());
	for (const FMcpToolDefinition* Tool : Tools)
	{
		Names.Add(Tool->GetName());
	}
	return Names;
}

FString FMcpToolRegistry::GetToolCategory(const FString& ToolName) const
{
	if (const FMcpToolDefinition* Tool = FindTool(ToolName))
	{
		return Tool->GetCategory();
	}
	return TEXT("utility");
}

void FMcpToolRegistry::EnsureCache()
{
	if (bCacheValid)
	{
		return;
	}

	CachedToolSchemas.Empty();
	CachedToolSchemas.Reserve(Tools.Num());
	for (FMcpToolDefinition* Tool : Tools)
	{
		CachedToolSchemas.Add(Tool->GetName(), BuildToolJson(Tool));
	}
	bCacheValid = true;
}

TSharedPtr<FJsonObject> FMcpToolRegistry::BuildToolJson(FMcpToolDefinition* Tool)
{
	auto ToolObj = MakeShared<FJsonObject>();
	ToolObj->SetStringField(TEXT("name"), Tool->GetName());
	ToolObj->SetStringField(TEXT("description"), Tool->GetDescription());
	ToolObj->SetStringField(TEXT("category"), Tool->GetCategory());

	TSharedPtr<FJsonObject> InputSchema = Tool->BuildInputSchema();
	if (InputSchema.IsValid())
	{
		ToolObj->SetObjectField(TEXT("inputSchema"), InputSchema);
	}

	return ToolObj;
}

TSharedPtr<FJsonObject> FMcpToolRegistry::GetToolsResponse()
{
	FScopeLock Lock(&CacheMutex);
	EnsureCache();

	TArray<TSharedPtr<FJsonValue>> ToolValues;
	ToolValues.Reserve(Tools.Num());

	for (const FMcpToolDefinition* Tool : Tools)
	{
		if (const auto* Cached = CachedToolSchemas.Find(Tool->GetName()))
		{
			ToolValues.Add(MakeShared<FJsonValueObject>(*Cached));
		}
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("tools"), ToolValues);
	return Result;
}
