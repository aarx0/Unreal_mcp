// McpStartupValidation.h — boot-time drift checks for the tool/action vocabulary

#pragma once

#include "HAL/Platform.h"

namespace McpStartupValidation
{
/**
 * Validate the action vocabulary at editor boot:
 *  - every canonical tool name has a registered definition,
 *  - no action is claimed by two routed families of the same tool,
 *  - no routed family shadows an action in the tool's core (fallthrough) list,
 *  - no list contains duplicates,
 *  - shared schema unions match the schema each facade actually publishes.
 *
 * Violations are UE_LOG(Error) with the exact action names, plus one ensure so
 * they cannot pass silently. Returns the violation count (0 = clean).
 */
int32 ValidateActionRouting();
}
