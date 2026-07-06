// LINT-TOOL: manage_combat
// LINT-SCHEMA-DERIVED
// manage_combat as FMcpCall classes — tenth classed family
// (docs/action-declarations.md). Adopts schema-from-decls: each class AUTHORS
// its schema fragment in a S_<Suffix>() function; the published facade schema
// folds those fragments and GetDecl() derives the validation decl from the same
// fragment via McpDeriveDecl(), so schema and decl are one source and cannot
// drift. Run() delegates to the subsystem member handlers (HandleCombat*,
// CombatHandlers.cpp) until the module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "McpAutomationBridgeSubsystem.h"

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

static void S_ConfigureWeaponSockets(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("muzzleSocketName"), TEXT("Muzzle socket name."))
	 .String(TEXT("ejectionSocketName"), TEXT("Shell ejection socket name."))
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

static void S_ConfigureHitscan(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("hitscanEnabled"), TEXT("Enable hitscan firing."))
	 .StringEnum(TEXT("traceChannel"), {
		TEXT("Visibility"), TEXT("Camera"), TEXT("Weapon"), TEXT("Custom")
	 }, TEXT("Trace channel for hitscan."))
	 .Number(TEXT("range"), TEXT(""))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureProjectile(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("projectileClass"), TEXT("Projectile class path."))
	 .Number(TEXT("projectileSpeed"), TEXT(""))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureSpreadPattern(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .StringEnum(TEXT("spreadPattern"), {
		TEXT("Random"), TEXT("Fixed"), TEXT("FixedWithRandom"), TEXT("Shotgun")
	 }, TEXT("Spread pattern type."))
	 .Number(TEXT("spreadIncrease"), TEXT("Spread increase per shot."))
	 .Number(TEXT("spreadRecovery"), TEXT("Spread recovery rate."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureRecoilPattern(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("recoilPitch"), TEXT("Vertical recoil (degrees)."))
	 .Number(TEXT("recoilYaw"), TEXT("Horizontal recoil (degrees)."))
	 .Number(TEXT("recoilRecovery"), TEXT("Recoil recovery speed."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureAimDownSights(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Bool(TEXT("adsEnabled"), TEXT("Enable aim down sights."))
	 .Number(TEXT("adsFov"), TEXT("FOV when aiming."))
	 .Number(TEXT("adsSpeed"), TEXT("Time to aim down sights."))
	 .Number(TEXT("adsSpreadMultiplier"), TEXT("Spread multiplier when aiming."))
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

static void S_ConfigureDamageExecution(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("damageImpulse"), TEXT("Impulse applied on hit."))
	 .Number(TEXT("criticalMultiplier"), TEXT("Critical hit damage multiplier (consumed by configure_damage_execution, not set_weapon_stats)."))
	 .Number(TEXT("headshotMultiplier"), TEXT("Headshot damage multiplier (consumed by configure_damage_execution, not set_weapon_stats)."))
	 .Required({TEXT("blueprintPath")});
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

static void S_SetupReloadSystem(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("magazineSize"), TEXT(""))
	 .Number(TEXT("reloadTime"), TEXT(""))
	 .String(TEXT("reloadAnimationPath"), TEXT("Path to reload animation."))
	 .Number(TEXT("maxAmmo"), TEXT("Total reserve ammo (consumed by setup_ammo_system, not setup_reload_system)."))
	 .Required({TEXT("blueprintPath")});
}

static void S_SetupAmmoSystem(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("ammoType"), TEXT("Ammo type identifier."))
	 .Number(TEXT("maxAmmo"), TEXT("Total reserve ammo (consumed by setup_ammo_system, not setup_reload_system)."))
	 .Number(TEXT("startingAmmo"), TEXT(""))
	 .Number(TEXT("ammoPerShot"), TEXT("Ammo consumed per shot."))
	 .Bool(TEXT("infiniteAmmo"), TEXT("Whether ammo is infinite."))
	 .Required({TEXT("blueprintPath")});
}

static void S_SetupAttachmentSystem(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .ArrayOfObjects(TEXT("attachmentSlots"), TEXT("Attachment slot definitions."))
	 .Required({TEXT("blueprintPath")});
}

static void S_SetupWeaponSwitching(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("switchInTime"), TEXT("Time to equip weapon."))
	 .Number(TEXT("switchOutTime"), TEXT("Time to unequip weapon."))
	 .String(TEXT("equipAnimationPath"), TEXT("Path to equip animation montage."))
	 .String(TEXT("unequipAnimationPath"), TEXT("Path to unequip animation montage."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureMuzzleFlash(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("muzzleFlashParticlePath"), TEXT("Path to muzzle flash particle."))
	 .Number(TEXT("muzzleFlashScale"), TEXT("Muzzle flash scale."))
	 .String(TEXT("muzzleSoundPath"), TEXT("Path to firing sound."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureTracer(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("tracerParticlePath"), TEXT("Path to tracer particle."))
	 .Number(TEXT("tracerSpeed"), TEXT("Tracer travel speed."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureImpactEffects(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("impactParticlePath"), TEXT("Path to impact particle."))
	 .String(TEXT("impactSoundPath"), TEXT("Path to impact sound."))
	 .String(TEXT("impactDecalPath"), TEXT("Path to impact decal."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureShellEjection(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("shellMeshPath"), TEXT("Path to shell casing mesh."))
	 .Number(TEXT("shellEjectionForce"), TEXT("Shell ejection impulse."))
	 .Number(TEXT("shellLifespan"), TEXT("Shell casing lifetime."))
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

static void S_ConfigureComboSystem(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("comboWindowTime"), TEXT("Time window for combo input."))
	 .Number(TEXT("maxComboCount"), TEXT("Maximum combo length."))
	 .Required({TEXT("blueprintPath")});
}

static void S_CreateHitPause(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("hitPauseDuration"), TEXT("Hitstop duration in seconds."))
	 .Number(TEXT("hitPauseTimeDilation"), TEXT("Time dilation during hitstop."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureHitReaction(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("hitReactionMontage"), TEXT("Path to hit reaction montage."))
	 .Number(TEXT("hitReactionStunTime"), TEXT("Stun duration on hit."))
	 .Required({TEXT("blueprintPath")});
}

static void S_SetupParryBlockSystem(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .Number(TEXT("parryWindowStart"), TEXT("Parry window start time (normalized)."))
	 .Number(TEXT("parryWindowEnd"), TEXT("Parry window end time (normalized)."))
	 .String(TEXT("parryAnimationPath"), TEXT("Path to parry animation."))
	 .Number(TEXT("blockDamageReduction"), TEXT("Damage reduction when blocking (0-1)."))
	 .Number(TEXT("blockStaminaCost"), TEXT("Stamina cost per blocked hit."))
	 .Required({TEXT("blueprintPath")});
}

static void S_ConfigureWeaponTrails(FMcpSchemaBuilder& B)
{
	B.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
	 .String(TEXT("weaponTrailParticlePath"), TEXT("Path to weapon trail particle."))
	 .String(TEXT("weaponTrailStartSocket"), TEXT("Trail start socket."))
	 .String(TEXT("weaponTrailEndSocket"), TEXT("Trail end socket."))
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
		return S.HandlerFn(RequestId, Payload, Socket);                                    \
	}                                                                                       \
};

// Weapon base (15.1)
MCP_CB_CALL(CreateWeaponBlueprint, "create_weapon_blueprint", HandleCombatCreateWeaponBlueprint, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureWeaponMesh, "configure_weapon_mesh", HandleCombatConfigureWeaponMesh, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureWeaponSockets, "configure_weapon_sockets", HandleCombatConfigureWeaponSockets, EMcpCallFlags::Mutating)
MCP_CB_CALL(SetWeaponStats, "set_weapon_stats", HandleCombatSetWeaponStats, EMcpCallFlags::Mutating)

// Firing modes (15.2)
MCP_CB_CALL(ConfigureHitscan, "configure_hitscan", HandleCombatConfigureHitscan, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureProjectile, "configure_projectile", HandleCombatConfigureProjectile, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureSpreadPattern, "configure_spread_pattern", HandleCombatConfigureSpreadPattern, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureRecoilPattern, "configure_recoil_pattern", HandleCombatConfigureRecoilPattern, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureAimDownSights, "configure_aim_down_sights", HandleCombatConfigureAimDownSights, EMcpCallFlags::Mutating)

// Projectiles (15.3)
MCP_CB_CALL(CreateProjectileBlueprint, "create_projectile_blueprint", HandleCombatCreateProjectileBlueprint, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureProjectileMovement, "configure_projectile_movement", HandleCombatConfigureProjectileMovement, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureProjectileCollision, "configure_projectile_collision", HandleCombatConfigureProjectileCollision, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureProjectileHoming, "configure_projectile_homing", HandleCombatConfigureProjectileHoming, EMcpCallFlags::Mutating)

// Damage system (15.4)
MCP_CB_CALL(CreateDamageType, "create_damage_type", HandleCombatCreateDamageType, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureDamageExecution, "configure_damage_execution", HandleCombatConfigureDamageExecution, EMcpCallFlags::Mutating)
MCP_CB_CALL(SetupHitboxComponent, "setup_hitbox_component", HandleCombatSetupHitboxComponent, EMcpCallFlags::Mutating)

// Weapon features (15.5)
MCP_CB_CALL(SetupReloadSystem, "setup_reload_system", HandleCombatSetupReloadSystem, EMcpCallFlags::Mutating)
MCP_CB_CALL(SetupAmmoSystem, "setup_ammo_system", HandleCombatSetupAmmoSystem, EMcpCallFlags::Mutating)
MCP_CB_CALL(SetupAttachmentSystem, "setup_attachment_system", HandleCombatSetupAttachmentSystem, EMcpCallFlags::Mutating)
MCP_CB_CALL(SetupWeaponSwitching, "setup_weapon_switching", HandleCombatSetupWeaponSwitching, EMcpCallFlags::Mutating)

// Effects (15.6)
MCP_CB_CALL(ConfigureMuzzleFlash, "configure_muzzle_flash", HandleCombatConfigureMuzzleFlash, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureTracer, "configure_tracer", HandleCombatConfigureTracer, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureImpactEffects, "configure_impact_effects", HandleCombatConfigureImpactEffects, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureShellEjection, "configure_shell_ejection", HandleCombatConfigureShellEjection, EMcpCallFlags::Mutating)

// Melee combat (15.7)
MCP_CB_CALL(CreateMeleeTrace, "create_melee_trace", HandleCombatCreateMeleeTrace, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureComboSystem, "configure_combo_system", HandleCombatConfigureComboSystem, EMcpCallFlags::Mutating)
MCP_CB_CALL(CreateHitPause, "create_hit_pause", HandleCombatCreateHitPause, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureHitReaction, "configure_hit_reaction", HandleCombatConfigureHitReaction, EMcpCallFlags::Mutating)
MCP_CB_CALL(SetupParryBlockSystem, "setup_parry_block_system", HandleCombatSetupParryBlockSystem, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureWeaponTrails, "configure_weapon_trails", HandleCombatConfigureWeaponTrails, EMcpCallFlags::Mutating)

// Utility
MCP_CB_CALL(GetInfo, "get_info", HandleCombatGetInfo, EMcpCallFlags::None)

// Aliases
MCP_CB_CALL(ConfigureHitDetection, "configure_hit_detection", HandleCombatConfigureHitDetection, EMcpCallFlags::Mutating)
MCP_CB_CALL(GetStats, "get_stats", HandleCombatGetStats, EMcpCallFlags::None)

// New sub-actions
MCP_CB_CALL(CreateDamageEffect, "create_damage_effect", HandleCombatCreateDamageEffect, EMcpCallFlags::Mutating)
MCP_CB_CALL(ApplyDamage, "apply_damage", HandleCombatApplyDamage, EMcpCallFlags::Mutating)
MCP_CB_CALL(Heal, "heal", HandleCombatHeal, EMcpCallFlags::Mutating)
MCP_CB_CALL(CreateShield, "create_shield", HandleCombatCreateShield, EMcpCallFlags::Mutating)
MCP_CB_CALL(ModifyArmor, "modify_armor", HandleCombatModifyArmor, EMcpCallFlags::Mutating)

#undef MCP_CB_CALL

} // namespace McpCalls::ManageCombat

void McpRegisterManageCombatCalls()
{
	using namespace McpCalls::ManageCombat;
	FMcpCallRegistry& Registry = FMcpCallRegistry::Get();
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_CreateWeaponBlueprint>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureWeaponMesh>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureWeaponSockets>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_SetWeaponStats>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureHitscan>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureProjectile>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureSpreadPattern>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureRecoilPattern>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureAimDownSights>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_CreateProjectileBlueprint>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureProjectileMovement>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureProjectileCollision>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureProjectileHoming>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_CreateDamageType>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureDamageExecution>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_SetupHitboxComponent>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_SetupReloadSystem>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_SetupAmmoSystem>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_SetupAttachmentSystem>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_SetupWeaponSwitching>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureMuzzleFlash>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureTracer>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureImpactEffects>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureShellEjection>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_CreateMeleeTrace>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureComboSystem>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_CreateHitPause>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureHitReaction>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_SetupParryBlockSystem>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureWeaponTrails>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_GetInfo>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureHitDetection>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_GetStats>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_CreateDamageEffect>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ApplyDamage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_Heal>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_CreateShield>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ModifyArmor>());
}
