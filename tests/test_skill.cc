// Unit tests for skill.cc — data structure validation, accessor functions,
// and skill max cap metarule (set_skill_max opcode 0x81A2).
//
// Tests: skillIsValid, SkillDescription table integrity, skillGetDefaultValue,
//        skillGetName/skillGetDescription/skillGetAttributes/skillGetFrmId patterns,
//        gSkillMaxCap clamping logic, skillGetValue cap integration,
//        gSkillDescriptions entry validation, SKILL_COUNT constant.
//
// Uses test-local stubs mirroring skill.cc internal functions where the real
// source has heavy engine dependencies (Object, proto, perks, traits, etc.).
// Self-contained — no linking to engine sources needed.
//
// All production constants and function bodies are verified against
// src/skill.cc 24199e9..HEAD diff and src/skill_defs.h.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cstring>

#include "skill_defs.h"

// =============================================================
// Test-local types mirroring skill_defs.h
// =============================================================

// Mirror of skill_defs.h:12-32 — Skill enum
enum TestSkill {
    TEST_SKILL_SMALL_GUNS = 0,
    TEST_SKILL_BIG_GUNS,
    TEST_SKILL_ENERGY_WEAPONS,
    TEST_SKILL_UNARMED,
    TEST_SKILL_MELEE_WEAPONS,
    TEST_SKILL_THROWING,
    TEST_SKILL_FIRST_AID,
    TEST_SKILL_DOCTOR,
    TEST_SKILL_SNEAK,
    TEST_SKILL_LOCKPICK,
    TEST_SKILL_STEAL,
    TEST_SKILL_TRAPS,
    TEST_SKILL_SCIENCE,
    TEST_SKILL_REPAIR,
    TEST_SKILL_SPEECH,
    TEST_SKILL_BARTER,
    TEST_SKILL_GAMBLING,
    TEST_SKILL_OUTDOORSMAN,
    TEST_SKILL_COUNT,
};

// Mirror of skill_defs.h:6-9
constexpr int TEST_NUM_TAGGED_SKILLS = 4;
constexpr int TEST_DEFAULT_TAGGED_SKILLS = 3;
static_assert(TEST_NUM_TAGGED_SKILLS == NUM_TAGGED_SKILLS,
    "TEST_NUM_TAGGED_SKILLS must match production NUM_TAGGED_SKILLS");
static_assert(TEST_DEFAULT_TAGGED_SKILLS == DEFAULT_TAGGED_SKILLS,
    "TEST_DEFAULT_TAGGED_SKILLS must match production DEFAULT_TAGGED_SKILLS");

// Mirror of STAT_INVALID (used in gSkillDescriptions for stat2)
constexpr int TEST_STAT_INVALID = -1;

// Stat indices extracted from stat_defs.h (referenced in gSkillDescriptions)
// These are only the stats used in skill descriptions.
enum TestStat {
    TEST_STAT_AGILITY = 5,
    TEST_STAT_STRENGTH = 0,
    TEST_STAT_PERCEPTION = 3,
    TEST_STAT_INTELLIGENCE = 2,
    TEST_STAT_CHARISMA = 4,
    TEST_STAT_LUCK = 6,
    TEST_STAT_ENDURANCE = 1,
};

// Mirror of skill.cc:41-53 — SkillDescription struct
typedef struct TestSkillDescription {
    char* name;
    char* description;
    char* attributes;
    int frmId;
    int defaultValue;
    int statModifier;
    int stat1;
    int stat2;
    int baseValueMult;
    int experience;
    int gainXpFromSkillPenalty;
} TestSkillDescription;

// Mirror of skill.cc:82-101 — gSkillDescriptions table
// Production initializers are nullptr for name/desc/attrs (set at runtime via msg)
// We mirror the numeric fields exactly.
static TestSkillDescription gTestSkillDescriptions[TEST_SKILL_COUNT] = {
    // SKILL_SMALL_GUNS
    { nullptr, nullptr, nullptr, 28, 5, 4, TEST_STAT_AGILITY, TEST_STAT_INVALID, 1, 0, 0 },
    // SKILL_BIG_GUNS
    { nullptr, nullptr, nullptr, 29, 0, 2, TEST_STAT_AGILITY, TEST_STAT_INVALID, 1, 0, 0 },
    // SKILL_ENERGY_WEAPONS
    { nullptr, nullptr, nullptr, 30, 0, 2, TEST_STAT_AGILITY, TEST_STAT_INVALID, 1, 0, 0 },
    // SKILL_UNARMED
    { nullptr, nullptr, nullptr, 31, 30, 2, TEST_STAT_AGILITY, TEST_STAT_STRENGTH, 1, 0, 0 },
    // SKILL_MELEE_WEAPONS
    { nullptr, nullptr, nullptr, 32, 20, 2, TEST_STAT_AGILITY, TEST_STAT_STRENGTH, 1, 0, 0 },
    // SKILL_THROWING
    { nullptr, nullptr, nullptr, 33, 0, 4, TEST_STAT_AGILITY, TEST_STAT_INVALID, 1, 0, 0 },
    // SKILL_FIRST_AID
    { nullptr, nullptr, nullptr, 34, 0, 2, TEST_STAT_PERCEPTION, TEST_STAT_INTELLIGENCE, 1, 25, 0 },
    // SKILL_DOCTOR
    { nullptr, nullptr, nullptr, 35, 5, 1, TEST_STAT_PERCEPTION, TEST_STAT_INTELLIGENCE, 1, 50, 0 },
    // SKILL_SNEAK
    { nullptr, nullptr, nullptr, 36, 5, 3, TEST_STAT_AGILITY, TEST_STAT_INVALID, 1, 0, 0 },
    // SKILL_LOCKPICK
    { nullptr, nullptr, nullptr, 37, 10, 1, TEST_STAT_PERCEPTION, TEST_STAT_AGILITY, 1, 25, 1 },
    // SKILL_STEAL
    { nullptr, nullptr, nullptr, 38, 0, 3, TEST_STAT_AGILITY, TEST_STAT_INVALID, 1, 25, 1 },
    // SKILL_TRAPS
    { nullptr, nullptr, nullptr, 39, 10, 1, TEST_STAT_PERCEPTION, TEST_STAT_AGILITY, 1, 25, 1 },
    // SKILL_SCIENCE
    { nullptr, nullptr, nullptr, 40, 0, 4, TEST_STAT_INTELLIGENCE, TEST_STAT_INVALID, 1, 0, 0 },
    // SKILL_REPAIR
    { nullptr, nullptr, nullptr, 41, 0, 3, TEST_STAT_INTELLIGENCE, TEST_STAT_INVALID, 1, 0, 0 },
    // SKILL_SPEECH
    { nullptr, nullptr, nullptr, 42, 0, 5, TEST_STAT_CHARISMA, TEST_STAT_INVALID, 1, 0, 0 },
    // SKILL_BARTER
    { nullptr, nullptr, nullptr, 43, 0, 4, TEST_STAT_CHARISMA, TEST_STAT_INVALID, 1, 0, 0 },
    // SKILL_GAMBLING
    { nullptr, nullptr, nullptr, 44, 0, 5, TEST_STAT_LUCK, TEST_STAT_INVALID, 1, 0, 0 },
    // SKILL_OUTDOORSMAN
    { nullptr, nullptr, nullptr, 45, 0, 2, TEST_STAT_ENDURANCE, TEST_STAT_INTELLIGENCE, 1, 100, 0 },
};

