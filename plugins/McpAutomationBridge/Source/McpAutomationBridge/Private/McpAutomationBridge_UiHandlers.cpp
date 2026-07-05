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

bool UMcpAutomationBridgeSubsystem::HandleUiCreateWidget(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("create_widget"));

  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

#if WITH_EDITOR && MCP_HAS_WIDGET_FACTORY
    FString WidgetName;
    if (!Payload->TryGetStringField(TEXT("name"), WidgetName) ||
        WidgetName.IsEmpty()) {
      Message = TEXT("name field required for create_widget");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/UI/Widgets");
      }

      FString WidgetType;
      Payload->TryGetStringField(TEXT("widgetType"), WidgetType);

      const FString NormalizedPath = SavePath.TrimStartAndEnd();
      const FString TargetPath =
          FString::Printf(TEXT("%s/%s"), *NormalizedPath, *WidgetName);
      if (UEditorAssetLibrary::DoesAssetExist(TargetPath)) {
        bSuccess = true;
        Message = FString::Printf(TEXT("Widget blueprint already exists at %s"),
                                  *TargetPath);
        Resp->SetStringField(TEXT("widgetPath"), TargetPath);
        Resp->SetBoolField(TEXT("exists"), true);
        if (!WidgetType.IsEmpty()) {
          Resp->SetStringField(TEXT("widgetType"), WidgetType);
        }
        Resp->SetStringField(TEXT("widgetName"), WidgetName);
      } else {
        UWidgetBlueprintFactory *Factory = NewObject<UWidgetBlueprintFactory>();
        if (!Factory) {
          Message = TEXT("Failed to create widget blueprint factory");
          ErrorCode = TEXT("FACTORY_CREATION_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          UObject *NewAsset = Factory->FactoryCreateNew(
              UWidgetBlueprint::StaticClass(),
              UEditorAssetLibrary::DoesAssetExist(NormalizedPath)
                  ? UEditorAssetLibrary::LoadAsset(NormalizedPath)
                  : nullptr,
              FName(*WidgetName), RF_Standalone, nullptr, GWarn);

          UWidgetBlueprint *WidgetBlueprint = Cast<UWidgetBlueprint>(NewAsset);

          if (!WidgetBlueprint) {
            Message = TEXT("Failed to create widget blueprint asset");
            ErrorCode = TEXT("ASSET_CREATION_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            // Force immediate save and registry scan
            SaveLoadedAssetThrottled(WidgetBlueprint, -1.0, true);
            ScanPathSynchronous(WidgetBlueprint->GetOutermost()->GetName());

            bSuccess = true;
            Message = FString::Printf(TEXT("Widget blueprint created at %s"),
                                      *WidgetBlueprint->GetPathName());
            Resp->SetStringField(TEXT("widgetPath"),
                                 WidgetBlueprint->GetPathName());
            Resp->SetStringField(TEXT("widgetName"), WidgetName);
            if (!WidgetType.IsEmpty()) {
              Resp->SetStringField(TEXT("widgetType"), WidgetType);
            }
          }
        }
      }
    }
#else
    Message =
        TEXT("create_widget requires editor build with widget factory support");
    ErrorCode = TEXT("NOT_AVAILABLE");
    Resp->SetStringField(TEXT("error"), Message);
#endif

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("System control action completed")
                       : TEXT("System control action failed");
  }

  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleUiAddWidgetChild(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("add_widget_child"));

  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

