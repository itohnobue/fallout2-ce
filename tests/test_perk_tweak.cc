// Unit tests for perk_tweak.cc — sfall [PerksTweak] section parsing.
//
// Tests: FO2 default values, PerksFile path resolution from ddraw.ini,
//       gate+clamp semantics per key (absent / below-gate / above-max),
//       clamped-both VaultCityInoculations semantics, no-file no-op.
//
// This test LINKS perk_tweak.cc. That file depends on:
//   config.cc (Config init/set/get/free) — already in test_sources
//   dictionary.cc                        — already in test_sources
//   memory.cc                            — already in test_sources
//   sfall_config.cc (gSfallConfig)       — already in test_sources
//   debugPrint                           — in test_stubs
//
// File I/O (compat_fopen) is stubbed to nullptr in the test harness,
// so perkTweakLoad() cannot read a real Perks.ini here; the parser core
// is exercised via perkTweakLoadFromConfigForTest (TEST_ACCESSORS_ENABLED)
// with a manually populated Config. perkTweakLoad() itself is tested for
// its no-file/no-op behavior.
//
// The [Perks] section parser lives in perk.cc (needs gPerkDescriptions,
// 30+ engine deps) and is not link-testable here — it uses the same
// configGetInt present-semantics API covered by the tests below.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <string.h>

#include "config.h"
#include "dictionary.h"
#include "perk_tweak.h"
#include "sfall_config.h"

using namespace fallout;

// =============================================================
// Reset helpers — ensure clean state between tests.
// =============================================================

static void resetPerkTweak() {
    gPerkTweak = PerkTweak();
    if (gSfallConfigInitialized) {
        configFree(&gSfallConfig);
        gSfallConfigInitialized = false;
    }
}

// Manually initializes gSfallConfig (no file read — configInit only)
// so perkTweakGetPerksFilePath can be exercised without sfallConfigInit's
// file-backed configRead.
static void initSfallConfigMemory() {
    if (!gSfallConfigInitialized) {
        CHECK(configInit(&gSfallConfig));
        gSfallConfigInitialized = true;
    }
}

// =============================================================
// Default values (FO2 engine values — sfall's PerksTweak defaults)
// =============================================================

TEST_CASE("gPerkTweak defaults match FO2 engine values") {
    resetPerkTweak();

    CHECK(gPerkTweak.nightVisionBonus == 20);
    CHECK(gPerkTweak.survivalistBonus == 25);
    CHECK(gPerkTweak.masterTraderBonus == 25);
    CHECK(gPerkTweak.mrFixitBonus == 10);
    CHECK(gPerkTweak.medicFirstAidBonus == 10);
    CHECK(gPerkTweak.medicDoctorBonus == 10);
    CHECK(gPerkTweak.masterThiefBonus == 15);
    CHECK(gPerkTweak.speakerBonus == 20);
    CHECK(gPerkTweak.ghostBonus == 20);
    CHECK(gPerkTweak.rangerOutdoorsmanBonus == 15);
    CHECK(gPerkTweak.weaponLongRangeBonus == 4);
    CHECK(gPerkTweak.weaponAccurateBonus == 20);
    CHECK(gPerkTweak.weaponScopeRangePenalty == 8);
    CHECK(gPerkTweak.weaponScopeRangeBonus == 5);
    CHECK(gPerkTweak.vaultCityInoculationsPoisonBonus == 10);
    CHECK(gPerkTweak.vaultCityInoculationsRadBonus == 10);
    CHECK(gPerkTweak.cautiousNatureBonus == 3);
    CHECK(gPerkTweak.demolitionExpertBonus == 10);
    CHECK(gPerkTweak.gamblerBonus == 20);
    CHECK(gPerkTweak.harmlessBonus == 20);
    CHECK(gPerkTweak.livingAnatomyBonus == 5);
    CHECK(gPerkTweak.livingAnatomyDoctorBonus == 10);
    CHECK(gPerkTweak.negotiatorBonus == 10);
    CHECK(gPerkTweak.pyromaniacBonus == 5);
    CHECK(gPerkTweak.salesmanBonus == 20);
    CHECK(gPerkTweak.stonewallPercent == 50);
    CHECK(gPerkTweak.thiefBonus == 10);
    CHECK(gPerkTweak.weaponHandlingBonus == 3);
    CHECK(gPerkTweak.vaultCityTrainingFirstAidBonus == 5);
    CHECK(gPerkTweak.vaultCityTrainingDoctorBonus == 5);
    CHECK(gPerkTweak.expertExcrementExpeditorBonus == 5);
    CHECK(gPerkTweak.educatedBonus == 2);
    CHECK(gPerkTweak.healerMinBonus == 4);
    CHECK(gPerkTweak.healerMaxBonus == 10);
    CHECK(gPerkTweak.lifegiverBonus == 4);
    CHECK(gPerkTweak.comprehensionBonus == 50);
}

