// FMcpCall::Execute — the shared per-call pipeline (envelope checks -> Run).
// Defined here in the main module, not upstream in McpToolSchema, because it
// drives UMcpAutomationBridgeSubsystem::SendAutomationError + GEditor, which the
// upstream schema module cannot see.
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

bool FMcpCall::Execute(UMcpAutomationBridgeSubsystem& Subsystem,
                       const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload,
                       FMcpResponseHandle ResponseHandle)
{
	const FMcpCallDecl& Decl = GetDecl();
	if (!Payload.IsValid())
	{
		Subsystem.SendAutomationError(ResponseHandle, RequestId,
			FString::Printf(TEXT("%s payload missing."), Decl.Tool),
			TEXT("INVALID_PAYLOAD"));
		return true;
	}
	if (EnumHasAnyFlags(Decl.Flags, EMcpCallFlags::RequiresEditor))
	{
#if WITH_EDITOR
		if (!GEditor)
		{
			Subsystem.SendAutomationError(ResponseHandle, RequestId,
				TEXT("Editor not available"), TEXT("EDITOR_NOT_AVAILABLE"));
			return true;
		}
#else
		Subsystem.SendAutomationError(ResponseHandle, RequestId,
			FString::Printf(TEXT("%s.%s requires an editor build."), Decl.Tool, Decl.Action),
			TEXT("NOT_IMPLEMENTED"));
		return true;
#endif
	}
	return Run(Subsystem, RequestId, Payload, ResponseHandle);
}
