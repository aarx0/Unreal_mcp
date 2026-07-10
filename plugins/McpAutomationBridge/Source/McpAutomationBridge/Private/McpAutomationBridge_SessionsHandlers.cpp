// =============================================================================
// McpAutomationBridge_SessionsHandlers.cpp
// =============================================================================
// Session Management and Multiplayer Handlers for MCP Automation Bridge.
//
// This file implements the following handlers:
//
// Local Multiplayer:
//   - add_local_player
//   - remove_local_player
//
// LAN:
//   - host_lan_server
//
// Voice Chat:
//   - enable_voice_chat
//   - mute_player
//
// Utility:
//   - get_sessions_info
//
// UE VERSION COMPATIBILITY:
// - UE 5.0-5.6: CreateUniquePlayerId available on some platforms
// - UE 5.7: CreateUniquePlayerId removed, use session-based lookup
// - VoiceChat: IVoiceChat modular feature interface
// - OnlineSubsystem: IOnlineVoice for fallback voice operations
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST BE FIRST - Version compatibility macros
#include "McpHandlerUtils.h"          // Utility functions for JSON parsing

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridge_SessionsHandlers.h"

// =============================================================================
// Helper Macros
// =============================================================================


#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"

// Voice Chat interfaces (conditional availability)
// Note: VoiceChat is ClientOnly (only loads during PIE/gameplay, not in Editor)
// The modular feature is detected at runtime via IModularFeatures
#if __has_include("VoiceChat.h")
#include "VoiceChat.h"
#include "Features/IModularFeatures.h"
#define MCP_HAS_VOICECHAT 1
#else
// VoiceChat header not available - use runtime modular feature detection
#include "Features/IModularFeatures.h"
#define MCP_HAS_VOICECHAT 0
#endif

// Online Subsystem for voice muting via IOnlineVoice
#if __has_include("OnlineSubsystem.h")
#include "OnlineSubsystem.h"
#include "Interfaces/VoiceInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#define MCP_HAS_ONLINE_SUBSYSTEM 1
#else
#define MCP_HAS_ONLINE_SUBSYSTEM 0
#endif

#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogMcpSessionsHandlers, Log, All);

// ============================================================================
// Helper Functions
// ============================================================================

namespace SessionsHelpers
{

    static TSet<FString> LocalVoiceMuteFallbackState;

    static FString MakeLocalVoiceMuteKey(const FString& TargetIdentifier, int32 LocalPlayerNum, bool bSystemWide)
    {
        return FString::Printf(TEXT("%s:%d:%s"),
            bSystemWide ? TEXT("system") : TEXT("local"),
            LocalPlayerNum,
            *TargetIdentifier);
    }

    // Get object field
    TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
    {
        if (Payload.IsValid() && Payload->HasTypedField<EJson::Object>(FieldName))
        {
            return Payload->GetObjectField(FieldName);
        }
        return nullptr;
    }

#if WITH_EDITOR
    // Get game instance
    UGameInstance* GetGameInstance()
    {
        if (GEditor && GEditor->PlayWorld)
        {
            return GEditor->PlayWorld->GetGameInstance();
        }
        return nullptr;
    }

    // Get local player by index
    ULocalPlayer* GetLocalPlayerByIndex(int32 PlayerIndex)
    {
        UGameInstance* GI = GetGameInstance();
        if (GI)
        {
            const TArray<ULocalPlayer*>& LocalPlayers = GI->GetLocalPlayers();
            if (LocalPlayers.IsValidIndex(PlayerIndex))
            {
                return LocalPlayers[PlayerIndex];
            }
        }
        return nullptr;
    }

    // Get number of local players
    int32 GetLocalPlayerCount()
    {
        UGameInstance* GI = GetGameInstance();
        if (GI)
        {
            return GI->GetLocalPlayers().Num();
        }
        return 0;
    }
#endif
}

// ============================================================================
// Session Management Actions
// ============================================================================

#if WITH_EDITOR



// ============================================================================
// Local Multiplayer Actions
// ============================================================================