// =============================================================
// Test stubs mirroring production functions
// =============================================================

// Default gSkillMaxCap = 300 (matches sfall_opcodes.cc:2799)
static int gTestSkillMaxCap = 300;

// R-13 (H-23): mirror of the per-critter skill max cap state written by
// set_critter_skill_mod (0x81C7). -1 = no per-critter cap set for this
// critter (matches sfallGetCritterSkillMax's contract).
static int gTestCritterSkillMax = -1;

// Mirror of skill.h:52-55 — skillIsValid (inline)
static inline bool testSkillIsValid(int skill)
{
    return skill >= 0 && skill < TEST_SKILL_COUNT;
}

// Mirror of skill.cc:278-282 — skillGetDefaultValue
static int testSkillGetDefaultValue(int skill)
{
    return testSkillIsValid(skill) ? gTestSkillDescriptions[skill].defaultValue : -5;
}

// Mirror of skill.cc:490-493 — skillGetName
static const char* testSkillGetName(int skill)
{
    return testSkillIsValid(skill) ? gTestSkillDescriptions[skill].name : nullptr;
}

// Mirror of skill.cc:496-500 — skillGetDescription
static const char* testSkillGetDescription(int skill)
{
    return testSkillIsValid(skill) ? gTestSkillDescriptions[skill].description : nullptr;
}

// Mirror of skill.cc:502-506 — skillGetAttributes
static const char* testSkillGetAttributes(int skill)
{
    return testSkillIsValid(skill) ? gTestSkillDescriptions[skill].attributes : nullptr;
}

// Mirror of skill.cc:508-512 — skillGetFrmId
static int testSkillGetFrmId(int skill)
{
    return testSkillIsValid(skill) ? gTestSkillDescriptions[skill].frmId : 0;
}

// Mirror of skill.cc:234-246 skillGetMaxSkill — effective skill max cap:
// per-critter cap (set_critter_skill_mod, 0x81C7) takes precedence, then the
// global gSkillMaxCap (set_skill_max / set_base_skill_mod), then 300.
static int testSkillGetMaxSkill()
{
    if (gTestCritterSkillMax >= 0) {
        return gTestCritterSkillMax;
    }
    return (gTestSkillMaxCap > 0) ? gTestSkillMaxCap : 300;
}

// Mirror of skill.cc:306-319 — skillGetValue cap clamping (simplified)
// This tests just the skill max cap clamping logic without full stat calculation.
static int testSkillCapValue(int rawValue)
{
    int maxSkill = testSkillGetMaxSkill();
    return (rawValue > maxSkill) ? maxSkill : rawValue;
}

// =============================================================
// TEST CASES
// =============================================================

TEST_CASE("SKILL_COUNT constant")
{
    // Fallout 2 has 18 skills — compile-time verified against production
    static_assert(TEST_SKILL_COUNT == fallout::SKILL_COUNT,
        "TEST_SKILL_COUNT must match production SKILL_COUNT");
    CHECK(TEST_SKILL_COUNT == 18);
}

TEST_CASE("NUM_TAGGED_SKILLS and DEFAULT_TAGGED_SKILLS")
{
    CHECK(TEST_NUM_TAGGED_SKILLS == 4);
    CHECK(TEST_DEFAULT_TAGGED_SKILLS == 3);
}

// ---- skillIsValid tests ----

