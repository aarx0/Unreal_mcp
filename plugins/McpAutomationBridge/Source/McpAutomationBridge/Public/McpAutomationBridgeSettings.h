#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "McpAutomationBridgeSettings.generated.h"

// Store these settings in the project's config (DefaultGame.ini) and expose
// them in Project Settings -> Plugins. Use defaultconfig so the values are
// written to the project's default INI file when persisted.
//
// Only settings the code actually reads live here; the WebSocket-era knobs
// (TLS, heartbeats, rate limits, multi-listen, ticker tuning) were deleted
// with that transport.

UCLASS(config=Game, defaultconfig, meta = (DisplayName = "MCP Automation Bridge"))
class MCPAUTOMATIONBRIDGE_API UMcpAutomationBridgeSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UMcpAutomationBridgeSettings();

    /** Host to bind the listening socket. Default: 127.0.0.1 (loopback).
     * To bind to LAN addresses (e.g., 0.0.0.0 or 192.168.x.x), enable bAllowNonLoopback in Security settings.
     */
    UPROPERTY(config, EditAnywhere, Category = "Connection")
    FString ListenHost;

    UPROPERTY(config, EditAnywhere, Category = "Security")
    FString CapabilityToken;

    /** When true, require a capability token for incoming connections (enforces matching token). */
    UPROPERTY(config, EditAnywhere, Category = "Security")
    bool bRequireCapabilityToken;

    /** SECURITY WARNING: When enabled, allows binding to non-loopback addresses (e.g., 0.0.0.0, 192.168.x.x).
     * This exposes the automation bridge to your local network. Only enable if you need LAN access
     * and understand the security implications. Default: false (loopback-only).
     */
    UPROPERTY(config, EditAnywhere, Category = "Security")
    bool bAllowNonLoopback;

    // ── Native MCP Streamable HTTP ──────────────────────────────────────

    /** Enable native MCP Streamable HTTP endpoint (POST /mcp).
     * AI clients (Claude Code, Cursor, etc.) connect directly.
     * Requires editor restart after changing. */
    UPROPERTY(config, EditAnywhere, Category = "Native MCP",
        meta = (DisplayName = "Enable Native MCP Server"))
    bool bEnableNativeMCP = false;

    /** Port for the native MCP HTTP server. */
    UPROPERTY(config, EditAnywhere, Category = "Native MCP",
        meta = (DisplayName = "Native MCP Port", EditCondition = "bEnableNativeMCP",
                ClampMin = "1024", ClampMax = "65535"))
    int32 NativeMCPPort = 3000;

    /** Load all 22 canonical tools on startup. */
    UPROPERTY(config, EditAnywhere, Category = "Native MCP",
        meta = (DisplayName = "Load All Tools on Start", EditCondition = "bEnableNativeMCP"))
    bool bLoadAllToolsOnStart = true;

    /** Additional instructions sent to AI clients in the MCP initialize response.
     * Use this to describe your project, conventions, or constraints.
     * Appended after the default server instructions from server-info.json. */
    UPROPERTY(config, EditAnywhere, Category = "Native MCP",
        meta = (DisplayName = "Server Instructions", EditCondition = "bEnableNativeMCP",
                MultiLine = "true"))
    FString NativeMCPInstructions;

    virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }
    virtual FText GetSectionText() const override;

#if WITH_EDITOR
    // Persist changed properties immediately when edited in Project Settings
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
    {
        Super::PostEditChangeProperty(PropertyChangedEvent);
        SaveConfig();
    }
#endif
};