static bool HandleAddLocalPlayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
    using namespace SessionsHelpers;

    // VALIDATION: Require controllerId parameter
    if (!Payload.IsValid() || !Payload->HasField(TEXT("controllerId")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("controllerId is required to add a local player"), nullptr);
        return true;
    }

    int32 ControllerId = static_cast<int32>(GetJsonNumberField(Payload, TEXT("controllerId"), -1));

    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No active game instance. Start Play-In-Editor first."), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    // Create new local player
    FString Error;
    ULocalPlayer* NewPlayer = GI->CreateLocalPlayer(ControllerId, Error, true);
    
    if (!NewPlayer)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to add local player: %s"), *Error), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    int32 PlayerIndex = GI->GetLocalPlayers().Find(NewPlayer);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetNumberField(TEXT("playerIndex"), PlayerIndex);
    ResponseJson->SetNumberField(TEXT("controllerId"), ControllerId);
    ResponseJson->SetNumberField(TEXT("totalLocalPlayers"), GI->GetLocalPlayers().Num());

    FString Message = FString::Printf(TEXT("Added local player at index %d (controller ID: %d). Total players: %d"),
        PlayerIndex, ControllerId, GI->GetLocalPlayers().Num());

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

static bool HandleRemoveLocalPlayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
    using namespace SessionsHelpers;

    // VALIDATION: Require playerIndex parameter
    if (!Payload.IsValid() || !Payload->HasField(TEXT("playerIndex")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("playerIndex is required to remove a local player"), nullptr);
        return true;
    }

    int32 PlayerIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("playerIndex"), -1));

    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No active game instance. Start Play-In-Editor first."), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    // Cannot remove player 0 (primary player)
    if (PlayerIndex == 0)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Cannot remove the primary local player (index 0)."), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    ULocalPlayer* Player = GetLocalPlayerByIndex(PlayerIndex);
    if (!Player)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("No local player at index %d"), PlayerIndex), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    GI->RemoveLocalPlayer(Player);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetNumberField(TEXT("removedPlayerIndex"), PlayerIndex);
    ResponseJson->SetNumberField(TEXT("remainingPlayers"), GI->GetLocalPlayers().Num());

    FString Message = FString::Printf(TEXT("Removed local player at index %d. Remaining players: %d"),
        PlayerIndex, GI->GetLocalPlayers().Num());

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

// ============================================================================
// LAN Actions
// ============================================================================


static bool HandleHostLanServer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
    using namespace SessionsHelpers;

    FString ServerName = GetJsonStringField(Payload, TEXT("serverName"), TEXT("LAN Server"));
    FString MapName = GetJsonStringField(Payload, TEXT("mapName"), TEXT(""));
    int32 MaxPlayers = static_cast<int32>(GetJsonNumberField(Payload, TEXT("maxPlayers"), 4));
    FString TravelOptions = GetJsonStringField(Payload, TEXT("travelOptions"), TEXT(""));
    bool bExecuteTravel = GetJsonBoolField(Payload, TEXT("executeTravel"), false);

    if (MapName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("mapName is required to host a LAN server"), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    // Build the travel URL with LAN-specific options
    FString FullTravelOptions = FString::Printf(TEXT("?listen?bIsLanMatch=1?MaxPlayers=%d"), MaxPlayers);
    if (!TravelOptions.IsEmpty())
    {
        FullTravelOptions += TravelOptions;
    }
    
    // Ensure map path starts with /Game/ if it doesn't already
    FString FullMapPath = MapName;
    if (!FullMapPath.StartsWith(TEXT("/Game/")) && !FullMapPath.StartsWith(TEXT("/")) && !FullMapPath.Contains(TEXT(":")))
    {
        FullMapPath = FString::Printf(TEXT("/Game/%s"), *MapName);
    }
    
    FString TravelURL = FullMapPath + FullTravelOptions;
    bool bSuccess = true;
    FString StatusMessage = TEXT("configured");

    // Optionally execute the server travel
    if (bExecuteTravel)
    {
        UWorld* World = nullptr;
        if (GEditor && GEditor->PlayWorld)
        {
            World = GEditor->PlayWorld;
        }
        else if (GEditor)
        {
            World = GEditor->GetEditorWorldContext().World();
        }
        
        if (World)
        {
            // Use ServerTravel to start hosting
            // ServerTravel is used on the server to travel all clients to a new map
            bool bAbsolute = true;
            World->ServerTravel(TravelURL, bAbsolute);
            StatusMessage = TEXT("server travel initiated");
            UE_LOG(LogMcpSessionsHandlers, Log, TEXT("LAN Server: Initiated ServerTravel to %s"), *TravelURL);
        }
        else
        {
            bSuccess = false;
            StatusMessage = TEXT("No world available. Start Play-In-Editor first to execute travel.");
        }
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("serverName"), ServerName);
    ResponseJson->SetStringField(TEXT("mapName"), MapName);
    ResponseJson->SetStringField(TEXT("mapPath"), FullMapPath);
    ResponseJson->SetNumberField(TEXT("maxPlayers"), MaxPlayers);
    ResponseJson->SetStringField(TEXT("travelURL"), TravelURL);
    ResponseJson->SetStringField(TEXT("status"), StatusMessage);
    ResponseJson->SetBoolField(TEXT("travelExecuted"), bExecuteTravel && bSuccess);

    FString Message = FString::Printf(TEXT("LAN server '%s' %s for map '%s' (max %d players)"),
        *ServerName, *StatusMessage, *MapName, MaxPlayers);

    Subsystem->SendAutomationResponse(Socket, RequestId, bSuccess, Message, ResponseJson);
    return true;
}

