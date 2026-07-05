// Unit tests for the combat system data structures and pure data operations.
//
// Validates:
//   - HitMode, HitLocation, CombatState, CombatBadShot enum values
//   - Hit location penalties — get/set/reset with bounds checking
//   - CriticalHitDescription union layout using REAL types from combat_defs.h
//   - Critical hit table data flow using proto_types.h dimension constants
//   - isUnarmedHitMode() inline function
//   - gLastAttacker/gLastTarget global state management (stub test)
//   - Attack struct layout and field access
//
// This test does NOT link combat.cc (which has 40+ engine dependencies).
// It uses local stub arrays and functions that mirror the production code
// patterns to validate indexing, aliasing, and data flow correctness.
// The real types from combat_defs.h and proto_types.h replace the Test*
// type mirrors used in test_criticals.cc.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

#include "combat_ai_defs.h"
#include "combat_defs.h"
#include "proto_types.h"
#include "stat_defs.h"

using namespace fallout;

// =============================================================
// Hit location penalties — stub arrays and functions
// mirrors combat.cc:184-200 and combat.cc:6959-6979
// =============================================================

static int hit_location_penalty_default[HIT_LOCATION_COUNT] = {
    -40,   // HIT_LOCATION_HEAD
    -30,   // HIT_LOCATION_LEFT_ARM
    -30,   // HIT_LOCATION_RIGHT_ARM
    0,     // HIT_LOCATION_TORSO
    -20,   // HIT_LOCATION_RIGHT_LEG
    -20,   // HIT_LOCATION_LEFT_LEG
    -60,   // HIT_LOCATION_EYES
    -30,   // HIT_LOCATION_GROIN
    0,     // HIT_LOCATION_UNCALLED
};

static int hit_location_penalty[HIT_LOCATION_COUNT];

static int stub_combat_get_hit_location_penalty(int hit_location)
{
    if (hit_location >= 0 && hit_location < HIT_LOCATION_COUNT) {
        return hit_location_penalty[hit_location];
    } else {
        return 0;
    }
}

static void stub_combat_set_hit_location_penalty(int hit_location, int penalty)
{
    if (hit_location >= 0 && hit_location < HIT_LOCATION_COUNT) {
        hit_location_penalty[hit_location] = penalty;
    }
}

static void stub_combat_reset_hit_location_penalty()
{
    for (int hit_location = 0; hit_location < HIT_LOCATION_COUNT; hit_location++) {
        hit_location_penalty[hit_location] = hit_location_penalty_default[hit_location];
    }
}

// =============================================================
// Critical hit tables — stub arrays and accessors
// mirrors combat.cc:205,1807,1983-1984 and combat.cc:6368-6393
// Using REAL CriticalHitDescription type from combat_defs.h
// =============================================================

static CriticalHitDescription gCriticalHitTables[SFALL_KILL_TYPE_COUNT][HIT_LOCATION_COUNT][CRTICIAL_EFFECT_COUNT];
static CriticalHitDescription gPlayerCriticalHitTable[HIT_LOCATION_COUNT][CRTICIAL_EFFECT_COUNT];
static CriticalHitDescription gBaseCriticalHitTables[SFALL_KILL_TYPE_COUNT][HIT_LOCATION_COUNT][CRTICIAL_EFFECT_COUNT];
static CriticalHitDescription gBasePlayerCriticalHitTable[HIT_LOCATION_COUNT][CRTICIAL_EFFECT_COUNT];

static int stub_criticalsGetValue(int killType, int hitLocation, int effect, int dataMember)
{
    if (killType == SFALL_KILL_TYPE_COUNT) {
        return gPlayerCriticalHitTable[hitLocation][effect].values[dataMember];
    } else {
        return gCriticalHitTables[killType][hitLocation][effect].values[dataMember];
    }
}

static void stub_criticalsSetValue(int killType, int hitLocation, int effect, int dataMember, int value)
{
    if (killType == SFALL_KILL_TYPE_COUNT) {
        gPlayerCriticalHitTable[hitLocation][effect].values[dataMember] = value;
    } else {
        gCriticalHitTables[killType][hitLocation][effect].values[dataMember] = value;
    }
}

static void stub_criticalsResetValue(int killType, int hitLocation, int effect, int dataMember)
{
    if (killType == SFALL_KILL_TYPE_COUNT) {
        gPlayerCriticalHitTable[hitLocation][effect].values[dataMember] = gBasePlayerCriticalHitTable[hitLocation][effect].values[dataMember];
    } else {
        gCriticalHitTables[killType][hitLocation][effect].values[dataMember] = gBaseCriticalHitTables[killType][hitLocation][effect].values[dataMember];
    }
}

// =============================================================
// gLastAttacker/gLastTarget — global state stub
// mirrors combat.cc:3587-3588 and sfall_opcodes.cc:3425-3426
// =============================================================

static int gLastAttacker = -1;
static int gLastTarget = -1;

static void stub_setLastAttacker(int attackerId, int targetId)
{
    gLastAttacker = attackerId;
    gLastTarget = targetId;
}

static int stub_getLastAttacker()
{
    return gLastAttacker;
}

static int stub_getLastTarget()
{
    return gLastTarget;
}

static void stub_resetLastTargets()
{
    gLastAttacker = -1;
    gLastTarget = -1;
}

