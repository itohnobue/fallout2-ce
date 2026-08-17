#ifndef PERK_TWEAK_H
#define PERK_TWEAK_H

#include "config.h"

namespace fallout {

// sfall [PerksTweak] section (config/Perks.ini) — perk bonus overrides.
//
// Mirrors sfall's PerksFile feature: the file configured in ddraw.ini
// [Misc] PerksFile (et tu ships PerksFile=config\Perks.ini) carries a
// [PerksTweak] section that replaces the hardcoded FO2 perk bonuses.
// sfall applies each value by patching the engine constant; CE applies
// them by replacing the same constants at the consumer sites with these
// fields. Defaults are the FO2 engine values, so an absent file or key
// yields byte-identical FO2 behavior.
//
// Per-key semantics mirror sfall's Perks.cpp exactly:
//   - key absent from the file  -> FO2 default stays (configGetInt
//     "present" semantics, no default overload)
//   - value below the key's minimum gate -> NOT applied (FO2 default
//     stays) — sfall gates with `value >= minValue` before patching
//   - value above the key's maximum -> clamped to the maximum
// The exception is the VaultCityInoculations pair: sfall clamps to
// [-100, 100] with no lower gate (any value applies, clamped).
struct PerkTweak {
    // Night Vision (ID 9) light bonus, percent of max light per rank.
    // Range 0..100, FO2 default 20 (65536 * 20 / 100 = 13107 = 20% of
    // LIGHT_INTENSITY_MAX, identical to CE's old LIGHT_LEVEL_NIGHT_VISION_BONUS).
    int nightVisionBonus = 20;

    // Survivalist (ID 16) Outdoorsman bonus. 0..125, default 25.
    int survivalistBonus = 25;

    // Master Trader (ID 17) barter price bonus. >= 0, default 25.
    int masterTraderBonus = 25;

    // Mr. Fixit (ID 31) Science/Repair bonus. 0..125, default 10.
    int mrFixitBonus = 10;

    // Medic (ID 32) First Aid/Doctor bonuses. 0..125, default 10 each.
    int medicFirstAidBonus = 10;
    int medicDoctorBonus = 10;

    // Master Thief (ID 33) Lockpick/Steal bonus. 0..125, default 15.
    int masterThiefBonus = 15;

    // Speaker (ID 34) Speech bonus. 0..125, default 20.
    int speakerBonus = 20;

    // Ghost (ID 38) Sneak-in-darkness bonus. 0..125, default 20.
    int ghostBonus = 20;

    // Ranger (ID 47) Outdoorsman bonus. 0..125, default 15.
    int rangerOutdoorsmanBonus = 15;

    // Weapon Long Range (ID 58) perception bonus multiplier. 2..100, default 4.
    int weaponLongRangeBonus = 4;

    // Weapon Accurate (ID 59) to-hit bonus. 0..125, default 20.
    int weaponAccurateBonus = 20;

    // Weapon Scope Range (ID 64) penalty distance / perception bonus multiplier.
    // Penalty 0..100 (default 8), bonus 2..100 (default 5).
    int weaponScopeRangePenalty = 8;
    int weaponScopeRangeBonus = 5;

    // Vault City Inoculations (ID 78) poison/rad resistance bonuses.
    // Clamped to [-100, 100] (no lower gate — any value applies), default 10.
    int vaultCityInoculationsPoisonBonus = 10;
    int vaultCityInoculationsRadBonus = 10;

    // Cautious Nature (ID 80) encounter spawn distance bonus. -12..20, default 3.
    // (-12 forces distance to 0 in the surrounding-encounter roll.)
    int cautiousNatureBonus = 3;

    // Demolition Expert (ID 82) explosive damage bonus. 0..999, default 10.
    int demolitionExpertBonus = 10;

    // Gambler (ID 83) Gambling bonus. 0..125, default 20.
    int gamblerBonus = 20;

    // Harmless (ID 91) Steal bonus. 0..125, default 20.
    int harmlessBonus = 20;

    // Living Anatomy (ID 97) damage bonus / Doctor bonus. 0..125, default 5 / 10.
    int livingAnatomyBonus = 5;
    int livingAnatomyDoctorBonus = 10;

    // Negotiator (ID 99) Speech/Barter bonus. 0..125, default 10.
    int negotiatorBonus = 10;

    // Pyromaniac (ID 101) fire damage bonus. 0..125, default 5.
    int pyromaniacBonus = 5;

    // Salesman (ID 103) Barter bonus per level. 0..999, default 20.
    int salesmanBonus = 20;

    // Stonewall (ID 104) knockback resist percent. 0..100, default 50.
    int stonewallPercent = 50;

    // Thief (ID 105) Lockpick/Steal/Traps bonus. 0..125, default 10.
    int thiefBonus = 10;

    // Weapon Handling (ID 106) strength reduction for weapon requirements.
    // 0..10, default 3.
    int weaponHandlingBonus = 3;

    // Vault City Training (ID 107) First Aid/Doctor bonuses. 0..125, default 5 each.
    int vaultCityTrainingFirstAidBonus = 5;
    int vaultCityTrainingDoctorBonus = 5;

    // Expert Excrement Expeditor (ID 116) Speech bonus. 0..125, default 5.
    int expertExcrementExpeditorBonus = 5;

    // FO2-only perks — present in sfall's template Perks.ini but omitted
    // from et tu's file. Wired for completeness so any mod's Perks.ini
    // works unchanged.
    // Educated (ID 18) skill points per level. 0..125, default 2.
    int educatedBonus = 2;
    // Healer (ID 19) heal range per rank. 0..999, default 4 / 10.
    int healerMinBonus = 4;
    int healerMaxBonus = 10;
    // Lifegiver (ID 28) max HP per level. 0..125, default 4.
    int lifegiverBonus = 4;
    // Comprehension (ID 81) book skill-point bonus percent. >= 0, default 50.
    int comprehensionBonus = 50;
};

// PerksTweak values loaded from the [PerksTweak] section of the file
// configured in ddraw.ini [Misc] PerksFile. Consumers read these instead
// of hardcoded FO2 bonuses.
extern PerkTweak gPerkTweak;

// Loads gPerkTweak from ddraw.ini [Misc] PerksFile (see struct docs for
// per-key semantics). Missing/empty PerksFile -> no-op (FO2 defaults).
// Safe to call multiple times; re-reads the file.
void perkTweakLoad();

// Returns the resolved PerksFile path from ddraw.ini [Misc] PerksFile,
// or nullptr when the key is absent/empty. The returned pointer is owned
// by gSfallConfig (process lifetime) — callers must NOT free it.
char* perkTweakGetPerksFilePath();

// ============================================================
// TEST-ONLY: config injection for the PerksTweak parser.
// Runs the same gate+clamp parsing perkTweakLoad() applies to the
// file-backed Config, against the given Config (populated via
// configSetInt). Lets tests exercise absent-key / below-gate / clamp
// semantics without real file I/O (compat_fopen is stubbed in the
// test harness). Guarded behind TEST_ACCESSORS_ENABLED.
// ============================================================
#if defined(TEST_ACCESSORS_ENABLED)
void perkTweakLoadFromConfigForTest(Config* config);
#endif

} // namespace fallout

#endif /* PERK_TWEAK_H */