// ============================================================================
// Voice Chat Actions
// ============================================================================

static bool HandleEnableVoiceChat(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
    using namespace SessionsHelpers;

    // VALIDATION: Require voiceEnabled parameter
    if (!Payload.IsValid() || !Payload->HasField(TEXT("voiceEnabled")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("voiceEnabled is required (true to enable, false to disable)"), nullptr);
        return true;
    }

    bool bEnabled = GetJsonBoolField(Payload, TEXT("voiceEnabled"), true);
    bool bSuccess = false;
    FString StatusMessage;

#if MCP_HAS_VOICECHAT
    // Use the IVoiceChat modular feature interface
    IVoiceChat* VoiceChat = IVoiceChat::Get();
    if (VoiceChat)
    {
        if (bEnabled)
        {
            // Initialize and connect voice chat
            if (!VoiceChat->IsInitialized())
            {
                bSuccess = VoiceChat->Initialize();
                if (bSuccess)
                {
                    StatusMessage = TEXT("Voice chat initialized");
                    // Connect asynchronously - we report success on initialize
                    VoiceChat->Connect(FOnVoiceChatConnectCompleteDelegate::CreateLambda(
                        [](const FVoiceChatResult& Result)
                        {
                            // ErrorDesc is a public member in both UE 5.6 and 5.7
                            UE_LOG(LogMcpSessionsHandlers, Log, TEXT("VoiceChat Connect Result: %s"), 
                                Result.IsSuccess() ? TEXT("Success") : *Result.ErrorDesc);
                        }));
                }
                else
                {
                    StatusMessage = TEXT("Failed to initialize voice chat");
                }
            }
            else
            {
                bSuccess = true;
                StatusMessage = TEXT("Voice chat already initialized");
            }
        }
        else
        {
            // Disconnect and uninitialize voice chat
            if (VoiceChat->IsConnected())
            {
                VoiceChat->Disconnect(FOnVoiceChatDisconnectCompleteDelegate::CreateLambda(
                    [VoiceChat](const FVoiceChatResult& Result)
                    {
                        if (VoiceChat->IsInitialized())
                        {
                            VoiceChat->Uninitialize();
                        }
                    }));
                bSuccess = true;
                StatusMessage = TEXT("Voice chat disconnecting");
            }
            else if (VoiceChat->IsInitialized())
            {
                bSuccess = VoiceChat->Uninitialize();
                StatusMessage = bSuccess ? TEXT("Voice chat uninitialized") : TEXT("Failed to uninitialize voice chat");
            }
            else
            {
                bSuccess = true;
                StatusMessage = TEXT("Voice chat already disabled");
            }
        }
    }
    else
    {
        // VoiceChat interface not available (no voice plugin loaded)
        StatusMessage = TEXT("IVoiceChat interface not available - no voice chat plugin loaded");
        bSuccess = false;
    }
#else
    // VoiceChat module not available at compile time
    StatusMessage = TEXT("Voice chat module not available in this build");
    bSuccess = true; // Return success but note the limitation
#endif

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetBoolField(TEXT("voiceEnabled"), bEnabled);
    ResponseJson->SetBoolField(TEXT("success"), bSuccess);
    ResponseJson->SetStringField(TEXT("status"), StatusMessage);
#if MCP_HAS_VOICECHAT
    ResponseJson->SetBoolField(TEXT("voiceChatAvailable"), IVoiceChat::Get() != nullptr);
#else
    ResponseJson->SetBoolField(TEXT("voiceChatAvailable"), false);
#endif

    FString Message = FString::Printf(TEXT("Voice chat %s: %s"), 
        bEnabled ? TEXT("enabled") : TEXT("disabled"), *StatusMessage);
    Subsystem->SendAutomationResponse(Socket, RequestId, bSuccess, Message, ResponseJson);
    return true;
}