// =============================================================
// HitMode Enum Tests
// =============================================================

TEST_CASE("HitMode enum values are correct")
{
    CHECK(static_cast<int>(HIT_MODE_LEFT_WEAPON_PRIMARY) == 0);
    CHECK(static_cast<int>(HIT_MODE_LEFT_WEAPON_SECONDARY) == 1);
    CHECK(static_cast<int>(HIT_MODE_RIGHT_WEAPON_PRIMARY) == 2);
    CHECK(static_cast<int>(HIT_MODE_RIGHT_WEAPON_SECONDARY) == 3);
    CHECK(static_cast<int>(HIT_MODE_PUNCH) == 4);
    CHECK(static_cast<int>(HIT_MODE_KICK) == 5);
    CHECK(static_cast<int>(HIT_MODE_LEFT_WEAPON_RELOAD) == 6);
    CHECK(static_cast<int>(HIT_MODE_RIGHT_WEAPON_RELOAD) == 7);

    // Advanced unarmed hit modes
    CHECK(static_cast<int>(HIT_MODE_STRONG_PUNCH) == 8);
    CHECK(static_cast<int>(HIT_MODE_HAMMER_PUNCH) == 9);
    CHECK(static_cast<int>(HIT_MODE_HAYMAKER) == 10);
    CHECK(static_cast<int>(HIT_MODE_JAB) == 11);
    CHECK(static_cast<int>(HIT_MODE_PALM_STRIKE) == 12);
    CHECK(static_cast<int>(HIT_MODE_PIERCING_STRIKE) == 13);

    CHECK(static_cast<int>(HIT_MODE_STRONG_KICK) == 14);
    CHECK(static_cast<int>(HIT_MODE_SNAP_KICK) == 15);
    CHECK(static_cast<int>(HIT_MODE_POWER_KICK) == 16);
    CHECK(static_cast<int>(HIT_MODE_HIP_KICK) == 17);
    CHECK(static_cast<int>(HIT_MODE_HOOK_KICK) == 18);
    CHECK(static_cast<int>(HIT_MODE_PIERCING_KICK) == 19);

    CHECK(static_cast<int>(HIT_MODE_COUNT) == 20);
}

TEST_CASE("Advanced unarmed hit mode ranges are correct")
{
    // FIRST_ADVANCED_PUNCH_HIT_MODE should be HIT_MODE_STRONG_PUNCH
    CHECK(static_cast<int>(FIRST_ADVANCED_PUNCH_HIT_MODE) == static_cast<int>(HIT_MODE_STRONG_PUNCH));
    CHECK(static_cast<int>(LAST_ADVANCED_PUNCH_HIT_MODE) == static_cast<int>(HIT_MODE_PIERCING_STRIKE));
    CHECK(static_cast<int>(FIRST_ADVANCED_KICK_HIT_MODE) == static_cast<int>(HIT_MODE_STRONG_KICK));
    CHECK(static_cast<int>(LAST_ADVANCED_KICK_HIT_MODE) == static_cast<int>(HIT_MODE_PIERCING_KICK));

    // FIRST_ADVANCED_UNARMED_HIT_MODE should span both punch and kick advanced modes
    CHECK(static_cast<int>(FIRST_ADVANCED_UNARMED_HIT_MODE) == static_cast<int>(HIT_MODE_STRONG_PUNCH));
    CHECK(static_cast<int>(LAST_ADVANCED_UNARMED_HIT_MODE) == static_cast<int>(HIT_MODE_PIERCING_KICK));

    // Verify punch modes are contiguous: 8-13
    CHECK(static_cast<int>(HIT_MODE_PIERCING_STRIKE) - static_cast<int>(HIT_MODE_STRONG_PUNCH) == 5);
    // Verify kick modes are contiguous: 14-19
    CHECK(static_cast<int>(HIT_MODE_PIERCING_KICK) - static_cast<int>(HIT_MODE_STRONG_KICK) == 5);
}

// =============================================================
// HitLocation Enum Tests
// =============================================================

TEST_CASE("HitLocation enum values are correct")
{
    CHECK(static_cast<int>(HIT_LOCATION_HEAD) == 0);
    CHECK(static_cast<int>(HIT_LOCATION_LEFT_ARM) == 1);
    CHECK(static_cast<int>(HIT_LOCATION_RIGHT_ARM) == 2);
    CHECK(static_cast<int>(HIT_LOCATION_TORSO) == 3);
    CHECK(static_cast<int>(HIT_LOCATION_RIGHT_LEG) == 4);
    CHECK(static_cast<int>(HIT_LOCATION_LEFT_LEG) == 5);
    CHECK(static_cast<int>(HIT_LOCATION_EYES) == 6);
    CHECK(static_cast<int>(HIT_LOCATION_GROIN) == 7);
    CHECK(static_cast<int>(HIT_LOCATION_UNCALLED) == 8);
    CHECK(static_cast<int>(HIT_LOCATION_COUNT) == 9);
    CHECK(static_cast<int>(HIT_LOCATION_SPECIFIC_COUNT) == 8); // COUNT minus uncalled
}

// =============================================================
// CombatState Enum Tests — de-anonymized enum
// =============================================================