// =============================================================
// PerksFile path resolution (ddraw.ini [Misc] PerksFile)
// =============================================================

TEST_CASE("perkTweakGetPerksFilePath resolution") {
    resetPerkTweak();

    SUBCASE("unset PerksFile returns nullptr") {
        initSfallConfigMemory();
        CHECK(perkTweakGetPerksFilePath() == nullptr);
    }

    SUBCASE("empty PerksFile returns nullptr") {
        initSfallConfigMemory();
        configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PERKS_FILE_KEY, "");
        CHECK(perkTweakGetPerksFilePath() == nullptr);
    }

    SUBCASE("configured PerksFile returns the path") {
        initSfallConfigMemory();
        configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PERKS_FILE_KEY, "config\\Perks.ini");
        char* path = perkTweakGetPerksFilePath();
        REQUIRE(path != nullptr);
        CHECK(strcmp(path, "config\\Perks.ini") == 0);
    }
}

// =============================================================
// perkTweakLoad no-op behavior (stubbed file I/O)
// =============================================================

TEST_CASE("perkTweakLoad with no PerksFile is a no-op") {
    resetPerkTweak();

    // Sanity: defaults before.
    CHECK(gPerkTweak.survivalistBonus == 25);

    // No PerksFile configured → no-op, defaults intact.
    initSfallConfigMemory();
    perkTweakLoad();
    CHECK(gPerkTweak.survivalistBonus == 25);

    // PerksFile configured but unreadable in the harness (compat_fopen
    // → nullptr) → graceful no-op, defaults intact, no crash.
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PERKS_FILE_KEY, "config\\Perks.ini");
    perkTweakLoad();
    CHECK(gPerkTweak.survivalistBonus == 25);
}

// =============================================================
// Gate+clamp semantics (mirrors sfall Perks.cpp TryPatchValue*)
// =============================================================