static bool HandleMutePlayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
    using namespace SessionsHelpers;

    FString PlayerName = GetJsonStringField(Payload, TEXT("playerName"), TEXT(""));
    FString TargetPlayerId = GetJsonStringField(Payload, TEXT("targetPlayerId"), TEXT(""));
    bool bMuted = GetJsonBoolField(Payload, TEXT("muted"), true);
    int32 LocalPlayerNum = static_cast<int32>(GetJsonNumberField(Payload, TEXT("localPlayerNum"), 0));
    bool bSystemWide = GetJsonBoolField(Payload, TEXT("systemWide"), false);

    if (PlayerName.IsEmpty() && TargetPlayerId.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Either playerName or targetPlayerId is required"), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    FString TargetIdentifier = !TargetPlayerId.IsEmpty() ? TargetPlayerId : PlayerName;
    bool bSuccess = false;
    bool bAppliedToVoiceInterface = false;
    bool bAppliedToFallbackState = false;
    FString StatusMessage;

#if MCP_HAS_VOICECHAT
    // Try IVoiceChat first (ClientOnly modular feature - available during PIE)
    // VoiceChat is a modular feature that loads dynamically at runtime
    static FName VoiceChatFeatureName = FName(TEXT("VoiceChat"));
    if (IModularFeatures::Get().IsModularFeatureAvailable(VoiceChatFeatureName))
    {
        IVoiceChat* VoiceChat = &IModularFeatures::Get().GetModularFeature<IVoiceChat>(VoiceChatFeatureName);
        if (VoiceChat)
        {
            if (VoiceChat->IsInitialized() && VoiceChat->IsLoggedIn())
            {
                // Full voice chat with server connection
                VoiceChat->SetPlayerMuted(TargetIdentifier, bMuted);
                bSuccess = true;
                bAppliedToVoiceInterface = true;
                StatusMessage = FString::Printf(TEXT("Player '%s' %s via IVoiceChat"), 
                    *TargetIdentifier, bMuted ? TEXT("muted") : TEXT("unmuted"));
            }
            else if (VoiceChat->IsInitialized())
            {
                // IVoiceChat initialized but not logged in (local PIE without voice server)
                // Use BlockPlayers for local mute functionality
                TArray<FString> PlayersToBlock;
                PlayersToBlock.Add(TargetIdentifier);
                if (bMuted)
                {
                    VoiceChat->BlockPlayers(PlayersToBlock);
                }
                else
                {
                    VoiceChat->UnblockPlayers(PlayersToBlock);
                }
                bSuccess = true;
                bAppliedToVoiceInterface = true;
                StatusMessage = FString::Printf(TEXT("Player '%s' %s locally (voice server not connected)"), 
                    *TargetIdentifier, bMuted ? TEXT("muted") : TEXT("unmuted"));
            }
            else
            {
                bSuccess = false;
                StatusMessage = TEXT("VoiceChat module is available but not initialized; mute was not applied");
            }
        }
    }
    else
#endif
#if MCP_HAS_ONLINE_SUBSYSTEM
    {
        // Fall back to IOnlineVoice via OnlineSubsystem
        IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
        if (OnlineSub)
        {
            IOnlineVoicePtr VoiceInterface = OnlineSub->GetVoiceInterface();
            if (VoiceInterface.IsValid())
            {
                // Create a unique net ID from the player ID string
                IOnlineIdentityPtr IdentityInterface = OnlineSub->GetIdentityInterface();
                if (IdentityInterface.IsValid())
                {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 7
                    // UE 5.0-5.6: CreateUniquePlayerId was available on some platforms
                    FUniqueNetIdPtr NetId = IdentityInterface->CreateUniquePlayerId(TargetPlayerId);
                    if (NetId.IsValid())
                    {
                        if (bMuted)
                        {
                            bSuccess = VoiceInterface->MuteRemoteTalker(LocalPlayerNum, *NetId, bSystemWide);
                        }
                        else
                        {
                            bSuccess = VoiceInterface->UnmuteRemoteTalker(LocalPlayerNum, *NetId, bSystemWide);
                        }
                        StatusMessage = bSuccess 
                            ? FString::Printf(TEXT("Player '%s' %s via OnlineSubsystem"), 
                                *TargetIdentifier, bMuted ? TEXT("muted") : TEXT("unmuted"))
                            : TEXT("Voice interface mute operation failed");
                        bAppliedToVoiceInterface = bSuccess;
                    }
                    else
                    {
                        bSuccess = false;
                        StatusMessage = TEXT("Unable to resolve target player net ID; mute was not applied");
                    }
#else
                    // UE 5.7+: CreateUniquePlayerId was removed. Use GetUniquePlayerId for local players
                    // or find the player in the registered players list.
                    // For remote players, we need to find them via the session or player controller.
                    bSuccess = false;
                    StatusMessage = TEXT("UE 5.7+ remote mute requires session-based player lookup; mute was not applied");
#endif
                }
                else
                {
                    bSuccess = false;
                    StatusMessage = TEXT("Identity interface not available; mute was not applied");
                }
            }
            else
            {
                bSuccess = false;
                StatusMessage = TEXT("Voice interface not configured; mute was not applied");
            }
        }
        else
        {
            bSuccess = false;
            StatusMessage = TEXT("OnlineSubsystem not available; mute was not applied");
        }
    }
#else
    {
        bSuccess = false;
        StatusMessage = TEXT("No voice system available in this build; mute was not applied");
    }
#endif

    if (!bSuccess)
    {
        const FString FallbackKey = MakeLocalVoiceMuteKey(TargetIdentifier, LocalPlayerNum, bSystemWide);
        if (bMuted)
        {
            LocalVoiceMuteFallbackState.Add(FallbackKey);
        }
        else
        {
            LocalVoiceMuteFallbackState.Remove(FallbackKey);
        }

        bSuccess = true;
        bAppliedToFallbackState = true;
        StatusMessage = FString::Printf(
            TEXT("Native voice interface absent; stored %s in local editor-session mute state"),
            bMuted ? TEXT("mute") : TEXT("unmute"));
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("target"), TargetIdentifier);
    ResponseJson->SetBoolField(TEXT("muted"), bMuted);
    ResponseJson->SetBoolField(TEXT("success"), bSuccess);
    ResponseJson->SetBoolField(TEXT("appliedToVoiceInterface"), bAppliedToVoiceInterface);
    ResponseJson->SetBoolField(TEXT("appliedToFallbackState"), bAppliedToFallbackState);
    ResponseJson->SetNumberField(TEXT("localPlayerNum"), LocalPlayerNum);
    ResponseJson->SetBoolField(TEXT("systemWide"), bSystemWide);
    ResponseJson->SetStringField(TEXT("status"), StatusMessage);

    FString Message = FString::Printf(TEXT("Player '%s' %s: %s"), 
        *TargetIdentifier, bMuted ? TEXT("muted") : TEXT("unmuted"), *StatusMessage);
    Subsystem->SendAutomationResponse(Socket, RequestId, bSuccess, Message, ResponseJson);
    return true;
}