TEST_CASE("CombatState enum values are correct")
{
    // The enum was de-anonymized: production uses COMBAT_STATE_0x01 for the
    // bitmask value 0x01, COMBAT_STATE_0x02 for 0x02, COMBAT_STATE_0x08 for 0x08.
    // See combat_defs.h:15-19 and combat_ai.cc:3194 reference.
    CHECK(static_cast<int>(COMBAT_STATE_0x01) == 0x01);
    CHECK(static_cast<int>(COMBAT_STATE_0x02) == 0x02);
    CHECK(static_cast<int>(COMBAT_STATE_0x08) == 0x08);

    // Verify the values are distinct and usable as bit flags
    int and01_02 = COMBAT_STATE_0x01 & COMBAT_STATE_0x02;
    CHECK(and01_02 == 0);
    int and02_08 = COMBAT_STATE_0x02 & COMBAT_STATE_0x08;
    CHECK(and02_08 == 0);
    int and01_08 = COMBAT_STATE_0x01 & COMBAT_STATE_0x08;
    CHECK(and01_08 == 0);
}

// =============================================================
// CombatBadShot Enum Tests
// =============================================================

TEST_CASE("CombatBadShot enum values are correct")
{
    CHECK(static_cast<int>(COMBAT_BAD_SHOT_OK) == 0);
    CHECK(static_cast<int>(COMBAT_BAD_SHOT_NO_AMMO) == 1);
    CHECK(static_cast<int>(COMBAT_BAD_SHOT_OUT_OF_RANGE) == 2);
    CHECK(static_cast<int>(COMBAT_BAD_SHOT_NOT_ENOUGH_AP) == 3);
    CHECK(static_cast<int>(COMBAT_BAD_SHOT_ALREADY_DEAD) == 4);
    CHECK(static_cast<int>(COMBAT_BAD_SHOT_AIM_BLOCKED) == 5);
    CHECK(static_cast<int>(COMBAT_BAD_SHOT_ARM_CRIPPLED) == 6);
    CHECK(static_cast<int>(COMBAT_BAD_SHOT_BOTH_ARMS_CRIPPLED) == 7);

    // Verify the ordering — these are sequential
    CHECK(COMBAT_BAD_SHOT_BOTH_ARMS_CRIPPLED == 7);
}

// =============================================================
// Hit Location Penalty Tests — pure data operations
// =============================================================

TEST_CASE("Hit location penalty defaults match expected values")
{
    memcpy(hit_location_penalty, hit_location_penalty_default, sizeof(hit_location_penalty));

    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_HEAD) == -40);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_LEFT_ARM) == -30);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_RIGHT_ARM) == -30);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_TORSO) == 0);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_RIGHT_LEG) == -20);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_LEFT_LEG) == -20);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_EYES) == -60);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_GROIN) == -30);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_UNCALLED) == 0);
}

TEST_CASE("Hit location penalty set and get round-trip")
{
    memcpy(hit_location_penalty, hit_location_penalty_default, sizeof(hit_location_penalty));

    stub_combat_set_hit_location_penalty(HIT_LOCATION_HEAD, -50);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_HEAD) == -50);

    stub_combat_set_hit_location_penalty(HIT_LOCATION_EYES, -80);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_EYES) == -80);

    stub_combat_set_hit_location_penalty(HIT_LOCATION_TORSO, 10);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_TORSO) == 10);

    // Other locations unchanged
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_GROIN) == -30);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_UNCALLED) == 0);
}

TEST_CASE("Hit location penalty out-of-bounds returns 0")
{
    memcpy(hit_location_penalty, hit_location_penalty_default, sizeof(hit_location_penalty));

    CHECK(stub_combat_get_hit_location_penalty(-1) == 0);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_COUNT) == 0);
    CHECK(stub_combat_get_hit_location_penalty(999) == 0);
}

TEST_CASE("Hit location penalty set with out-of-bounds is no-op")
{
    memcpy(hit_location_penalty, hit_location_penalty_default, sizeof(hit_location_penalty));

    // Setting out of bounds should not crash and not change anything
    stub_combat_set_hit_location_penalty(-1, 100);
    stub_combat_set_hit_location_penalty(HIT_LOCATION_COUNT, 100);
    stub_combat_set_hit_location_penalty(999, 100);

    // All values should still be defaults
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_HEAD) == -40);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_TORSO) == 0);
}

TEST_CASE("Hit location penalty reset restores default values")
{
    // Set all to custom values
    for (int i = 0; i < HIT_LOCATION_COUNT; i++) {
        stub_combat_set_hit_location_penalty(i, i * 10);
    }

    // Verify they changed
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_HEAD) == 0);

    // Reset
    stub_combat_reset_hit_location_penalty();

    // Verify all restored to defaults
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_HEAD) == -40);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_LEFT_ARM) == -30);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_RIGHT_ARM) == -30);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_TORSO) == 0);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_RIGHT_LEG) == -20);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_LEFT_LEG) == -20);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_EYES) == -60);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_GROIN) == -30);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_UNCALLED) == 0);
}

