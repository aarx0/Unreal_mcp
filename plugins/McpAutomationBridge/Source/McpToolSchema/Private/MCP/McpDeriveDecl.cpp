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

// Grow-only owning pools shared by the whole (recursive) derivation. Every Name
// pointer, member/element sub-array, and the decl itself must outlive the call —
// the returned views point straight into these and live for the process. The
// caller (McpDeriveDecl) holds a process-wide mutex across all pool writes.
using FMcpNamePool  = TArray<TUniquePtr<TArray<FString>>>;
using FMcpParamPool = TArray<TUniquePtr<TArray<FMcpParamDecl>>>;

static TConstArrayView<FMcpParamDecl> McpDeriveElement(
    const TSharedPtr<FJsonObject>& ArraySchema, FMcpNamePool& NamePools, FMcpParamPool& ParamPools);

// Derive the member schemas of one object node (its "properties", with "required"
// folded into each member's bRequired for the TOP level — nested nodes carry it
// too but the transport never reads nested-required). Recurses into Object members
// and Array elements. Returns a view into a freshly pooled, never-again-grown array.
static TConstArrayView<FMcpParamDecl> McpDeriveMembers(
    const TSharedPtr<FJsonObject>& Schema, FMcpNamePool& NamePools, FMcpParamPool& ParamPools)
{
    if (!Schema.IsValid())
    {
        return TConstArrayView<FMcpParamDecl>();
    }
    const TSharedPtr<FJsonObject> Props = Schema->GetObjectField(TEXT("properties"));
    if (!Props.IsValid())
    {
        return TConstArrayView<FMcpParamDecl>();
    }

    TSet<FString> RequiredSet;
    const TArray<TSharedPtr<FJsonValue>>* ReqArr = nullptr;
    if (Schema->TryGetArrayField(TEXT("required"), ReqArr) && ReqArr)
    {
        for (const TSharedPtr<FJsonValue>& V : *ReqArr) { RequiredSet.Add(V->AsString()); }
    }

    TArray<FString>&       Names  = *NamePools.Add_GetRef(MakeUnique<TArray<FString>>());
    TArray<FMcpParamDecl>& Params = *ParamPools.Add_GetRef(MakeUnique<TArray<FMcpParamDecl>>());

    Names.Reserve(Props->Values.Num());              // pass 1: stable name storage
    for (const auto& KV : Props->Values) { Names.Add(KV.Key); }
    Params.Reserve(Names.Num());                     // pass 2: same map order → aligned
    int32 Idx = 0;
    for (const auto& KV : Props->Values)
    {
        FString Type;
        const TSharedPtr<FJsonObject> P = KV.Value->AsObject();
        if (P.IsValid())
        {
            P->TryGetStringField(TEXT("type"), Type);
        }
        const EMcpParamKind Kind = McpJsonTypeToKind(Type);

        FMcpParamDecl Decl;
        Decl.Name = *Names[Idx];
        Decl.Kind = Kind;
        Decl.bRequired = RequiredSet.Contains(KV.Key);
        // Recurse into structured members. Each recursion appends its own pooled
        // arrays; because Params is reserved to its final size it never reallocates,
        // and the child views point into separate stable pools.
        if (Kind == EMcpParamKind::Object && P.IsValid())
        {
            Decl.Members = McpDeriveMembers(P, NamePools, ParamPools);
        }
        else if (Kind == EMcpParamKind::Array && P.IsValid())
        {
            Decl.Element = McpDeriveElement(P, NamePools, ParamPools);
        }
        Params.Add(Decl);
        ++Idx;
    }
    return Params;
}

