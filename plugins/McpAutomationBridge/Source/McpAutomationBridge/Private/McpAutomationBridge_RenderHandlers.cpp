// =============================================================================
// McpAutomationBridge_RenderHandlers.cpp
// =============================================================================
// MCP Automation Bridge - Rendering Handlers
//
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
//
// Handler Summary:
// -----------------------------------------------------------------------------
// system_control member handler (Editor Only); dispatch lives in the FMcpCall
// classes (Private/MCP/Calls/McpCalls_SystemControl.cpp).
//   - lumen_update_scene: Trigger Lumen scene recapture
//
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem, McpAutomationBridgeHelpers
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"

// -----------------------------------------------------------------------------
// Engine Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "UObject/Package.h"
#include "Runtime/Launch/Resources/Version.h"
#endif

// =============================================================================
// Handler Implementation
// =============================================================================

#if WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleRenderLumenUpdateScene(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FMcpResponseHandle RequestingSocket)
{
        // Execute via console command
        if (GEditor)
        {
            UWorld* World = GEditor->GetEditorWorldContext().World();
            if (World)
            {
                GEngine->Exec(World, TEXT("r.Lumen.Scene.Recapture"));

                TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
                Result->SetStringField(TEXT("action"), TEXT("system_control"));
                Result->SetStringField(TEXT("subAction"), TEXT("lumen_update_scene"));
                Result->SetStringField(TEXT("command"), TEXT("r.Lumen.Scene.Recapture"));
                Result->SetBoolField(TEXT("executed"), true);

                SendAutomationResponse(RequestingSocket, RequestId, true,
                    TEXT("Lumen scene recapture triggered."), Result);
                return true;
            }
        }

        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Could not execute command (no world context)."), TEXT("EXECUTION_FAILED"));
        return true;
}

#else

// RequiresEditor means Execute() rejects before Run() in non-editor builds;
// this exists only so the module links.
bool UMcpAutomationBridgeSubsystem::HandleRenderLumenUpdateScene(
    const FString&, const TSharedPtr<FJsonObject>&, FMcpResponseHandle)
{
    return false;
}

#endif // WITH_EDITOR
