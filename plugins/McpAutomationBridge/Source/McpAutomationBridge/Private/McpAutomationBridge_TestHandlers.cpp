// =============================================================================
// McpAutomationBridge_TestHandlers.cpp
// =============================================================================
// MCP Automation Bridge - Test Automation Handlers
// 
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
// 
// Handler Summary:
// -----------------------------------------------------------------------------
// Action: manage_tests
//   - run_tests: Execute automation tests by filter via FAutomationTestFramework
// 
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem, McpAutomationBridgeHelpers
//   - Engine: AutomationTest module
// 
// Notes:
//   - Tests run asynchronously; results appear in logs
//   - StartTestByName() initiates test execution
//   - For synchronous results, would require OnTestEnd delegate binding
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
#include "Misc/AutomationTest.h"

// =============================================================================
// Handler Implementation
// =============================================================================