#if WITH_EDITOR && MCP_HAS_WIDGET_FACTORY
    FString WidgetPath;
    if (!Payload->TryGetStringField(TEXT("widgetPath"), WidgetPath) ||
        WidgetPath.IsEmpty()) {
      Message = TEXT("widgetPath required for add_widget_child");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UWidgetBlueprint *WidgetBP =
          LoadObject<UWidgetBlueprint>(nullptr, *WidgetPath);
      if (!WidgetBP) {
        Message = FString::Printf(TEXT("Could not find Widget Blueprint at %s"),
                                  *WidgetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        FString ChildClassPath;
        if (!Payload->TryGetStringField(TEXT("childClass"), ChildClassPath) ||
            ChildClassPath.IsEmpty()) {
          Message = TEXT("childClass required (e.g. /Script/UMG.Button)");
          ErrorCode = TEXT("INVALID_ARGUMENT");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          UClass *WidgetClass =
              UEditorAssetLibrary::FindAssetData(ChildClassPath)
                      .GetAsset()
                      .IsValid()
                  ? LoadClass<UObject>(nullptr, *ChildClassPath)
                  : FindObject<UClass>(nullptr, *ChildClassPath);

          // Try partial search for common UMG widgets
          if (!WidgetClass) {
            if (ChildClassPath.Contains(TEXT(".")))
              WidgetClass = FindObject<UClass>(nullptr, *ChildClassPath);
            else
              WidgetClass = FindObject<UClass>(
                  nullptr,
                  *FString::Printf(TEXT("/Script/UMG.%s"), *ChildClassPath));
          }

          if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass())) {
            Message = FString::Printf(
                TEXT("Could not resolve valid UWidget class from '%s'"),
                *ChildClassPath);
            ErrorCode = TEXT("CLASS_NOT_FOUND");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            FString ParentName;
            Payload->TryGetStringField(TEXT("parentName"), ParentName);

            WidgetBP->Modify();

            UWidget *NewWidget =
                WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass);

            bool bAdded = false;
            bool bIsRoot = false;

            if (ParentName.IsEmpty()) {
              // Try to set as RootWidget if empty
              if (WidgetBP->WidgetTree->RootWidget == nullptr) {
                WidgetBP->WidgetTree->RootWidget = NewWidget;
                bAdded = true;
                bIsRoot = true;
              } else {
                // Try to add to existing root if it's a panel
                UPanelWidget *RootPanel =
                    Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
                if (RootPanel) {
                  RootPanel->AddChild(NewWidget);
                  bAdded = true;
                } else {
                  Message = TEXT("Root widget is not a panel and already "
                                 "exists. Specify parentName.");
                  ErrorCode = TEXT("ROOT_Full");
                }
              }
            } else {
              // Find parent
              UWidget *ParentWidget =
                  WidgetBP->WidgetTree->FindWidget(FName(*ParentName));
              UPanelWidget *ParentPanel = Cast<UPanelWidget>(ParentWidget);
              if (ParentPanel) {
                ParentPanel->AddChild(NewWidget);
                bAdded = true;
              } else {
                Message = FString::Printf(
                    TEXT("Parent '%s' not found or is not a PanelWidget"),
                    *ParentName);
                ErrorCode = TEXT("PARENT_NOT_FOUND");
              }
            }

            if (bAdded) {
              bSuccess = true;
              Message = FString::Printf(TEXT("Added %s to %s"),
                                        *WidgetClass->GetName(),
                                        *WidgetBP->GetName());
              Resp->SetStringField(TEXT("widgetName"), NewWidget->GetName());
              Resp->SetStringField(TEXT("childClass"), WidgetClass->GetName());
            } else {
              if (Message.IsEmpty())
                Message = TEXT("Failed to add widget child.");
              Resp->SetStringField(TEXT("error"), Message);
            }
          }
        }
      }
    }
#else
    Message = TEXT("add_widget_child requires editor build");
    ErrorCode = TEXT("NOT_AVAILABLE");
    Resp->SetStringField(TEXT("error"), Message);
#endif

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("System control action completed")
                       : TEXT("System control action failed");
  }

  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleUiScreenshot(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle RequestingSocket) {
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("screenshot"));

  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