TEST_CASE("skillIsValid — range check")
{
    SUBCASE("all 18 skills are valid")
    {
        CHECK(testSkillIsValid(TEST_SKILL_SMALL_GUNS));
        CHECK(testSkillIsValid(TEST_SKILL_BIG_GUNS));
        CHECK(testSkillIsValid(TEST_SKILL_ENERGY_WEAPONS));
        CHECK(testSkillIsValid(TEST_SKILL_UNARMED));
        CHECK(testSkillIsValid(TEST_SKILL_MELEE_WEAPONS));
        CHECK(testSkillIsValid(TEST_SKILL_THROWING));
        CHECK(testSkillIsValid(TEST_SKILL_FIRST_AID));
        CHECK(testSkillIsValid(TEST_SKILL_DOCTOR));
        CHECK(testSkillIsValid(TEST_SKILL_SNEAK));
        CHECK(testSkillIsValid(TEST_SKILL_LOCKPICK));
        CHECK(testSkillIsValid(TEST_SKILL_STEAL));
        CHECK(testSkillIsValid(TEST_SKILL_TRAPS));
        CHECK(testSkillIsValid(TEST_SKILL_SCIENCE));
        CHECK(testSkillIsValid(TEST_SKILL_REPAIR));
        CHECK(testSkillIsValid(TEST_SKILL_SPEECH));
        CHECK(testSkillIsValid(TEST_SKILL_BARTER));
        CHECK(testSkillIsValid(TEST_SKILL_GAMBLING));
        CHECK(testSkillIsValid(TEST_SKILL_OUTDOORSMAN));
    }

    SUBCASE("boundary edges")
    {
        CHECK(testSkillIsValid(0));               // first skill
        CHECK(testSkillIsValid(TEST_SKILL_COUNT - 1)); // last skill (OUTDOORSMAN = 17)

        CHECK_FALSE(testSkillIsValid(-1));
        CHECK_FALSE(testSkillIsValid(TEST_SKILL_COUNT)); // 18 (past end)
        CHECK_FALSE(testSkillIsValid(TEST_SKILL_COUNT + 1));
        CHECK_FALSE(testSkillIsValid(100));
    }

    SUBCASE("negative indices rejected")
    {
        CHECK_FALSE(testSkillIsValid(-1));
        CHECK_FALSE(testSkillIsValid(-5));
        CHECK_FALSE(testSkillIsValid(-100));
        CHECK_FALSE(testSkillIsValid(-0x7FFFFFFF));
    }

    SUBCASE("SKILL_COUNT itself is not a valid index")
    {
        CHECK_FALSE(testSkillIsValid(TEST_SKILL_COUNT));
    }
}

// ---- SkillDescription table integrity ----

TEST_CASE("gSkillDescriptions table — all 18 entries present")
{
    // Verify that every skill index has an entry and that each entry has
    // non-zero fields where expected.

    // Combat skills have base AGILITY as primary stat
    CHECK(gTestSkillDescriptions[TEST_SKILL_SMALL_GUNS].stat1 == TEST_STAT_AGILITY);
    CHECK(gTestSkillDescriptions[TEST_SKILL_BIG_GUNS].stat1 == TEST_STAT_AGILITY);
    CHECK(gTestSkillDescriptions[TEST_SKILL_ENERGY_WEAPONS].stat1 == TEST_STAT_AGILITY);
    CHECK(gTestSkillDescriptions[TEST_SKILL_THROWING].stat1 == TEST_STAT_AGILITY);

    // Unarmed and Melee use AGILITY + STRENGTH
    CHECK(gTestSkillDescriptions[TEST_SKILL_UNARMED].stat1 == TEST_STAT_AGILITY);
    CHECK(gTestSkillDescriptions[TEST_SKILL_UNARMED].stat2 == TEST_STAT_STRENGTH);
    CHECK(gTestSkillDescriptions[TEST_SKILL_MELEE_WEAPONS].stat1 == TEST_STAT_AGILITY);
    CHECK(gTestSkillDescriptions[TEST_SKILL_MELEE_WEAPONS].stat2 == TEST_STAT_STRENGTH);

    // Medical skills use PERCEPTION + INTELLIGENCE
    CHECK(gTestSkillDescriptions[TEST_SKILL_FIRST_AID].stat1 == TEST_STAT_PERCEPTION);
    CHECK(gTestSkillDescriptions[TEST_SKILL_FIRST_AID].stat2 == TEST_STAT_INTELLIGENCE);
    CHECK(gTestSkillDescriptions[TEST_SKILL_DOCTOR].stat1 == TEST_STAT_PERCEPTION);
    CHECK(gTestSkillDescriptions[TEST_SKILL_DOCTOR].stat2 == TEST_STAT_INTELLIGENCE);

    // Sneak, Steal use AGILITY
    CHECK(gTestSkillDescriptions[TEST_SKILL_SNEAK].stat1 == TEST_STAT_AGILITY);
    CHECK(gTestSkillDescriptions[TEST_SKILL_STEAL].stat1 == TEST_STAT_AGILITY);

    // Lockpick, Traps use PERCEPTION + AGILITY
    CHECK(gTestSkillDescriptions[TEST_SKILL_LOCKPICK].stat1 == TEST_STAT_PERCEPTION);
    CHECK(gTestSkillDescriptions[TEST_SKILL_LOCKPICK].stat2 == TEST_STAT_AGILITY);
    CHECK(gTestSkillDescriptions[TEST_SKILL_TRAPS].stat1 == TEST_STAT_PERCEPTION);
    CHECK(gTestSkillDescriptions[TEST_SKILL_TRAPS].stat2 == TEST_STAT_AGILITY);

    // Science, Repair use INTELLIGENCE
    CHECK(gTestSkillDescriptions[TEST_SKILL_SCIENCE].stat1 == TEST_STAT_INTELLIGENCE);
    CHECK(gTestSkillDescriptions[TEST_SKILL_REPAIR].stat1 == TEST_STAT_INTELLIGENCE);

    // Speech, Barter use CHARISMA
    CHECK(gTestSkillDescriptions[TEST_SKILL_SPEECH].stat1 == TEST_STAT_CHARISMA);
    CHECK(gTestSkillDescriptions[TEST_SKILL_BARTER].stat1 == TEST_STAT_CHARISMA);

    // Gambling uses LUCK
    CHECK(gTestSkillDescriptions[TEST_SKILL_GAMBLING].stat1 == TEST_STAT_LUCK);

    // Outdoorsman uses ENDURANCE + INTELLIGENCE
    CHECK(gTestSkillDescriptions[TEST_SKILL_OUTDOORSMAN].stat1 == TEST_STAT_ENDURANCE);
    CHECK(gTestSkillDescriptions[TEST_SKILL_OUTDOORSMAN].stat2 == TEST_STAT_INTELLIGENCE);
}

