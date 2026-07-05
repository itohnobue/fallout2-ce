// Unit tests for the critical hit table data structure semantics.
//
// Validates the CriticalHitDescription union layout (member aliasing via
// values[]), table dimension constants, and the set/get/reset data flow
// through multi-dimensional arrays.
//
// The production accessor functions live in combat.h:62-64 and combat.cc:6368-6393.
// This test does NOT link combat.cc (which has 40+ engine dependencies) — it uses
// local test stubs that mirror the same data structure patterns to validate
// indexing and aliasing correctness.
//
// All type names use a "Test" prefix to avoid collision with the real types in
// combat_defs.h and proto_types.h. Once CMakeLists.txt adds the src/ include path
// for test_criticals, the Test* types can be replaced with #include "combat_defs.h"
// and the real CriticalHitDescriptionDataMember enum / CriticalHitDescription union.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

// Test-local type definitions mirroring combat_defs.h and proto_types.h.
// Prefixed with "Test" to avoid symbol collision with the real headers.
// See src/combat_defs.h:75-87 for HitLocation, src/combat_defs.h:126-161 for
// CriticalHitDescriptionDataMember / CriticalHitDescription, and
// src/proto_types.h:104-130 for KILL_TYPE_COUNT / SFALL_KILL_TYPE_COUNT.
namespace fallout {

enum {
    TEST_HIT_LOCATION_HEAD = 0,
    TEST_HIT_LOCATION_LEFT_ARM,
    TEST_HIT_LOCATION_RIGHT_ARM,
    TEST_HIT_LOCATION_TORSO,
    TEST_HIT_LOCATION_RIGHT_LEG,
    TEST_HIT_LOCATION_LEFT_LEG,
    TEST_HIT_LOCATION_EYES,
    TEST_HIT_LOCATION_GROIN,
    TEST_HIT_LOCATION_UNCALLED,
    TEST_HIT_LOCATION_COUNT,
};

enum {
    TEST_CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER = 0,
    TEST_CRIT_DATA_MEMBER_FLAGS,
    TEST_CRIT_DATA_MEMBER_MASSIVE_CRITICAL_STAT,
    TEST_CRIT_DATA_MEMBER_MASSIVE_CRITICAL_STAT_MODIFIER,
    TEST_CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS,
    TEST_CRIT_DATA_MEMBER_MESSAGE_ID,
    TEST_CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID,
    TEST_CRIT_DATA_MEMBER_COUNT,
};

constexpr int TEST_CRTICIAL_EFFECT_COUNT = 6;
constexpr int TEST_KILL_TYPE_COUNT = 19;
constexpr int TEST_SFALL_KILL_TYPE_COUNT = TEST_KILL_TYPE_COUNT * 2;

// Test mirror of combat_defs.h:138-161 CriticalHitDescription union.
typedef union TestCriticalHitDescription {
    struct {
        int damageMultiplier;
        int flags;
        int massiveCriticalStat;
        int massiveCriticalStatModifier;
        int massiveCriticalFlags;
        int messageId;
        int massiveCriticalMessageId;
    };
    int values[TEST_CRIT_DATA_MEMBER_COUNT];
} TestCriticalHitDescription;

} // namespace fallout

using namespace fallout;

// Simulated critical tables (mirrors the static arrays in combat.cc:197-1976).
static TestCriticalHitDescription gCriticalHitTables[TEST_SFALL_KILL_TYPE_COUNT][TEST_HIT_LOCATION_COUNT][TEST_CRTICIAL_EFFECT_COUNT];
static TestCriticalHitDescription gPlayerCriticalHitTable[TEST_HIT_LOCATION_COUNT][TEST_CRTICIAL_EFFECT_COUNT];
static TestCriticalHitDescription gBaseCriticalHitTables[TEST_SFALL_KILL_TYPE_COUNT][TEST_HIT_LOCATION_COUNT][TEST_CRTICIAL_EFFECT_COUNT];
static TestCriticalHitDescription gBasePlayerCriticalHitTable[TEST_HIT_LOCATION_COUNT][TEST_CRTICIAL_EFFECT_COUNT];