TEST_CASE("Hit location penalty set all locations")
{
    memcpy(hit_location_penalty, hit_location_penalty_default, sizeof(hit_location_penalty));

    // Set each location and verify
    stub_combat_set_hit_location_penalty(HIT_LOCATION_HEAD, -100);
    stub_combat_set_hit_location_penalty(HIT_LOCATION_LEFT_ARM, -90);
    stub_combat_set_hit_location_penalty(HIT_LOCATION_RIGHT_ARM, -80);
    stub_combat_set_hit_location_penalty(HIT_LOCATION_TORSO, 5);
    stub_combat_set_hit_location_penalty(HIT_LOCATION_RIGHT_LEG, -70);
    stub_combat_set_hit_location_penalty(HIT_LOCATION_LEFT_LEG, -60);
    stub_combat_set_hit_location_penalty(HIT_LOCATION_EYES, -110);
    stub_combat_set_hit_location_penalty(HIT_LOCATION_GROIN, -50);
    stub_combat_set_hit_location_penalty(HIT_LOCATION_UNCALLED, 10);

    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_HEAD) == -100);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_LEFT_ARM) == -90);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_RIGHT_ARM) == -80);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_TORSO) == 5);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_RIGHT_LEG) == -70);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_LEFT_LEG) == -60);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_EYES) == -110);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_GROIN) == -50);
    CHECK(stub_combat_get_hit_location_penalty(HIT_LOCATION_UNCALLED) == 10);
}

// =============================================================
// CriticalHitDescription Union Layout — using REAL types
// =============================================================

TEST_CASE("CriticalHitDescription union layout — named members alias values[]")
{
    CriticalHitDescription desc;
    memset(&desc, 0, sizeof(desc));

    desc.damageMultiplier = 4;
    CHECK(desc.values[CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER] == 4);

    desc.values[CRIT_DATA_MEMBER_FLAGS] = 0xDEAD;
    CHECK(desc.flags == 0xDEAD);

    desc.massiveCriticalStat = STAT_ENDURANCE;
    CHECK(desc.values[CRIT_DATA_MEMBER_MASSIVE_CRITICAL_STAT] == STAT_ENDURANCE);

    desc.massiveCriticalStatModifier = -3;
    CHECK(desc.values[CRIT_DATA_MEMBER_MASSIVE_CRITICAL_STAT_MODIFIER] == -3);

    desc.massiveCriticalFlags = DAM_KNOCKED_OUT;
    CHECK(desc.values[CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS] == DAM_KNOCKED_OUT);

    desc.messageId = 5001;
    CHECK(desc.values[CRIT_DATA_MEMBER_MESSAGE_ID] == 5001);

    desc.massiveCriticalMessageId = 5000;
    CHECK(desc.values[CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID] == 5000);
}

TEST_CASE("CriticalHitDescription size matches CRIT_DATA_MEMBER_COUNT")
{
    CHECK(CRIT_DATA_MEMBER_COUNT == 7);
    CHECK(sizeof(CriticalHitDescription) == CRIT_DATA_MEMBER_COUNT * sizeof(int));
}

TEST_CASE("CriticalHitDescription — all fields independently settable")
{
    CriticalHitDescription desc;
    memset(&desc, 0, sizeof(desc));

    desc.damageMultiplier = 3;
    desc.flags = 0xAA;
    desc.massiveCriticalStat = -1;
    desc.massiveCriticalStatModifier = 0;
    desc.massiveCriticalFlags = 0;
    desc.messageId = 1000;
    desc.massiveCriticalMessageId = 2000;

    CHECK(desc.damageMultiplier == 3);
    CHECK(desc.flags == 0xAA);
    CHECK(desc.massiveCriticalStat == -1);
    CHECK(desc.massiveCriticalStatModifier == 0);
    CHECK(desc.massiveCriticalFlags == 0);
    CHECK(desc.messageId == 1000);
    CHECK(desc.massiveCriticalMessageId == 2000);
}

// =============================================================
// Critical hit table data flow — using real dimension constants
// =============================================================

TEST_CASE("Critical hit table dimension constants")
{
    CHECK(KILL_TYPE_COUNT == 19);
    CHECK(SFALL_KILL_TYPE_COUNT == 38); // 2x KILL_TYPE_COUNT
    CHECK(HIT_LOCATION_COUNT == 9);
    CHECK(CRTICIAL_EFFECT_COUNT == 6);
    CHECK(CRIT_DATA_MEMBER_COUNT == 7);
}