// Derive the single element schema of an array node (its "items"). Returns a
// 0-or-1-entry view; the entry's Name is null (elements are positional).
static TConstArrayView<FMcpParamDecl> McpDeriveElement(
    const TSharedPtr<FJsonObject>& ArraySchema, FMcpNamePool& NamePools, FMcpParamPool& ParamPools)
{
    const TSharedPtr<FJsonObject> Items =
        ArraySchema.IsValid() ? ArraySchema->GetObjectField(TEXT("items")) : nullptr;
    if (!Items.IsValid())
    {
        return TConstArrayView<FMcpParamDecl>();
    }
    FString Type;
    Items->TryGetStringField(TEXT("type"), Type);
    const EMcpParamKind Kind = McpJsonTypeToKind(Type);

    TArray<FMcpParamDecl>& Elem = *ParamPools.Add_GetRef(MakeUnique<TArray<FMcpParamDecl>>());
    Elem.Reserve(1);
    FMcpParamDecl Decl;
    Decl.Name = nullptr;
    Decl.Kind = Kind;
    if (Kind == EMcpParamKind::Object)
    {
        Decl.Members = McpDeriveMembers(Items, NamePools, ParamPools);
    }
    else if (Kind == EMcpParamKind::Array)
    {
        Decl.Element = McpDeriveElement(Items, NamePools, ParamPools);
    }
    Elem.Add(Decl);
    return Elem;
}

const FMcpCallDecl& McpDeriveDecl(const TCHAR* Tool, const TCHAR* Action,
                                  EMcpCallFlags Flags, void (*SchemaFn)(FMcpSchemaBuilder&))
{
    static FCriticalSection Mutex;
    FScopeLock Lock(&Mutex);

    // Grow-only owning pools: the returned decl and everything its views point at
    // must outlive the call.
    static FMcpNamePool                     NamePools;
    static FMcpParamPool                    ParamPools;
    static TArray<TUniquePtr<FMcpCallDecl>> DeclPool;
    // Required-group pools (Feature 1): stable member-name strings, the TCHAR*
    // pointers into them (grouped contiguously), and one view per group.
    static TArray<TUniquePtr<TArray<FString>>>                       GroupNamePools;
    static TArray<TUniquePtr<TArray<const TCHAR*>>>                  GroupPtrPools;
    static TArray<TUniquePtr<TArray<TConstArrayView<const TCHAR*>>>> GroupViewPools;

    FMcpSchemaBuilder Builder;
    SchemaFn(Builder);
    const TSharedPtr<FJsonObject> Schema = Builder.Build();

    FMcpCallDecl& Decl = *DeclPool.Add_GetRef(MakeUnique<FMcpCallDecl>());
    Decl.Tool = Tool; Decl.Action = Action; Decl.Flags = Flags;
    Decl.Params = McpDeriveMembers(Schema, NamePools, ParamPools);

    // Required any-of groups: authored via FMcpSchemaBuilder::RequiredAnyOf(), read
    // back here — they never appear in the built JSON (published schema unchanged).
    const TArray<TArray<FString>>& AuthoredGroups = Builder.GetRequiredGroups();
    if (AuthoredGroups.Num() > 0)
    {
        TArray<FString>&                       GNames = *GroupNamePools.Add_GetRef(MakeUnique<TArray<FString>>());
        TArray<const TCHAR*>&                  GPtrs  = *GroupPtrPools.Add_GetRef(MakeUnique<TArray<const TCHAR*>>());
        TArray<TConstArrayView<const TCHAR*>>& GViews = *GroupViewPools.Add_GetRef(MakeUnique<TArray<TConstArrayView<const TCHAR*>>>());

        int32 Total = 0;
        for (const TArray<FString>& G : AuthoredGroups) { Total += G.Num(); }
        GNames.Reserve(Total);                        // pass 1: stable member strings
        for (const TArray<FString>& G : AuthoredGroups)
        {
            for (const FString& Member : G) { GNames.Add(Member); }
        }
        GPtrs.Reserve(Total);                         // pass 2: TCHAR* into GNames (aligned)
        for (const FString& Member : GNames) { GPtrs.Add(*Member); }
        GViews.Reserve(AuthoredGroups.Num());         // pass 3: one contiguous slice per group
        int32 Off = 0;
        for (const TArray<FString>& G : AuthoredGroups)
        {
            GViews.Add(TConstArrayView<const TCHAR*>(GPtrs.GetData() + Off, G.Num()));
            Off += G.Num();
        }
        Decl.RequiredGroups = GViews;
    }
    return Decl;
}