#if WITH_EDITOR
    // Take a screenshot of the viewport and return as base64
    FString RawScreenshotPath;
    Payload->TryGetStringField(TEXT("path"), RawScreenshotPath);

    FString ScreenshotPath;
    if (RawScreenshotPath.IsEmpty()) {
      ScreenshotPath =
          FPaths::ProjectSavedDir() / TEXT("Screenshots/WindowsEditor");
    } else {
      FString SafePath = SanitizeProjectFilePath(RawScreenshotPath);
      if (SafePath.IsEmpty()) {
        Message = FString::Printf(TEXT("Invalid or unsafe screenshot path: %s. Path must be relative to project."), *RawScreenshotPath);
        ErrorCode = TEXT("SECURITY_VIOLATION");
        Resp->SetStringField(TEXT("error"), Message);
        SendAutomationResponse(RequestingSocket, RequestId, false, Message, Resp, ErrorCode);
        return true;
      }

      ScreenshotPath = FPaths::ProjectDir() / SafePath;
      ScreenshotPath = FPaths::ConvertRelativePathToFull(ScreenshotPath);
      FPaths::NormalizeFilename(ScreenshotPath);

      FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
      FPaths::NormalizeDirectoryName(NormalizedProjectDir);
      if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
        NormalizedProjectDir += TEXT("/");
      }

      if (!ScreenshotPath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
        Message = FString::Printf(TEXT("Invalid or unsafe screenshot path: %s. Path escapes project directory."), *RawScreenshotPath);
        ErrorCode = TEXT("SECURITY_VIOLATION");
        Resp->SetStringField(TEXT("error"), Message);
        SendAutomationResponse(RequestingSocket, RequestId, false, Message, Resp, ErrorCode);
        return true;
      }
    }

    FString Filename;
    Payload->TryGetStringField(TEXT("filename"), Filename);

    // SECURITY: Sanitize filename to prevent path traversal
    // Strip directory components and validate against traversal patterns
    Filename = FPaths::GetCleanFilename(Filename);
    if (Filename.Contains(TEXT("..")) || Filename.Contains(TEXT("/")) || Filename.Contains(TEXT("\\"))) {
      // Reject suspicious filename and use default
      Filename = FString::Printf(TEXT("Screenshot_%lld"),
                                 FDateTime::Now().ToUnixTimestamp());
    }

    if (Filename.IsEmpty()) {
      Filename = FString::Printf(TEXT("Screenshot_%lld"),
                                 FDateTime::Now().ToUnixTimestamp());
    }

    bool bReturnBase64 = true;
    Payload->TryGetBoolField(TEXT("returnBase64"), bReturnBase64);

    // Get viewport
    if (!GEngine || !GEngine->GameViewport) {
      Message = TEXT("No game viewport available");
      ErrorCode = TEXT("NO_VIEWPORT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UGameViewportClient *ViewportClient = GEngine->GameViewport;
      FViewport *Viewport = ViewportClient->Viewport;

      if (!Viewport) {
        Message = TEXT("No viewport available");
        ErrorCode = TEXT("NO_VIEWPORT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        // Capture viewport pixels
        TArray<FColor> Bitmap;
        FIntVector Size(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, 0);

        bool bReadSuccess = Viewport->ReadPixels(Bitmap);

        if (!bReadSuccess || Bitmap.Num() == 0) {
          Message = TEXT("Failed to read viewport pixels");
          ErrorCode = TEXT("CAPTURE_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          // Ensure we have the right size
          const int32 Width = Size.X;
          const int32 Height = Size.Y;

          // Compress to PNG
          // Note: ThumbnailCompressImageArray was introduced in UE 5.1
          TArray<uint8> PngData;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
          FImageUtils::ThumbnailCompressImageArray(Width, Height, Bitmap,
                                                   PngData);
#else
          // UE 5.0 fallback - use CompressImageArray
          FImageUtils::CompressImageArray(Width, Height, Bitmap, PngData);
#endif

          if (PngData.Num() == 0) {
            // Alternative: compress as PNG using IImageWrapper
            IImageWrapperModule &ImageWrapperModule =
                FModuleManager::LoadModuleChecked<IImageWrapperModule>(
                    FName("ImageWrapper"));
            TSharedPtr<IImageWrapper> ImageWrapper =
                ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

            if (ImageWrapper.IsValid()) {
              TArray<uint8> RawData;
              RawData.SetNum(Width * Height * 4);
              for (int32 i = 0; i < Bitmap.Num(); ++i) {
                RawData[i * 4 + 0] = Bitmap[i].R;
                RawData[i * 4 + 1] = Bitmap[i].G;
                RawData[i * 4 + 2] = Bitmap[i].B;
                RawData[i * 4 + 3] = Bitmap[i].A;
              }

              if (ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), Width,
                                       Height, ERGBFormat::RGBA, 8)) {
                PngData = ImageWrapper->GetCompressed(100);
              }
            }
          }

          FString FullPath =
              FPaths::Combine(ScreenshotPath, Filename + TEXT(".png"));
          FPaths::MakeStandardFilename(FullPath);

          // Always save to disk
          IFileManager::Get().MakeDirectory(*ScreenshotPath, true);
          bool bSaved = FFileHelper::SaveArrayToFile(PngData, *FullPath);

          bSuccess = true;
          Message = FString::Printf(TEXT("Screenshot captured (%dx%d)"), Width,
                                    Height);
          Resp->SetStringField(TEXT("screenshotPath"), FullPath);
          Resp->SetStringField(TEXT("filename"), Filename);
          Resp->SetNumberField(TEXT("width"), Width);
          Resp->SetNumberField(TEXT("height"), Height);
          Resp->SetNumberField(TEXT("sizeBytes"), PngData.Num());

          // Return base64 encoded image if requested
          if (bReturnBase64 && PngData.Num() > 0) {
            FString Base64Data = FBase64::Encode(PngData);
            Resp->SetStringField(TEXT("imageBase64"), Base64Data);
            Resp->SetStringField(TEXT("mimeType"), TEXT("image/png"));
          }
        }
      }
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

  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleUiGetProjectSettings(
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

  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleUiSetProjectSetting(
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

  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
}