TEST_CASE("stub_criticalsSetValue / stub_criticalsGetValue — real types")
{
    memset(gCriticalHitTables, 0, sizeof(gCriticalHitTables));
    memset(gPlayerCriticalHitTable, 0, sizeof(gPlayerCriticalHitTable));

    SUBCASE("set and get for normal kill type")
    {
        stub_criticalsSetValue(0, HIT_LOCATION_HEAD, 2,
            CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER, 4);
        CHECK(stub_criticalsGetValue(0, HIT_LOCATION_HEAD, 2,
            CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == 4);
    }

    SUBCASE("set and get for player kill type (SFALL_KILL_TYPE_COUNT sentinel)")
    {
        stub_criticalsSetValue(SFALL_KILL_TYPE_COUNT, HIT_LOCATION_TORSO, 3,
            CRIT_DATA_MEMBER_FLAGS, 0xBEEF);
        CHECK(stub_criticalsGetValue(SFALL_KILL_TYPE_COUNT, HIT_LOCATION_TORSO, 3,
            CRIT_DATA_MEMBER_FLAGS) == 0xBEEF);
    }

    SUBCASE("normal table and player table are independent")
    {
        stub_criticalsSetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 100);
        stub_criticalsSetValue(SFALL_KILL_TYPE_COUNT, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 200);

        CHECK(stub_criticalsGetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID) == 100);
        CHECK(stub_criticalsGetValue(SFALL_KILL_TYPE_COUNT, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID) == 200);
    }

    SUBCASE("independent kill types do not interfere")
    {
        stub_criticalsSetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 100);
        stub_criticalsSetValue(1, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 200);
        stub_criticalsSetValue(2, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 300);

        CHECK(stub_criticalsGetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID) == 100);
        CHECK(stub_criticalsGetValue(1, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID) == 200);
        CHECK(stub_criticalsGetValue(2, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID) == 300);
    }

    SUBCASE("independent hit locations do not interfere")
    {
        stub_criticalsSetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 10);
        stub_criticalsSetValue(0, HIT_LOCATION_TORSO, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 20);
        stub_criticalsSetValue(0, HIT_LOCATION_EYES, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 30);

        CHECK(stub_criticalsGetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID) == 10);
        CHECK(stub_criticalsGetValue(0, HIT_LOCATION_TORSO, 0, CRIT_DATA_MEMBER_MESSAGE_ID) == 20);
        CHECK(stub_criticalsGetValue(0, HIT_LOCATION_EYES, 0, CRIT_DATA_MEMBER_MESSAGE_ID) == 30);
    }

    SUBCASE("independent effects do not interfere")
    {
        for (int eff = 0; eff < CRTICIAL_EFFECT_COUNT; eff++) {
            stub_criticalsSetValue(0, HIT_LOCATION_HEAD, eff, CRIT_DATA_MEMBER_MESSAGE_ID, eff * 100);
        }
        for (int eff = 0; eff < CRTICIAL_EFFECT_COUNT; eff++) {
            CHECK(stub_criticalsGetValue(0, HIT_LOCATION_HEAD, eff, CRIT_DATA_MEMBER_MESSAGE_ID) == eff * 100);
        }
    }

    SUBCASE("set all data members on same entry")
    {
        stub_criticalsSetValue(0, HIT_LOCATION_HEAD, 2, CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER, 4);
        stub_criticalsSetValue(0, HIT_LOCATION_HEAD, 2, CRIT_DATA_MEMBER_FLAGS, DAM_BYPASS);
        stub_criticalsSetValue(0, HIT_LOCATION_HEAD, 2, CRIT_DATA_MEMBER_MESSAGE_ID, 5001);
        stub_criticalsSetValue(0, HIT_LOCATION_HEAD, 2, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_STAT, -1);

        CHECK(stub_criticalsGetValue(0, HIT_LOCATION_HEAD, 2, CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == 4);
        CHECK(stub_criticalsGetValue(0, HIT_LOCATION_HEAD, 2, CRIT_DATA_MEMBER_FLAGS) == DAM_BYPASS);
        CHECK(stub_criticalsGetValue(0, HIT_LOCATION_HEAD, 2, CRIT_DATA_MEMBER_MESSAGE_ID) == 5001);
        CHECK(stub_criticalsGetValue(0, HIT_LOCATION_HEAD, 2, CRIT_DATA_MEMBER_MASSIVE_CRITICAL_STAT) == -1);
    }
}

TEST_CASE("stub_criticalsResetValue — base-value restoration with real types")
{
    memset(gCriticalHitTables, 0, sizeof(gCriticalHitTables));
    memset(gPlayerCriticalHitTable, 0, sizeof(gPlayerCriticalHitTable));
    memset(gBaseCriticalHitTables, 0, sizeof(gBaseCriticalHitTables));
    memset(gBasePlayerCriticalHitTable, 0, sizeof(gBasePlayerCriticalHitTable));

    SUBCASE("reset restores base value for normal kill type")
    {
        int baseVal = 42;
        memcpy(&gBaseCriticalHitTables[0][HIT_LOCATION_HEAD][0].values[CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER],
               &baseVal, sizeof(baseVal));

        stub_criticalsSetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER, 99);
        CHECK(stub_criticalsGetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == 99);

        stub_criticalsResetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER);
        CHECK(stub_criticalsGetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == 42);
    }

    SUBCASE("reset restores base value for player table")
    {
        int baseVal = 77;
        memcpy(&gBasePlayerCriticalHitTable[HIT_LOCATION_TORSO][2].values[CRIT_DATA_MEMBER_FLAGS],
               &baseVal, sizeof(baseVal));

        stub_criticalsSetValue(SFALL_KILL_TYPE_COUNT, HIT_LOCATION_TORSO, 2, CRIT_DATA_MEMBER_FLAGS, 999);
        CHECK(stub_criticalsGetValue(SFALL_KILL_TYPE_COUNT, HIT_LOCATION_TORSO, 2, CRIT_DATA_MEMBER_FLAGS) == 999);

        stub_criticalsResetValue(SFALL_KILL_TYPE_COUNT, HIT_LOCATION_TORSO, 2, CRIT_DATA_MEMBER_FLAGS);
        CHECK(stub_criticalsGetValue(SFALL_KILL_TYPE_COUNT, HIT_LOCATION_TORSO, 2, CRIT_DATA_MEMBER_FLAGS) == 77);
    }

    SUBCASE("normal reset does not affect player table")
    {
        int baseNorm = 10;
        int basePlay = 88;
        memcpy(&gBaseCriticalHitTables[0][HIT_LOCATION_HEAD][0].values[CRIT_DATA_MEMBER_MESSAGE_ID], &baseNorm, sizeof(baseNorm));
        memcpy(&gBasePlayerCriticalHitTable[HIT_LOCATION_HEAD][0].values[CRIT_DATA_MEMBER_MESSAGE_ID], &basePlay, sizeof(basePlay));

        stub_criticalsSetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 999);
        stub_criticalsSetValue(SFALL_KILL_TYPE_COUNT, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID, 999);

        stub_criticalsResetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID);

        CHECK(stub_criticalsGetValue(0, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID) == 10);
        CHECK(stub_criticalsGetValue(SFALL_KILL_TYPE_COUNT, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID) == 999); // unchanged
    }
}

