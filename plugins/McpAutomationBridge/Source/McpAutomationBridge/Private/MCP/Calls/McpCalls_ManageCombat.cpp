// LINT-TOOL: manage_combat
// LINT-SCHEMA-DERIVED
// manage_combat as FMcpCall classes — tenth classed family
// (docs/action-declarations.md). Adopts schema-from-decls: each class AUTHORS
// its schema fragment in a S_<Suffix>() function; the published facade schema
// folds those fragments and GetDecl() derives the validation decl from the same
// fragment via McpDeriveDecl(), so schema and decl are one source and cannot
// drift. Run() delegates to the namespaced free handlers (McpHandlers::Combat::HandleCombat*,
// McpAutomationBridge_CombatHandlers.cpp).
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_CombatHandlers.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope helpers would collide across families otherwise.
namespace McpCalls::ManageCombat
{

// ─── Schema fragments ────────────────────────────────────────────────────────
// One S_<Suffix>() per action, authoring exactly the params that action reads
// (the fold dedups shared params to one entry). Descriptions are the tool's
// authored help text; McpDeriveDecl() reads the param kinds + required-set back
// out of these to build the transport validation decl. set_weapon_stats and
// setup_reload_system author criticalMultiplier/headshotMultiplier and maxAmmo
// as numbers (they belong to configure_damage_execution / setup_ammo_system;
// the handler accepts-but-reports-ignored them here) — the published schema
// already typed them number, so the derived decl only tightens an already-
// advertised-numeric param that used to validate as any.

static void S_CreateWeaponBlueprint(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier: asset name for create actions; hitbox component name for setup_hitbox_component."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .String(TEXT("weaponMeshPath"), TEXT("Path to weapon static/skeletal mesh."))
	 .Number(TEXT("baseDamage"), TEXT(""))
	 .Number(TEXT("fireRate"), TEXT(""))
	 .Number(TEXT("range"), TEXT(""))
	 .Number(TEXT("spread"), TEXT(""))
	 .Required({TEXT("name")});
}

static void S_ConfigureWeaponMesh(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("weaponMeshPath"), TEXT("Path to weapon static/skeletal mesh."))
	 .Required({TEXT("blueprintPath")});
}


static void S_SetWeaponStats(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("baseDamage"), TEXT(""))
	 .Number(TEXT("fireRate"), TEXT(""))
	 .Number(TEXT("range"), TEXT(""))
	 .Number(TEXT("spread"), TEXT(""))
	 .Number(TEXT("criticalMultiplier"), TEXT("Critical hit damage multiplier (consumed by configure_damage_execution, not set_weapon_stats)."))
	 .Number(TEXT("headshotMultiplier"), TEXT("Headshot damage multiplier (consumed by configure_damage_execution, not set_weapon_stats)."))
	 .Required({TEXT("blueprintPath")});
}






static void S_CreateProjectileBlueprint(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier: asset name for create actions; hitbox component name for setup_hitbox_component."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Number(TEXT("collisionRadius"), TEXT(""))
	 .String(TEXT("projectileMeshPath"), TEXT("Path to projectile mesh."))
	 .Number(TEXT("projectileSpeed"), TEXT(""))
	 .Number(TEXT("projectileGravityScale"), TEXT(""))
	 .Required({TEXT("name")});
}

static void S_ConfigureProjectileMovement(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("projectileSpeed"), TEXT(""))
	 .Number(TEXT("projectileGravityScale"), TEXT(""))
	 .Number(TEXT("projectileLifespan"), TEXT(""))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureProjectileCollision(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("collisionRadius"), TEXT(""))
	 .Bool(TEXT("bounceEnabled"), TEXT("Enable projectile bouncing."))
	 .Number(TEXT("bounceVelocityRatio"), TEXT("Velocity retained on bounce (0-1)."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureProjectileHoming(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("homingEnabled"), TEXT("Enable homing behavior."))
	 .Number(TEXT("homingAcceleration"), TEXT("Homing turn rate."))
	 .Required({TEXT("blueprintPath")});
}

static void S_CreateDamageType(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier: asset name for create actions; hitbox component name for setup_hitbox_component."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Required({TEXT("name")});
}


static void S_SetupHitboxComponent(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("name"), TEXT("Name identifier: asset name for create actions; hitbox component name for setup_hitbox_component."))
	 .StringEnum(TEXT("hitboxType"), {
		TEXT("Capsule"), TEXT("Box"), TEXT("Sphere")
	 }, TEXT("Hitbox collision shape."))
	 .String(TEXT("hitboxBoneName"), TEXT("Bone name for the hitbox; stored as the HitboxBoneName variable by setup_hitbox_component."))
	 .Bool(TEXT("isDamageZoneHead"), TEXT("Mark as headshot zone."))
	 .Number(TEXT("damageMultiplier"), TEXT("Damage multiplier for this hitbox."))
	 .Object(TEXT("hitboxSize"), TEXT("Hitbox dimensions."),
		[](FMcpSchemaBuilder& S) {
		S.Number(TEXT("radius"))
		 .Number(TEXT("halfHeight"))
		 .Object(TEXT("extent"), TEXT("3D extent (half-size)."),
			[](FMcpSchemaBuilder& Inner) {
			Inner.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
		});
	})
	 .Required({TEXT("blueprintPath")});
}



