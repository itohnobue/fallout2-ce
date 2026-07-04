// Unit tests for the critical hit table data structure and accessor semantics.
//
// The criticalsGetValue/criticalsSetValue/criticalsResetValue functions in
// combat.cc operate on static arrays. This test validates the CriticalHitDescription
// union layout and the table indexing patterns, verifying the set/get/reset contract.
//
// NOTE: This does NOT link combat.cc (which has 40+ engine dependencies).
// It validates the data structure semantics against the types defined in combat_defs.h.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

// Include only the type definitions needed (no linking against combat.cc).
// These mirror the declarations in combat_defs.h and proto_types.h.
namespace fallout {

enum {
    HIT_LOCATION_HEAD = 0,
    HIT_LOCATION_LEFT_ARM,
    HIT_LOCATION_RIGHT_ARM,
    HIT_LOCATION_TORSO,
    HIT_LOCATION_RIGHT_LEG,
    HIT_LOCATION_LEFT_LEG,
    HIT_LOCATION_EYES,
    HIT_LOCATION_GROIN,
    HIT_LOCATION_UNCALLED,
    HIT_LOCATION_COUNT,
};

enum {
    CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER = 0,
    CRIT_DATA_MEMBER_FLAGS,
    CRIT_DATA_MEMBER_MASSIVE_CRITICAL_STAT,
    CRIT_DATA_MEMBER_MASSIVE_CRITICAL_STAT_MODIFIER,
    CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS,
    CRIT_DATA_MEMBER_MESSAGE_ID,
    CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID,
    CRIT_DATA_MEMBER_COUNT,
};

constexpr int CRTICIAL_EFFECT_COUNT = 6;
constexpr int KILL_TYPE_COUNT = 19;
constexpr int SFALL_KILL_TYPE_COUNT = KILL_TYPE_COUNT * 2;

// Mirrors combat_defs.h CriticalHitDescription
typedef union CriticalHitDescription {
    struct {
        int damageMultiplier;
        int flags;
        int massiveCriticalStat;
        int massiveCriticalStatModifier;
        int massiveCriticalFlags;
        int messageId;
        int massiveCriticalMessageId;
    };
    int values[CRIT_DATA_MEMBER_COUNT];
} CriticalHitDescription;

} // namespace fallout

using namespace fallout;

// Simulated critical tables (mirrors the static arrays in combat.cc).
static CriticalHitDescription gCriticalHitTables[SFALL_KILL_TYPE_COUNT][HIT_LOCATION_COUNT][CRTICIAL_EFFECT_COUNT];
static CriticalHitDescription gPlayerCriticalHitTable[HIT_LOCATION_COUNT][CRTICIAL_EFFECT_COUNT];
static CriticalHitDescription gBaseCriticalHitTables[SFALL_KILL_TYPE_COUNT][HIT_LOCATION_COUNT][CRTICIAL_EFFECT_COUNT];
static CriticalHitDescription gBasePlayerCriticalHitTable[HIT_LOCATION_COUNT][CRTICIAL_EFFECT_COUNT];

// Exact copies of the accessor functions from combat.cc lines 6343-6368.
static int criticalsGetValue(int killType, int hitLocation, int effect, int dataMember)
{
    if (killType == SFALL_KILL_TYPE_COUNT) {
        return gPlayerCriticalHitTable[hitLocation][effect].values[dataMember];
    } else {
        return gCriticalHitTables[killType][hitLocation][effect].values[dataMember];
    }
}

static void criticalsSetValue(int killType, int hitLocation, int effect, int dataMember, int value)
{
    if (killType == SFALL_KILL_TYPE_COUNT) {
        gPlayerCriticalHitTable[hitLocation][effect].values[dataMember] = value;
    } else {
        gCriticalHitTables[killType][hitLocation][effect].values[dataMember] = value;
    }
}

static void criticalsResetValue(int killType, int hitLocation, int effect, int dataMember)
{
    if (killType == SFALL_KILL_TYPE_COUNT) {
        gPlayerCriticalHitTable[hitLocation][effect].values[dataMember] = gBasePlayerCriticalHitTable[hitLocation][effect].values[dataMember];
    } else {
        gCriticalHitTables[killType][hitLocation][effect].values[dataMember] = gBaseCriticalHitTables[killType][hitLocation][effect].values[dataMember];
    }
}

