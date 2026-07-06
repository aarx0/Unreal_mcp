// McpDeriveDecl.cpp — derive a lean validation decl from an authored schema
// fragment. schema-from-decls: AppendSchema() is the single source; the
// FMcpCallDecl (transport validation) is read back out of the built JSON here,
// so the two can never drift.
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "Dom/JsonObject.h"
#include "Misc/ScopeLock.h"

static EMcpParamKind McpJsonTypeToKind(const FString& Type)
{
    if (Type == TEXT("string"))  return EMcpParamKind::String;
    if (Type == TEXT("number"))  return EMcpParamKind::Number;
    if (Type == TEXT("integer")) return EMcpParamKind::Number; // validation treats both as numeric
    if (Type == TEXT("boolean")) return EMcpParamKind::Bool;
    if (Type == TEXT("object"))  return EMcpParamKind::Object;
    if (Type == TEXT("array"))   return EMcpParamKind::Array;
    return EMcpParamKind::Any;
}

const FMcpCallDecl& McpDeriveDecl(const TCHAR* Tool, const TCHAR* Action,
                                  EMcpCallFlags Flags, void (*SchemaFn)(FMcpSchemaBuilder&))
{
    static FCriticalSection Mutex;
    FScopeLock Lock(&Mutex);

    // Grow-only owning pools: the returned decl, its Params view, and each Name
    // pointer must outlive the call.
    static TArray<TUniquePtr<TArray<FString>>>       NamePools;
    static TArray<TUniquePtr<TArray<FMcpParamDecl>>> ParamPools;
    static TArray<TUniquePtr<FMcpCallDecl>>          DeclPool;

    FMcpSchemaBuilder Builder;
    SchemaFn(Builder);
    const TSharedPtr<FJsonObject> Schema = Builder.Build();

    TArray<FString>&       Names  = *NamePools.Add_GetRef(MakeUnique<TArray<FString>>());
    TArray<FMcpParamDecl>& Params = *ParamPools.Add_GetRef(MakeUnique<TArray<FMcpParamDecl>>());

    TSet<FString> RequiredSet;
    const TArray<TSharedPtr<FJsonValue>>* ReqArr = nullptr;
    if (Schema.IsValid() && Schema->TryGetArrayField(TEXT("required"), ReqArr) && ReqArr)
    {
        for (const TSharedPtr<FJsonValue>& V : *ReqArr) { RequiredSet.Add(V->AsString()); }
    }

    const TSharedPtr<FJsonObject> Props =
        Schema.IsValid() ? Schema->GetObjectField(TEXT("properties")) : nullptr;
    if (Props.IsValid())
    {
        Names.Reserve(Props->Values.Num());          // pass 1: stable name storage
        for (const auto& KV : Props->Values) { Names.Add(KV.Key); }
        Params.Reserve(Names.Num());                 // pass 2: same map order → aligned
        int32 Idx = 0;
        for (const auto& KV : Props->Values)
        {
            FString Type;
            if (const TSharedPtr<FJsonObject> P = KV.Value->AsObject())
            {
                P->TryGetStringField(TEXT("type"), Type);
            }
            Params.Add(FMcpParamDecl{ *Names[Idx], McpJsonTypeToKind(Type), RequiredSet.Contains(KV.Key) });
            ++Idx;
        }
    }

    FMcpCallDecl& Decl = *DeclPool.Add_GetRef(MakeUnique<FMcpCallDecl>());
    Decl.Tool = Tool; Decl.Action = Action; Decl.Params = Params; Decl.Flags = Flags;
    return Decl;
}
