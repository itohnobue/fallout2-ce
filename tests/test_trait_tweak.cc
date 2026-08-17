// Unit tests for trait_tweak.cc — sfall [Traits] section parsing.
//
// Tests: defaults, Enable gating, [tN] section parsing, StatMod/SkillMod
// pair lists (incl. malformed/out-of-range), Name/Desc/Image overrides,
// reload overwrite + strdup ownership.
//
// This test LINKS trait_tweak.cc. That file depends on:
//   config.cc (Config init/set/get/free) — already in test_sources
//   dictionary.cc                        — already in test_sources
//   memory.cc (internal_strdup/free)     — already in test_sources
//   sfall_config.cc (gSfallConfig)       — already in test_sources
//   perk_tweak.cc (perkTweakGetPerksFilePath) — already in test_sources
//   debugPrint                           — in test_stubs
//
// File I/O (compat_fopen) is stubbed to nullptr, so the parser core is
// exercised via traitTweakLoadFromConfigForTest (TEST_ACCESSORS_ENABLED)
// with a manually populated Config.
//
// The application logic (traitGetStatModifier NoHardcode gating +
// StatMod application) lives in trait.cc (30+ engine deps, not linkable)
// — it is a direct extension of the hardcoded switch it gates, covered
// by the semantics asserted here (pair parsing + flag storage).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <string.h>

#include "config.h"
#include "memory.h"
#include "trait_tweak.h"

using namespace fallout;

static void resetTraitTweak() {
    for (int i = 0; i < TRAIT_COUNT; i++) {
        if (gTraitTweak[i].name != nullptr) {
            internal_free(gTraitTweak[i].name);
            gTraitTweak[i].name = nullptr;
        }
        if (gTraitTweak[i].description != nullptr) {
            internal_free(gTraitTweak[i].description);
            gTraitTweak[i].description = nullptr;
        }
        gTraitTweak[i] = TraitTweak();
    }
}

TEST_CASE("gTraitTweak defaults are inert") {
    resetTraitTweak();

    for (int i = TRAIT_FIRST; i < TRAIT_COUNT; i++) {
        CHECK_FALSE(gTraitTweak[i].noHardcode);
        CHECK(gTraitTweak[i].frmId == -1);
        CHECK(gTraitTweak[i].name == nullptr);
        CHECK(gTraitTweak[i].description == nullptr);
        CHECK(gTraitTweak[i].statModCount == 0);
        CHECK(gTraitTweak[i].skillModCount == 0);
    }
    CHECK_FALSE(traitTweakHasNoHardcode(TRAIT_SKILLED));
    CHECK_FALSE(traitTweakHasNoHardcode(static_cast<Trait>(-1)));
    CHECK_FALSE(traitTweakHasNoHardcode(static_cast<Trait>(TRAIT_COUNT)));
}

TEST_CASE("[Traits] Enable gating") {
    resetTraitTweak();

    Config config;
    REQUIRE(configInit(&config));

    SUBCASE("absent Enable -> no-op") {
        configSetInt(&config, "t14", "NoHardcode", 1);
        traitTweakLoadFromConfigForTest(&config);
        CHECK_FALSE(gTraitTweak[TRAIT_SKILLED].noHardcode);
    }

    SUBCASE("Enable=0 -> no-op") {
        configSetInt(&config, "Traits", "Enable", 0);
        configSetInt(&config, "t14", "NoHardcode", 1);
        traitTweakLoadFromConfigForTest(&config);
        CHECK_FALSE(gTraitTweak[TRAIT_SKILLED].noHardcode);
    }

    SUBCASE("Enable=1 -> applies") {
        configSetInt(&config, "Traits", "Enable", 1);
        configSetInt(&config, "t14", "NoHardcode", 1);
        traitTweakLoadFromConfigForTest(&config);
        CHECK(gTraitTweak[TRAIT_SKILLED].noHardcode);
    }

    configFree(&config);
}