// Test stubs mirroring the production accessor functions declared in combat.h:62-64
// and implemented in combat.cc:6368-6393. These stubs operate on the local test
// tables above and follow the same indexing patterns as the real implementation:
//   - killType == SFALL_KILL_TYPE_COUNT selects the player table
//   - otherwise selects gCriticalHitTables[killType]
static int testCriticalsGetValue(int killType, int hitLocation, int effect, int dataMember)
{
    if (killType == TEST_SFALL_KILL_TYPE_COUNT) {
        return gPlayerCriticalHitTable[hitLocation][effect].values[dataMember];
    } else {
        return gCriticalHitTables[killType][hitLocation][effect].values[dataMember];
    }
}

static void testCriticalsSetValue(int killType, int hitLocation, int effect, int dataMember, int value)
{
    if (killType == TEST_SFALL_KILL_TYPE_COUNT) {
        gPlayerCriticalHitTable[hitLocation][effect].values[dataMember] = value;
    } else {
        gCriticalHitTables[killType][hitLocation][effect].values[dataMember] = value;
    }
}

static void testCriticalsResetValue(int killType, int hitLocation, int effect, int dataMember)
{
    if (killType == TEST_SFALL_KILL_TYPE_COUNT) {
        gPlayerCriticalHitTable[hitLocation][effect].values[dataMember] = gBasePlayerCriticalHitTable[hitLocation][effect].values[dataMember];
    } else {
        gCriticalHitTables[killType][hitLocation][effect].values[dataMember] = gBaseCriticalHitTables[killType][hitLocation][effect].values[dataMember];
    }
}

TEST_CASE("TestCriticalHitDescription union layout")
{
    // Verify that the union's values[] and named members alias correctly.
    TestCriticalHitDescription desc;
    memset(&desc, 0, sizeof(desc));

    desc.damageMultiplier = 2;
    CHECK(desc.values[TEST_CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER] == 2);

    desc.values[TEST_CRIT_DATA_MEMBER_FLAGS] = 0x1234;
    CHECK(desc.flags == 0x1234);

    desc.messageId = 5001;
    CHECK(desc.values[TEST_CRIT_DATA_MEMBER_MESSAGE_ID] == 5001);

    desc.massiveCriticalStat = -1;
    CHECK(desc.values[TEST_CRIT_DATA_MEMBER_MASSIVE_CRITICAL_STAT] == -1);

    desc.massiveCriticalFlags = 0xABCD;
    CHECK(desc.values[TEST_CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS] == 0xABCD);

    desc.massiveCriticalMessageId = 9999;
    CHECK(desc.values[TEST_CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID] == 9999);
}