static void S_SetupAttachmentSystem(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .ArrayOfObjects(TEXT("attachmentSlots"), TEXT("Attachment slot definitions."))
	 .Required({TEXT("blueprintPath")});
}






static void S_CreateMeleeTrace(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("meleeTraceStartSocket"), TEXT("Socket for trace start."))
	 .String(TEXT("meleeTraceEndSocket"), TEXT("Socket for trace end."))
	 .Number(TEXT("meleeTraceRadius"), TEXT("Sphere trace radius."))
	 .Required({TEXT("blueprintPath")});
}


static void S_CreateHitPause(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("hitPauseDuration"), TEXT("Hitstop duration in seconds."))
	 .Number(TEXT("hitPauseTimeDilation"), TEXT("Time dilation during hitstop."))
	 .Required({TEXT("blueprintPath")});
}




static void S_GetInfo(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureHitDetection(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .StringEnum(TEXT("hitboxType"), {
		TEXT("Capsule"), TEXT("Box"), TEXT("Sphere")
	 }, TEXT("Hitbox collision shape."))
	 .Number(TEXT("damageMultiplier"), TEXT("Damage multiplier for this hitbox."))
	 .Required({TEXT("blueprintPath")});
}

static void S_GetStats(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Required({TEXT("blueprintPath")});
}

static void S_CreateDamageEffect(FMcpSchemaBuilder& B)
{
	B.String(TEXT("name"), TEXT("Name identifier: asset name for create actions; hitbox component name for setup_hitbox_component."))
	 .String(TEXT("path"), TEXT("Directory path for asset creation."))
	 .Number(TEXT("duration"), TEXT("Effect duration in seconds."))
	 .Number(TEXT("damagePerSecond"), TEXT("Damage per second for damage effects."))
	 .String(TEXT("effectType"), TEXT("Damage effect type."))
	 .Required({TEXT("name")});
}

static void S_ApplyDamage(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("damageAmount"), TEXT("Damage amount to apply."))
	 .String(TEXT("damageType"), TEXT("Damage type identifier."))
	 .Required({TEXT("blueprintPath")});
}

static void S_Heal(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("healAmount"), TEXT("Healing amount to apply."))
	 .Number(TEXT("maxHealth"), TEXT("Maximum health value."))
	 .Required({TEXT("blueprintPath")});
}

