#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"

TMap<FString, TArray<TPair<FString, FMcpResponseHandle>>>
    GBlueprintCreateInflight;
TMap<FString, double> GBlueprintCreateInflightTs;
FCriticalSection GBlueprintCreateMutex;
double GBlueprintCreateStaleTimeoutSec = 60.0;
TSet<FString> GBlueprintBusySet;
TMap<FString, TSharedPtr<FJsonObject>> GBlueprintRegistry;

FString GCurrentSequencePath;

TMap<FString, TSharedPtr<FJsonObject>> GNiagaraRegistry;

// Recent asset save tracking (throttle across plugin to avoid frequent
// SavePackage calls)
TMap<FString, double> GRecentAssetSaveTs;
FCriticalSection GRecentAssetSaveMutex;
double GRecentAssetSaveThrottleSeconds = 0.5;
