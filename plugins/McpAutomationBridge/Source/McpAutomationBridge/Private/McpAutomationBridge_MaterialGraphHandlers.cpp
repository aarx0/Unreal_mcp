// =============================================================================
// McpAutomationBridge_MaterialGraphHandlers.cpp
// =============================================================================
// Material Graph Manipulation Handlers for MCP Automation Bridge.
//
// This file implements the following handlers:
// - manage_material_graph (main dispatcher)
//   - add_node: Add expression node to material
//   - remove_node: Remove expression from material
//   - connect_nodes/connect_pins: Connect expressions or to main material
//   - break_connections: Disconnect expression inputs
//   - get_node_details: Get node info or list all nodes
// - add_material_texture_sample: Add TextureSample expression
// - add_material_expression: Add generic expression by class name
// - create_material_nodes: Batch create multiple nodes
//
// UE VERSION COMPATIBILITY:
// - UE 5.0: Material->Expressions (direct TArray access)
// - UE 5.1+: Material->GetEditorOnlyData()->ExpressionCollection.Expressions
// - MCP_GET_MATERIAL_EXPRESSIONS macro abstracts this difference
//
// NODE IDENTIFICATION:
// - GUID string (MaterialExpressionGuid)
// - Node name (GetName())
// - Object path (GetPathName())
// - Parameter name (for parameter nodes)
// - Numeric index (0-based position in Expressions array)
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST BE FIRST - Version compatibility macros
#include "McpHandlerUtils.h"          // Utility functions for JSON parsing

#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR

// =============================================================================
// Engine Includes - Material System
// =============================================================================
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Engine/Texture.h"

// Material API compatibility macros are defined in McpAutomationBridgeHelpers.h

#endif // WITH_EDITOR

// =============================================================================
// Handler: manage_material_graph
// =============================================================================

