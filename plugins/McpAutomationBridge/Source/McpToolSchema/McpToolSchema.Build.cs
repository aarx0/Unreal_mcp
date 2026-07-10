using UnrealBuildTool;

public class McpToolSchema : ModuleRules
{
    public McpToolSchema(ReadOnlyTargetRules Target) : base(Target)
    {
        // NoPCHs matches the parent module and sidesteps the UE 5.0-5.2 MSVC
        // SharedPCH __has_feature issue; this module's headers pull only Core.
        PCHUsage = PCHUsageMode.NoPCHs;
        bUseUnity = true;

        // Tool schema + action registry only: no engine/editor surface. Public
        // because the exported headers (McpSchemaBuilder.h, McpJsonRpc.h, ...)
        // include Dom/JsonObject.h and are consumed by the main module.
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Json"
        });
    }
}
