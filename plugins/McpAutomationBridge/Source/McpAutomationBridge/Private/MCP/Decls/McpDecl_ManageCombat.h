// Action declarations for manage_combat — the server's contract: which params
// each action reads, and which are required. Fleet-authored from handler
// source (three-witness cross-check, 2026-07-04), hand-maintained since:
// adding an action = adding its declaration here (the boot validation and
// tests/schema/action-decl-lint.ps1 enforce both directions).
// UnverifiedDecl = no reachable read path was attributable; validation skips
// those actions and the lint nags until someone verifies or removes them.
#pragma once

#include "MCP/McpCallRegistry.h"

namespace McpDecls
{
inline const FMcpParamDecl P_ManageCombat_0[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("damageAmount"), EMcpParamKind::Number, false }, { TEXT("damageType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageCombat_1[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("adsEnabled"), EMcpParamKind::Bool, false }, { TEXT("adsFov"), EMcpParamKind::Number, false }, { TEXT("adsSpeed"), EMcpParamKind::Number, false }, { TEXT("adsSpreadMultiplier"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_2[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("comboWindowTime"), EMcpParamKind::Number, false }, { TEXT("maxComboCount"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_3[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("damageImpulse"), EMcpParamKind::Number, false }, { TEXT("criticalMultiplier"), EMcpParamKind::Number, false }, { TEXT("headshotMultiplier"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_4[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("hitboxType"), EMcpParamKind::String, false }, { TEXT("damageMultiplier"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_5[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("hitReactionMontage"), EMcpParamKind::String, false }, { TEXT("hitReactionStunTime"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_6[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("hitscanEnabled"), EMcpParamKind::Bool, false }, { TEXT("traceChannel"), EMcpParamKind::String, false }, { TEXT("range"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_7[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("impactParticlePath"), EMcpParamKind::String, false }, { TEXT("impactSoundPath"), EMcpParamKind::String, false }, { TEXT("impactDecalPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageCombat_8[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("muzzleFlashParticlePath"), EMcpParamKind::String, false }, { TEXT("muzzleFlashScale"), EMcpParamKind::Number, false }, { TEXT("muzzleSoundPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageCombat_9[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("projectileClass"), EMcpParamKind::String, false }, { TEXT("projectileSpeed"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_10[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("collisionRadius"), EMcpParamKind::Number, false }, { TEXT("bounceEnabled"), EMcpParamKind::Bool, false }, { TEXT("bounceVelocityRatio"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_11[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("homingEnabled"), EMcpParamKind::Bool, false }, { TEXT("homingAcceleration"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_12[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("projectileSpeed"), EMcpParamKind::Number, false }, { TEXT("projectileGravityScale"), EMcpParamKind::Number, false }, { TEXT("projectileLifespan"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_13[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("recoilPitch"), EMcpParamKind::Number, false }, { TEXT("recoilYaw"), EMcpParamKind::Number, false }, { TEXT("recoilRecovery"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_14[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("shellMeshPath"), EMcpParamKind::String, false }, { TEXT("shellEjectionForce"), EMcpParamKind::Number, false }, { TEXT("shellLifespan"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_15[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("spreadPattern"), EMcpParamKind::String, false }, { TEXT("spreadIncrease"), EMcpParamKind::Number, false }, { TEXT("spreadRecovery"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_16[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("tracerParticlePath"), EMcpParamKind::String, false }, { TEXT("tracerSpeed"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_17[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("weaponMeshPath"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageCombat_18[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("muzzleSocketName"), EMcpParamKind::String, false }, { TEXT("ejectionSocketName"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageCombat_19[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("weaponTrailParticlePath"), EMcpParamKind::String, false }, { TEXT("weaponTrailStartSocket"), EMcpParamKind::String, false }, { TEXT("weaponTrailEndSocket"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageCombat_20[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("duration"), EMcpParamKind::Number, false }, { TEXT("damagePerSecond"), EMcpParamKind::Number, false }, { TEXT("effectType"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageCombat_21[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageCombat_22[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("hitPauseDuration"), EMcpParamKind::Number, false }, { TEXT("hitPauseTimeDilation"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_23[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("meleeTraceStartSocket"), EMcpParamKind::String, false }, { TEXT("meleeTraceEndSocket"), EMcpParamKind::String, false }, { TEXT("meleeTraceRadius"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_24[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("collisionRadius"), EMcpParamKind::Number, false }, { TEXT("projectileMeshPath"), EMcpParamKind::String, false }, { TEXT("projectileSpeed"), EMcpParamKind::Number, false }, { TEXT("projectileGravityScale"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_25[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("shieldAmount"), EMcpParamKind::Number, false }, { TEXT("maxShield"), EMcpParamKind::Number, false }, { TEXT("shieldRegenRate"), EMcpParamKind::Number, false }, { TEXT("shieldRegenDelay"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_26[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false }, { TEXT("weaponMeshPath"), EMcpParamKind::String, false }, { TEXT("baseDamage"), EMcpParamKind::Number, false }, { TEXT("fireRate"), EMcpParamKind::Number, false }, { TEXT("range"), EMcpParamKind::Number, false }, { TEXT("spread"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_27[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageCombat_28[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true } };
inline const FMcpParamDecl P_ManageCombat_29[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("healAmount"), EMcpParamKind::Number, false }, { TEXT("maxHealth"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_30[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("armorValue"), EMcpParamKind::Number, false }, { TEXT("damageReduction"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_31[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("baseDamage"), EMcpParamKind::Number, false }, { TEXT("fireRate"), EMcpParamKind::Number, false }, { TEXT("range"), EMcpParamKind::Number, false }, { TEXT("spread"), EMcpParamKind::Number, false }, { TEXT("criticalMultiplier"), EMcpParamKind::Any, false }, { TEXT("headshotMultiplier"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ManageCombat_32[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("ammoType"), EMcpParamKind::String, false }, { TEXT("maxAmmo"), EMcpParamKind::Number, false }, { TEXT("startingAmmo"), EMcpParamKind::Number, false }, { TEXT("ammoPerShot"), EMcpParamKind::Number, false }, { TEXT("infiniteAmmo"), EMcpParamKind::Bool, false } };
inline const FMcpParamDecl P_ManageCombat_33[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("attachmentSlots"), EMcpParamKind::Array, false } };
inline const FMcpParamDecl P_ManageCombat_34[] = { { TEXT("name"), EMcpParamKind::String, true }, { TEXT("path"), EMcpParamKind::String, false } };
inline const FMcpParamDecl P_ManageCombat_35[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("name"), EMcpParamKind::String, false }, { TEXT("hitboxType"), EMcpParamKind::String, false }, { TEXT("hitboxBoneName"), EMcpParamKind::String, false }, { TEXT("isDamageZoneHead"), EMcpParamKind::Bool, false }, { TEXT("damageMultiplier"), EMcpParamKind::Number, false }, { TEXT("hitboxSize"), EMcpParamKind::Object, false } };
inline const FMcpParamDecl P_ManageCombat_36[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("parryWindowStart"), EMcpParamKind::Number, false }, { TEXT("parryWindowEnd"), EMcpParamKind::Number, false }, { TEXT("parryAnimationPath"), EMcpParamKind::String, false }, { TEXT("blockDamageReduction"), EMcpParamKind::Number, false }, { TEXT("blockStaminaCost"), EMcpParamKind::Number, false } };
inline const FMcpParamDecl P_ManageCombat_37[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("magazineSize"), EMcpParamKind::Number, false }, { TEXT("reloadTime"), EMcpParamKind::Number, false }, { TEXT("reloadAnimationPath"), EMcpParamKind::String, false }, { TEXT("maxAmmo"), EMcpParamKind::Any, false } };
inline const FMcpParamDecl P_ManageCombat_38[] = { { TEXT("blueprintPath"), EMcpParamKind::String, true }, { TEXT("switchInTime"), EMcpParamKind::Number, false }, { TEXT("switchOutTime"), EMcpParamKind::Number, false }, { TEXT("equipAnimationPath"), EMcpParamKind::String, false }, { TEXT("unequipAnimationPath"), EMcpParamKind::String, false } };

inline const FMcpCallDecl GManageCombat[] =
{
	{ TEXT("manage_combat"), TEXT("apply_damage"), P_ManageCombat_0, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_aim_down_sights"), P_ManageCombat_1, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_combo_system"), P_ManageCombat_2, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_damage_execution"), P_ManageCombat_3, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_hit_detection"), P_ManageCombat_4, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_hit_reaction"), P_ManageCombat_5, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_hitscan"), P_ManageCombat_6, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_impact_effects"), P_ManageCombat_7, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_muzzle_flash"), P_ManageCombat_8, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_projectile"), P_ManageCombat_9, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_projectile_collision"), P_ManageCombat_10, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_projectile_homing"), P_ManageCombat_11, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_projectile_movement"), P_ManageCombat_12, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_recoil_pattern"), P_ManageCombat_13, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_shell_ejection"), P_ManageCombat_14, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_spread_pattern"), P_ManageCombat_15, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_tracer"), P_ManageCombat_16, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_weapon_mesh"), P_ManageCombat_17, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_weapon_sockets"), P_ManageCombat_18, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("configure_weapon_trails"), P_ManageCombat_19, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("create_damage_effect"), P_ManageCombat_20, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("create_damage_type"), P_ManageCombat_21, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("create_hit_pause"), P_ManageCombat_22, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("create_melee_trace"), P_ManageCombat_23, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("create_projectile_blueprint"), P_ManageCombat_24, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("create_shield"), P_ManageCombat_25, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("create_weapon_blueprint"), P_ManageCombat_26, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("get_combat_info"), P_ManageCombat_27, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("get_combat_stats"), P_ManageCombat_28, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("heal"), P_ManageCombat_29, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("modify_armor"), P_ManageCombat_30, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("set_weapon_stats"), P_ManageCombat_31, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("setup_ammo_system"), P_ManageCombat_32, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("setup_attachment_system"), P_ManageCombat_33, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("setup_damage_type"), P_ManageCombat_34, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("setup_hitbox_component"), P_ManageCombat_35, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("setup_parry_block_system"), P_ManageCombat_36, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("setup_reload_system"), P_ManageCombat_37, EMcpCallFlags::None },
	{ TEXT("manage_combat"), TEXT("setup_weapon_switching"), P_ManageCombat_38, EMcpCallFlags::None },
};
}
