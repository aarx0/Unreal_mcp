#include "MCP/McpNativeTransport.h"
#include "MCP/McpJsonRpc.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeSettings.h"
#include "Misc/Guid.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpNativeTransport, Log, All);

// Best-effort description of any active Slate modal window. A request that times
// out can then report whether the editor was blocked on a modal dialog (the
// classic game-thread starvation) versus a genuine handler hang — so we KNOW
// instead of guessing. Game-thread only (Slate access).
static FString McpDescribeActiveModalWindow()
{
	if (!FSlateApplication::IsInitialized())
	{
		return TEXT("slate-uninitialized");
	}
	FSlateApplication& Slate = FSlateApplication::Get();
	TSharedPtr<SWindow> Modal = Slate.GetActiveModalWindow();
	if (!Modal.IsValid())
	{
		return TEXT("none");
	}
	const FString Title = Modal->GetTitle().ToString();
	return Title.IsEmpty() ? TEXT("<untitled modal>") : FString::Printf(TEXT("'%s'"), *Title);
}

// ─── Lifecycle ──────────────────────────────────────────────────────────────

FMcpNativeTransport::FMcpNativeTransport(UMcpAutomationBridgeSubsystem* InSubsystem)
	: Subsystem(InSubsystem)
{
}

FMcpNativeTransport::~FMcpNativeTransport()
{
	Shutdown();
}

bool FMcpNativeTransport::Start(int32 Port, const FString& PluginDir,
	const FString& InUserInstructions, const FString& InListenHost, bool bInAllowNonLoopback)
{
	if (Port <= 0 || Port > 65535)
	{
		UE_LOG(LogMcpNativeTransport, Error,
			TEXT("Invalid Native MCP port %d. Port must be between 1 and 65535."), Port);
		return false;
	}

	ListenPort = Port;
	UserInstructions = InUserInstructions;
	bAllowNonLoopback = bInAllowNonLoopback;

	// Validate listen host against loopback policy
	ListenHost = InListenHost.IsEmpty() ? TEXT("127.0.0.1") : InListenHost;
	if (ListenHost.Equals(TEXT("localhost"), ESearchCase::IgnoreCase))
	{
		ListenHost = TEXT("127.0.0.1");
	}
	else if (ListenHost == TEXT("[::1]"))
	{
		ListenHost = TEXT("::1");
	}

	bool bIsLoopback = ListenHost == TEXT("127.0.0.1") || ListenHost == TEXT("::1");
	if (!bIsLoopback && !bAllowNonLoopback)
	{
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("ListenHost '%s' is not loopback and AllowNonLoopback is false — falling back to 127.0.0.1"),
			*ListenHost);
		ListenHost = TEXT("127.0.0.1");
	}
	else if (!bIsLoopback)
	{
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("SECURITY: Binding to non-loopback address '%s'. Native MCP is exposed to your local network."),
			*ListenHost);
	}

	// Load server identity & instructions from server-info.json
	{
		FString ServerInfoPath = FPaths::Combine(PluginDir, TEXT("Resources/MCP/server-info.json"));
		FString JsonString;
		if (FFileHelper::LoadFileToString(JsonString, *ServerInfoPath))
		{
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				JsonObj->TryGetStringField(TEXT("name"), ServerName);
				JsonObj->TryGetStringField(TEXT("version"), ServerVersion);
				JsonObj->TryGetStringField(TEXT("instructions"), BaseInstructions);

				UE_LOG(LogMcpNativeTransport, Log,
					TEXT("Loaded server-info.json: %s v%s"), *ServerName, *ServerVersion);
			}
			else
			{
				UE_LOG(LogMcpNativeTransport, Warning,
					TEXT("Failed to parse server-info.json -- using defaults"));
			}
		}
		else
		{
			UE_LOG(LogMcpNativeTransport, Warning,
				TEXT("server-info.json not found at %s -- using defaults"), *ServerInfoPath);
		}
	}

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("Tool registry: %d self-describing tools registered"),
		FMcpToolRegistry::Get().GetToolCount());

	// Create stop event and launch accept thread
	StopEvent = FPlatformProcess::GetSynchEventFromPool(true);
	BindCompleteEvent = FPlatformProcess::GetSynchEventFromPool(false);
	bStopping.store(false);
	bBindSuccess.store(false);

	Thread = FRunnableThread::Create(this, TEXT("McpNativeHTTPServer"), 0, TPri_Normal);
	if (!Thread)
	{
		UE_LOG(LogMcpNativeTransport, Error, TEXT("Failed to create HTTP server thread"));
		FPlatformProcess::ReturnSynchEventToPool(BindCompleteEvent);
		BindCompleteEvent = nullptr;
		FPlatformProcess::ReturnSynchEventToPool(StopEvent);
		StopEvent = nullptr;
		return false;
	}

	// Wait for the thread to complete bind/listen before reporting success
	BindCompleteEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(BindCompleteEvent);
	BindCompleteEvent = nullptr;

	if (!bBindSuccess.load())
	{
		UE_LOG(LogMcpNativeTransport, Error,
			TEXT("Failed to start native MCP server on %s:%d — bind/listen failed"), *ListenHost, Port);
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
		FPlatformProcess::ReturnSynchEventToPool(StopEvent);
		StopEvent = nullptr;
		return false;
	}

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("Native MCP server started on http://%s:%d/mcp"), *ListenHost, Port);
	return true;
}

void FMcpNativeTransport::Stop()
{
	// FRunnable::Stop() — lightweight signal, called by Thread->Kill()
	bStopping.store(true);
	if (StopEvent)
	{
		StopEvent->Trigger();
	}
	// Close listen socket to unblock Accept()
	if (ListenSocket)
	{
		ListenSocket->Close();
	}
}