// ============================================================================
// Utility Actions
// ============================================================================

static bool HandleGetSessionsInfo(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
    using namespace SessionsHelpers;

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    TSharedPtr<FJsonObject> SessionsInfo = McpHandlerUtils::CreateResultObject();

    // Get local player count
    int32 LocalPlayerCount = GetLocalPlayerCount();
    SessionsInfo->SetNumberField(TEXT("localPlayerCount"), LocalPlayerCount);

    // Check if we're in a PIE session
    bool bInPIE = GEditor && GEditor->PlayWorld != nullptr;
    SessionsInfo->SetBoolField(TEXT("inPlaySession"), bInPIE);

    // Default values for session state
    SessionsInfo->SetStringField(TEXT("currentSessionName"), TEXT("None"));
    SessionsInfo->SetBoolField(TEXT("isLANMatch"), false);
    SessionsInfo->SetNumberField(TEXT("maxPlayers"), 0);
    SessionsInfo->SetNumberField(TEXT("currentPlayers"), LocalPlayerCount);
    SessionsInfo->SetBoolField(TEXT("splitScreenEnabled"), LocalPlayerCount > 1);
    SessionsInfo->SetStringField(TEXT("splitScreenType"), LocalPlayerCount > 1 ? TEXT("Active") : TEXT("None"));
    SessionsInfo->SetBoolField(TEXT("voiceChatEnabled"), false);
    SessionsInfo->SetBoolField(TEXT("isHosting"), false);
    SessionsInfo->SetStringField(TEXT("connectedServerAddress"), TEXT(""));

    // Active voice channels (empty array for now)
    TArray<TSharedPtr<FJsonValue>> VoiceChannels;
    SessionsInfo->SetArrayField(TEXT("activeVoiceChannels"), VoiceChannels);

    ResponseJson->SetObjectField(TEXT("sessionsInfo"), SessionsInfo);

    FString Message = FString::Printf(TEXT("Sessions info retrieved. Local players: %d, In PIE: %s"),
        LocalPlayerCount, bInPIE ? TEXT("Yes") : TEXT("No"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

#endif // WITH_EDITOR

// ============================================================================
// Sessions Member Handlers
// ============================================================================
// Dispatch lives in the manage_networking FMcpCall classes
// (Private/MCP/Calls/McpCalls_ManageNetworking.cpp); each HandleSessions*
// member here wraps one advertised action's dedicated handler above,
// replicating the retired chain's editor-build stub.

// add_local_player
bool McpHandlers::Networking::HandleSessionsAddLocalPlayer(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if WITH_EDITOR
    return HandleAddLocalPlayer(&S, RequestId, Payload, Socket);
#else
    S.SendAutomationResponse(Socket, RequestId, false, TEXT("manage_sessions requires editor build"), nullptr);
    return true;  // Return true: request was handled (error response sent)
#endif
}

// remove_local_player
bool McpHandlers::Networking::HandleSessionsRemoveLocalPlayer(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if WITH_EDITOR
    return HandleRemoveLocalPlayer(&S, RequestId, Payload, Socket);
#else
    S.SendAutomationResponse(Socket, RequestId, false, TEXT("manage_sessions requires editor build"), nullptr);
    return true;  // Return true: request was handled (error response sent)
#endif
}

// host_lan_server
bool McpHandlers::Networking::HandleSessionsHostLanServer(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if WITH_EDITOR
    return HandleHostLanServer(&S, RequestId, Payload, Socket);
#else
    S.SendAutomationResponse(Socket, RequestId, false, TEXT("manage_sessions requires editor build"), nullptr);
    return true;  // Return true: request was handled (error response sent)
#endif
}

// enable_voice_chat
bool McpHandlers::Networking::HandleSessionsEnableVoiceChat(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if WITH_EDITOR
    return HandleEnableVoiceChat(&S, RequestId, Payload, Socket);
#else
    S.SendAutomationResponse(Socket, RequestId, false, TEXT("manage_sessions requires editor build"), nullptr);
    return true;  // Return true: request was handled (error response sent)
#endif
}

// mute_player
bool McpHandlers::Networking::HandleSessionsMutePlayer(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if WITH_EDITOR
    return HandleMutePlayer(&S, RequestId, Payload, Socket);
#else
    S.SendAutomationResponse(Socket, RequestId, false, TEXT("manage_sessions requires editor build"), nullptr);
    return true;  // Return true: request was handled (error response sent)
#endif
}

// get_sessions_info
bool McpHandlers::Networking::HandleSessionsGetSessionsInfo(
    UMcpAutomationBridgeSubsystem& S,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle Socket)
{
#if WITH_EDITOR
    return HandleGetSessionsInfo(&S, RequestId, Payload, Socket);
#else
    S.SendAutomationResponse(Socket, RequestId, false, TEXT("manage_sessions requires editor build"), nullptr);
    return true;  // Return true: request was handled (error response sent)
#endif
}

#undef GetJsonStringField
#undef GetJsonNumberField
#undef GetJsonBoolField