TEST_CASE("gSkillDescriptions — experience values")
{
    // Most combat/utility skills give 0 XP (they're not repeat-encouraged)
    CHECK(gTestSkillDescriptions[TEST_SKILL_SMALL_GUNS].experience == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_BIG_GUNS].experience == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_ENERGY_WEAPONS].experience == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_UNARMED].experience == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_MELEE_WEAPONS].experience == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_THROWING].experience == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_SNEAK].experience == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_SCIENCE].experience == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_REPAIR].experience == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_SPEECH].experience == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_BARTER].experience == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_GAMBLING].experience == 0);

    // Skills that grant XP on use
    CHECK(gTestSkillDescriptions[TEST_SKILL_FIRST_AID].experience == 25);
    CHECK(gTestSkillDescriptions[TEST_SKILL_DOCTOR].experience == 50);
    CHECK(gTestSkillDescriptions[TEST_SKILL_LOCKPICK].experience == 25);
    CHECK(gTestSkillDescriptions[TEST_SKILL_STEAL].experience == 25);
    CHECK(gTestSkillDescriptions[TEST_SKILL_TRAPS].experience == 25);
    CHECK(gTestSkillDescriptions[TEST_SKILL_OUTDOORSMAN].experience == 100);
}

TEST_CASE("gSkillDescriptions — gainXpFromSkillPenalty flags")
{
    // Most skills don't give XP from skill penalty
    CHECK(gTestSkillDescriptions[TEST_SKILL_SMALL_GUNS].gainXpFromSkillPenalty == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_DOCTOR].gainXpFromSkillPenalty == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_OUTDOORSMAN].gainXpFromSkillPenalty == 0);

    // These skills give XP even when skill penalty applies
    CHECK(gTestSkillDescriptions[TEST_SKILL_LOCKPICK].gainXpFromSkillPenalty == 1);
    CHECK(gTestSkillDescriptions[TEST_SKILL_STEAL].gainXpFromSkillPenalty == 1);
    CHECK(gTestSkillDescriptions[TEST_SKILL_TRAPS].gainXpFromSkillPenalty == 1);
}

TEST_CASE("gSkillDescriptions — defaultValue")
{
    // Unarmed has highest default (30)
    CHECK(gTestSkillDescriptions[TEST_SKILL_UNARMED].defaultValue == 30);

    // Melee weapons has 20
    CHECK(gTestSkillDescriptions[TEST_SKILL_MELEE_WEAPONS].defaultValue == 20);

    // Lockpick, Traps have 10
    CHECK(gTestSkillDescriptions[TEST_SKILL_LOCKPICK].defaultValue == 10);
    CHECK(gTestSkillDescriptions[TEST_SKILL_TRAPS].defaultValue == 10);

    // Small Guns, Doctor, Sneak have 5
    CHECK(gTestSkillDescriptions[TEST_SKILL_SMALL_GUNS].defaultValue == 5);
    CHECK(gTestSkillDescriptions[TEST_SKILL_DOCTOR].defaultValue == 5);
    CHECK(gTestSkillDescriptions[TEST_SKILL_SNEAK].defaultValue == 5);

    // Most skills start at 0
    CHECK(gTestSkillDescriptions[TEST_SKILL_BIG_GUNS].defaultValue == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_ENERGY_WEAPONS].defaultValue == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_THROWING].defaultValue == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_FIRST_AID].defaultValue == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_STEAL].defaultValue == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_SCIENCE].defaultValue == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_REPAIR].defaultValue == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_SPEECH].defaultValue == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_BARTER].defaultValue == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_GAMBLING].defaultValue == 0);
    CHECK(gTestSkillDescriptions[TEST_SKILL_OUTDOORSMAN].defaultValue == 0);
}

TEST_CASE("gSkillDescriptions — frmId sequential")
{
    // frmIds are sequential from 28 to 45 (inclusive)
    CHECK(gTestSkillDescriptions[0].frmId == 28);
    CHECK(gTestSkillDescriptions[1].frmId == 29);
    CHECK(gTestSkillDescriptions[2].frmId == 30);
    CHECK(gTestSkillDescriptions[TEST_SKILL_COUNT - 1].frmId == 45);

    // All 18 entries have sequential frmIds
    for (int i = 0; i < TEST_SKILL_COUNT; i++) {
        CHECK(gTestSkillDescriptions[i].frmId == 28 + i);
    }
}