TEST_CASE("Critical hit table: all kill type × hit location × effect traversal")
{
    memset(gCriticalHitTables, 0, sizeof(gCriticalHitTables));
    memset(gPlayerCriticalHitTable, 0, sizeof(gPlayerCriticalHitTable));

    // Write unique values to every entry in the normal tables
    for (int kt = 0; kt < KILL_TYPE_COUNT; kt++) {
        for (int hl = 0; hl < HIT_LOCATION_COUNT; hl++) {
            for (int eff = 0; eff < CRTICIAL_EFFECT_COUNT; eff++) {
                int val = kt * 10000 + hl * 100 + eff;
                stub_criticalsSetValue(kt, hl, eff, CRIT_DATA_MEMBER_MESSAGE_ID, val);
            }
        }
    }

    // Spot-check a few entries
    CHECK(stub_criticalsGetValue(5, HIT_LOCATION_EYES, 3, CRIT_DATA_MEMBER_MESSAGE_ID) == 5 * 10000 + HIT_LOCATION_EYES * 100 + 3);
    CHECK(stub_criticalsGetValue(KILL_TYPE_MAN, HIT_LOCATION_HEAD, 0, CRIT_DATA_MEMBER_MESSAGE_ID) == 0); // kt=0, hl=0, eff=0
    CHECK(stub_criticalsGetValue(KILL_TYPE_BIG_BAD_BOSS, HIT_LOCATION_UNCALLED, 5, CRIT_DATA_MEMBER_MESSAGE_ID) == 18 * 10000 + 8 * 100 + 5);
}

// =============================================================
// isUnarmedHitMode — local mirror of combat.h:85-90 inline
// Duplicated here to avoid pulling in combat.h (which declares
// 50+ extern functions and includes db.h).
// =============================================================

static inline bool test_isUnarmedHitMode(int hitMode)
{
    return hitMode == HIT_MODE_PUNCH
        || hitMode == HIT_MODE_KICK
        || (hitMode >= FIRST_ADVANCED_UNARMED_HIT_MODE && hitMode <= LAST_ADVANCED_UNARMED_HIT_MODE);
}

// =============================================================
// isUnarmedHitMode inline function tests
// =============================================================

TEST_CASE("isUnarmedHitMode correctly identifies unarmed hit modes")
{
    // Weapon hit modes are NOT unarmed
    CHECK_FALSE(test_isUnarmedHitMode(HIT_MODE_LEFT_WEAPON_PRIMARY));
    CHECK_FALSE(test_isUnarmedHitMode(HIT_MODE_LEFT_WEAPON_SECONDARY));
    CHECK_FALSE(test_isUnarmedHitMode(HIT_MODE_RIGHT_WEAPON_PRIMARY));
    CHECK_FALSE(test_isUnarmedHitMode(HIT_MODE_RIGHT_WEAPON_SECONDARY));
    CHECK_FALSE(test_isUnarmedHitMode(HIT_MODE_LEFT_WEAPON_RELOAD));
    CHECK_FALSE(test_isUnarmedHitMode(HIT_MODE_RIGHT_WEAPON_RELOAD));

    // Basic unarmed hit modes ARE unarmed
    CHECK(test_isUnarmedHitMode(HIT_MODE_PUNCH));
    CHECK(test_isUnarmedHitMode(HIT_MODE_KICK));

    // Advanced unarmed punch hit modes (8-13) ARE unarmed
    CHECK(test_isUnarmedHitMode(HIT_MODE_STRONG_PUNCH));
    CHECK(test_isUnarmedHitMode(HIT_MODE_HAMMER_PUNCH));
    CHECK(test_isUnarmedHitMode(HIT_MODE_HAYMAKER));
    CHECK(test_isUnarmedHitMode(HIT_MODE_JAB));
    CHECK(test_isUnarmedHitMode(HIT_MODE_PALM_STRIKE));
    CHECK(test_isUnarmedHitMode(HIT_MODE_PIERCING_STRIKE));

    // Advanced unarmed kick hit modes (14-19) ARE unarmed
    CHECK(test_isUnarmedHitMode(HIT_MODE_STRONG_KICK));
    CHECK(test_isUnarmedHitMode(HIT_MODE_SNAP_KICK));
    CHECK(test_isUnarmedHitMode(HIT_MODE_POWER_KICK));
    CHECK(test_isUnarmedHitMode(HIT_MODE_HIP_KICK));
    CHECK(test_isUnarmedHitMode(HIT_MODE_HOOK_KICK));
    CHECK(test_isUnarmedHitMode(HIT_MODE_PIERCING_KICK));
}