TEST_CASE("CriticalHitDescription union layout")
{
    // Verify that the union's values[] and named members alias correctly.
    CriticalHitDescription desc;
    memset(&desc, 0, sizeof(desc));

    desc.damageMultiplier = 2;
    CHECK(desc.values[CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER] == 2);

    desc.values[CRIT_DATA_MEMBER_FLAGS] = 0x1234;
    CHECK(desc.flags == 0x1234);

    desc.messageId = 5001;
    CHECK(desc.values[CRIT_DATA_MEMBER_MESSAGE_ID] == 5001);

    desc.massiveCriticalStat = -1;
    CHECK(desc.values[CRIT_DATA_MEMBER_MASSIVE_CRITICAL_STAT] == -1);

    desc.massiveCriticalFlags = 0xABCD;
    CHECK(desc.values[CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS] == 0xABCD);

    desc.massiveCriticalMessageId = 9999;
    CHECK(desc.values[CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID] == 9999);
}

TEST_CASE("criticalsSetValue / criticalsGetValue")
{
    // Zero-initialize tables
    memset(gCriticalHitTables, 0, sizeof(gCriticalHitTables));
    memset(gPlayerCriticalHitTable, 0, sizeof(gPlayerCriticalHitTable));

    SUBCASE("set and get for normal kill type")
    {
        criticalsSetValue(KILL_TYPE_COUNT - 1, HIT_LOCATION_HEAD, 2,
            CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER, 4);
        CHECK(criticalsGetValue(KILL_TYPE_COUNT - 1, HIT_LOCATION_HEAD, 2,
            CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == 4);
    }

    SUBCASE("set and get for player kill type (SFALL_KILL_TYPE_COUNT)")
    {
        criticalsSetValue(SFALL_KILL_TYPE_COUNT, HIT_LOCATION_TORSO, 3,
            CRIT_DATA_MEMBER_FLAGS, 0xDEAD);
        CHECK(criticalsGetValue(SFALL_KILL_TYPE_COUNT, HIT_LOCATION_TORSO, 3,
            CRIT_DATA_MEMBER_FLAGS) == 0xDEAD);
    }

    SUBCASE("set multiple data members on same entry")
    {
        criticalsSetValue(0, HIT_LOCATION_UNCALLED, 2,
            CRIT_DATA_MEMBER_FLAGS, 0x01);
        criticalsSetValue(0, HIT_LOCATION_UNCALLED, 2,
            CRIT_DATA_MEMBER_MESSAGE_ID, 5019);
        criticalsSetValue(0, HIT_LOCATION_UNCALLED, 2,
            CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER, 3);

        CHECK(criticalsGetValue(0, HIT_LOCATION_UNCALLED, 2,
            CRIT_DATA_MEMBER_FLAGS) == 0x01);
        CHECK(criticalsGetValue(0, HIT_LOCATION_UNCALLED, 2,
            CRIT_DATA_MEMBER_MESSAGE_ID) == 5019);
        CHECK(criticalsGetValue(0, HIT_LOCATION_UNCALLED, 2,
            CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == 3);
    }

    SUBCASE("independent entries do not interfere")
    {
        criticalsSetValue(0, HIT_LOCATION_HEAD, 0,
            CRIT_DATA_MEMBER_MESSAGE_ID, 100);
        criticalsSetValue(0, HIT_LOCATION_HEAD, 1,
            CRIT_DATA_MEMBER_MESSAGE_ID, 200);
        criticalsSetValue(1, HIT_LOCATION_HEAD, 0,
            CRIT_DATA_MEMBER_MESSAGE_ID, 300);

        CHECK(criticalsGetValue(0, HIT_LOCATION_HEAD, 0,
            CRIT_DATA_MEMBER_MESSAGE_ID) == 100);
        CHECK(criticalsGetValue(0, HIT_LOCATION_HEAD, 1,
            CRIT_DATA_MEMBER_MESSAGE_ID) == 200);
        CHECK(criticalsGetValue(1, HIT_LOCATION_HEAD, 0,
            CRIT_DATA_MEMBER_MESSAGE_ID) == 300);
    }

    SUBCASE("all kill types and hit locations accessible")
    {
        // Verify we can write to every kill type and hit location
        for (int kt = 0; kt < SFALL_KILL_TYPE_COUNT; kt++) {
            for (int hl = 0; hl < HIT_LOCATION_COUNT; hl++) {
                criticalsSetValue(kt, hl, 0, CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER, kt * 100 + hl);
            }
        }
        // Spot-check a few
        CHECK(criticalsGetValue(5, HIT_LOCATION_EYES, 0,
            CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == 5 * 100 + HIT_LOCATION_EYES);
        CHECK(criticalsGetValue(SFALL_KILL_TYPE_COUNT - 1, HIT_LOCATION_GROIN, 0,
            CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == (SFALL_KILL_TYPE_COUNT - 1) * 100 + HIT_LOCATION_GROIN);
    }

    SUBCASE("all critical effects accessible")
    {
        for (int eff = 0; eff < CRTICIAL_EFFECT_COUNT; eff++) {
            criticalsSetValue(0, HIT_LOCATION_HEAD, eff,
                CRIT_DATA_MEMBER_MESSAGE_ID, eff * 10);
        }
        for (int eff = 0; eff < CRTICIAL_EFFECT_COUNT; eff++) {
            CHECK(criticalsGetValue(0, HIT_LOCATION_HEAD, eff,
                CRIT_DATA_MEMBER_MESSAGE_ID) == eff * 10);
        }
    }
}

TEST_CASE("criticalsResetValue")
{
    memset(gCriticalHitTables, 0, sizeof(gCriticalHitTables));
    memset(gPlayerCriticalHitTable, 0, sizeof(gPlayerCriticalHitTable));
    memset(gBaseCriticalHitTables, 0, sizeof(gBaseCriticalHitTables));
    memset(gBasePlayerCriticalHitTable, 0, sizeof(gBasePlayerCriticalHitTable));

    SUBCASE("reset restores base value for normal kill type")
    {
        // Set base value
        int baseVal = 42;
        memcpy(&gBaseCriticalHitTables[0][HIT_LOCATION_HEAD][0].values[CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER],
               &baseVal, sizeof(baseVal));

        // Set working value
        criticalsSetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER, 99);
        CHECK(criticalsGetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == 99);

        // Reset
        criticalsResetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER);
        CHECK(criticalsGetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == 42);
    }

    SUBCASE("reset restores base value for player table")
    {
        int baseVal = 7;
        memcpy(&gBasePlayerCriticalHitTable[HIT_LOCATION_TORSO][1].values[CRIT_DATA_MEMBER_FLAGS],
               &baseVal, sizeof(baseVal));

        criticalsSetValue(SFALL_KILL_TYPE_COUNT, HIT_LOCATION_TORSO, 1, CRIT_DATA_MEMBER_FLAGS, 999);
        CHECK(criticalsGetValue(SFALL_KILL_TYPE_COUNT, HIT_LOCATION_TORSO, 1, CRIT_DATA_MEMBER_FLAGS) == 999);

        criticalsResetValue(SFALL_KILL_TYPE_COUNT, HIT_LOCATION_TORSO, 1, CRIT_DATA_MEMBER_FLAGS);
        CHECK(criticalsGetValue(SFALL_KILL_TYPE_COUNT, HIT_LOCATION_TORSO, 1, CRIT_DATA_MEMBER_FLAGS) == 7);
    }
}

TEST_CASE("CRIT_DATA_MEMBER_COUNT matches union field count")
{
    // The count should match the number of named int fields in the union
    CHECK(CRIT_DATA_MEMBER_COUNT == 7);
    CHECK(sizeof(CriticalHitDescription) == CRIT_DATA_MEMBER_COUNT * sizeof(int));
}

TEST_CASE("table dimensions match constants")
{
    CHECK(HIT_LOCATION_COUNT == 9);        // head, left arm, right arm, torso, right leg, left leg, eyes, groin, uncalled
    CHECK(CRTICIAL_EFFECT_COUNT == 6);     // 0-5 effects
    CHECK(KILL_TYPE_COUNT == 19);          // man, woman, child, super mutant, ghoul, brahmin, radscorpion, rat, floater, centaur, robot, dog, mantis, deathclaw, plant, gecko, alien, giant ant, big bad boss
    CHECK(SFALL_KILL_TYPE_COUNT == 38);    // 2x KILL_TYPE_COUNT for sfall extended tables
    CHECK(CRIT_DATA_MEMBER_COUNT == 7);
}
