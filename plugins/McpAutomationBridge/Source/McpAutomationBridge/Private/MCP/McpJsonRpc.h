#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/** Why the request was rejected (if bValid == false). */
enum class EMcpJsonRpcError : uint8
{
	None,            // No error — request is valid
	ParseError,      // JSON could not be deserialized (-32700, id must be null)
	InvalidRequest   // Valid JSON but bad structure (-32600, echo id if available)
};

/**
 * Parsed JSON-RPC 2.0 request.
 */
struct FMcpJsonRpcRequest
{
	FString Method;
	TSharedPtr<FJsonValue> Id;
	TSharedPtr<FJsonObject> Params;
	bool bIsNotification = false;  // true if no "id" field
	bool bValid = false;
	EMcpJsonRpcError ErrorType = EMcpJsonRpcError::None;
};

/**
 * Static JSON-RPC 2.0 helpers for MCP Streamable HTTP transport.
 * No state — pure utility functions.
 */
class FMcpJsonRpc
{
public:
	// JSON-RPC error codes (standard)
	static constexpr int32 ErrorParseError = -32700;
	static constexpr int32 ErrorInvalidRequest = -32600;
	static constexpr int32 ErrorMethodNotFound = -32601;
	static constexpr int32 ErrorInvalidParams = -32602;
	static constexpr int32 ErrorInternalError = -32603;

	/** Parse a JSON-RPC 2.0 request from raw body string. */
	static FMcpJsonRpcRequest ParseRequest(const FString& Body);

	/** Build a JSON-RPC 2.0 success response string. */
	static FString BuildResponse(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Result);

	/** Build a JSON-RPC 2.0 error response string. */
	static FString BuildError(const TSharedPtr<FJsonValue>& Id, int32 Code, const FString& Message);

	/** Build an MCP tool result object (content array + isError). */
	static TSharedPtr<FJsonObject> BuildToolResult(
		bool bSuccess, const FString& Message,
		const TSharedPtr<FJsonObject>& Data = nullptr,
		const FString& ErrorCode = FString());

private:
	/** Serialize a JSON object to a compact string. */
	static FString JsonToString(const TSharedPtr<FJsonObject>& Obj);
};