// =============================================================
// gLastAttacker/gLastTarget global state management
// mirrors combat.cc:3587-3588 write site
// =============================================================

TEST_CASE("gLastAttacker/gLastTarget global state lifecycle")
{
    stub_resetLastTargets();
    CHECK(stub_getLastAttacker() == -1);
    CHECK(stub_getLastTarget() == -1);
}

TEST_CASE("gLastAttacker/gLastTarget track attack pairs")
{
    stub_resetLastTargets();

    stub_setLastAttacker(1001, 2001);
    CHECK(stub_getLastAttacker() == 1001);
    CHECK(stub_getLastTarget() == 2001);

    stub_setLastAttacker(1002, 2002);
    CHECK(stub_getLastAttacker() == 1002);
    CHECK(stub_getLastTarget() == 2002);

    // Last write wins (persistent globals)
    CHECK(stub_getLastAttacker() == 1002);
    CHECK(stub_getLastTarget() == 2002);

    stub_resetLastTargets();
    CHECK(stub_getLastAttacker() == -1);
    CHECK(stub_getLastTarget() == -1);
}

TEST_CASE("gLastAttacker/gLastTarget edge cases")
{
    stub_resetLastTargets();

    SUBCASE("set to zero is valid")
    {
        stub_setLastAttacker(0, 0);
        CHECK(stub_getLastAttacker() == 0);
        CHECK(stub_getLastTarget() == 0);
    }

    SUBCASE("set to negative values (invalid but not prevented by simple int write)")
    {
        // The production code writes attacker->id and defender->id which are
        // always >= 0 for valid objects. The read side checks >= 0 before
        // calling objectFindById. This test validates the raw int behavior.
        stub_setLastAttacker(-5, -5);
        CHECK(stub_getLastAttacker() == -5);
        CHECK(stub_getLastTarget() == -5);
    }

    SUBCASE("multiple overwrites track latest")
    {
        for (int i = 1; i <= 10; i++) {
            stub_setLastAttacker(i, i + 100);
        }
        CHECK(stub_getLastAttacker() == 10);
        CHECK(stub_getLastTarget() == 110);
    }
}

// =============================================================
// Attack struct layout and sizing
// =============================================================

TEST_CASE("Attack struct has expected field count")
{
    // Attack struct has: attacker, hitMode, weapon, attackHitLocation,
    // attackerDamage, attackerFlags, ammoQuantity, criticalMessageId,
    // defender, tile, defenderHitLocation, defenderDamage, defenderFlags,
    // defenderKnockback, intendedTarget, extrasLength,
    // extras[6], extrasHitLocation[6], extrasDamage[6],
    // extrasFlags[6], extrasKnockback[6]
    //
    // sizeof(Attack) should be reasonable — verify it's non-zero and sane
    CHECK(sizeof(Attack) > 0);
}

TEST_CASE("EXPLOSION_TARGET_COUNT is correct")
{
    CHECK(EXPLOSION_TARGET_COUNT == 6);
    // Verify the struct's embedded array matches
    CHECK(sizeof(Attack::extras) / sizeof(Attack::extras[0]) == 6);
}

TEST_CASE("CriticalHitDescriptionDataMember enum values")
{
    CHECK(static_cast<int>(CRIT_DATA_MEMBER_DAMAGE_MULTIPLIER) == 0);
    CHECK(static_cast<int>(CRIT_DATA_MEMBER_FLAGS) == 1);
    CHECK(static_cast<int>(CRIT_DATA_MEMBER_MASSIVE_CRITICAL_STAT) == 2);
    CHECK(static_cast<int>(CRIT_DATA_MEMBER_MASSIVE_CRITICAL_STAT_MODIFIER) == 3);
    CHECK(static_cast<int>(CRIT_DATA_MEMBER_MASSIVE_CRITICAL_FLAGS) == 4);
    CHECK(static_cast<int>(CRIT_DATA_MEMBER_MESSAGE_ID) == 5);
    CHECK(static_cast<int>(CRIT_DATA_MEMBER_MASSIVE_CRITICAL_MESSAGE_ID) == 6);
    CHECK(static_cast<int>(CRIT_DATA_MEMBER_COUNT) == 7);
}

// =============================================================
// CombatStartData struct test
// =============================================================

TEST_CASE("CombatStartData struct is usable")
{
    CombatStartData csd = {};
    csd.attacker = nullptr;
    csd.defender = nullptr;
    csd.actionPointsBonus = 5;
    csd.accuracyBonus = 10;
    csd.damageBonus = 3;
    csd.minDamage = 1;
    csd.maxDamage = 50;
    csd.overrideAttackResults = 0;
    csd.attackerResults = 0;
    csd.targetResults = 0;

    CHECK(csd.actionPointsBonus == 5);
    CHECK(csd.accuracyBonus == 10);
    CHECK(csd.damageBonus == 3);
    CHECK(csd.minDamage == 1);
    CHECK(csd.maxDamage == 50);
    CHECK(csd.overrideAttackResults == 0);
}

// =============================================================
// AI enum value tests (from combat_ai_defs.h)
// =============================================================

