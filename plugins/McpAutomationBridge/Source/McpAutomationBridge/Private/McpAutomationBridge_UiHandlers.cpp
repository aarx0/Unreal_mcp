// =============================================================================
// McpAutomationBridge_UiHandlers.cpp
// =============================================================================
// system_control UI/widget member handlers (HandleUi*). Dispatch lives in the
// FMcpCall classes (Private/MCP/Calls/McpCalls_SystemControl.cpp); each member
// here implements one advertised action.
//
// HANDLERS IMPLEMENTED:
// ---------------------
//   - create_widget: Create UMG widget blueprint
//   - add_widget_child: Add child widget to widget tree
//   - screenshot: Capture viewport screenshot with base64 encoding
//   - get_project_settings: Read common project settings
//   - set_project_setting: Write a config value to DefaultEngine.ini
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0: FImageUtils::CompressImageArray (no ThumbnailCompressImageArray)
// UE 5.1+: FImageUtils::ThumbnailCompressImageArray available
// WidgetBlueprintFactory: Header location varies by UE version
//
// SECURITY:
// ---------
// - Screenshot paths validated and sanitized
// - No arbitrary code execution via widget operations
// =============================================================================

// =============================================================================
// Version Compatibility Header (MUST BE FIRST)
// =============================================================================
#include "McpVersionCompatibility.h"

// =============================================================================
// Core Headers
// =============================================================================
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_UiHandlers.h"

// =============================================================================
// Editor-Only Headers
// =============================================================================
#if WITH_EDITOR

// Asset Management
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"

// Widget Support
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "WidgetBlueprint.h"

// Engine & Rendering
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UnrealClient.h"

// Widget Factory (version-dependent header location)
#if __has_include("Factories/WidgetBlueprintFactory.h")
#include "Factories/WidgetBlueprintFactory.h"
#define MCP_HAS_WIDGET_FACTORY 1
#else
#define MCP_HAS_WIDGET_FACTORY 0
#endif

#endif // WITH_EDITOR

// =============================================================================
// Handler Implementation
// =============================================================================

bool McpHandlers::SystemControl::HandleUiGetProjectSettings(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("get_project_settings"));

  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

#if WITH_EDITOR
    FString Section;
    Payload->TryGetStringField(TEXT("section"), Section);
    Payload->TryGetStringField(TEXT("category"), Section);  // Accept both

    TSharedPtr<FJsonObject> SettingsObj = MakeShared<FJsonObject>();

    // Get common project settings
    if (GEngine) {
      // Engine settings
      SettingsObj->SetStringField(TEXT("engineVersion"), FString::Printf(TEXT("%d.%d"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION));

      // Project name
      FString ProjectName = FApp::GetProjectName();
      SettingsObj->SetStringField(TEXT("projectName"), ProjectName);

      // Project directory
      FString ProjectDir = FPaths::ProjectDir();
      SettingsObj->SetStringField(TEXT("projectDir"), ProjectDir);

      // Game engine settings via config
      FString ResolutionX, ResolutionY;
      GConfig->GetString(TEXT("/Script/Engine.GameUserSettings"), TEXT("ResolutionSizeX"), ResolutionX, GGameUserSettingsIni);
      GConfig->GetString(TEXT("/Script/Engine.GameUserSettings"), TEXT("ResolutionSizeY"), ResolutionY, GGameUserSettingsIni);
      if (!ResolutionX.IsEmpty() && !ResolutionY.IsEmpty()) {
        TSharedPtr<FJsonObject> ResObj = MakeShared<FJsonObject>();
        ResObj->SetStringField(TEXT("width"), ResolutionX);
        ResObj->SetStringField(TEXT("height"), ResolutionY);
        SettingsObj->SetObjectField(TEXT("resolution"), ResObj);
      }

      // Fullscreen mode
      FString FullscreenMode;
      GConfig->GetString(TEXT("/Script/Engine.GameUserSettings"), TEXT("LastConfirmedFullscreenMode"), FullscreenMode, GGameUserSettingsIni);
      if (!FullscreenMode.IsEmpty()) {
        SettingsObj->SetStringField(TEXT("fullscreenMode"), FullscreenMode);
      }
    }

    Resp->SetObjectField(TEXT("settings"), SettingsObj);
    bSuccess = true;
    Message = TEXT("Project settings retrieved");
#else
  Message = TEXT("System control actions require editor build.");
  ErrorCode = TEXT("NOT_IMPLEMENTED");
  Resp->SetStringField(TEXT("error"), Message);
#endif

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("System control action completed")
                       : TEXT("System control action failed");
  }

  S.SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
}

bool McpHandlers::SystemControl::HandleUiSetProjectSetting(UMcpAutomationBridgeSubsystem& S,
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("set_project_setting"));

  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

#if WITH_EDITOR
    FString Section, Key, Value;
    Payload->TryGetStringField(TEXT("section"), Section);
    Payload->TryGetStringField(TEXT("key"), Key);
    Payload->TryGetStringField(TEXT("value"), Value);

    if (Section.IsEmpty() || Key.IsEmpty()) {
      Message = TEXT("section and key are required for set_project_setting");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      // Try to set the config value
      // First, normalize section format (ensure it starts with /Script/ if it looks like a UE section)
      FString NormalizedSection = Section;
      if (!NormalizedSection.StartsWith(TEXT("/")) && !NormalizedSection.StartsWith(TEXT("["))) {
        NormalizedSection = FString::Printf(TEXT("/Script/%s"), *Section);
      }

      // Set the value in the appropriate config file
      // For project settings, use DefaultEngine.ini
      FString ConfigFile = FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini");

      // Use GConfig to set the value
      GConfig->SetString(*NormalizedSection, *Key, *Value, ConfigFile);
      GConfig->Flush(false, ConfigFile);

      Resp->SetStringField(TEXT("section"), NormalizedSection);
      Resp->SetStringField(TEXT("key"), Key);
      Resp->SetStringField(TEXT("value"), Value);
      bSuccess = true;
      Message = FString::Printf(TEXT("Set %s.%s = %s"), *NormalizedSection, *Key, *Value);
    }
#else
  Message = TEXT("System control actions require editor build.");
  ErrorCode = TEXT("NOT_IMPLEMENTED");
  Resp->SetStringField(TEXT("error"), Message);
#endif

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("System control action completed")
                       : TEXT("System control action failed");
  }

  S.SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
}