static void S_CreateShield(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("shieldAmount"), TEXT("Initial shield value; stored by create_shield as the CurrentShield variable default."))
	 .Number(TEXT("maxShield"), TEXT("Maximum shield value."))
	 .Number(TEXT("shieldRegenRate"), TEXT("Shield regeneration rate."))
	 .Number(TEXT("shieldRegenDelay"), TEXT("Delay before shield regeneration."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ModifyArmor(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("armorValue"), TEXT("Armor value to apply."))
	 .Number(TEXT("damageReduction"), TEXT("Armor damage reduction multiplier."))
	 .Required({TEXT("blueprintPath")});
}

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row (the implementation TU is editor-
// gated; the members answer the EDITOR_ONLY stub in non-editor builds).
// Mutating on all writers; the only readers are get_info and
// get_stats.

#define MCP_CB_CALL(ClassSuffix, ActionLiteral, HandlerFn, ExtraFlags)                     \
class FMcpCall_ManageCombat_##ClassSuffix final : public FMcpCall                          \
{                                                                                          \
	void AppendSchema(FMcpSchemaBuilder& B) const override { S_##ClassSuffix(B); }         \
	const FMcpCallDecl& GetDecl() const override                                           \
	{                                                                                       \
		static const FMcpCallDecl& D = McpDeriveDecl(TEXT("manage_combat"),                \
			TEXT(ActionLiteral), EMcpCallFlags::RequiresEditor | (ExtraFlags),            \
			&S_##ClassSuffix);                                                             \
		return D;                                                                          \
	}                                                                                       \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                   \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override   \
	{                                                                                       \
		return HandlerFn(S, RequestId, Payload, Socket);                                    \
	}                                                                                       \
};

// Weapon base (15.1)
MCP_CB_CALL(CreateWeaponBlueprint, "create_weapon_blueprint", McpHandlers::Combat::HandleCombatCreateWeaponBlueprint, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureWeaponMesh, "configure_weapon_mesh", McpHandlers::Combat::HandleCombatConfigureWeaponMesh, EMcpCallFlags::Mutating)
MCP_CB_CALL(SetWeaponStats, "set_weapon_stats", McpHandlers::Combat::HandleCombatSetWeaponStats, EMcpCallFlags::Mutating)

// Firing modes (15.2)

// Projectiles (15.3)
MCP_CB_CALL(CreateProjectileBlueprint, "create_projectile_blueprint", McpHandlers::Combat::HandleCombatCreateProjectileBlueprint, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureProjectileMovement, "configure_projectile_movement", McpHandlers::Combat::HandleCombatConfigureProjectileMovement, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureProjectileCollision, "configure_projectile_collision", McpHandlers::Combat::HandleCombatConfigureProjectileCollision, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureProjectileHoming, "configure_projectile_homing", McpHandlers::Combat::HandleCombatConfigureProjectileHoming, EMcpCallFlags::Mutating)

// Damage system (15.4)
MCP_CB_CALL(CreateDamageType, "create_damage_type", McpHandlers::Combat::HandleCombatCreateDamageType, EMcpCallFlags::Mutating)
MCP_CB_CALL(SetupHitboxComponent, "setup_hitbox_component", McpHandlers::Combat::HandleCombatSetupHitboxComponent, EMcpCallFlags::Mutating)

// Weapon features (15.5)
MCP_CB_CALL(SetupAttachmentSystem, "setup_attachment_system", McpHandlers::Combat::HandleCombatSetupAttachmentSystem, EMcpCallFlags::Mutating)

// Effects (15.6)

// Melee combat (15.7)
MCP_CB_CALL(CreateMeleeTrace, "create_melee_trace", McpHandlers::Combat::HandleCombatCreateMeleeTrace, EMcpCallFlags::Mutating)
MCP_CB_CALL(CreateHitPause, "create_hit_pause", McpHandlers::Combat::HandleCombatCreateHitPause, EMcpCallFlags::Mutating)

// Utility
MCP_CB_CALL(GetInfo, "get_info", McpHandlers::Combat::HandleCombatGetInfo, EMcpCallFlags::None)

// Aliases
MCP_CB_CALL(ConfigureHitDetection, "configure_hit_detection", McpHandlers::Combat::HandleCombatConfigureHitDetection, EMcpCallFlags::Mutating)
MCP_CB_CALL(GetStats, "get_stats", McpHandlers::Combat::HandleCombatGetStats, EMcpCallFlags::None)

// New sub-actions
MCP_CB_CALL(CreateDamageEffect, "create_damage_effect", McpHandlers::Combat::HandleCombatCreateDamageEffect, EMcpCallFlags::Mutating)
MCP_CB_CALL(ApplyDamage, "apply_damage", McpHandlers::Combat::HandleCombatApplyDamage, EMcpCallFlags::Mutating)
MCP_CB_CALL(Heal, "heal", McpHandlers::Combat::HandleCombatHeal, EMcpCallFlags::Mutating)
MCP_CB_CALL(CreateShield, "create_shield", McpHandlers::Combat::HandleCombatCreateShield, EMcpCallFlags::Mutating)
MCP_CB_CALL(ModifyArmor, "modify_armor", McpHandlers::Combat::HandleCombatModifyArmor, EMcpCallFlags::Mutating)

#undef MCP_CB_CALL

} // namespace McpCalls::ManageCombat

void McpRegisterManageCombatCalls()
{
	using namespace McpCalls::ManageCombat;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_CreateWeaponBlueprint>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureWeaponMesh>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_SetWeaponStats>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_CreateProjectileBlueprint>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureProjectileMovement>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureProjectileCollision>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureProjectileHoming>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_CreateDamageType>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_SetupHitboxComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_SetupAttachmentSystem>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_CreateMeleeTrace>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_CreateHitPause>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_GetInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureHitDetection>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_GetStats>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_CreateDamageEffect>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ApplyDamage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_Heal>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_CreateShield>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ModifyArmor>());
}
