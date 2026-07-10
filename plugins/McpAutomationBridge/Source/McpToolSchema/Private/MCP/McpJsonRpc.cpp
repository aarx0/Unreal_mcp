#include "MCP/McpJsonRpc.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

FMcpJsonRpcRequest FMcpJsonRpc::ParseRequest(const FString& Body)
{
	FMcpJsonRpcRequest Result;

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	TSharedPtr<FJsonObject> Root;

	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		Result.ErrorType = EMcpJsonRpcError::ParseError;
		return Result;  // bValid = false, Id stays null per JSON-RPC 2.0
	}

	// Extract id early so it can be echoed in InvalidRequest errors
	TSharedPtr<FJsonValue> IdField = Root->TryGetField(TEXT("id"));
	if (IdField.IsValid() && (IdField->Type == EJson::Number || IdField->Type == EJson::String))
	{
		Result.Id = IdField;
		Result.bIsNotification = false;
	}

	// Validate jsonrpc field
	FString Version;
	if (!Root->TryGetStringField(TEXT("jsonrpc"), Version) || Version != TEXT("2.0"))
	{
		Result.ErrorType = EMcpJsonRpcError::InvalidRequest;
		return Result;
	}

	// Method is required
	if (!Root->TryGetStringField(TEXT("method"), Result.Method))
	{
		Result.ErrorType = EMcpJsonRpcError::InvalidRequest;
		return Result;
	}

	// Validate id type if present — null/bool/array/object are invalid per MCP spec
	if (IdField.IsValid() && !Result.Id.IsValid())
	{
		Result.ErrorType = EMcpJsonRpcError::InvalidRequest;
		return Result;
	}

	// No id field = notification
	if (!IdField.IsValid())
	{
		Result.bIsNotification = true;
	}

	// Params is optional
	const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
	if (Root->TryGetObjectField(TEXT("params"), ParamsObj) && ParamsObj)
	{
		Result.Params = *ParamsObj;
	}

	Result.bValid = true;
	return Result;
}

FString FMcpJsonRpc::BuildResponse(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Result)
{
	auto Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Root->SetField(TEXT("id"), Id.IsValid() ? Id : MakeShared<FJsonValueNull>());

	if (Result.IsValid())
	{
		Root->SetObjectField(TEXT("result"), Result);
	}
	else
	{
		Root->SetObjectField(TEXT("result"), MakeShared<FJsonObject>());
	}

	return JsonToString(Root);
}

FString FMcpJsonRpc::BuildError(const TSharedPtr<FJsonValue>& Id, int32 Code, const FString& Message)
{
	auto ErrorObj = MakeShared<FJsonObject>();
	ErrorObj->SetNumberField(TEXT("code"), Code);
	ErrorObj->SetStringField(TEXT("message"), Message);

	auto Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Root->SetField(TEXT("id"), Id.IsValid() ? Id : MakeShared<FJsonValueNull>());
	Root->SetObjectField(TEXT("error"), ErrorObj);

	return JsonToString(Root);
}

TSharedPtr<FJsonObject> FMcpJsonRpc::BuildToolResult(
	bool bSuccess, const FString& Message,
	const TSharedPtr<FJsonObject>& Data, const FString& ErrorCode)
{
	auto Result = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> Content;

	FString Text;
	if (bSuccess)
	{
		Text = Message;
	}
	else if (ErrorCode.IsEmpty())
	{
		Text = FString::Printf(TEXT("Error: %s"), *Message);
	}
	else
	{
		Text = FString::Printf(TEXT("Error [%s]: %s"), *ErrorCode, *Message);
	}
	if (Data.IsValid())
	{
		Text += TEXT("\n\n") + JsonToString(Data);
	}

	auto TextContent = MakeShared<FJsonObject>();
	TextContent->SetStringField(TEXT("type"), TEXT("text"));
	TextContent->SetStringField(TEXT("text"), Text);
	Content.Add(MakeShared<FJsonValueObject>(TextContent));

	Result->SetArrayField(TEXT("content"), Content);
	Result->SetBoolField(TEXT("isError"), !bSuccess);

	// Uniform envelope so typed clients need not parse the text block. `data`
	// holds the handler's result object verbatim — never spread to the top
	// level, where handler keys would collide with the envelope's.
	auto Structured = MakeShared<FJsonObject>();
	Structured->SetBoolField(TEXT("success"), bSuccess);
	Structured->SetStringField(TEXT("message"), Message);
	if (!bSuccess && !ErrorCode.IsEmpty())
	{
		Structured->SetStringField(TEXT("errorCode"), ErrorCode);
	}
	if (Data.IsValid())
	{
		Structured->SetObjectField(TEXT("data"), Data);
	}
	Result->SetObjectField(TEXT("structuredContent"), Structured);

	return Result;
}

FString FMcpJsonRpc::JsonToString(const TSharedPtr<FJsonObject>& Obj)
{
	FString Output;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return Output;
}
