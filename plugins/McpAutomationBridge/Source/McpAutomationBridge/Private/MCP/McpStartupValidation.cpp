// McpStartupValidation.cpp — boot-time drift checks for the tool/action vocabulary

#include "MCP/McpStartupValidation.h"
#include "MCP/McpConsolidatedActionRouting.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpToolDefinition.h"
#include "Dom/JsonObject.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpStartupValidation, Log, All);

namespace
{
TArray<FString> ExtractSchemaActionEnum(const FMcpToolDefinition* Tool)
{
	TArray<FString> Out;
	if (!Tool)
	{
		return Out;
	}
	const TSharedPtr<FJsonObject> Schema = const_cast<FMcpToolDefinition*>(Tool)->BuildInputSchema();
	if (!Schema.IsValid())
	{
		return Out;
	}
	// Per-action `oneOf` shape (Phase 3): the advertised actions are the `action`
	// const of each branch, not a single flat `properties.action.enum`.
	const TArray<TSharedPtr<FJsonValue>>* OneOf = nullptr;
	if (Schema->TryGetArrayField(TEXT("oneOf"), OneOf) && OneOf)
	{
		for (const TSharedPtr<FJsonValue>& BranchVal : *OneOf)
		{
			const TSharedPtr<FJsonObject> Branch = BranchVal->AsObject();
			if (!Branch.IsValid())
			{
				continue;
			}
			const TSharedPtr<FJsonObject>* BranchProps = nullptr;
			const TSharedPtr<FJsonObject>* BranchActionProp = nullptr;
			FString ConstValue;
			if (Branch->TryGetObjectField(TEXT("properties"), BranchProps) &&
				(*BranchProps)->TryGetObjectField(Tool->GetActionFieldName(), BranchActionProp) &&
				(*BranchActionProp)->TryGetStringField(TEXT("const"), ConstValue))
			{
				Out.Add(ConstValue);
			}
		}
		return Out;
	}

	const TSharedPtr<FJsonObject>* Props = nullptr;
	if (!Schema->TryGetObjectField(TEXT("properties"), Props))
	{
		return Out;
	}
	const TSharedPtr<FJsonObject>* ActionProp = nullptr;
	if (!(*Props)->TryGetObjectField(Tool->GetActionFieldName(), ActionProp))
	{
		return Out;
	}
	const TArray<TSharedPtr<FJsonValue>>* EnumArr = nullptr;
	if (!(*ActionProp)->TryGetArrayField(TEXT("enum"), EnumArr))
	{
		return Out;
	}
	for (const TSharedPtr<FJsonValue>& Value : *EnumArr)
	{
		FString S;
		if (Value->TryGetString(S))
		{
			Out.Add(S);
		}
	}
	return Out;
}

int32 ReportDuplicatesInList(const TCHAR* ToolName, const TCHAR* ListName, const TArray<FString>& List)
{
	int32 Violations = 0;
	TSet<FString> Seen;
	for (const FString& Action : List)
	{
		bool bAlreadyInSet = false;
		Seen.Add(Action, &bAlreadyInSet);
		if (bAlreadyInSet)
		{
			UE_LOG(LogMcpStartupValidation, Error,
				TEXT("%s: list %s contains '%s' twice."), ToolName, ListName, *Action);
			++Violations;
		}
	}
	return Violations;
}

int32 ReportOverlap(const TCHAR* ToolName, const TCHAR* WinnerName, const TArray<FString>& Winner,
	const TCHAR* LoserName, const TArray<FString>& Loser)
{
	int32 Violations = 0;
	for (const FString& Action : Loser)
	{
		if (Winner.Contains(Action))
		{
			UE_LOG(LogMcpStartupValidation, Error,
				TEXT("%s: action '%s' is in both %s and %s; %s is tested first, so the %s "
					"implementation is unreachable. Remove it from one list."),
				ToolName, *Action, WinnerName, LoserName, WinnerName, LoserName);
			++Violations;
		}
	}
	return Violations;
}
}

int32 McpStartupValidation::ValidateActionRouting()
{
	int32 Violations = 0;
	FMcpToolRegistry& Registry = FMcpToolRegistry::Get();

	for (const FString& Name : FMcpToolRegistry::GetCanonicalToolNames())
	{
		if (!Registry.FindTool(Name))
		{
			UE_LOG(LogMcpStartupValidation, Error,
				TEXT("Canonical tool '%s' has no registered definition; its facade .cpp is "
					"missing from the build or its MCP_REGISTER_TOOL did not run."),
				*Name);
			++Violations;
		}
	}

	int32 ToolsChecked = 0;
	for (const McpConsolidatedActions::FMcpToolRouting& Routing : McpConsolidatedActions::GetToolRoutingTable())
	{
		++ToolsChecked;

		for (const McpConsolidatedActions::FMcpRoutedFamily& Family : Routing.RoutedFamilies)
		{
			Violations += ReportDuplicatesInList(Routing.ToolName, Family.Name, Family.List());
		}
		if (Routing.CoreActions)
		{
			Violations += ReportDuplicatesInList(Routing.ToolName, TEXT("Core"), Routing.CoreActions());
		}

		for (int32 i = 0; i < Routing.RoutedFamilies.Num(); ++i)
		{
			const McpConsolidatedActions::FMcpRoutedFamily& Earlier = Routing.RoutedFamilies[i];
			for (int32 j = i + 1; j < Routing.RoutedFamilies.Num(); ++j)
			{
				const McpConsolidatedActions::FMcpRoutedFamily& Later = Routing.RoutedFamilies[j];
				Violations += ReportOverlap(Routing.ToolName, Earlier.Name, Earlier.List(), Later.Name, Later.List());
			}
			if (Routing.CoreActions)
			{
				Violations += ReportOverlap(Routing.ToolName, Earlier.Name, Earlier.List(), TEXT("Core"), Routing.CoreActions());
			}
		}

		if (Routing.SchemaUnion)
		{
			const TArray<FString> Expected = Routing.SchemaUnion();
			const TArray<FString> Published = ExtractSchemaActionEnum(Registry.FindTool(Routing.ToolName));
			if (Published.IsEmpty())
			{
				UE_LOG(LogMcpStartupValidation, Error,
					TEXT("%s: no action enum found in the published schema, but a shared union exists."),
					Routing.ToolName);
				++Violations;
				continue;
			}
			for (const FString& Action : Expected)
			{
				if (!Published.Contains(Action))
				{
					UE_LOG(LogMcpStartupValidation, Error,
						TEXT("%s: routed action '%s' is missing from the published schema enum; "
							"schema-validating clients cannot call it."),
						Routing.ToolName, *Action);
					++Violations;
				}
			}
			for (const FString& Action : Published)
			{
				if (!Expected.Contains(Action))
				{
					UE_LOG(LogMcpStartupValidation, Error,
						TEXT("%s: schema advertises '%s' but no routing list claims it."),
						Routing.ToolName, *Action);
					++Violations;
				}
			}
		}
	}

	if (Violations == 0)
	{
		UE_LOG(LogMcpStartupValidation, Log,
			TEXT("Action routing validation passed (%d canonical tools, %d routing entries)."),
			FMcpToolRegistry::GetCanonicalToolNames().Num(), ToolsChecked);
	}
	else
	{
		ensureMsgf(false,
			TEXT("MCP action routing validation found %d violation(s); see LogMcpStartupValidation errors."),
			Violations);
	}
	return Violations;
}