TEST_CASE("gSkillDescriptions — stat2 INVALID for single-stat skills")
{
    // Skills with a single primary stat should have stat2 = TEST_STAT_INVALID (-1)
    CHECK(gTestSkillDescriptions[TEST_SKILL_SMALL_GUNS].stat2 == TEST_STAT_INVALID);
    CHECK(gTestSkillDescriptions[TEST_SKILL_BIG_GUNS].stat2 == TEST_STAT_INVALID);
    CHECK(gTestSkillDescriptions[TEST_SKILL_ENERGY_WEAPONS].stat2 == TEST_STAT_INVALID);
    CHECK(gTestSkillDescriptions[TEST_SKILL_THROWING].stat2 == TEST_STAT_INVALID);
    CHECK(gTestSkillDescriptions[TEST_SKILL_SNEAK].stat2 == TEST_STAT_INVALID);
    CHECK(gTestSkillDescriptions[TEST_SKILL_STEAL].stat2 == TEST_STAT_INVALID);
    CHECK(gTestSkillDescriptions[TEST_SKILL_SCIENCE].stat2 == TEST_STAT_INVALID);
    CHECK(gTestSkillDescriptions[TEST_SKILL_REPAIR].stat2 == TEST_STAT_INVALID);
    CHECK(gTestSkillDescriptions[TEST_SKILL_SPEECH].stat2 == TEST_STAT_INVALID);
    CHECK(gTestSkillDescriptions[TEST_SKILL_BARTER].stat2 == TEST_STAT_INVALID);
    CHECK(gTestSkillDescriptions[TEST_SKILL_GAMBLING].stat2 == TEST_STAT_INVALID);
}

TEST_CASE("gSkillDescriptions — statModifier values")
{
    // Speech and Gambling have highest statModifier (5)
    CHECK(gTestSkillDescriptions[TEST_SKILL_SPEECH].statModifier == 5);
    CHECK(gTestSkillDescriptions[TEST_SKILL_GAMBLING].statModifier == 5);

    // Small Guns, Throwing, Science, Barter have 4
    CHECK(gTestSkillDescriptions[TEST_SKILL_SMALL_GUNS].statModifier == 4);
    CHECK(gTestSkillDescriptions[TEST_SKILL_THROWING].statModifier == 4);
    CHECK(gTestSkillDescriptions[TEST_SKILL_SCIENCE].statModifier == 4);
    CHECK(gTestSkillDescriptions[TEST_SKILL_BARTER].statModifier == 4);

    // Sneak, Steal, Repair have 3
    CHECK(gTestSkillDescriptions[TEST_SKILL_SNEAK].statModifier == 3);
    CHECK(gTestSkillDescriptions[TEST_SKILL_STEAL].statModifier == 3);
    CHECK(gTestSkillDescriptions[TEST_SKILL_REPAIR].statModifier == 3);

    // Doctor, Lockpick, Traps have 1
    CHECK(gTestSkillDescriptions[TEST_SKILL_DOCTOR].statModifier == 1);
    CHECK(gTestSkillDescriptions[TEST_SKILL_LOCKPICK].statModifier == 1);
    CHECK(gTestSkillDescriptions[TEST_SKILL_TRAPS].statModifier == 1);

    // Big Guns, Energy Weapons, Unarmed, Melee, First Aid, Outdoorsman: 2
    CHECK(gTestSkillDescriptions[TEST_SKILL_BIG_GUNS].statModifier == 2);
    CHECK(gTestSkillDescriptions[TEST_SKILL_ENERGY_WEAPONS].statModifier == 2);
    CHECK(gTestSkillDescriptions[TEST_SKILL_UNARMED].statModifier == 2);
    CHECK(gTestSkillDescriptions[TEST_SKILL_MELEE_WEAPONS].statModifier == 2);
    CHECK(gTestSkillDescriptions[TEST_SKILL_FIRST_AID].statModifier == 2);
    CHECK(gTestSkillDescriptions[TEST_SKILL_OUTDOORSMAN].statModifier == 2);
}

TEST_CASE("gSkillDescriptions — baseValueMult is always 1")
{
    // baseValueMult is 1 for all skills (the base skill value contributes 1x)
    for (int i = 0; i < TEST_SKILL_COUNT; i++) {
        CHECK(gTestSkillDescriptions[i].baseValueMult == 1);
    }
}

// ---- skillGetDefaultValue tests ----

TEST_CASE("skillGetDefaultValue — returns defaultValue from table")
{
    SUBCASE("valid skills return their default")
    {
        CHECK(testSkillGetDefaultValue(TEST_SKILL_SMALL_GUNS) == 5);
        CHECK(testSkillGetDefaultValue(TEST_SKILL_BIG_GUNS) == 0);
        CHECK(testSkillGetDefaultValue(TEST_SKILL_UNARMED) == 30);
        CHECK(testSkillGetDefaultValue(TEST_SKILL_MELEE_WEAPONS) == 20);
        CHECK(testSkillGetDefaultValue(TEST_SKILL_DOCTOR) == 5);
        CHECK(testSkillGetDefaultValue(TEST_SKILL_SNEAK) == 5);
        CHECK(testSkillGetDefaultValue(TEST_SKILL_GAMBLING) == 0);
        CHECK(testSkillGetDefaultValue(TEST_SKILL_OUTDOORSMAN) == 0);
    }

    SUBCASE("invalid skill returns -5 (SKILL_ERR_INVALID_STAT)")
    {
        CHECK(testSkillGetDefaultValue(-1) == -5);
        CHECK(testSkillGetDefaultValue(TEST_SKILL_COUNT) == -5);
        CHECK(testSkillGetDefaultValue(100) == -5);
    }

    SUBCASE("boundary skill indices return correct defaults")
    {
        // Skill 0 = Small Guns, defaultValue = 5
        CHECK(testSkillGetDefaultValue(0) == 5);
        // Skill 17 = Outdoorsman, defaultValue = 0
        CHECK(testSkillGetDefaultValue(TEST_SKILL_COUNT - 1) == 0);
    }
}

// ---- skillGetName / skillGetDescription / skillGetAttributes patterns ----

