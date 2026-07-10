// Response-routing handle shared across modules. Extracted from the main
// module's globals so upstream (McpToolSchema) can name it in FMcpCall
// signatures without depending downstream.
#pragma once

#include "Templates/SharedPointer.h"

// Inert response-routing handle threaded through handler signatures. Always
// null: the legacy WebSocket transport was deleted and responses route by
// RequestId via FMcpNativeTransport's pending-request map. The class is never
// defined, so the handle cannot be dereferenced.
class FMcpBridgeWebSocket;
using FMcpResponseHandle = TSharedPtr<FMcpBridgeWebSocket>;