TEST_CASE("PerksTweak gate+clamp semantics") {
    resetPerkTweak();

    Config config;
    REQUIRE(configInit(&config));

    SUBCASE("absent key keeps the FO2 default") {
        // Config has no PerksTweak section at all.
        perkTweakLoadFromConfigForTest(&config);
        CHECK(gPerkTweak.survivalistBonus == 25);
        CHECK(gPerkTweak.nightVisionBonus == 20);
    }

    SUBCASE("valid value applies") {
        configSetInt(&config, "PerksTweak", "SurvivalistBonus", 0);
        configSetInt(&config, "PerksTweak", "MrFixitBonus", 20);
        configSetInt(&config, "PerksTweak", "NightVisionBonus", 10);
        perkTweakLoadFromConfigForTest(&config);
        CHECK(gPerkTweak.survivalistBonus == 0);
        CHECK(gPerkTweak.mrFixitBonus == 20);
        CHECK(gPerkTweak.nightVisionBonus == 10);
    }

    SUBCASE("value below the minimum gate is NOT applied") {
        configSetInt(&config, "PerksTweak", "SurvivalistBonus", -1);
        configSetInt(&config, "PerksTweak", "WeaponLongRangeBonus", 1);
        configSetInt(&config, "PerksTweak", "WeaponHandlingBonus", -5);
        perkTweakLoadFromConfigForTest(&config);
        CHECK(gPerkTweak.survivalistBonus == 25);       // gate 0
        CHECK(gPerkTweak.weaponLongRangeBonus == 4);    // gate 2
        CHECK(gPerkTweak.weaponHandlingBonus == 3);     // gate 0
    }

    SUBCASE("value above the maximum is clamped") {
        configSetInt(&config, "PerksTweak", "SurvivalistBonus", 1250);
        configSetInt(&config, "PerksTweak", "NightVisionBonus", 250);
        configSetInt(&config, "PerksTweak", "StonewallPercent", 101);
        configSetInt(&config, "PerksTweak", "DemolitionExpertBonus", 10000);
        configSetInt(&config, "PerksTweak", "CautiousNatureBonus", 50);
        configSetInt(&config, "PerksTweak", "WeaponHandlingBonus", 99);
        perkTweakLoadFromConfigForTest(&config);
        CHECK(gPerkTweak.survivalistBonus == 125);
        CHECK(gPerkTweak.nightVisionBonus == 100);
        CHECK(gPerkTweak.stonewallPercent == 100);
        CHECK(gPerkTweak.demolitionExpertBonus == 999);
        CHECK(gPerkTweak.cautiousNatureBonus == 20);
        CHECK(gPerkTweak.weaponHandlingBonus == 10);
    }

    SUBCASE("negative values within range apply (CautiousNature)") {
        configSetInt(&config, "PerksTweak", "CautiousNatureBonus", -12);
        perkTweakLoadFromConfigForTest(&config);
        CHECK(gPerkTweak.cautiousNatureBonus == -12);
    }

    SUBCASE("no upper clamp for MasterTrader / Comprehension") {
        configSetInt(&config, "PerksTweak", "MasterTraderBonus", 500);
        configSetInt(&config, "PerksTweak", "ComprehensionBonus", 300);
        perkTweakLoadFromConfigForTest(&config);
        CHECK(gPerkTweak.masterTraderBonus == 500);
        CHECK(gPerkTweak.comprehensionBonus == 300);
    }

    configFree(&config);
}

TEST_CASE("VaultCityInoculations clamped-both semantics") {
    resetPerkTweak();

    Config config;
    REQUIRE(configInit(&config));

    SUBCASE("within range applies directly") {
        configSetInt(&config, "PerksTweak", "VaultCityInoculationsPoisonBonus", 15);
        configSetInt(&config, "PerksTweak", "VaultCityInoculationsRadBonus", -20);
        perkTweakLoadFromConfigForTest(&config);
        CHECK(gPerkTweak.vaultCityInoculationsPoisonBonus == 15);
        CHECK(gPerkTweak.vaultCityInoculationsRadBonus == -20);
    }

    SUBCASE("out of range clamps to [-100, 100] (no lower gate)") {
        configSetInt(&config, "PerksTweak", "VaultCityInoculationsPoisonBonus", -150);
        configSetInt(&config, "PerksTweak", "VaultCityInoculationsRadBonus", 150);
        perkTweakLoadFromConfigForTest(&config);
        CHECK(gPerkTweak.vaultCityInoculationsPoisonBonus == -100);
        CHECK(gPerkTweak.vaultCityInoculationsRadBonus == 100);
    }

    configFree(&config);
}

TEST_CASE("PerksTweak reload overwrites previous values") {
    resetPerkTweak();

    Config config;
    REQUIRE(configInit(&config));

    SUBCASE("reload applies a new value for an existing key") {
        configSetInt(&config, "PerksTweak", "SurvivalistBonus", 0);
        perkTweakLoadFromConfigForTest(&config);
        CHECK(gPerkTweak.survivalistBonus == 0);

        configSetInt(&config, "PerksTweak", "SurvivalistBonus", 50);
        perkTweakLoadFromConfigForTest(&config);
        CHECK(gPerkTweak.survivalistBonus == 50);
    }

    SUBCASE("absent key after a previous load keeps the applied value") {
        // Semantics: an absent key leaves the current value untouched
        // (configGetInt present-semantics — no default overload). This
        // mirrors sfall's read-once-at-startup behavior, where a key
        // missing from the file never resets an earlier value.
        configSetInt(&config, "PerksTweak", "SurvivalistBonus", 0);
        perkTweakLoadFromConfigForTest(&config);
        CHECK(gPerkTweak.survivalistBonus == 0);

        dictionaryRemoveValue(&config, "PerksTweak");
        perkTweakLoadFromConfigForTest(&config);
        CHECK(gPerkTweak.survivalistBonus == 0);
    }

    configFree(&config);
}