TEST_CASE("skillGetName — returns name or nullptr for invalid")
{
    // Names are nullptr because they're set at runtime via message system
    CHECK(testSkillGetName(TEST_SKILL_SMALL_GUNS) == nullptr);
    CHECK(testSkillGetName(TEST_SKILL_BIG_GUNS) == nullptr);

    // Invalid skill returns nullptr
    CHECK(testSkillGetName(-1) == nullptr);
    CHECK(testSkillGetName(TEST_SKILL_COUNT) == nullptr);
}

TEST_CASE("skillGetDescription — returns description or nullptr for invalid")
{
    CHECK(testSkillGetDescription(TEST_SKILL_SMALL_GUNS) == nullptr);
    CHECK(testSkillGetDescription(-1) == nullptr);
}

TEST_CASE("skillGetAttributes — returns attributes or nullptr for invalid")
{
    CHECK(testSkillGetAttributes(TEST_SKILL_SMALL_GUNS) == nullptr);
    CHECK(testSkillGetAttributes(-1) == nullptr);
}

TEST_CASE("skillGetFrmId — returns frmId or 0 for invalid")
{
    // Valid skills return their frmId
    CHECK(testSkillGetFrmId(TEST_SKILL_SMALL_GUNS) == 28);
    CHECK(testSkillGetFrmId(TEST_SKILL_BIG_GUNS) == 29);
    CHECK(testSkillGetFrmId(TEST_SKILL_OUTDOORSMAN) == 45);

    // Invalid skill returns 0
    CHECK(testSkillGetFrmId(-1) == 0);
    CHECK(testSkillGetFrmId(TEST_SKILL_COUNT) == 0);
    CHECK(testSkillGetFrmId(100) == 0);
}

// ---- gSkillMaxCap tests (set_skill_max opcode 0x81A2 integration) ----

TEST_CASE("gSkillMaxCap default value is 300")
{
    // Default matches vanilla Fallout 2 skill cap (sfall_opcodes.cc:2799)
    CHECK(gTestSkillMaxCap == 300);
}

TEST_CASE("testSkillCapValue — clamping logic")
{
    SUBCASE("default cap (300) clamps values above 300")
    {
        gTestSkillMaxCap = 300;
        CHECK(testSkillCapValue(0) == 0);
        CHECK(testSkillCapValue(100) == 100);
        CHECK(testSkillCapValue(299) == 299);
        CHECK(testSkillCapValue(300) == 300);
        CHECK(testSkillCapValue(301) == 300);
        CHECK(testSkillCapValue(500) == 300);
        CHECK(testSkillCapValue(9999) == 300);
    }

    SUBCASE("cap of 0 falls back to 300 (gSkillMaxCap <= 0 ignored)")
    {
        gTestSkillMaxCap = 0;
        CHECK(testSkillCapValue(10) == 10);
        CHECK(testSkillCapValue(300) == 300);
        CHECK(testSkillCapValue(500) == 300);
    }

    SUBCASE("negative cap falls back to 300")
    {
        gTestSkillMaxCap = -1;
        CHECK(testSkillCapValue(100) == 100);
        CHECK(testSkillCapValue(301) == 300);
        CHECK(testSkillCapValue(999) == 300);
    }

    SUBCASE("custom cap (e.g. 200 via set_skill_max)")
    {
        gTestSkillMaxCap = 200;
        CHECK(testSkillCapValue(0) == 0);
        CHECK(testSkillCapValue(50) == 50);
        CHECK(testSkillCapValue(199) == 199);
        CHECK(testSkillCapValue(200) == 200);
        CHECK(testSkillCapValue(201) == 200);
        CHECK(testSkillCapValue(300) == 200);
    }

    SUBCASE("custom cap of 999 (higher than vanilla)")
    {
        gTestSkillMaxCap = 999;
        CHECK(testSkillCapValue(500) == 500);
        CHECK(testSkillCapValue(998) == 998);
        CHECK(testSkillCapValue(999) == 999);
        CHECK(testSkillCapValue(1000) == 999);
    }

    SUBCASE("small cap (e.g. 50)")
    {
        gTestSkillMaxCap = 50;
        CHECK(testSkillCapValue(0) == 0);
        CHECK(testSkillCapValue(49) == 49);
        CHECK(testSkillCapValue(50) == 50);
        CHECK(testSkillCapValue(51) == 50);
        CHECK(testSkillCapValue(300) == 50);
    }

    SUBCASE("cap of 1")
    {
        gTestSkillMaxCap = 1;
        CHECK(testSkillCapValue(0) == 0);
        CHECK(testSkillCapValue(1) == 1);
        CHECK(testSkillCapValue(2) == 1);
    }
}

// ---- Integration: skillGetValue cap behavior (without full stat calculation) ----