void FMcpNativeTransport::Shutdown()
{
	Stop();  // Signal the accept thread

	// Wait for accept thread to finish
	if (Thread)
	{
		Thread->Kill(true);  // Calls Stop() again (no-op — already signaled)
		delete Thread;
		Thread = nullptr;
	}

	// Accept thread is joined; single-threaded from here. Destroy the listen
	// socket exactly once (Stop() only Close()s it to unblock the accept wait).
	if (ListenSocket)
	{
		ListenSocket->Close();
		if (ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
		{
			SocketSub->DestroySocket(ListenSocket);
		}
		ListenSocket = nullptr;
	}

	// Wait for in-flight connection handlers and async writes to finish.
	// IMPORTANT: Shutdown runs on GameThread. HandleToolsCall dispatches
	// ProcessAutomationRequest → CompletePendingRequest to GameThread via
	// AsyncTask. If we block GameThread with a plain spin-wait, those tasks
	// can never execute and PendingAsyncWrites never reaches 0 → deadlock.
	// Solution: pump GameThread tasks while waiting so queued handlers drain.
	{
		double WaitStart = FPlatformTime::Seconds();
		constexpr double WarnAfter = 15.0;
		bool bWarned = false;
		while (ActiveConnectionCount.load() > 0 || PendingAsyncWrites.load() > 0)
		{
			if (IsInGameThread())
			{
				FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			}
			FPlatformProcess::Sleep(0.01f);
			double Elapsed = FPlatformTime::Seconds() - WaitStart;
			if (!bWarned && Elapsed > WarnAfter)
			{
				UE_LOG(LogMcpNativeTransport, Warning,
					TEXT("Shutdown stalled: %d active connections, %d pending async writes after %.0fs"),
					ActiveConnectionCount.load(), PendingAsyncWrites.load(), Elapsed);
				bWarned = true;
			}
		}
	}

	if (StopEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(StopEvent);
		StopEvent = nullptr;
	}

	// Answer any still-parked tools/call connections with an error, then close them.
	// WriteMutex is taken per-connection to synchronize with in-flight async writes.
	{
		ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		FScopeLock Lock(&PendingConnectionsMutex);
		for (auto& [RequestId, Conn] : PendingConnections)
		{
			if (Conn.IsValid())
			{
				FScopeLock WriteLock(&Conn->WriteMutex);
				if (Conn->Socket)
				{
					// Pull-only: the socket is still awaiting its single HTTP response, so
					// send a plain JSON-RPC error response (not an SSE frame — that would be
					// a malformed reply with no status line/headers).
					FString ErrorJson = FMcpJsonRpc::BuildError(
						Conn->JsonRpcId, FMcpJsonRpc::ErrorInternalError,
						TEXT("Server shutting down"));
					SendHttpResponse(Conn->Socket, 503, TEXT("application/json"),
						ErrorJson, {}, Conn->CorsOrigin);

					Conn->Socket->Close();
					if (SocketSub)
					{
						SocketSub->DestroySocket(Conn->Socket);
					}
					Conn->Socket = nullptr;
				}
			}
		}
		PendingConnections.Empty();
	}

	{
		FScopeLock Lock(&SessionMutex);
		ActiveSessions.Empty();
	}

	UE_LOG(LogMcpNativeTransport, Log, TEXT("Native MCP server stopped"));
}

// ─── Socket Helper ──────────────────────────────────────────────────────────

bool FMcpNativeTransport::SendAllBytes(FSocket* Socket, const uint8* Data, int32 Length)
{
	static constexpr double WriteTimeoutSeconds = 5.0;
	const double Deadline = FPlatformTime::Seconds() + WriteTimeoutSeconds;

	int32 TotalSent = 0;
	while (TotalSent < Length)
	{
		const double Remaining = Deadline - FPlatformTime::Seconds();
		if (Remaining <= 0.0)
		{
			return false;
		}
		if (!Socket->Wait(ESocketWaitConditions::WaitForWrite,
			FTimespan::FromSeconds(FMath::Min(Remaining, 1.0))))
		{
			return false;
		}
		int32 BytesSent = 0;
		if (!Socket->Send(Data + TotalSent, Length - TotalSent, BytesSent))
		{
			return false;
		}
		if (BytesSent <= 0)
		{
			return false;
		}
		TotalSent += BytesSent;
	}
	return true;
}

// ─── Accept Loop (FRunnable::Run) ───────────────────────────────────────────

uint32 FMcpNativeTransport::Run()
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSub)
	{
		UE_LOG(LogMcpNativeTransport, Error, TEXT("Failed to get socket subsystem"));
		bBindSuccess.store(false);
		if (BindCompleteEvent) BindCompleteEvent->Trigger();
		return 1;
	}

	ListenSocket = SocketSub->CreateSocket(NAME_Stream,
		TEXT("McpNativeHTTPListenSocket"), FName());
	if (!ListenSocket)
	{
		UE_LOG(LogMcpNativeTransport, Error, TEXT("Failed to create listen socket"));
		bBindSuccess.store(false);
		if (BindCompleteEvent) BindCompleteEvent->Trigger();
		return 1;
	}

	ListenSocket->SetReuseAddr(true);
	ListenSocket->SetNonBlocking(false);

	TSharedRef<FInternetAddr> BindAddr = SocketSub->CreateInternetAddr();
	bool bIsValid = false;
	BindAddr->SetIp(*ListenHost, bIsValid);
	BindAddr->SetPort(ListenPort);

	if (!bIsValid)
	{
		UE_LOG(LogMcpNativeTransport, Error,
			TEXT("Invalid listen host: %s — falling back to 127.0.0.1"), *ListenHost);
		BindAddr->SetIp(TEXT("127.0.0.1"), bIsValid);
	}

	if (!ListenSocket->Bind(*BindAddr))
	{
		UE_LOG(LogMcpNativeTransport, Error,
			TEXT("Failed to bind to %s:%d"), *ListenHost, ListenPort);
		SocketSub->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
		bBindSuccess.store(false);
		if (BindCompleteEvent) BindCompleteEvent->Trigger();
		return 1;
	}

	if (!ListenSocket->Listen(5))
	{
		UE_LOG(LogMcpNativeTransport, Error, TEXT("Failed to listen on socket"));
		SocketSub->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
		bBindSuccess.store(false);
		if (BindCompleteEvent) BindCompleteEvent->Trigger();
		return 1;
	}

	// Signal Start() that bind/listen succeeded
	bBindSuccess.store(true);
	if (BindCompleteEvent) BindCompleteEvent->Trigger();

	UE_LOG(LogMcpNativeTransport, Verbose,
		TEXT("Accept loop started on port %d"), ListenPort);

	double LastAcceptThreadSweepSeconds = 0.0;
	while (!bStopping.load())
	{
		// Timed wait instead of a blocking Accept so this thread can also run
		// the stale-request sweep. The game-thread sweep (Tick) starves during
		// blocking modals and long synchronous handlers — exactly when parked
		// requests need to time out — so this thread is the reliable reaper.
		bool bHasPendingConnection = false;
		if (!ListenSocket->WaitForPendingConnection(bHasPendingConnection, FTimespan::FromSeconds(1.0)))
		{
			if (bStopping.load())
			{
				break;
			}
			FPlatformProcess::Sleep(0.25f);
		}

		if (FPlatformTime::Seconds() - LastAcceptThreadSweepSeconds > 5.0)
		{
			LastAcceptThreadSweepSeconds = FPlatformTime::Seconds();
			CleanupStaleRequests();
		}

		if (!bHasPendingConnection)
		{
			continue;
		}

		FSocket* ClientSocket = ListenSocket->Accept(TEXT("McpNativeHTTPClient"));

		if (bStopping.load() || !ListenSocket)
		{
			if (ClientSocket)
			{
				ClientSocket->Close();
				SocketSub->DestroySocket(ClientSocket);
			}
			break;
		}

		if (ClientSocket)
		{
			ClientSocket->SetNoDelay(true);
			int32 Count = ActiveConnectionCount.fetch_add(1);
			if (Count >= MaxConcurrentConnections)
			{
				ActiveConnectionCount.fetch_sub(1);
				SendHttpResponse(ClientSocket, 503, TEXT("text/plain"), TEXT("Service Unavailable"));
				ClientSocket->Close();
				SocketSub->DestroySocket(ClientSocket);
			}
			else
			{
				// Lifetime safety: ActiveConnectionCount is incremented before dispatch,
				// and Shutdown waits for it to drain before destroying this transport.
				FMcpNativeTransport* Transport = this;
				Async(EAsyncExecution::ThreadPool, [Transport, ClientSocket]()
				{
					Transport->HandleConnection(ClientSocket);
					Transport->ActiveConnectionCount.fetch_sub(1);
				});
			}
		}
		else
		{
			// Accept failed (transient) — brief sleep before retrying
			FPlatformProcess::Sleep(0.01f);
		}
	}

	// ListenSocket is destroyed by Shutdown() after this thread is joined.
	// Destroying it here raced Stop()'s Close() (Thread->Kill re-invokes Stop
	// from the game thread) — use-after-free on editor exit.

	UE_LOG(LogMcpNativeTransport, Verbose, TEXT("Accept loop exited"));
	return 0;
}

// ─── Connection Handler ─────────────────────────────────────────────────────