TEST_CASE("[Traits] section parsing — Night Person (t13) / Skilled (t14)") {
    resetTraitTweak();

    Config config;
    REQUIRE(configInit(&config));
    configSetInt(&config, "Traits", "Enable", 1);

    SUBCASE("t13 Night Person: NoHardcode + Image + StatMod") {
        configSetInt(&config, "t13", "NoHardcode", 1);
        configSetInt(&config, "t13", "Image", 68);
        configSetString(&config, "t13", "StatMod", "1|-1|4|-1");

        traitTweakLoadFromConfigForTest(&config);

        CHECK(gTraitTweak[13].noHardcode);
        CHECK(gTraitTweak[13].frmId == 68);
        CHECK(gTraitTweak[13].statModCount == 2);
        if (gTraitTweak[13].statModCount == 2) {
            CHECK(gTraitTweak[13].statMods[0].stat == STAT_PERCEPTION);
            CHECK(gTraitTweak[13].statMods[0].mod == -1);
            CHECK(gTraitTweak[13].statMods[1].stat == STAT_INTELLIGENCE);
            CHECK(gTraitTweak[13].statMods[1].mod == -1);
        }
        CHECK(gTraitTweak[13].skillModCount == 0);
    }

    SUBCASE("t14 Skilled: SkillMod all skills") {
        configSetString(&config, "t14", "SkillMod", "0|10|1|10|17|10");

        traitTweakLoadFromConfigForTest(&config);

        CHECK_FALSE(gTraitTweak[TRAIT_SKILLED].noHardcode);
        CHECK(gTraitTweak[TRAIT_SKILLED].skillModCount == 3);
        if (gTraitTweak[TRAIT_SKILLED].skillModCount == 3) {
            CHECK(gTraitTweak[TRAIT_SKILLED].skillMods[0].skill == SKILL_SMALL_GUNS);
            CHECK(gTraitTweak[TRAIT_SKILLED].skillMods[0].mod == 10);
            CHECK(gTraitTweak[TRAIT_SKILLED].skillMods[1].skill == SKILL_BIG_GUNS);
            CHECK(gTraitTweak[TRAIT_SKILLED].skillMods[1].mod == 10);
            CHECK(gTraitTweak[TRAIT_SKILLED].skillMods[2].skill == static_cast<Skill>(17));
            CHECK(gTraitTweak[TRAIT_SKILLED].skillMods[2].mod == 10);
        }
    }

    SUBCASE("Name/Desc overrides are strdup'd and survive config free") {
        configSetString(&config, "t13", "Name", "Night Person");
        configSetString(&config, "t13", "Desc", "FO1 night person");

        traitTweakLoadFromConfigForTest(&config);
        configFree(&config);

        REQUIRE(gTraitTweak[13].name != nullptr);
        CHECK(strcmp(gTraitTweak[13].name, "Night Person") == 0);
        REQUIRE(gTraitTweak[13].description != nullptr);
        CHECK(strcmp(gTraitTweak[13].description, "FO1 night person") == 0);
    }

    configFree(&config);
}

TEST_CASE("[Traits] malformed and out-of-range pair lists") {
    resetTraitTweak();

    Config config;
    REQUIRE(configInit(&config));
    configSetInt(&config, "Traits", "Enable", 1);

    SUBCASE("out-of-range stat/skill ids are skipped, valid ones apply") {
        configSetString(&config, "t14", "StatMod", "1|-1|99|5|4|-1");
        configSetString(&config, "t14", "SkillMod", "0|10|99|10|17|-5");

        traitTweakLoadFromConfigForTest(&config);

        CHECK(gTraitTweak[TRAIT_SKILLED].statModCount == 2);
        CHECK(gTraitTweak[TRAIT_SKILLED].statMods[0].stat == STAT_PERCEPTION);
        CHECK(gTraitTweak[TRAIT_SKILLED].statMods[0].mod == -1);
        CHECK(gTraitTweak[TRAIT_SKILLED].statMods[1].stat == STAT_INTELLIGENCE);
        CHECK(gTraitTweak[TRAIT_SKILLED].statMods[1].mod == -1);
        CHECK(gTraitTweak[TRAIT_SKILLED].skillModCount == 2);
        CHECK(gTraitTweak[TRAIT_SKILLED].skillMods[1].skill == static_cast<Skill>(17));
        CHECK(gTraitTweak[TRAIT_SKILLED].skillMods[1].mod == -5);
    }

    SUBCASE("trailing id without a mod is dropped (malformed pair)") {
        configSetString(&config, "t13", "StatMod", "1|-1|4");

        traitTweakLoadFromConfigForTest(&config);

        CHECK(gTraitTweak[13].statModCount == 1);
        CHECK(gTraitTweak[13].statMods[0].stat == STAT_PERCEPTION);
        CHECK(gTraitTweak[13].statMods[0].mod == -1);
    }

    configFree(&config);
}

TEST_CASE("[Traits] non-trait sections and invalid ids are ignored") {
    resetTraitTweak();

    Config config;
    REQUIRE(configInit(&config));
    configSetInt(&config, "Traits", "Enable", 1);

    configSetInt(&config, "t99", "NoHardcode", 1);
    configSetInt(&config, "Perks", "Enable", 1);
    configSetInt(&config, "PerksTweak", "SurvivalistBonus", 0);
    configSetInt(&config, "1", "Ranks", 3);

    traitTweakLoadFromConfigForTest(&config);

    // No trait should have been touched by the non-trait sections.
    for (int i = TRAIT_FIRST; i < TRAIT_COUNT; i++) {
        CHECK_FALSE(gTraitTweak[i].noHardcode);
        CHECK(gTraitTweak[i].statModCount == 0);
        CHECK(gTraitTweak[i].skillModCount == 0);
    }

    configFree(&config);
}

TEST_CASE("[Traits] reload frees prior overrides without double-free") {
    resetTraitTweak();

    Config config;
    REQUIRE(configInit(&config));
    configSetInt(&config, "Traits", "Enable", 1);

    configSetString(&config, "t13", "Name", "First");
    traitTweakLoadFromConfigForTest(&config);
    REQUIRE(gTraitTweak[13].name != nullptr);
    CHECK(strcmp(gTraitTweak[13].name, "First") == 0);

    configSetString(&config, "t13", "Name", "Second");
    traitTweakLoadFromConfigForTest(&config);
    REQUIRE(gTraitTweak[13].name != nullptr);
    CHECK(strcmp(gTraitTweak[13].name, "Second") == 0);

    traitTweakFree();
    CHECK(gTraitTweak[13].name == nullptr);
    CHECK(gTraitTweak[13].description == nullptr);

    // traitTweakFree is idempotent.
    traitTweakFree();

    configFree(&config);
}