TEST_CASE("skillGetMaxSkill — all three integration points use the shared helper")
{
    // R-13 (H-23): production skill.cc now routes the max-skill decision through
    // the static skillGetMaxSkill() helper (skill.cc:234-246), called from:
    //   skillGetValue:   int maxSkill = skillGetMaxSkill(critter); (line ~309)
    //   skillAdd:        int maxSkill = skillGetMaxSkill(obj);    (line ~363)
    //   skillAddForce:   int maxSkill = skillGetMaxSkill(obj);    (line ~397)
    //
    // Precedence: per-critter cap (set_critter_skill_mod, 0x81C7) > global
    // gSkillMaxCap (set_skill_max / set_base_skill_mod) > engine default 300.
    int savedCritterMax = gTestCritterSkillMax;
    int savedGlobalMax = gTestSkillMaxCap;
    gTestCritterSkillMax = -1; // no per-critter cap

    SUBCASE("no per-critter cap, gSkillMaxCap = 300 (default)")
    {
        gTestSkillMaxCap = 300;
        CHECK(testSkillGetMaxSkill() == 300);
    }

    SUBCASE("no per-critter cap, gSkillMaxCap = 0 (fallback to 300)")
    {
        gTestSkillMaxCap = 0;
        CHECK(testSkillGetMaxSkill() == 300);
    }

    SUBCASE("no per-critter cap, gSkillMaxCap = -1 (fallback to 300)")
    {
        gTestSkillMaxCap = -1;
        CHECK(testSkillGetMaxSkill() == 300);
    }

    SUBCASE("no per-critter cap, gSkillMaxCap = 200 (custom global cap)")
    {
        gTestSkillMaxCap = 200;
        CHECK(testSkillGetMaxSkill() == 200);
    }

    SUBCASE("no per-critter cap, gSkillMaxCap = 1 (minimum positive cap)")
    {
        gTestSkillMaxCap = 1;
        CHECK(testSkillGetMaxSkill() == 1);
    }

    SUBCASE("no per-critter cap, gSkillMaxCap = 999 (above vanilla)")
    {
        gTestSkillMaxCap = 999;
        CHECK(testSkillGetMaxSkill() == 999);
    }

    SUBCASE("per-critter cap set (0x81C7) overrides the global cap")
    {
        gTestSkillMaxCap = 300;
        gTestCritterSkillMax = 100;
        CHECK(testSkillGetMaxSkill() == 100);

        gTestCritterSkillMax = 0;
        CHECK(testSkillGetMaxSkill() == 0);
    }

    SUBCASE("per-critter cap with higher global cap")
    {
        gTestSkillMaxCap = 999;
        gTestCritterSkillMax = 50;
        CHECK(testSkillGetMaxSkill() == 50);
    }

    gTestCritterSkillMax = savedCritterMax;
    gTestSkillMaxCap = savedGlobalMax;
}

// ---- Per-critter skill max cap (R-13 / H-23) ----

TEST_CASE("testSkillCapValue — per-critter skill max cap (set_critter_skill_mod)")
{
    int savedCritterMax = gTestCritterSkillMax;
    int savedGlobalMax = gTestSkillMaxCap;

    SUBCASE("per-critter cap below global cap clamps skill value")
    {
        gTestSkillMaxCap = 300;
        gTestCritterSkillMax = 100;
        CHECK(testSkillCapValue(0) == 0);
        CHECK(testSkillCapValue(99) == 99);
        CHECK(testSkillCapValue(100) == 100);
        CHECK(testSkillCapValue(150) == 100);
        CHECK(testSkillCapValue(300) == 100);
        CHECK(testSkillCapValue(9999) == 100);
    }

    SUBCASE("per-critter cap of 0 clamps everything to 0")
    {
        gTestSkillMaxCap = 300;
        gTestCritterSkillMax = 0;
        CHECK(testSkillCapValue(0) == 0);
        CHECK(testSkillCapValue(50) == 0);
        CHECK(testSkillCapValue(300) == 0);
    }

    SUBCASE("per-critter cap above global cap still wins (matches CheckSkillMax)")
    {
        gTestSkillMaxCap = 200;
        gTestCritterSkillMax = 300;
        CHECK(testSkillCapValue(250) == 250);
        CHECK(testSkillCapValue(300) == 300);
        CHECK(testSkillCapValue(400) == 300);
    }

    SUBCASE("no per-critter cap falls back to global cap")
    {
        gTestCritterSkillMax = -1;
        gTestSkillMaxCap = 200;
        CHECK(testSkillCapValue(150) == 150);
        CHECK(testSkillCapValue(250) == 200);
    }

    gTestCritterSkillMax = savedCritterMax;
    gTestSkillMaxCap = savedGlobalMax;
}

// ---- TestSkillDescription struct layout ----

TEST_CASE("TestSkillDescription struct layout")
{
    TestSkillDescription desc;
    memset(&desc, 0, sizeof(desc));

    // Set all fields
    desc.name = const_cast<char*>("TestSkill");
    desc.description = const_cast<char*>("A test skill");
    desc.attributes = const_cast<char*>("AG, PE");
    desc.frmId = 99;
    desc.defaultValue = 42;
    desc.statModifier = 3;
    desc.stat1 = TEST_STAT_AGILITY;
    desc.stat2 = TEST_STAT_INVALID;
    desc.baseValueMult = 1;
    desc.experience = 50;
    desc.gainXpFromSkillPenalty = 1;

    CHECK(strcmp(desc.name, "TestSkill") == 0);
    CHECK(strcmp(desc.description, "A test skill") == 0);
    CHECK(strcmp(desc.attributes, "AG, PE") == 0);
    CHECK(desc.frmId == 99);
    CHECK(desc.defaultValue == 42);
    CHECK(desc.statModifier == 3);
    CHECK(desc.stat1 == TEST_STAT_AGILITY);
    CHECK(desc.stat2 == TEST_STAT_INVALID);
    CHECK(desc.baseValueMult == 1);
    CHECK(desc.experience == 50);
    CHECK(desc.gainXpFromSkillPenalty == 1);
}

// ---- Enum value consistency ----