void FMcpNativeTransport::HandleConnection(FSocket* ClientSocket)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	FParsedHttpRequest HttpReq;
	if (!ReadHttpRequest(ClientSocket, HttpReq))
	{
		SendHttpResponse(ClientSocket, 400, TEXT("text/plain"), TEXT("Bad Request"));
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Only accept /mcp path
	if (HttpReq.Path != TEXT("/mcp"))
	{
		SendHttpResponse(ClientSocket, 404, TEXT("text/plain"), TEXT("Not Found"));
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Browser access to the native MCP endpoint is only allowed when capability
	// tokens are enabled; non-browser local clients do not require CORS.
	if (HttpReq.Method == TEXT("OPTIONS"))
	{
		if (IsAllowedCorsOrigin(HttpReq.Origin))
		{
			SendHttpResponse(ClientSocket, 204, TEXT("text/plain"), FString(), {}, HttpReq.Origin);
		}
		else
		{
			SendHttpResponse(ClientSocket, 403, TEXT("text/plain"),
				TEXT("CORS preflight requires capability-token protection"));
		}
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	if (!HttpReq.Origin.IsEmpty() && !IsAllowedCorsOrigin(HttpReq.Origin))
	{
		SendHttpResponse(ClientSocket, 403, TEXT("text/plain"), TEXT("Invalid Origin"));
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Capability token validation
	{
		const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
		if (Settings && Settings->bRequireCapabilityToken)
		{
			if (HttpReq.CapabilityToken.IsEmpty() || HttpReq.CapabilityToken != Settings->CapabilityToken)
			{
				UE_LOG(LogMcpNativeTransport, Warning, TEXT("Capability token mismatch - rejecting connection"));
				FString ErrorBody = FMcpJsonRpc::BuildError(
					MakeShared<FJsonValueNull>(), FMcpJsonRpc::ErrorInvalidRequest,
					TEXT("Invalid capability token"));
				SendHttpResponse(ClientSocket, 401, TEXT("application/json"), ErrorBody, {}, HttpReq.Origin);
				ClientSocket->Close();
				SocketSub->DestroySocket(ClientSocket);
				return;
			}
		}
	}

	// ── DELETE /mcp — session termination ──
	if (HttpReq.Method == TEXT("DELETE"))
	{
		FString SessionError;
		ESessionValidationResult SessionStatus = ValidateSession(HttpReq.SessionId, SessionError);
		if (SessionStatus != ESessionValidationResult::Valid)
		{
			SendHttpResponse(ClientSocket, GetSessionValidationStatusCode(SessionStatus),
				TEXT("text/plain"), SessionError, {}, HttpReq.Origin);
			ClientSocket->Close();
			SocketSub->DestroySocket(ClientSocket);
			return;
		}

		if (!HttpReq.SessionId.IsEmpty())
		{
			{
				FScopeLock Lock(&SessionMutex);
				if (ActiveSessions.Remove(HttpReq.SessionId) > 0)
				{
					UE_LOG(LogMcpNativeTransport, Log,
						TEXT("Session %s terminated by client (remaining: %d)"),
						*HttpReq.SessionId, ActiveSessions.Num());
				}
			}
		}
		SendHttpResponse(ClientSocket, 200, TEXT("text/plain"), FString(), {}, HttpReq.Origin);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// ── GET /mcp — pull-only: no server→client notification stream (405, spec-allowed) ──
	if (HttpReq.Method == TEXT("GET"))
	{
		SendHttpResponse(ClientSocket, 405, TEXT("text/plain"),
			TEXT("Method Not Allowed: this server does not offer an SSE notification stream"),
			{}, HttpReq.Origin);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// ── POST /mcp — JSON-RPC ──
	if (HttpReq.Method != TEXT("POST"))
	{
		SendHttpResponse(ClientSocket, 405, TEXT("text/plain"), TEXT("Method Not Allowed"), {}, HttpReq.Origin);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	FMcpJsonRpcRequest Rpc = FMcpJsonRpc::ParseRequest(HttpReq.Body);
	if (!Rpc.bValid)
	{
		int32 ErrorCode = (Rpc.ErrorType == EMcpJsonRpcError::ParseError)
			? FMcpJsonRpc::ErrorParseError
			: FMcpJsonRpc::ErrorInvalidRequest;
		// Echo id for InvalidRequest if available; null for ParseError per JSON-RPC 2.0
		TSharedPtr<FJsonValue> ErrorId = (Rpc.ErrorType == EMcpJsonRpcError::ParseError)
			? MakeShared<FJsonValueNull>()
			: (Rpc.Id.IsValid() ? Rpc.Id : MakeShared<FJsonValueNull>());
		FString ErrorBody = FMcpJsonRpc::BuildError(ErrorId, ErrorCode,
			(Rpc.ErrorType == EMcpJsonRpcError::ParseError)
				? TEXT("Parse error") : TEXT("Invalid Request"));
		SendHttpResponse(ClientSocket, 400, TEXT("application/json"), ErrorBody, {}, HttpReq.Origin);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	if (Rpc.bIsNotification && Rpc.Method == TEXT("initialize"))
	{
		FString ErrorBody = FMcpJsonRpc::BuildError(
			MakeShared<FJsonValueNull>(), FMcpJsonRpc::ErrorInvalidRequest,
			TEXT("initialize must include an id"));
		SendHttpResponse(ClientSocket, 400, TEXT("application/json"), ErrorBody, {}, HttpReq.Origin);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Session validation (skip for initialize)
	if (Rpc.Method != TEXT("initialize"))
	{
		FString SessionError;
		ESessionValidationResult SessionStatus = ValidateSession(HttpReq.SessionId, SessionError);
		if (SessionStatus != ESessionValidationResult::Valid)
		{
			FString ErrorBody = FMcpJsonRpc::BuildError(
				Rpc.Id, FMcpJsonRpc::ErrorInvalidRequest, SessionError);
			SendHttpResponse(ClientSocket, GetSessionValidationStatusCode(SessionStatus),
				TEXT("application/json"), ErrorBody, {}, HttpReq.Origin);
			ClientSocket->Close();
			SocketSub->DestroySocket(ClientSocket);
			return;
		}
	}

	// Notifications (no id) — 202 Accepted after session validation.
	if (Rpc.bIsNotification)
	{
		UE_LOG(LogMcpNativeTransport, Log,
			TEXT("Received notification: %s"), *Rpc.Method);
		SendHttpResponse(ClientSocket, 202, TEXT("text/plain"), FString(), {}, HttpReq.Origin);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// ── Method dispatch ──

	if (Rpc.Method == TEXT("initialize"))
	{
		FString NewSessionId;
		FString ResponseBody = HandleInitialize(Rpc.Params, Rpc.Id, NewSessionId);
		TMap<FString, FString> Headers;
		Headers.Add(TEXT("Mcp-Session-Id"), NewSessionId);
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ResponseBody, Headers, HttpReq.Origin);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	if (Rpc.Method == TEXT("tools/list"))
	{
		FString ResponseBody = HandleToolsList(Rpc.Id);
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ResponseBody, {}, HttpReq.Origin);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	if (Rpc.Method == TEXT("tools/call"))
	{
		// HandleToolsCall takes ownership of the socket: it's parked until the
		// game-thread request completes, then a single application/json response
		// is written (pull-only — no SSE streaming).
		HandleToolsCall(Rpc.Params, Rpc.Id, ClientSocket, HttpReq.SessionId, HttpReq.Origin);
		return;  // Socket NOT closed here — parked until the response is sent
	}

	// Unknown method
	FString ErrorBody = FMcpJsonRpc::BuildError(
		Rpc.Id, FMcpJsonRpc::ErrorMethodNotFound,
		FString::Printf(TEXT("Unknown method: %s"), *Rpc.Method));
	SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ErrorBody, {}, HttpReq.Origin);
	ClientSocket->Close();
	SocketSub->DestroySocket(ClientSocket);
}

// ─── HTTP Parsing ───────────────────────────────────────────────────────────

bool FMcpNativeTransport::ReadHttpRequest(FSocket* Socket, FParsedHttpRequest& OutRequest)
{
	// Read headers: drain the socket in chunks (not one byte per syscall) and scan for
	// the CRLFCRLF terminator. When nothing is buffered, block on Socket->Wait(WaitForRead)
	// instead of sleep-spinning. Bytes read past the terminator are the start of the body
	// and are carried into the body phase below via LeftoverBodyBytes.
	static constexpr int32 MaxHeaderSize = 8192;
	const double Deadline = FPlatformTime::Seconds() + 5.0;  // 5s overall read timeout

	TArray<uint8> Buffer;
	Buffer.Reserve(2048);
	uint8 Chunk[2048];
	int32 HeaderEnd = INDEX_NONE;  // index one past the terminating \r\n\r\n

	while (HeaderEnd == INDEX_NONE)
	{
		uint32 PendingSize = 0;
		if (Socket->HasPendingData(PendingSize) && PendingSize > 0)
		{
			int32 BytesRead = 0;
			if (!Socket->Recv(Chunk, sizeof(Chunk), BytesRead))
			{
				UE_LOG(LogMcpNativeTransport, Warning, TEXT("HTTP header read: recv error"));
				return false;
			}
			if (BytesRead > 0)
			{
				// Scan from the previous length (clamped to >=3) so a terminator split
				// across two chunks is still found via the 4-byte window.
				const int32 ScanFrom = FMath::Max(3, Buffer.Num());
				Buffer.Append(Chunk, BytesRead);
				for (int32 i = ScanFrom; i < Buffer.Num(); ++i)
				{
					if (Buffer[i - 3] == '\r' && Buffer[i - 2] == '\n'
						&& Buffer[i - 1] == '\r' && Buffer[i] == '\n')
					{
						HeaderEnd = i + 1;
						break;
					}
				}
				if (HeaderEnd != INDEX_NONE)
				{
					break;
				}
				if (Buffer.Num() > MaxHeaderSize)
				{
					UE_LOG(LogMcpNativeTransport, Warning, TEXT("HTTP headers too large"));
					return false;
				}
				continue;  // keep draining whatever else is buffered before we block
			}
		}

		const double Now = FPlatformTime::Seconds();
		if (Now >= Deadline)
		{
			UE_LOG(LogMcpNativeTransport, Warning, TEXT("HTTP header read timeout"));
			return false;
		}
		// Block until readable (or a 1s slice) instead of busy-polling. A socket that
		// reports readable but has no pending data means the peer closed the connection.
		if (Socket->Wait(ESocketWaitConditions::WaitForRead,
				FTimespan::FromSeconds(FMath::Min(Deadline - Now, 1.0))))
		{
			uint32 P = 0;
			if (!Socket->HasPendingData(P) || P == 0)
			{
				UE_LOG(LogMcpNativeTransport, Warning,
					TEXT("HTTP header read: peer closed connection"));
				return false;
			}
		}
	}

	// Parse headers (the [0, HeaderEnd) span; anything after is the start of the body).
	FUTF8ToTCHAR HeaderConverter(reinterpret_cast<const ANSICHAR*>(Buffer.GetData()), HeaderEnd);
	FString HeaderStr(HeaderConverter.Length(), HeaderConverter.Get());

	TArray<FString> Lines;
	HeaderStr.ParseIntoArray(Lines, TEXT("\r\n"));
	if (Lines.Num() == 0)
	{
		return false;
	}

	// Request line: "POST /mcp HTTP/1.1"
	TArray<FString> RequestParts;
	Lines[0].ParseIntoArrayWS(RequestParts);
	if (RequestParts.Num() < 2)
	{
		return false;
	}
	OutRequest.Method = RequestParts[0];
	OutRequest.Path = RequestParts[1];

	// Parse headers
	OutRequest.ContentLength = 0;
	bool bHasContentLength = false;
	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		FString Key, Value;
		if (Lines[i].Split(TEXT(":"), &Key, &Value))
		{
			Key.TrimStartAndEndInline();
			Value.TrimStartAndEndInline();

			if (Key.Equals(TEXT("Content-Length"), ESearchCase::IgnoreCase))
			{
				if (bHasContentLength)
				{
					UE_LOG(LogMcpNativeTransport, Warning, TEXT("Duplicate Content-Length header"));
					return false;
				}

				int32 ParsedContentLength = 0;
				if (!LexTryParseString(ParsedContentLength, *Value))
				{
					UE_LOG(LogMcpNativeTransport, Warning,
						TEXT("Invalid Content-Length header: %s"), *Value);
					return false;
				}

				OutRequest.ContentLength = ParsedContentLength;
				bHasContentLength = true;
			}
			else if (Key.Equals(TEXT("Mcp-Session-Id"), ESearchCase::IgnoreCase))
			{
				OutRequest.SessionId = Value;
			}
			else if (Key.Equals(TEXT("Accept"), ESearchCase::IgnoreCase))
			{
				OutRequest.Accept = Value;
			}
			else if (Key.Equals(TEXT("X-MCP-Capability-Token"), ESearchCase::IgnoreCase))
			{
				OutRequest.CapabilityToken = Value;
			}
			else if (Key.Equals(TEXT("Origin"), ESearchCase::IgnoreCase))
			{
				OutRequest.Origin = Value;
			}
		}
	}

	// Read body
	static constexpr int32 MaxBodySize = 5 * 1024 * 1024;  // 5MB
	if (OutRequest.ContentLength < 0 || OutRequest.ContentLength > MaxBodySize)
	{
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("Invalid HTTP body size: %d bytes"), OutRequest.ContentLength);
		return false;
	}

	if (OutRequest.ContentLength > 0)
	{
		TArray<uint8> BodyBuf;
		BodyBuf.SetNumUninitialized(OutRequest.ContentLength);
		int32 TotalRead = 0;

		// Seed the body with any bytes we already read past the header terminator.
		const int32 LeftoverBodyBytes = Buffer.Num() - HeaderEnd;
		if (LeftoverBodyBytes > 0)
		{
			const int32 CopyN = FMath::Min(LeftoverBodyBytes, OutRequest.ContentLength);
			FMemory::Memcpy(BodyBuf.GetData(), Buffer.GetData() + HeaderEnd, CopyN);
			TotalRead = CopyN;
		}

		while (TotalRead < OutRequest.ContentLength)
		{
			uint32 PendingData = 0;
			if (Socket->HasPendingData(PendingData) && PendingData > 0)
			{
				int32 BytesRead = 0;
				if (!Socket->Recv(BodyBuf.GetData() + TotalRead,
					OutRequest.ContentLength - TotalRead, BytesRead))
				{
					UE_LOG(LogMcpNativeTransport, Warning, TEXT("HTTP body read: recv error (read %d/%d)"), TotalRead, OutRequest.ContentLength);
					return false;
				}
				if (BytesRead > 0)
				{
					TotalRead += BytesRead;
					continue;
				}
			}

			const double Now = FPlatformTime::Seconds();
			if (Now >= Deadline)
			{
				UE_LOG(LogMcpNativeTransport, Warning, TEXT("HTTP body read timeout (read %d/%d)"), TotalRead, OutRequest.ContentLength);
				return false;
			}
			// Block until more data arrives instead of sleep-spinning; readable-with-no-data
			// means the peer closed before sending the full body.
			if (Socket->Wait(ESocketWaitConditions::WaitForRead,
					FTimespan::FromSeconds(FMath::Min(Deadline - Now, 1.0))))
			{
				uint32 P = 0;
				if (!Socket->HasPendingData(P) || P == 0)
				{
					UE_LOG(LogMcpNativeTransport, Warning, TEXT("HTTP body read: peer closed connection (read %d/%d)"), TotalRead, OutRequest.ContentLength);
					return false;
				}
			}
		}

		FUTF8ToTCHAR BodyConverter(reinterpret_cast<const ANSICHAR*>(BodyBuf.GetData()), TotalRead);
		OutRequest.Body = FString(BodyConverter.Length(), BodyConverter.Get());
	}

	return true;
}

// ─── HTTP Response Helpers ──────────────────────────────────────────────────

bool FMcpNativeTransport::SendHttpResponse(FSocket* Socket, int32 StatusCode,
	const FString& ContentType, const FString& Body,
	const TMap<FString, FString>& ExtraHeaders,
	const FString& CorsOrigin)
{
	FString StatusText;
	switch (StatusCode)
	{
	case 200: StatusText = TEXT("OK"); break;
	case 202: StatusText = TEXT("Accepted"); break;
	case 204: StatusText = TEXT("No Content"); break;
	case 400: StatusText = TEXT("Bad Request"); break;
	case 403: StatusText = TEXT("Forbidden"); break;
	case 404: StatusText = TEXT("Not Found"); break;
	case 405: StatusText = TEXT("Method Not Allowed"); break;
	case 401: StatusText = TEXT("Unauthorized"); break;
	case 406: StatusText = TEXT("Not Acceptable"); break;
	case 429: StatusText = TEXT("Too Many Requests"); break;
	case 500: StatusText = TEXT("Internal Server Error"); break;
	case 503: StatusText = TEXT("Service Unavailable"); break;
	default:  StatusText = TEXT("Unknown"); break;
	}

	FTCHARToUTF8 BodyUtf8(*Body);
	const int32 BodyLength = BodyUtf8.Length();

	FString Response = FString::Printf(
		TEXT("HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n"),
		StatusCode, *StatusText, *ContentType, BodyLength);
	AppendCorsHeaders(Response, CorsOrigin);

	for (const auto& [Key, Value] : ExtraHeaders)
	{
		Response += FString::Printf(TEXT("%s: %s\r\n"), *Key, *Value);
	}
	Response += TEXT("\r\n");

	// Send header + body
	FTCHARToUTF8 HeaderUtf8(*Response);
	if (!SendAllBytes(Socket, reinterpret_cast<const uint8*>(HeaderUtf8.Get()),
		HeaderUtf8.Length()))
	{
		return false;
	}

	if (BodyLength > 0)
	{
		if (!SendAllBytes(Socket, reinterpret_cast<const uint8*>(BodyUtf8.Get()),
			BodyLength))
		{
			return false;
		}
	}

	return true;
}

bool FMcpNativeTransport::IsCorsEnabled() const
{
	const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
	return (ListenHost == TEXT("127.0.0.1") || ListenHost == TEXT("::1"))
		&& Settings
		&& Settings->bRequireCapabilityToken
		&& !Settings->CapabilityToken.IsEmpty();
}

bool FMcpNativeTransport::IsAllowedCorsOrigin(const FString& Origin) const
{
	if (!IsCorsEnabled())
	{
		return false;
	}

	const FString TrimmedOrigin = Origin.TrimStartAndEnd();
	if (TrimmedOrigin.IsEmpty() || TrimmedOrigin.Equals(TEXT("null"), ESearchCase::IgnoreCase) ||
		TrimmedOrigin.Contains(TEXT("\r")) || TrimmedOrigin.Contains(TEXT("\n")))
	{
		return false;
	}

	FString Scheme;
	FString Remainder;
	if (!TrimmedOrigin.Split(TEXT("://"), &Scheme, &Remainder))
	{
		return false;
	}
	if (!Scheme.Equals(TEXT("http"), ESearchCase::IgnoreCase) &&
		!Scheme.Equals(TEXT("https"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	if (Remainder.IsEmpty() || Remainder.Contains(TEXT("/")))
	{
		return false;
	}

	auto IsDigitsOnly = [](const FString& Value) -> bool
	{
		if (Value.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (Character < '0' || Character > '9')
			{
				return false;
			}
		}
		return true;
	};
	auto IsValidPort = [&IsDigitsOnly](const FString& PortText) -> bool
	{
		if (!IsDigitsOnly(PortText))
		{
			return false;
		}

		int32 PortNumber = 0;
		if (!LexTryParseString(PortNumber, *PortText))
		{
			return false;
		}
		return PortNumber > 0 && PortNumber <= 65535;
	};

	FString Host = Remainder;
	if (Host.StartsWith(TEXT("[")))
	{
		int32 EndBracket = INDEX_NONE;
		if (!Host.FindChar(TEXT(']'), EndBracket) || EndBracket <= 1)
		{
			return false;
		}

		const FString PortSuffix = Host.Mid(EndBracket + 1);
		if (!PortSuffix.IsEmpty())
		{
			if (!PortSuffix.StartsWith(TEXT(":")) || !IsValidPort(PortSuffix.Mid(1)))
			{
				return false;
			}
		}

		const FString BracketedHost = Host.Mid(1, EndBracket - 1);
		if (BracketedHost != TEXT("::1"))
		{
			return false;
		}
		Host = BracketedHost;
	}
	else
	{
		FString HostOnly;
		FString Port;
		if (Host.Split(TEXT(":"), &HostOnly, &Port))
		{
			if (!IsValidPort(Port))
			{
				return false;
			}
			Host = HostOnly;
		}
	}

	return Host.Equals(TEXT("localhost"), ESearchCase::IgnoreCase) ||
		Host == TEXT("127.0.0.1") ||
		Host == TEXT("::1");
}

void FMcpNativeTransport::AppendCorsHeaders(FString& Response, const FString& Origin) const
{
	if (!IsAllowedCorsOrigin(Origin))
	{
		return;
	}

	Response += FString::Printf(TEXT("Access-Control-Allow-Origin: %s\r\n"), *Origin.TrimStartAndEnd());
	Response += TEXT("Vary: Origin\r\n");
	Response += TEXT("Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n");
	Response += TEXT("Access-Control-Allow-Headers: Content-Type, Accept, Mcp-Session-Id, MCP-Protocol-Version, X-MCP-Capability-Token\r\n");
	Response += TEXT("Access-Control-Expose-Headers: Mcp-Session-Id, MCP-Protocol-Version\r\n");
}

// ─── Persistent Notification Streams (GET /mcp) ────────────────────────────

// HandleGetMcp / WriteNotificationEvent / WriteNotificationKeepalive / CloseNotificationStream
// removed — pull-only transport offers no GET /mcp notification stream (see docs/pull-architecture.md).

// ─── Initialize ─────────────────────────────────────────────────────────────

FString FMcpNativeTransport::HandleInitialize(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id,
	FString& OutSessionId)
{
	// Extract client info for logging
	FString ClientName = TEXT("unknown");
	FString ClientVersion = TEXT("unknown");
	if (Params.IsValid())
	{
		const TSharedPtr<FJsonObject>* ClientInfoObj = nullptr;
		if (Params->TryGetObjectField(TEXT("clientInfo"), ClientInfoObj) && ClientInfoObj)
		{
			(*ClientInfoObj)->TryGetStringField(TEXT("name"), ClientName);
			(*ClientInfoObj)->TryGetStringField(TEXT("version"), ClientVersion);
		}
	}

	OutSessionId = FGuid::NewGuid().ToString();
	int32 CurrentSessionCount;
	{
		FScopeLock Lock(&SessionMutex);
		ActiveSessions.Add(OutSessionId, FPlatformTime::Seconds());
		CurrentSessionCount = ActiveSessions.Num();
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("protocolVersion"), TEXT("2025-03-26"));

	auto Capabilities = MakeShared<FJsonObject>();
	auto ToolsCap = MakeShared<FJsonObject>();
	// Pull-only, and the tool set is fixed for the server's lifetime.
	ToolsCap->SetBoolField(TEXT("listChanged"), false);
	Capabilities->SetObjectField(TEXT("tools"), ToolsCap);
	Result->SetObjectField(TEXT("capabilities"), Capabilities);

	auto ServerInfo = MakeShared<FJsonObject>();
	ServerInfo->SetStringField(TEXT("name"), ServerName);
	ServerInfo->SetStringField(TEXT("version"), ServerVersion);
	Result->SetObjectField(TEXT("serverInfo"), ServerInfo);

	// Combine base instructions (from server-info.json) + user instructions (from settings)
	FString CombinedInstructions = BaseInstructions;
	if (!UserInstructions.IsEmpty())
	{
		if (!CombinedInstructions.IsEmpty())
		{
			CombinedInstructions += TEXT("\n\n");
		}
		CombinedInstructions += UserInstructions;
	}
	if (!CombinedInstructions.IsEmpty())
	{
		Result->SetStringField(TEXT("instructions"), CombinedInstructions);
	}

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("MCP session initialized: %s (client: %s %s, active sessions: %d)"),
		*OutSessionId, *ClientName, *ClientVersion, CurrentSessionCount);

	return FMcpJsonRpc::BuildResponse(Id, Result);
}

// ─── Tools List ─────────────────────────────────────────────────────────────

FString FMcpNativeTransport::HandleToolsList(const TSharedPtr<FJsonValue>& Id)
{
	TSharedPtr<FJsonObject> ToolsList = FMcpToolRegistry::Get().GetToolsResponse();

	if (ToolsList.IsValid())
	{
		return FMcpJsonRpc::BuildResponse(Id, ToolsList);
	}

	auto EmptyResult = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> EmptyArray;
	EmptyResult->SetArrayField(TEXT("tools"), EmptyArray);
	return FMcpJsonRpc::BuildResponse(Id, EmptyResult);
}

int32 FMcpNativeTransport::GetTotalToolCount() const
{
	return FMcpToolRegistry::Get().GetToolCount();
}

// ─── Tools Call (park connection until GameThread handler completes) ────────

void FMcpNativeTransport::HandleToolsCall(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id,
	FSocket* ClientSocket, const FString& SessionId, const FString& CorsOrigin)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	if (!Params.IsValid())
	{
		FString ErrorBody = FMcpJsonRpc::BuildError(
			Id, FMcpJsonRpc::ErrorInvalidParams, TEXT("Missing params"));
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ErrorBody, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	FString ToolName;
	if (!Params->TryGetStringField(TEXT("name"), ToolName))
	{
		FString ErrorBody = FMcpJsonRpc::BuildError(
			Id, FMcpJsonRpc::ErrorInvalidParams, TEXT("Missing tool name"));
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ErrorBody, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	TSharedPtr<FJsonObject> Arguments;
	const TSharedPtr<FJsonValue> ArgsValue = Params->TryGetField(TEXT("arguments"));

	if (ArgsValue.IsValid() && ArgsValue->Type != EJson::Null)
	{
		if (ArgsValue->Type != EJson::Object)
		{
			FString ErrorBody = FMcpJsonRpc::BuildError(
				Id, FMcpJsonRpc::ErrorInvalidParams,
				TEXT("'arguments' must be an object if provided"));
			SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ErrorBody, {}, CorsOrigin);
			ClientSocket->Close();
			if (SocketSub) SocketSub->DestroySocket(ClientSocket);
			return;
		}
		Arguments = ArgsValue->AsObject();
	}

	if (!Arguments.IsValid())
	{
		Arguments = MakeShared<FJsonObject>();
	}

	// Normalize action/subAction both ways BEFORE validation: some handlers
	// read "subAction", and a legacy subAction-only payload must not be
	// rejected for a missing required "action".
	if (Arguments.IsValid())
	{
		FString ActionVal;
		if (!Arguments->HasField(TEXT("subAction")) &&
			Arguments->TryGetStringField(TEXT("action"), ActionVal))
		{
			Arguments->SetStringField(TEXT("subAction"), ActionVal);
		}
		else if (!Arguments->HasField(TEXT("action")) &&
				 Arguments->TryGetStringField(TEXT("subAction"), ActionVal))
		{
			Arguments->SetStringField(TEXT("action"), ActionVal);
		}
	}

	// Argument validation against the published schema (arch review F7).
	// Shipped warn-first 2026-07-02; promoted to INVALID_PARAMS rejection
	// 2026-07-03 after the warnings proved quiet (11 editor sessions, hundreds
	// of calls, 5 warnings — every one a genuinely wrong call). Required-arg
	// and enum violations reject; unknown top-level keys stay log-only
	// forever: handlers legitimately read params the schema doesn't declare.
	auto CollectSchemaViolations = [](const FString& InToolName, const TSharedPtr<FJsonObject>& InArguments) -> TArray<FString>
	{
		TArray<FString> Violations;
		FMcpToolDefinition* Def = FMcpToolRegistry::Get().FindTool(InToolName);
		if (!Def || !InArguments.IsValid())
		{
			return Violations;
		}
		const TSharedPtr<FJsonObject> Schema = Def->BuildInputSchema();
		if (!Schema.IsValid())
		{
			return Violations;
		}
		const TSharedPtr<FJsonObject>* Props = nullptr;
		Schema->TryGetObjectField(TEXT("properties"), Props);

		const TArray<TSharedPtr<FJsonValue>>* Required = nullptr;
		if (Schema->TryGetArrayField(TEXT("required"), Required))
		{
			for (const TSharedPtr<FJsonValue>& R : *Required)
			{
				FString Name;
				if (R->TryGetString(Name) && !InArguments->HasField(Name))
				{
					Violations.Add(FString::Printf(TEXT("required argument '%s' is missing"), *Name));
				}
			}
		}

		if (!Props)
		{
			return Violations;
		}
		for (const auto& Pair : InArguments->Values)
		{
			const TSharedPtr<FJsonObject>* PropSchema = nullptr;
			if (!(*Props)->TryGetObjectField(Pair.Key, PropSchema))
			{
				UE_LOG(LogMcpNativeTransport, Verbose,
					TEXT("%s: argument '%s' is not declared in the schema"), *InToolName, *Pair.Key);
				continue;
			}
			const TArray<TSharedPtr<FJsonValue>>* EnumArr = nullptr;
			FString Value;
			if ((*PropSchema)->TryGetArrayField(TEXT("enum"), EnumArr) && Pair.Value->TryGetString(Value))
			{
				bool bFound = false;
				for (const TSharedPtr<FJsonValue>& E : *EnumArr)
				{
					FString EnumValue;
					if (E->TryGetString(EnumValue) && EnumValue == Value)
					{
						bFound = true;
						break;
					}
				}
				if (!bFound)
				{
					// Suggest near-misses: enum values sharing any 3+ char
					// token of the rejected value (small enums list fully).
					TArray<FString> AllValues;
					for (const TSharedPtr<FJsonValue>& E : *EnumArr)
					{
						FString EnumValue;
						if (E->TryGetString(EnumValue))
						{
							AllValues.Add(EnumValue);
						}
					}
					FString Hint;
					if (AllValues.Num() <= 15)
					{
						Hint = FString::Printf(TEXT(" (allowed: %s)"), *FString::Join(AllValues, TEXT(", ")));
					}
					else
					{
						TArray<FString> Tokens;
						Value.ParseIntoArray(Tokens, TEXT("_"));
						TArray<FString> Suggestions;
						for (const FString& EnumValue : AllValues)
						{
							for (const FString& Token : Tokens)
							{
								if (Token.Len() >= 3 && EnumValue.Contains(Token, ESearchCase::IgnoreCase))
								{
									Suggestions.AddUnique(EnumValue);
									break;
								}
							}
							if (Suggestions.Num() >= 6)
							{
								break;
							}
						}
						Hint = Suggestions.Num() > 0
							? FString::Printf(TEXT(" (did you mean: %s?)"), *FString::Join(Suggestions, TEXT(", ")))
							: FString::Printf(TEXT(" (%d allowed values — see the tool schema)"), AllValues.Num());
					}
					Violations.Add(FString::Printf(TEXT("argument '%s' value '%s' is not in the schema enum%s"),
						*Pair.Key, *Value, *Hint));
				}
			}
		}
		return Violations;
	};
	const TArray<FString> SchemaViolations = CollectSchemaViolations(ToolName, Arguments);
	if (SchemaViolations.Num() > 0)
	{
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("tools/call rejected (INVALID_PARAMS): %s: %s"),
			*ToolName, *FString::Join(SchemaViolations, TEXT("; ")));
		TSharedPtr<FJsonObject> Details = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> ViolationsJson;
		for (const FString& V : SchemaViolations)
		{
			ViolationsJson.Add(MakeShared<FJsonValueString>(V));
		}
		Details->SetArrayField(TEXT("violations"), ViolationsJson);
		TSharedPtr<FJsonObject> ToolResult = FMcpJsonRpc::BuildToolResult(
			false,
			FString::Printf(TEXT("%s: %s"), *ToolName, *FString::Join(SchemaViolations, TEXT("; "))),
			Details, TEXT("INVALID_PARAMS"));
		FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), Body, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Per-action param check against the declaration registry (McpDecl_*.h —
	// the server's authored contract; see docs/action-declarations.md).
	// Schemas declare params per TOOL, so a param sent to an action that
	// never reads it sails through the schema check and is silently ignored —
	// the biggest remaining silent-no-op class. Violations REJECT: the caller
	// is an LLM, and a named, explained refusal gets fixed in one turn while
	// a silent ignore never surfaces. Because a wrong declaration can only be
	// corrected with an edit + rebuild, the error names the escape hatch
	// (bypassParamCheck:true → proceed, findings ride the response as
	// paramWarnings) and routes the fix back to the repo. UnverifiedDecl
	// entries are skipped — validation never runs on unattributed truth.
	TArray<FString> BypassedParamWarnings;
	if (Arguments.IsValid())
	{
		FString ActionValue;
		if (Arguments->TryGetStringField(TEXT("action"), ActionValue))
		{
			const FMcpCallDecl* Decl = FMcpCallRegistry::Get().FindDecl(ToolName, ActionValue);
			if (Decl && !EnumHasAnyFlags(Decl->Flags, EMcpCallFlags::UnverifiedDecl))
			{
				TArray<FString> ForeignParams;
				for (const auto& Pair : Arguments->Values)
				{
					if (Pair.Key == TEXT("action") || Pair.Key == TEXT("subAction") ||
						Pair.Key == TEXT("bypassParamCheck"))
					{
						continue;
					}
					bool bDeclared = false;
					for (const FMcpParamDecl& Param : Decl->Params)
					{
						if (Pair.Key == Param.Name)
						{
							bDeclared = true;
							break;
						}
					}
					if (!bDeclared)
					{
						ForeignParams.Add(Pair.Key);
					}
				}
				if (ForeignParams.Num() > 0)
				{
					TArray<FString> Accepted;
					for (const FMcpParamDecl& Param : Decl->Params)
					{
						Accepted.Add(Param.Name);
					}
					Accepted.Sort();
					constexpr int32 MaxListed = 30;
					FString AcceptedStr = FString::Join(
						TArray<FString>(Accepted.GetData(), FMath::Min(Accepted.Num(), MaxListed)), TEXT(", "));
					if (Accepted.Num() > MaxListed)
					{
						AcceptedStr += FString::Printf(TEXT(" (+%d more)"), Accepted.Num() - MaxListed);
					}
					const FString AcceptedClause = Accepted.Num() > 0
						? FString::Printf(TEXT("Params this action reads: %s."), *AcceptedStr)
						: TEXT("This action takes no params besides 'action' itself.");

					bool bBypass = false;
					Arguments->TryGetBoolField(TEXT("bypassParamCheck"), bBypass);
					if (!bBypass)
					{
						const FString Message = FString::Printf(
							TEXT("%s.%s does not read: %s. %s ")
							TEXT("Remove or rename the unread param(s). If the handler source DOES read them, the ")
							TEXT("action's declaration is wrong: retry with bypassParamCheck:true to proceed ")
							TEXT("now, and report the miss (fork TODO.md) so its McpDecl_*.h entry gets fixed."),
							*ToolName, *ActionValue, *FString::Join(ForeignParams, TEXT(", ")), *AcceptedClause);
						UE_LOG(LogMcpNativeTransport, Warning,
							TEXT("tools/call rejected (per-action params): %s.%s: %s"),
							*ToolName, *ActionValue, *FString::Join(ForeignParams, TEXT(", ")));
						TSharedPtr<FJsonObject> Details = MakeShared<FJsonObject>();
						TArray<TSharedPtr<FJsonValue>> ForeignJson, AcceptedJson;
						for (const FString& P : ForeignParams) { ForeignJson.Add(MakeShared<FJsonValueString>(P)); }
						for (const FString& P : Accepted) { AcceptedJson.Add(MakeShared<FJsonValueString>(P)); }
						Details->SetArrayField(TEXT("unreadParams"), ForeignJson);
						Details->SetArrayField(TEXT("acceptedParams"), AcceptedJson);
						TSharedPtr<FJsonObject> ToolResult = FMcpJsonRpc::BuildToolResult(
							false, Message, Details, TEXT("INVALID_PARAMS"));
						FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
						SendHttpResponse(ClientSocket, 200, TEXT("application/json"), Body, {}, CorsOrigin);
						ClientSocket->Close();
						if (SocketSub) SocketSub->DestroySocket(ClientSocket);
						return;
					}
					for (const FString& P : ForeignParams)
					{
						BypassedParamWarnings.Add(FString::Printf(
							TEXT("param '%s' is not read by %s.%s (per-action table; bypassed)"),
							*P, *ToolName, *ActionValue));
					}
					UE_LOG(LogMcpNativeTransport, Warning,
						TEXT("per-action param check BYPASSED: %s.%s: %s — if these are real reads, fix the table"),
						*ToolName, *ActionValue, *FString::Join(ForeignParams, TEXT(", ")));
				}
			}
		}
	}

	if (!FMcpToolRegistry::Get().FindTool(ToolName))
	{
		TSharedPtr<FJsonObject> ToolResult = FMcpJsonRpc::BuildToolResult(
			false,
			FString::Printf(TEXT("Unknown tool '%s'. Use tools/list to see available tools."), *ToolName),
			nullptr, TEXT("UNKNOWN_TOOL"));
		FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), Body, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Pull-only: send nothing upfront. Park the connection and answer with a single
	// plain HTTP/JSON response once the GameThread handler completes (see
	// docs/pull-architecture.md). The socket is held until then.
	const FString RequestId = FGuid::NewGuid().ToString();

	{
		FScopeLock Lock(&PendingConnectionsMutex);
		// One local client sending serial requests should never approach this;
		// a runaway or misbehaving client must not grow the map unboundedly
		// while the game thread is stalled.
		constexpr int32 MaxParkedRequests = 64;
		if (PendingConnections.Num() >= MaxParkedRequests)
		{
			FString ErrorBody = FMcpJsonRpc::BuildError(
				Id, FMcpJsonRpc::ErrorInternalError,
				FString::Printf(TEXT("Too many requests in flight (%d parked); retry later"),
					PendingConnections.Num()));
			SendHttpResponse(ClientSocket, 503, TEXT("application/json"), ErrorBody, {}, CorsOrigin);
			ClientSocket->Close();
			if (SocketSub) SocketSub->DestroySocket(ClientSocket);
			return;
		}
		TSharedPtr<FPendingConnection> Conn = MakeShared<FPendingConnection>();
		Conn->Socket = ClientSocket;
		Conn->JsonRpcId = Id;
		Conn->StartTime = FPlatformTime::Seconds();
		Conn->ToolName = ToolName;
		Conn->SessionId = SessionId;
		Conn->CorsOrigin = CorsOrigin;
		Conn->ParamWarnings = MoveTemp(BypassedParamWarnings);
		PendingConnections.Add(RequestId, Conn);
	}

	// Log the peer and session so multi-client traffic on the port is
	// attributable (the session id joins to the init line's client name/version;
	// an unfamiliar peer/session means another MCP client is hitting this port).
	FString PeerDesc = TEXT("unknown");
	if (SocketSub && ClientSocket)
	{
		TSharedRef<FInternetAddr> PeerAddr = SocketSub->CreateInternetAddr();
		if (ClientSocket->GetPeerAddress(*PeerAddr))
		{
			PeerDesc = PeerAddr->ToString(true);
		}
	}
	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("tools/call: %s (RequestId=%s, session=%s, peer=%s)"),
		*ToolName, *RequestId,
		SessionId.IsEmpty() ? TEXT("<no-session>") : *SessionId,
		*PeerDesc);

	// Every tool dispatches by its own name; the handler reads the sub-action
	// from the payload (Pattern B tool-definition dispatch was removed unused).
	// action/subAction were mirrored bidirectionally before schema validation.
	const FString DispatchAction = ToolName;

	// Dispatch through the subsystem queue. The queue is drained by the core
	// ticker after world ticking, which is required for safe map transitions.
	TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(Subsystem);
	FString CapturedRequestId = RequestId;
	FString CapturedDispatchAction = DispatchAction;
	TSharedPtr<FJsonObject> CapturedArguments = Arguments;

	if (UMcpAutomationBridgeSubsystem* Sub = WeakSubsystem.Get())
	{
		Sub->QueueAutomationRequest(
			CapturedRequestId, CapturedDispatchAction, CapturedArguments, nullptr,
			ERequestOrigin::NativeHTTP);
	}
}

// ─── Pending Connection Management ──────────────────────────────────────────

bool FMcpNativeTransport::CompletePendingRequest(
	const FString& RequestId, bool bSuccess, const FString& Message,
	const TSharedPtr<FJsonObject>& Result, const FString& ErrorCode)
{
	TSharedPtr<FPendingConnection> Conn;
	{
		FScopeLock Lock(&PendingConnectionsMutex);
		TSharedPtr<FPendingConnection>* Found = PendingConnections.Find(RequestId);
		if (!Found)
		{
			return false;
		}
		Conn = *Found;
		PendingConnections.Remove(RequestId);
	}

	if (!Conn.IsValid())
	{
		return true;  // Already cleaned up
	}

	// Serialize AND write on the thread pool: for fat results (multi-hundred-
	// node graph dumps, schema listings) JSON serialization is the expensive
	// half, and the game thread needs neither. Safe because the Result object
	// is handed off at the SendAutomationResponse call — audited 2026-07-03:
	// every send site passes a function-local it never touches again. That
	// handoff is the documented ownership contract (see the declaration).
	FString CapturedRequestId = RequestId;
	FString CapturedToolName = Conn->ToolName;
	FString CapturedSessionId = Conn->SessionId;
	bool bCapturedSuccess = bSuccess;
	PendingAsyncWrites.fetch_add(1);

	Async(EAsyncExecution::ThreadPool,
		[this, Conn, Message, Result, ErrorCode,
		 CapturedRequestId, CapturedToolName, CapturedSessionId, bCapturedSuccess]()
	{
		TSharedPtr<FJsonObject> ToolResult = FMcpJsonRpc::BuildToolResult(
			bCapturedSuccess, Message, Result, ErrorCode);
		// Findings from a bypassed per-action param check ride the response so
		// the caller sees exactly what was flagged (text + envelope).
		if (Conn->ParamWarnings.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> WarnJson;
			for (const FString& W : Conn->ParamWarnings)
			{
				WarnJson.Add(MakeShared<FJsonValueString>(W));
			}
			const TSharedPtr<FJsonObject>* Structured = nullptr;
			if (ToolResult->TryGetObjectField(TEXT("structuredContent"), Structured))
			{
				(*Structured)->SetArrayField(TEXT("paramWarnings"), WarnJson);
			}
			const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
			if (ToolResult->TryGetArrayField(TEXT("content"), Content) && Content->Num() > 0)
			{
				TSharedPtr<FJsonObject> TextObj = (*Content)[0]->AsObject();
				if (TextObj.IsValid())
				{
					FString Text;
					TextObj->TryGetStringField(TEXT("text"), Text);
					Text += TEXT("\n\nParam warnings:\n- ") +
						FString::Join(Conn->ParamWarnings, TEXT("\n- "));
					TextObj->SetStringField(TEXT("text"), Text);
				}
			}
		}
		const FString ResponseBody = FMcpJsonRpc::BuildResponse(Conn->JsonRpcId, ToolResult);

		bool bWroteResponse = false;
		{
			FScopeLock WriteLock(&Conn->WriteMutex);
			if (!Conn->Socket)
			{
				PendingAsyncWrites.fetch_sub(1);
				return;  // Already cleaned up by Shutdown
			}

			// Pull-only: write a single plain HTTP/JSON response (no SSE framing).
			bWroteResponse = SendHttpResponse(Conn->Socket, 200,
				TEXT("application/json"), ResponseBody, {}, Conn->CorsOrigin);

			Conn->Socket->Close();
			ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
			if (SocketSub)
			{
				SocketSub->DestroySocket(Conn->Socket);
			}
			Conn->Socket = nullptr;
		}
		if (bWroteResponse)
		{
			TouchSession(CapturedSessionId);
		}

		UE_LOG(LogMcpNativeTransport, Log,
			TEXT("tools/call completed: %s (tool=%s, success=%s)"),
			*CapturedRequestId, *CapturedToolName,
			bCapturedSuccess ? TEXT("true") : TEXT("false"));

		PendingAsyncWrites.fetch_sub(1);
	});

	return true;
}

bool FMcpNativeTransport::HasPendingRequest(const FString& RequestId) const
{
	FScopeLock Lock(&PendingConnectionsMutex);
	return PendingConnections.Contains(RequestId);
}

void FMcpNativeTransport::CleanupStaleRequests()
{
	const double Now = FPlatformTime::Seconds();

	// Clean up timed-out parked connections
	TArray<FString> Expired;
	{
		FScopeLock Lock(&PendingConnectionsMutex);
		for (const auto& [RequestId, Conn] : PendingConnections)
		{
			if (Conn.IsValid() && Now - Conn->StartTime > RequestTimeoutSeconds)
			{
				Expired.Add(RequestId);
			}
		}
	}

	// Slate introspection is game-thread-only; the accept-thread sweep (which
	// fires exactly when the game thread is stalled) reports that instead.
	const bool bGameThread = IsInGameThread();
	const bool bAnyExpired = Expired.Num() > 0;
	const FString ActiveModal =
		!bAnyExpired ? FString()
		: bGameThread ? McpDescribeActiveModalWindow()
		              : TEXT("<unknown: game thread stalled; swept from accept thread>");
	const bool bSlateAppActive = bGameThread && FSlateApplication::IsInitialized() &&
		FSlateApplication::Get().IsActive();
	for (const FString& RequestId : Expired)
	{
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("Parked request %s timed out after %.0f seconds "
			     "(activeModal: %s, slateAppActive: %s)"),
			*RequestId, RequestTimeoutSeconds, *ActiveModal,
			bGameThread ? (bSlateAppActive ? TEXT("true") : TEXT("false")) : TEXT("n/a"));
		CompletePendingRequest(RequestId, false, TEXT("Request timed out"),
			nullptr, TEXT("TIMEOUT"));
	}

	// Clean up inactive sessions
	{
		TArray<FString> ExpiredSessions;
		{
			FScopeLock Lock(&SessionMutex);
			for (const auto& [SessionId, LastActivity] : ActiveSessions)
			{
				if (Now - LastActivity > SessionTimeoutSeconds)
				{
					ExpiredSessions.Add(SessionId);
				}
			}
			for (const FString& SessionId : ExpiredSessions)
			{
				ActiveSessions.Remove(SessionId);
				UE_LOG(LogMcpNativeTransport, Log,
					TEXT("Session %s expired after %.0f min inactivity (remaining: %d)"),
					*SessionId, SessionTimeoutSeconds / 60.0, ActiveSessions.Num());
			}
		}
	}

	// (notification-stream cleanup + keepalive removed — pull-only transport)
}

// ─── Session Validation ─────────────────────────────────────────────────────

FMcpNativeTransport::ESessionValidationResult FMcpNativeTransport::ValidateSession(
	const FString& SessionId, FString& OutError)
{
	if (SessionId.IsEmpty())
	{
		OutError = TEXT("Missing Mcp-Session-Id header");
		return ESessionValidationResult::Missing;
	}

	FScopeLock Lock(&SessionMutex);
	double* LastActivity = ActiveSessions.Find(SessionId);
	if (!LastActivity)
	{
		OutError = TEXT("Invalid or expired session ID");
		return ESessionValidationResult::Invalid;
	}

	const double Now = FPlatformTime::Seconds();
	if (Now - *LastActivity > SessionTimeoutSeconds)
	{
		ActiveSessions.Remove(SessionId);
		OutError = TEXT("Invalid or expired session ID");
		return ESessionValidationResult::Invalid;
	}

	// Touch session activity
	*LastActivity = Now;
	return ESessionValidationResult::Valid;
}

int32 FMcpNativeTransport::GetSessionValidationStatusCode(ESessionValidationResult Result)
{
	switch (Result)
	{
	case ESessionValidationResult::Missing:
		return 400;
	case ESessionValidationResult::Invalid:
		return 404;
	case ESessionValidationResult::Valid:
	default:
		return 200;
	}
}

void FMcpNativeTransport::TouchSession(const FString& SessionId)
{
	if (SessionId.IsEmpty())
	{
		return;
	}
	FScopeLock Lock(&SessionMutex);
	double* LastActivity = ActiveSessions.Find(SessionId);
	if (LastActivity)
	{
		*LastActivity = FPlatformTime::Seconds();
	}
}

// ─── Helpers ────────────────────────────────────────────────────────────────

// OnToolsListChanged / BroadcastToolsListChanged removed — pull-only transport has no
// server->client push stream (see docs/pull-architecture.md).

int32 FMcpNativeTransport::GetActiveSessionCount() const
{
	FScopeLock Lock(&SessionMutex);
	return ActiveSessions.Num();
}
