#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Dom/JsonValue.h"
#include <atomic>

class UMcpAutomationBridgeSubsystem;
class FSocket;
class FRunnableThread;
class FEvent;
class ISocketSubsystem;

/**
 * Native MCP Streamable HTTP transport (pull-only: one request -> one response).
 * Raw socket HTTP server speaking JSON-RPC 2.0 (MCP protocol 2025-03-26).
 * tools/call parks the connection while the GameThread handler runs, then answers
 * with a single plain application/json response — no SSE streaming, no server push.
 * (GET /mcp returns 405; see docs/pull-architecture.md.)
 */
class FMcpNativeTransport : public FRunnable
{
public:
	explicit FMcpNativeTransport(UMcpAutomationBridgeSubsystem* InSubsystem);
	~FMcpNativeTransport();

	/** Start HTTP server on given host:port. Returns false on failure. */
	bool Start(int32 Port, const FString& PluginDir,
		const FString& InUserInstructions = TEXT(""),
		const FString& InListenHost = TEXT("127.0.0.1"),
		bool bInAllowNonLoopback = false);

	/** Shut down HTTP server, stop accept thread, close all parked connections. */
	void Shutdown();

	/** Status accessors for UI. */
	bool IsRunning() const { return Thread != nullptr && !bStopping.load(); }
	int32 GetListenPort() const { return ListenPort; }
	int32 GetActiveSessionCount() const;
	int32 GetTotalToolCount() const;

	/**
	 * Complete a parked tools/call request with the handler's result: writes a single
	 * plain HTTP/JSON-RPC response, then closes the connection.
	 * Called from Subsystem::SendAutomationResponse when Socket==nullptr.
	 * Returns true if a pending request was found and completed.
	 */
	bool CompletePendingRequest(const FString& RequestId, bool bSuccess,
		const FString& Message, const TSharedPtr<FJsonObject>& Result,
		const FString& ErrorCode);

	/** Check if a request ID belongs to a parked (awaiting-response) connection. */
	bool HasPendingRequest(const FString& RequestId) const;

	/** Clean up requests that have exceeded the timeout. Called from Tick. */
	void CleanupStaleRequests();

	// FRunnable interface
	virtual bool Init() override { return true; }
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	/** Parsed HTTP request (minimal — only POST/DELETE /mcp). */
	struct FParsedHttpRequest
	{
		FString Method;      // "GET", "POST", or "DELETE"
		FString Path;        // "/mcp"
		FString Body;
		FString SessionId;   // from Mcp-Session-Id header
		FString Accept;      // from Accept header
		FString CapabilityToken;  // from X-MCP-Capability-Token header
		FString Origin;      // from Origin header for browser CORS validation
		int32 ContentLength = 0;
	};

	/** Parked tools/call request: the socket is held until the GameThread handler
	 *  completes, then answered with a single plain HTTP/JSON response (pull-only,
	 *  no SSE). */
	struct FPendingConnection
	{
		FSocket* Socket = nullptr;
		TSharedPtr<FJsonValue> JsonRpcId;
		double StartTime = 0.0;
		FString ToolName;
		FString SessionId;   // for touching ActiveSessions
		FString CorsOrigin;  // preserved from the request for the response headers
		// Per-action param-check findings under bypassParamCheck:true — merged
		// into the response envelope so the caller sees what was flagged.
		// Written once at park time, read by the response writer after; no lock.
		TArray<FString> ParamWarnings;
		FCriticalSection WriteMutex;  // protects the socket write
	};

	enum class ESessionValidationResult
	{
		Valid,
		Missing,
		Invalid
	};

	// Accept loop: handle one client connection (runs on ThreadPool)
	void HandleConnection(FSocket* ClientSocket);

	// Low-level socket helpers
	static bool SendAllBytes(FSocket* Socket, const uint8* Data, int32 Length);

	// HTTP parsing and response helpers
	bool ReadHttpRequest(FSocket* Socket, FParsedHttpRequest& OutRequest);
	bool IsCorsEnabled() const;
	bool IsAllowedCorsOrigin(const FString& Origin) const;
	void AppendCorsHeaders(FString& Response, const FString& Origin) const;
	bool SendHttpResponse(FSocket* Socket, int32 StatusCode,
		const FString& ContentType, const FString& Body,
		const TMap<FString, FString>& ExtraHeaders = {},
		const FString& CorsOrigin = FString());

	// JSON-RPC method handlers (return response body string)
	FString HandleInitialize(const TSharedPtr<FJsonObject>& Params,
		const TSharedPtr<FJsonValue>& Id, FString& OutSessionId);
	FString HandleToolsList(const TSharedPtr<FJsonValue>& Id);
	void HandleToolsCall(const TSharedPtr<FJsonObject>& Params,
		const TSharedPtr<FJsonValue>& Id, FSocket* ClientSocket,
		const FString& SessionId, const FString& CorsOrigin);

	// Session validation
	ESessionValidationResult ValidateSession(const FString& SessionId, FString& OutError);
	static int32 GetSessionValidationStatusCode(ESessionValidationResult Result);
	void TouchSession(const FString& SessionId);

	UMcpAutomationBridgeSubsystem* Subsystem;
	int32 ListenPort = 0;

	// Server identity & instructions (loaded from server-info.json + settings)
	FString ServerName = TEXT("unreal-mcp");
	FString ServerVersion = TEXT("0.6.0");
	FString BaseInstructions;
	FString UserInstructions;

	// Bind configuration
	FString ListenHost = TEXT("127.0.0.1");
	bool bAllowNonLoopback = false;

	// Socket infrastructure
	FSocket* ListenSocket = nullptr;
	FRunnableThread* Thread = nullptr;
	FEvent* StopEvent = nullptr;
	FEvent* BindCompleteEvent = nullptr;
	std::atomic<bool> bStopping{false};
	std::atomic<bool> bBindSuccess{false};
	std::atomic<int32> ActiveConnectionCount{0};
	std::atomic<int32> PendingAsyncWrites{0};  // tracks in-flight async completion writes
	static constexpr int32 MaxConcurrentConnections = 16;

	// Session state (multi-session, with activity tracking)
	TMap<FString, double> ActiveSessions;  // SessionId → LastActivityTime
	mutable FCriticalSection SessionMutex;

	static constexpr double SessionTimeoutSeconds = 3600.0;  // 1 hour

	// Parked tools/call connections awaiting their single response (RequestId → connection)
	TMap<FString, TSharedPtr<FPendingConnection>> PendingConnections;
	mutable FCriticalSection PendingConnectionsMutex;

	static constexpr double RequestTimeoutSeconds = 300.0;  // 5 minutes

};
