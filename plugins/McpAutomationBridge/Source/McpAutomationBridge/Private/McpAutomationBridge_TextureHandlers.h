#pragma once

// manage_asset texture-authoring handlers as namespaced free functions.
// De-membered off UMcpAutomationBridgeSubsystem (F1 module split); the subsystem
// is passed by reference as the first parameter (S), and handlers touch instance
// state only through its public response API (S.SendAutomationResponse /
// S.SendAutomationError). Dispatch: MCP/Calls/McpCalls_ManageAsset.cpp.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h" // FMcpResponseHandle

class UMcpAutomationBridgeSubsystem;

namespace McpHandlers::Asset
{
bool HandleTextureCreateNoiseTexture(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureCreateGradientTexture(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureCreatePatternTexture(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureCreateNormalFromHeight(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureSetCompressionSettings(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureSetTextureGroup(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureSetLodBias(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureConfigureVirtualTexture(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureSetStreamingPriority(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureGetTextureInfo(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureResizeTexture(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureInvert(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureDesaturate(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureAdjustLevels(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureBlur(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureSharpen(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureChannelPack(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureCombineTextures(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureAdjustCurves(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureChannelExtract(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureCreateRenderTarget(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
bool HandleTextureCreateAoFromMesh(UMcpAutomationBridgeSubsystem& S, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle RequestingSocket);
} // namespace McpHandlers::Asset