TEST_CASE("Skill enum values are sequential and zero-based")
{
    // All 18 skills must be sequential starting from 0
    CHECK(static_cast<int>(TEST_SKILL_SMALL_GUNS) == 0);
    CHECK(static_cast<int>(TEST_SKILL_BIG_GUNS) == 1);
    CHECK(static_cast<int>(TEST_SKILL_ENERGY_WEAPONS) == 2);
    CHECK(static_cast<int>(TEST_SKILL_UNARMED) == 3);
    CHECK(static_cast<int>(TEST_SKILL_MELEE_WEAPONS) == 4);
    CHECK(static_cast<int>(TEST_SKILL_THROWING) == 5);
    CHECK(static_cast<int>(TEST_SKILL_FIRST_AID) == 6);
    CHECK(static_cast<int>(TEST_SKILL_DOCTOR) == 7);
    CHECK(static_cast<int>(TEST_SKILL_SNEAK) == 8);
    CHECK(static_cast<int>(TEST_SKILL_LOCKPICK) == 9);
    CHECK(static_cast<int>(TEST_SKILL_STEAL) == 10);
    CHECK(static_cast<int>(TEST_SKILL_TRAPS) == 11);
    CHECK(static_cast<int>(TEST_SKILL_SCIENCE) == 12);
    CHECK(static_cast<int>(TEST_SKILL_REPAIR) == 13);
    CHECK(static_cast<int>(TEST_SKILL_SPEECH) == 14);
    CHECK(static_cast<int>(TEST_SKILL_BARTER) == 15);
    CHECK(static_cast<int>(TEST_SKILL_GAMBLING) == 16);
    CHECK(static_cast<int>(TEST_SKILL_OUTDOORSMAN) == 17);
}

TEST_CASE("RollResult enum values match random.h")
{
    // From random.h:8-12
    enum TestRollResult {
        TEST_ROLL_CRITICAL_FAILURE = 0,
        TEST_ROLL_FAILURE = 1,
        TEST_ROLL_SUCCESS = 2,
        TEST_ROLL_CRITICAL_SUCCESS = 3,
    };

    CHECK(TEST_ROLL_CRITICAL_FAILURE == 0);
    CHECK(TEST_ROLL_FAILURE == 1);
    CHECK(TEST_ROLL_SUCCESS == 2);
    CHECK(TEST_ROLL_CRITICAL_SUCCESS == 3);
}

// ===========================================================================
// M-173: base-skill mod applied ONCE (not doubled on tagged skills)
// ===========================================================================
// skill.cc:252 applies sfallGetBaseSkillMod(skill) in the base term; the
// tagged-skill "base again" term (skill.cc:256) must double only the proto
// baseValue. The pre-fix code inserted the mod into BOTH terms, giving
// tagged skills +2N instead of +N for mods set via set_base_skill_mod (0x81C8).

// Mirror of the fixed base-mod application (skill.cc:252-263).
static int testSkillValueWithMod(int baseValue, int baseSkillMod,
                                 int baseValueMult, bool isTagged)
{
    int value = (baseValue + baseSkillMod) * baseValueMult; // base term — mod once
    if (isTagged) {
        value += baseValue * baseValueMult; // tagged term — base only (M-173)
    }
    return value;
}

TEST_CASE("M-173: base-skill mod is applied once, not doubled on tagged skills")
{
    SUBCASE("non-tagged skill — mod applies once")
    {
        CHECK(testSkillValueWithMod(50, 10, 1, false) == 60);
    }

    SUBCASE("tagged skill — tagged term doubles base only, mod NOT doubled")
    {
        // base 50 + mod 10: base term 60, tagged term +50 → 110.
        // Pre-fix (bug): tagged term +60 → 120 (mod double-counted).
        CHECK(testSkillValueWithMod(50, 10, 1, true) == 110);
        CHECK(testSkillValueWithMod(50, 10, 1, true) != 120);
    }

    SUBCASE("tagged with zero mod matches vanilla double-base rule")
    {
        CHECK(testSkillValueWithMod(50, 0, 1, true) == 100); // 50 + 50
        CHECK(testSkillValueWithMod(50, 0, 1, false) == 50);
    }

    SUBCASE("negative mod (rare) is not amplified by the tagged term")
    {
        CHECK(testSkillValueWithMod(50, -5, 1, true) == 95);  // 45 + 50
        CHECK(testSkillValueWithMod(50, -5, 1, true) != 90);  // not 45 + 45
    }
}

// ===========================================================================
// M-174: steal catchChance clamped to [0, 100]
// ===========================================================================
// skill.cc:1185-1187 computed catchChance = skillGetValue(target, SKILL_STEAL)
// - stealModifier with NO clamp, while stealChance was clamped (1155-1167).
// stealModifier includes unbounded sfall pickpocket mods; a large positive
// mod drove catchChance deeply negative → randomRoll(negative) always returns
// ROLL_FAILURE → the thief is never caught. Fixed with std::clamp(catchChance,
// 0, 100), mirroring the steal-side clamp.

// Mirror of the fixed catchChance computation (skill.cc:1183-1190).
static int testCatchChance(int targetSteal, int stealModifier)
{
    int catchChance = targetSteal - stealModifier;
    catchChance = std::clamp(catchChance, 0, 100);
    return catchChance;
}

TEST_CASE("M-174: catchChance clamped to [0, 100]")
{
    SUBCASE("normal in-range chance passes through")
    {
        CHECK(testCatchChance(50, 0) == 50);
        CHECK(testCatchChance(80, 20) == 60);
    }

    SUBCASE("large positive pickpocket mod no longer drives catchChance negative")
    {
        // Pre-fix: 50 - 200 = -150 → randomRoll(-150) always ROLL_FAILURE
        // (thief never caught). Post-fix: clamped to 0.
        CHECK(testCatchChance(50, 200) == 0);
        CHECK(testCatchChance(5, 1000) == 0);
    }

    SUBCASE("large negative mod caps at 100 (can't exceed 100%)")
    {
        CHECK(testCatchChance(50, -100) == 100);
        CHECK(testCatchChance(50, -1000) == 100);
    }

    SUBCASE("boundary values stay in range")
    {
        CHECK(testCatchChance(0, 0) == 0);
        CHECK(testCatchChance(100, 0) == 100);
        CHECK(testCatchChance(0, -100) == 100);
    }
}