TEST_CASE("AreaAttackMode enum values")
{
    CHECK(static_cast<int>(AREA_ATTACK_MODE_ALWAYS) == 0);
    CHECK(static_cast<int>(AREA_ATTACK_MODE_SOMETIMES) == 1);
    CHECK(static_cast<int>(AREA_ATTACK_MODE_BE_SURE) == 2);
    CHECK(static_cast<int>(AREA_ATTACK_MODE_BE_CAREFUL) == 3);
    CHECK(static_cast<int>(AREA_ATTACK_MODE_BE_ABSOLUTELY_SURE) == 4);
    CHECK(static_cast<int>(AREA_ATTACK_MODE_COUNT) == 5);
}

TEST_CASE("RunAwayMode enum values")
{
    CHECK(static_cast<int>(RUN_AWAY_MODE_NONE) == 0);
    CHECK(static_cast<int>(RUN_AWAY_MODE_COWARD) == 1);
    CHECK(static_cast<int>(RUN_AWAY_MODE_FINGER_HURTS) == 2);
    CHECK(static_cast<int>(RUN_AWAY_MODE_BLEEDING) == 3);
    CHECK(static_cast<int>(RUN_AWAY_MODE_NOT_FEELING_GOOD) == 4);
    CHECK(static_cast<int>(RUN_AWAY_MODE_TOURNIQUET) == 5);
    CHECK(static_cast<int>(RUN_AWAY_MODE_NEVER) == 6);
    CHECK(static_cast<int>(RUN_AWAY_MODE_COUNT) == 7);
}

TEST_CASE("BestWeapon enum values")
{
    CHECK(static_cast<int>(BEST_WEAPON_NO_PREF) == 0);
    CHECK(static_cast<int>(BEST_WEAPON_MELEE) == 1);
    CHECK(static_cast<int>(BEST_WEAPON_MELEE_OVER_RANGED) == 2);
    CHECK(static_cast<int>(BEST_WEAPON_RANGED_OVER_MELEE) == 3);
    CHECK(static_cast<int>(BEST_WEAPON_RANGED) == 4);
    CHECK(static_cast<int>(BEST_WEAPON_UNARMED) == 5);
    CHECK(static_cast<int>(BEST_WEAPON_UNARMED_OVER_THROW) == 6);
    CHECK(static_cast<int>(BEST_WEAPON_RANDOM) == 7);
    CHECK(static_cast<int>(BEST_WEAPON_COUNT) == 8);
}

TEST_CASE("DistanceMode enum values")
{
    CHECK(static_cast<int>(DISTANCE_STAY_CLOSE) == 0);
    CHECK(static_cast<int>(DISTANCE_CHARGE) == 1);
    CHECK(static_cast<int>(DISTANCE_SNIPE) == 2);
    CHECK(static_cast<int>(DISTANCE_ON_YOUR_OWN) == 3);
    CHECK(static_cast<int>(DISTANCE_STAY) == 4);
    CHECK(static_cast<int>(DISTANCE_COUNT) == 5);
}

TEST_CASE("AttackWho enum values")
{
    CHECK(static_cast<int>(ATTACK_WHO_WHOMEVER_ATTACKING_ME) == 0);
    CHECK(static_cast<int>(ATTACK_WHO_STRONGEST) == 1);
    CHECK(static_cast<int>(ATTACK_WHO_WEAKEST) == 2);
    CHECK(static_cast<int>(ATTACK_WHO_WHOMEVER) == 3);
    CHECK(static_cast<int>(ATTACK_WHO_CLOSEST) == 4);
    CHECK(static_cast<int>(ATTACK_WHO_COUNT) == 5);
}

TEST_CASE("ChemUse enum values")
{
    CHECK(static_cast<int>(CHEM_USE_CLEAN) == 0);
    CHECK(static_cast<int>(CHEM_USE_STIMS_WHEN_HURT_LITTLE) == 1);
    CHECK(static_cast<int>(CHEM_USE_STIMS_WHEN_HURT_LOTS) == 2);
    CHECK(static_cast<int>(CHEM_USE_SOMETIMES) == 3);
    CHECK(static_cast<int>(CHEM_USE_ANYTIME) == 4);
    CHECK(static_cast<int>(CHEM_USE_ALWAYS) == 5);
    CHECK(static_cast<int>(CHEM_USE_COUNT) == 6);
}

TEST_CASE("Disposition enum values")
{
    CHECK(static_cast<int>(DISPOSITION_NONE) == 0);
    CHECK(static_cast<int>(DISPOSITION_CUSTOM) == 1);
    CHECK(static_cast<int>(DISPOSITION_COWARD) == 2);
    CHECK(static_cast<int>(DISPOSITION_DEFENSIVE) == 3);
    CHECK(static_cast<int>(DISPOSITION_AGGRESSIVE) == 4);
    CHECK(static_cast<int>(DISPOSITION_BERKSERK) == 5);
    CHECK(static_cast<int>(DISPOSITION_COUNT) == 6);
}

TEST_CASE("HurtTooMuch enum values")
{
    CHECK(static_cast<int>(HURT_BLIND) == 0);
    CHECK(static_cast<int>(HURT_CRIPPLED) == 1);
    CHECK(static_cast<int>(HURT_CRIPPLED_LEGS) == 2);
    CHECK(static_cast<int>(HURT_CRIPPLED_ARMS) == 3);
    CHECK(static_cast<int>(HURT_COUNT) == 4);
}
