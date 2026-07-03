#include "McpAutomationBridgeSettings.h"

#include "Internationalization/Text.h"

UMcpAutomationBridgeSettings::UMcpAutomationBridgeSettings()
{
    ListenHost = TEXT("127.0.0.1");
    CapabilityToken = TEXT("");
    bRequireCapabilityToken = false;
    bAllowNonLoopback = false; // Security: default to loopback-only binding
}

/**
 * @brief Returns the localized text used as the settings section header for the MCP Automation Bridge.
 *
 * @return FText The localized label "MCP Automation Bridge" for display in the settings UI.
 */
FText UMcpAutomationBridgeSettings::GetSectionText() const
{
    return NSLOCTEXT("McpAutomationBridge", "SettingsSection", "MCP Automation Bridge");
}
