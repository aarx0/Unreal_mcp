#pragma once

// manage_asset material-authoring handlers as namespaced free functions.
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
bool HandleMaterialCreateMaterial(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialSetBlendMode(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialSetShadingModel(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialSetMaterialDomain(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialAddTextureSample(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialAddTextureCoordinate(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialAddScalarParameter(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialAddVectorParameter(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialAddStaticSwitchParameter(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialAddMathNode(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialAddSimpleExpression(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &SubAction, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialAddConditional(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &SubAction, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialAddCustomExpression(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialConnectNodes(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialDisconnectNodes(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialCreateMaterialFunction(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialAddFunctionIO(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &SubAction, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialUseMaterialFunction(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialCreateMaterialInstance(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialSetScalarParameterValue(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialSetVectorParameterValue(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialSetTextureParameterValue(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialCreateSpecialMaterial(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const FString &SubAction, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialAddLandscapeLayer(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialConfigureLayerBlend(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialCompileMaterial(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialGetMaterialInfo(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialGetMaterialFunctionInfo(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialFindNode(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialGetNodeConnections(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialGetNodeProperties(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialSetNodeValue(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialSetStaticSwitchParameterValue(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialDeleteNode(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialUpdateCustomExpression(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialGetNodeChain(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialGetConnectedSubgraph(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialAddMaterialNode(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialRemoveMaterialNode(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialSetMaterialParameter(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialGetMaterialNodeDetails(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialSetTwoSided(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
bool HandleMaterialArrangeGraph(UMcpAutomationBridgeSubsystem& S, const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, FMcpResponseHandle Socket);
} // namespace McpHandlers::Asset