TEST_CASE("testCriticalsSetValue / testCriticalsGetValue — data flow through tables")
{
    // Zero-initialize tables
    memset(gCriticalHitTables, 0, sizeof(gCriticalHitTables));
    memset(gPlayerCriticalHitTable, 0, sizeof(gPlayerCriticalHitTable));

    SUBCASE("set and get for normal kill type")
    {
        testCriticalsSetValue(TEST_KILL_TYPE_COUNT - 1, TEST_HIT_LOCATION_HEAD, 2,
            TEST_CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER, 4);
        CHECK(testCriticalsGetValue(TEST_KILL_TYPE_COUNT - 1, TEST_HIT_LOCATION_HEAD, 2,
            TEST_CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == 4);
    }

    SUBCASE("set and get for player kill type (TEST_SFALL_KILL_TYPE_COUNT)")
    {
        testCriticalsSetValue(TEST_SFALL_KILL_TYPE_COUNT, TEST_HIT_LOCATION_TORSO, 3,
            TEST_CRIT_DATA_MEMBER_FLAGS, 0xDEAD);
        CHECK(testCriticalsGetValue(TEST_SFALL_KILL_TYPE_COUNT, TEST_HIT_LOCATION_TORSO, 3,
            TEST_CRIT_DATA_MEMBER_FLAGS) == 0xDEAD);
    }

    SUBCASE("set multiple data members on same entry")
    {
        testCriticalsSetValue(0, TEST_HIT_LOCATION_UNCALLED, 2,
            TEST_CRIT_DATA_MEMBER_FLAGS, 0x01);
        testCriticalsSetValue(0, TEST_HIT_LOCATION_UNCALLED, 2,
            TEST_CRIT_DATA_MEMBER_MESSAGE_ID, 5019);
        testCriticalsSetValue(0, TEST_HIT_LOCATION_UNCALLED, 2,
            TEST_CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER, 3);

        CHECK(testCriticalsGetValue(0, TEST_HIT_LOCATION_UNCALLED, 2,
            TEST_CRIT_DATA_MEMBER_FLAGS) == 0x01);
        CHECK(testCriticalsGetValue(0, TEST_HIT_LOCATION_UNCALLED, 2,
            TEST_CRIT_DATA_MEMBER_MESSAGE_ID) == 5019);
        CHECK(testCriticalsGetValue(0, TEST_HIT_LOCATION_UNCALLED, 2,
            TEST_CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == 3);
    }

    SUBCASE("independent entries do not interfere")
    {
        testCriticalsSetValue(0, TEST_HIT_LOCATION_HEAD, 0,
            TEST_CRIT_DATA_MEMBER_MESSAGE_ID, 100);
        testCriticalsSetValue(0, TEST_HIT_LOCATION_HEAD, 1,
            TEST_CRIT_DATA_MEMBER_MESSAGE_ID, 200);
        testCriticalsSetValue(1, TEST_HIT_LOCATION_HEAD, 0,
            TEST_CRIT_DATA_MEMBER_MESSAGE_ID, 300);

        CHECK(testCriticalsGetValue(0, TEST_HIT_LOCATION_HEAD, 0,
            TEST_CRIT_DATA_MEMBER_MESSAGE_ID) == 100);
        CHECK(testCriticalsGetValue(0, TEST_HIT_LOCATION_HEAD, 1,
            TEST_CRIT_DATA_MEMBER_MESSAGE_ID) == 200);
        CHECK(testCriticalsGetValue(1, TEST_HIT_LOCATION_HEAD, 0,
            TEST_CRIT_DATA_MEMBER_MESSAGE_ID) == 300);
    }

    SUBCASE("all kill types and hit locations accessible")
    {
        // Verify we can write to every kill type and hit location
        for (int kt = 0; kt < TEST_SFALL_KILL_TYPE_COUNT; kt++) {
            for (int hl = 0; hl < TEST_HIT_LOCATION_COUNT; hl++) {
                testCriticalsSetValue(kt, hl, 0, TEST_CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER, kt * 100 + hl);
            }
        }
        // Spot-check a few
        CHECK(testCriticalsGetValue(5, TEST_HIT_LOCATION_EYES, 0,
            TEST_CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == 5 * 100 + TEST_HIT_LOCATION_EYES);
        CHECK(testCriticalsGetValue(TEST_SFALL_KILL_TYPE_COUNT - 1, TEST_HIT_LOCATION_GROIN, 0,
            TEST_CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == (TEST_SFALL_KILL_TYPE_COUNT - 1) * 100 + TEST_HIT_LOCATION_GROIN);
    }

    SUBCASE("all critical effects accessible")
    {
        for (int eff = 0; eff < TEST_CRTICIAL_EFFECT_COUNT; eff++) {
            testCriticalsSetValue(0, TEST_HIT_LOCATION_HEAD, eff,
                TEST_CRIT_DATA_MEMBER_MESSAGE_ID, eff * 10);
        }
        for (int eff = 0; eff < TEST_CRTICIAL_EFFECT_COUNT; eff++) {
            CHECK(testCriticalsGetValue(0, TEST_HIT_LOCATION_HEAD, eff,
                TEST_CRIT_DATA_MEMBER_MESSAGE_ID) == eff * 10);
        }
    }
}

TEST_CASE("testCriticalsResetValue — base-value restoration")
{
    memset(gCriticalHitTables, 0, sizeof(gCriticalHitTables));
    memset(gPlayerCriticalHitTable, 0, sizeof(gPlayerCriticalHitTable));
    memset(gBaseCriticalHitTables, 0, sizeof(gBaseCriticalHitTables));
    memset(gBasePlayerCriticalHitTable, 0, sizeof(gBasePlayerCriticalHitTable));

    SUBCASE("reset restores base value for normal kill type")
    {
        // Set base value
        int baseVal = 42;
        memcpy(&gBaseCriticalHitTables[0][TEST_HIT_LOCATION_HEAD][0].values[TEST_CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER],
               &baseVal, sizeof(baseVal));

        // Set working value
        testCriticalsSetValue(0, TEST_HIT_LOCATION_HEAD, 0, TEST_CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER, 99);
        CHECK(testCriticalsGetValue(0, TEST_HIT_LOCATION_HEAD, 0, TEST_CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == 99);

        // Reset
        testCriticalsResetValue(0, TEST_HIT_LOCATION_HEAD, 0, TEST_CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER);
        CHECK(testCriticalsGetValue(0, TEST_HIT_LOCATION_HEAD, 0, TEST_CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == 42);
    }

    SUBCASE("reset restores base value for player table")
    {
        int baseVal = 7;
        memcpy(&gBasePlayerCriticalHitTable[TEST_HIT_LOCATION_TORSO][1].values[TEST_CRIT_DATA_MEMBER_FLAGS],
               &baseVal, sizeof(baseVal));

        testCriticalsSetValue(TEST_SFALL_KILL_TYPE_COUNT, TEST_HIT_LOCATION_TORSO, 1, TEST_CRIT_DATA_MEMBER_FLAGS, 999);
        CHECK(testCriticalsGetValue(TEST_SFALL_KILL_TYPE_COUNT, TEST_HIT_LOCATION_TORSO, 1, TEST_CRIT_DATA_MEMBER_FLAGS) == 999);

        testCriticalsResetValue(TEST_SFALL_KILL_TYPE_COUNT, TEST_HIT_LOCATION_TORSO, 1, TEST_CRIT_DATA_MEMBER_FLAGS);
        CHECK(testCriticalsGetValue(TEST_SFALL_KILL_TYPE_COUNT, TEST_HIT_LOCATION_TORSO, 1, TEST_CRIT_DATA_MEMBER_FLAGS) == 7);
    }
}

TEST_CASE("TEST_CRIT_DATA_MEMBER_COUNT matches union field count")
{
    // The count should match the number of named int fields in the union
    CHECK(TEST_CRIT_DATA_MEMBER_COUNT == 7);
    CHECK(sizeof(TestCriticalHitDescription) == TEST_CRIT_DATA_MEMBER_COUNT * sizeof(int));
}

TEST_CASE("table dimensions match constants")
{
    CHECK(TEST_HIT_LOCATION_COUNT == 9);        // head, left arm, right arm, torso, right leg, left leg, eyes, groin, uncalled
    CHECK(TEST_CRTICIAL_EFFECT_COUNT == 6);     // 0-5 effects
    CHECK(TEST_KILL_TYPE_COUNT == 19);          // man, woman, child, super mutant, ghoul, brahmin, radscorpion, rat, floater, centaur, robot, dog, mantis, deathclaw, plant, gecko, alien, giant ant, big bad boss
    CHECK(TEST_SFALL_KILL_TYPE_COUNT == 38);    // 2x KILL_TYPE_COUNT for sfall extended tables
    CHECK(TEST_CRIT_DATA_MEMBER_COUNT == 7);
}
