// LINT-TOOL: manage_combat
// manage_combat as FMcpCall classes — tenth classed family
// (docs/action-declarations.md). Each class co-locates the action's
// declaration with its implementation. Run() delegates to the subsystem
// member handlers (HandleCombat*, CombatHandlers.cpp) until the
// module split de-members those bodies.
#include "MCP/Calls/McpCalls.h"
#include "MCP/McpCallRegistry.h"
#include "McpAutomationBridgeSubsystem.h"

// Per-family namespace: unity builds compile several McpCalls_*.cpp in one TU,
// so file-scope param arrays would collide across families otherwise.
namespace McpCalls::ManageCombat
{

// ─── Param contracts ─────────────────────────────────────────────────────────
// Ported from this family's retired shim declarations (McpDecl_ManageCombat.h)
// and re-verified against the extracted HandleCombat* bodies — every
// declared param is read by its member. The retired dispatcher's prologue
// read name/path/blueprintPath for every action, but the shim rows only ever
// declared the spellings each body reads (name/path on the five creators,
// name alongside blueprintPath on setup_hitbox_component, blueprintPath on
// the other 33), so the rows port unchanged.

inline const FMcpParamDecl P_CreateWeaponBlueprint[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("weaponMeshPath"), EMcpParamKind::String, false }, { TEXT("baseDamage"), EMcpParamKind::Number, false }, { TEXT("fireRate"), EMcpParamKind::Number, false }, { TEXT("range"), EMcpParamKind::Number, false }, { TEXT("spread"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureWeaponMesh[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("weaponMeshPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureWeaponSockets[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("muzzleSocketName"), EMcpParamKind::String, false }, { TEXT("ejectionSocketName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_SetWeaponStats[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("baseDamage"), EMcpParamKind::Number, false }, { TEXT("fireRate"), EMcpParamKind::Number, false }, { TEXT("range"), EMcpParamKind::Number, false }, { TEXT("spread"), EMcpParamKind::Number, false }, { TEXT("criticalMultiplier"), EMcpParamKind::Any, false }, { TEXT("headshotMultiplier"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ConfigureHitscan[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("hitscanEnabled"), EMcpParamKind::Bool, false }, { TEXT("traceChannel"), EMcpParamKind::String, false }, { TEXT("range"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureProjectile[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("projectileClass"), EMcpParamKind::String, false }, { TEXT("projectileSpeed"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureSpreadPattern[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("spreadPattern"), EMcpParamKind::String, false }, { TEXT("spreadIncrease"), EMcpParamKind::Number, false }, { TEXT("spreadRecovery"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureRecoilPattern[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("recoilPitch"), EMcpParamKind::Number, false }, { TEXT("recoilYaw"), EMcpParamKind::Number, false }, { TEXT("recoilRecovery"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureAimDownSights[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("adsEnabled"), EMcpParamKind::Bool, false }, { TEXT("adsFov"), EMcpParamKind::Number, false }, { TEXT("adsSpeed"), EMcpParamKind::Number, false }, { TEXT("adsSpreadMultiplier"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateProjectileBlueprint[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("collisionRadius"), EMcpParamKind::Number, false }, { TEXT("projectileMeshPath"), EMcpParamKind::String, false }, { TEXT("projectileSpeed"), EMcpParamKind::Number, false }, { TEXT("projectileGravityScale"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureProjectileMovement[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("projectileSpeed"), EMcpParamKind::Number, false }, { TEXT("projectileGravityScale"), EMcpParamKind::Number, false }, { TEXT("projectileLifespan"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureProjectileCollision[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("collisionRadius"), EMcpParamKind::Number, false }, { TEXT("bounceEnabled"), EMcpParamKind::Bool, false }, { TEXT("bounceVelocityRatio"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureProjectileHoming[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("homingEnabled"), EMcpParamKind::Bool, false }, { TEXT("homingAcceleration"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateDamageType[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureDamageExecution[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("damageImpulse"), EMcpParamKind::Number, false }, { TEXT("criticalMultiplier"), EMcpParamKind::Number, false }, { TEXT("headshotMultiplier"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetupHitboxComponent[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("hitboxType"), EMcpParamKind::String, false }, { TEXT("hitboxBoneName"), EMcpParamKind::String, false }, { TEXT("isDamageZoneHead"), EMcpParamKind::Bool, false }, { TEXT("damageMultiplier"), EMcpParamKind::Number, false }, { TEXT("hitboxSize"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_SetupReloadSystem[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("magazineSize"), EMcpParamKind::Number, false }, { TEXT("reloadTime"), EMcpParamKind::Number, false }, { TEXT("reloadAnimationPath"), EMcpParamKind::String, false }, { TEXT("maxAmmo"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_SetupAmmoSystem[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("ammoType"), EMcpParamKind::String, false }, { TEXT("maxAmmo"), EMcpParamKind::Number, false }, { TEXT("startingAmmo"), EMcpParamKind::Number, false }, { TEXT("ammoPerShot"), EMcpParamKind::Number, false }, { TEXT("infiniteAmmo"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_SetupAttachmentSystem[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("attachmentSlots"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_SetupWeaponSwitching[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("switchInTime"), EMcpParamKind::Number, false }, { TEXT("switchOutTime"), EMcpParamKind::Number, false }, { TEXT("equipAnimationPath"), EMcpParamKind::String, false }, { TEXT("unequipAnimationPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureMuzzleFlash[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("muzzleFlashParticlePath"), EMcpParamKind::String, false }, { TEXT("muzzleFlashScale"), EMcpParamKind::Number, false }, { TEXT("muzzleSoundPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureTracer[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("tracerParticlePath"), EMcpParamKind::String, false }, { TEXT("tracerSpeed"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureImpactEffects[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("impactParticlePath"), EMcpParamKind::String, false }, { TEXT("impactSoundPath"), EMcpParamKind::String, false }, { TEXT("impactDecalPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureShellEjection[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("shellMeshPath"), EMcpParamKind::String, false }, { TEXT("shellEjectionForce"), EMcpParamKind::Number, false }, { TEXT("shellLifespan"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateMeleeTrace[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("meleeTraceStartSocket"), EMcpParamKind::String, false }, { TEXT("meleeTraceEndSocket"), EMcpParamKind::String, false }, { TEXT("meleeTraceRadius"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureComboSystem[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("comboWindowTime"), EMcpParamKind::Number, false }, { TEXT("maxComboCount"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateHitPause[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("hitPauseDuration"), EMcpParamKind::Number, false }, { TEXT("hitPauseTimeDilation"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureHitReaction[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("hitReactionMontage"), EMcpParamKind::String, false }, { TEXT("hitReactionStunTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_SetupParryBlockSystem[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("parryWindowStart"), EMcpParamKind::Number, false }, { TEXT("parryWindowEnd"), EMcpParamKind::Number, false }, { TEXT("parryAnimationPath"), EMcpParamKind::String, false }, { TEXT("blockDamageReduction"), EMcpParamKind::Number, false }, { TEXT("blockStaminaCost"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ConfigureWeaponTrails[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("weaponTrailParticlePath"), EMcpParamKind::String, false }, { TEXT("weaponTrailStartSocket"), EMcpParamKind::String, false }, { TEXT("weaponTrailEndSocket"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_GetCombatInfo[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_SetupDamageType[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ConfigureHitDetection[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("hitboxType"), EMcpParamKind::String, false }, { TEXT("damageMultiplier"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_GetCombatStats[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_CreateDamageEffect[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("duration"), EMcpParamKind::Number, false }, { TEXT("damagePerSecond"), EMcpParamKind::Number, false }, { TEXT("effectType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ApplyDamage[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("damageAmount"), EMcpParamKind::Number, false }, { TEXT("damageType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_Heal[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("healAmount"), EMcpParamKind::Number, false }, { TEXT("maxHealth"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_CreateShield[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("shieldAmount"), EMcpParamKind::Number, false }, { TEXT("maxShield"), EMcpParamKind::Number, false }, { TEXT("shieldRegenRate"), EMcpParamKind::Number, false }, { TEXT("shieldRegenDelay"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ModifyArmor[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("armorValue"), EMcpParamKind::Number, false }, { TEXT("damageReduction"), EMcpParamKind::Number, false } };

// ─── Classes ─────────────────────────────────────────────────────────────────
// RequiresEditor is baked into every row (the implementation TU is editor-
// gated; the members answer the EDITOR_ONLY stub in non-editor builds).
// Mutating on all writers; the only readers are get_combat_info and
// get_combat_stats.

#define MCP_CB_CALL(ClassSuffix, ActionLiteral, ParamsArray, HandlerFn, ExtraFlags)        \
class FMcpCall_ManageCombat_##ClassSuffix final : public FMcpCall                          \
{                                                                                          \
	const FMcpCallDecl& GetDecl() const override                                           \
	{                                                                                      \
		static const FMcpCallDecl Decl{ TEXT("manage_combat"), TEXT(ActionLiteral),        \
			ParamsArray, EMcpCallFlags::RequiresEditor | (ExtraFlags) };                   \
		return Decl;                                                                       \
	}                                                                                      \
	bool Run(UMcpAutomationBridgeSubsystem& S, const FString& RequestId,                   \
	         const TSharedPtr<FJsonObject>& Payload, FMcpResponseHandle Socket) override   \
	{                                                                                      \
		return S.HandlerFn(RequestId, Payload, Socket);                                    \
	}                                                                                      \
};

// Weapon base (15.1)
MCP_CB_CALL(CreateWeaponBlueprint, "create_weapon_blueprint", P_CreateWeaponBlueprint, HandleCombatCreateWeaponBlueprint, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureWeaponMesh, "configure_weapon_mesh", P_ConfigureWeaponMesh, HandleCombatConfigureWeaponMesh, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureWeaponSockets, "configure_weapon_sockets", P_ConfigureWeaponSockets, HandleCombatConfigureWeaponSockets, EMcpCallFlags::Mutating)
MCP_CB_CALL(SetWeaponStats, "set_weapon_stats", P_SetWeaponStats, HandleCombatSetWeaponStats, EMcpCallFlags::Mutating)

// Firing modes (15.2)
MCP_CB_CALL(ConfigureHitscan, "configure_hitscan", P_ConfigureHitscan, HandleCombatConfigureHitscan, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureProjectile, "configure_projectile", P_ConfigureProjectile, HandleCombatConfigureProjectile, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureSpreadPattern, "configure_spread_pattern", P_ConfigureSpreadPattern, HandleCombatConfigureSpreadPattern, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureRecoilPattern, "configure_recoil_pattern", P_ConfigureRecoilPattern, HandleCombatConfigureRecoilPattern, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureAimDownSights, "configure_aim_down_sights", P_ConfigureAimDownSights, HandleCombatConfigureAimDownSights, EMcpCallFlags::Mutating)

// Projectiles (15.3)
MCP_CB_CALL(CreateProjectileBlueprint, "create_projectile_blueprint", P_CreateProjectileBlueprint, HandleCombatCreateProjectileBlueprint, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureProjectileMovement, "configure_projectile_movement", P_ConfigureProjectileMovement, HandleCombatConfigureProjectileMovement, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureProjectileCollision, "configure_projectile_collision", P_ConfigureProjectileCollision, HandleCombatConfigureProjectileCollision, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureProjectileHoming, "configure_projectile_homing", P_ConfigureProjectileHoming, HandleCombatConfigureProjectileHoming, EMcpCallFlags::Mutating)

// Damage system (15.4)
MCP_CB_CALL(CreateDamageType, "create_damage_type", P_CreateDamageType, HandleCombatCreateDamageType, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureDamageExecution, "configure_damage_execution", P_ConfigureDamageExecution, HandleCombatConfigureDamageExecution, EMcpCallFlags::Mutating)
MCP_CB_CALL(SetupHitboxComponent, "setup_hitbox_component", P_SetupHitboxComponent, HandleCombatSetupHitboxComponent, EMcpCallFlags::Mutating)

// Weapon features (15.5)
MCP_CB_CALL(SetupReloadSystem, "setup_reload_system", P_SetupReloadSystem, HandleCombatSetupReloadSystem, EMcpCallFlags::Mutating)
MCP_CB_CALL(SetupAmmoSystem, "setup_ammo_system", P_SetupAmmoSystem, HandleCombatSetupAmmoSystem, EMcpCallFlags::Mutating)
MCP_CB_CALL(SetupAttachmentSystem, "setup_attachment_system", P_SetupAttachmentSystem, HandleCombatSetupAttachmentSystem, EMcpCallFlags::Mutating)
MCP_CB_CALL(SetupWeaponSwitching, "setup_weapon_switching", P_SetupWeaponSwitching, HandleCombatSetupWeaponSwitching, EMcpCallFlags::Mutating)

// Effects (15.6)
MCP_CB_CALL(ConfigureMuzzleFlash, "configure_muzzle_flash", P_ConfigureMuzzleFlash, HandleCombatConfigureMuzzleFlash, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureTracer, "configure_tracer", P_ConfigureTracer, HandleCombatConfigureTracer, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureImpactEffects, "configure_impact_effects", P_ConfigureImpactEffects, HandleCombatConfigureImpactEffects, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureShellEjection, "configure_shell_ejection", P_ConfigureShellEjection, HandleCombatConfigureShellEjection, EMcpCallFlags::Mutating)

// Melee combat (15.7)
MCP_CB_CALL(CreateMeleeTrace, "create_melee_trace", P_CreateMeleeTrace, HandleCombatCreateMeleeTrace, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureComboSystem, "configure_combo_system", P_ConfigureComboSystem, HandleCombatConfigureComboSystem, EMcpCallFlags::Mutating)
MCP_CB_CALL(CreateHitPause, "create_hit_pause", P_CreateHitPause, HandleCombatCreateHitPause, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureHitReaction, "configure_hit_reaction", P_ConfigureHitReaction, HandleCombatConfigureHitReaction, EMcpCallFlags::Mutating)
MCP_CB_CALL(SetupParryBlockSystem, "setup_parry_block_system", P_SetupParryBlockSystem, HandleCombatSetupParryBlockSystem, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureWeaponTrails, "configure_weapon_trails", P_ConfigureWeaponTrails, HandleCombatConfigureWeaponTrails, EMcpCallFlags::Mutating)

// Utility
MCP_CB_CALL(GetInfo, "get_combat_info", P_GetCombatInfo, HandleCombatGetInfo, EMcpCallFlags::None)

// Aliases
MCP_CB_CALL(SetupDamageType, "setup_damage_type", P_SetupDamageType, HandleCombatSetupDamageType, EMcpCallFlags::Mutating)
MCP_CB_CALL(ConfigureHitDetection, "configure_hit_detection", P_ConfigureHitDetection, HandleCombatConfigureHitDetection, EMcpCallFlags::Mutating)
MCP_CB_CALL(GetStats, "get_combat_stats", P_GetCombatStats, HandleCombatGetStats, EMcpCallFlags::None)

// New sub-actions
MCP_CB_CALL(CreateDamageEffect, "create_damage_effect", P_CreateDamageEffect, HandleCombatCreateDamageEffect, EMcpCallFlags::Mutating)
MCP_CB_CALL(ApplyDamage, "apply_damage", P_ApplyDamage, HandleCombatApplyDamage, EMcpCallFlags::Mutating)
MCP_CB_CALL(Heal, "heal", P_Heal, HandleCombatHeal, EMcpCallFlags::Mutating)
MCP_CB_CALL(CreateShield, "create_shield", P_CreateShield, HandleCombatCreateShield, EMcpCallFlags::Mutating)
MCP_CB_CALL(ModifyArmor, "modify_armor", P_ModifyArmor, HandleCombatModifyArmor, EMcpCallFlags::Mutating)

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
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_SetupDamageType>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ConfigureHitDetection>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_GetStats>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_CreateDamageEffect>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ApplyDamage>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_Heal>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_CreateShield>());
	Registry.RegisterCall(MakeUnique<FMcpCall_ManageCombat_ModifyArmor>());
}
