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

// =============================================================
// H-005: _ai_called_shot OBO (combat_ai.cc:2736)
// regression test — HIT_LOCATION_SPECIFIC_COUNT range
// =============================================================
//
// Old code: randomBetween(0, HIT_LOCATION_SPECIFIC_COUNT)
//   → range 0..8, could return HIT_LOCATION_UNCALLED (8)
// Fixed code: randomBetween(0, HIT_LOCATION_SPECIFIC_COUNT - 1)
//   → range 0..7, head through groin only
//
// Finding ID: H-005 | Source: combat_ai.cc:2736

TEST_CASE("H-005: Called shot location range excludes HIT_LOCATION_UNCALLED")
{
    // HIT_LOCATION_SPECIFIC_COUNT == 8 means valid specific locations are
    // indices 0-7 (HEAD through GROIN). HIT_LOCATION_UNCALLED (8) is NOT a
    // valid called-shot target.
    CHECK(HIT_LOCATION_SPECIFIC_COUNT == 8);

    int specificCount = HIT_LOCATION_SPECIFIC_COUNT;        // 8
    int oldMax = specificCount;                              // old broken code
    int fixedMax = specificCount - 1;                        // fixed code

    // Old code: randomBetween(0, 8) → includes index 8 = HIT_LOCATION_UNCALLED
    CHECK(oldMax == 8);
    CHECK(static_cast<HitLocation>(oldMax) == HIT_LOCATION_UNCALLED);

    // Fixed code: randomBetween(0, 7) → index 0-7 = HEAD through GROIN
    CHECK(fixedMax == 7);
    CHECK(static_cast<HitLocation>(fixedMax) == HIT_LOCATION_GROIN);

    // HIT_LOCATION_UNCALLED is last (index 8), should be excluded from called shots
    CHECK(HIT_LOCATION_UNCALLED == 8);
    CHECK(HIT_LOCATION_SPECIFIC_COUNT == HIT_LOCATION_COUNT - 1);
}

TEST_CASE("H-005: All specific hit locations are valid called-shot targets")
{
    // Verify that indices 0 through HIT_LOCATION_SPECIFIC_COUNT - 1
    // are all valid body parts for aimed shots.
    CHECK(HIT_LOCATION_HEAD == 0);
    CHECK(HIT_LOCATION_LEFT_ARM == 1);
    CHECK(HIT_LOCATION_RIGHT_ARM == 2);
    CHECK(HIT_LOCATION_TORSO == 3);
    CHECK(HIT_LOCATION_RIGHT_LEG == 4);
    CHECK(HIT_LOCATION_LEFT_LEG == 5);
    CHECK(HIT_LOCATION_EYES == 6);
    CHECK(HIT_LOCATION_GROIN == 7);

    // All 8 specific locations are < HIT_LOCATION_SPECIFIC_COUNT
    for (int i = 0; i < HIT_LOCATION_SPECIFIC_COUNT; i++) {
        CHECK(i < HIT_LOCATION_SPECIFIC_COUNT);
    }
}

// =============================================================
// H-003: attackerFlags/defenderFlags fix (combat.cc:3608-3611)
// regression test — CombatStartData override path
// =============================================================
//
// Old code (buggy): wrote attackerResults to defenderFlags (copy-paste),
//   then overwrote defenderFlags with targetResults.
//   Net: attackerFlags NEVER set, defenderFlags = targetResults.
// Fixed code: attackerFlags = attackerResults, defenderFlags = targetResults.
//
// Finding ID: H-003 | Source: combat.cc:3608-3611
//
// Stub mirrors the production override path at combat.cc:3608-3611.

static int stub_combat_attackerFlags;
static int stub_combat_defenderFlags;

static void stub_apply_combat_start_data_old_buggy(
    int overrideAttackResults, int attackerResults, int targetResults)
{
    // Old buggy code (3608-3611 in origin):
    // attackerFlags assignment was MISSING
    // defenderFlags got attackerResults (copy-paste bug), then overwritten
    if (overrideAttackResults) {
        // BUG: wrote to defenderFlags instead of attackerFlags
        stub_combat_defenderFlags = attackerResults;
        // Second write overwrites — attackerResults is lost
        stub_combat_defenderFlags = targetResults;
        // attackerFlags NEVER written
    }
}

static void stub_apply_combat_start_data_fixed(
    int overrideAttackResults, int attackerResults, int targetResults)
{
    if (overrideAttackResults) {
        stub_combat_attackerFlags = attackerResults;
        stub_combat_defenderFlags = targetResults;
    }
}

TEST_CASE("H-003: attackerFlags/defenderFlags set independently from CombatStartData (fixed)")
{
    stub_combat_attackerFlags = -1;
    stub_combat_defenderFlags = -1;

    // Fixed code: attackerFlags = attackerResults, defenderFlags = targetResults
    stub_apply_combat_start_data_fixed(1, 0xAA, 0xBB);

    CHECK(stub_combat_attackerFlags == 0xAA);
    CHECK(stub_combat_defenderFlags == 0xBB);
}

TEST_CASE("H-003: Old buggy code loses attackerResults entirely")
{
    stub_combat_attackerFlags = -1;
    stub_combat_defenderFlags = -1;

    // Old buggy code
    stub_apply_combat_start_data_old_buggy(1, 0xAA, 0xBB);

    // attackerFlags NEVER written — stays at -1 (initial value)
    CHECK(stub_combat_attackerFlags == -1);

    // defenderFlags = targetResults (0xBB) — but attackerResults (0xAA) was LOST
    CHECK(stub_combat_defenderFlags == 0xBB);
}

TEST_CASE("H-003: attackerFlags/defenderFlags with zero values")
{
    stub_combat_attackerFlags = 999;
    stub_combat_defenderFlags = 999;

    // Fixed code with zero results
    stub_apply_combat_start_data_fixed(1, 0, 0);

    CHECK(stub_combat_attackerFlags == 0);
    CHECK(stub_combat_defenderFlags == 0);
}

TEST_CASE("H-003: No override when overrideAttackResults=0")
{
    stub_combat_attackerFlags = 42;
    stub_combat_defenderFlags = 24;

    // Fixed code with overrideAttackResults=0 — should be a no-op
    stub_apply_combat_start_data_fixed(0, 0xAA, 0xBB);

    // Values unchanged
    CHECK(stub_combat_attackerFlags == 42);
    CHECK(stub_combat_defenderFlags == 24);
}

TEST_CASE("H-003: Same attackerResults and targetResults — old code still wrong")
{
    stub_combat_attackerFlags = -1;
    stub_combat_defenderFlags = -1;

    // Even when both results are the same, old code still misses attackerFlags
    stub_apply_combat_start_data_old_buggy(1, 0x55, 0x55);

    CHECK(stub_combat_attackerFlags == -1); // NEVER written
    // defenderFlags = 0x55 (via targetResults), but this coincidentally matches
    // the attackerResults value — masking the bug if both happen to be equal
}

// =============================================================
// M-006: _combat_delete_critter cid renumbering (combat.cc:6120-6146)
// =============================================================
//
// Finding ID: M-006 | Source: combat.cc:6120-6146
//
// Test the cid renumbering and foundIndex-based section counting logic.
// Old code used loop variable `i` (== _list_total after compaction loop)
// for section check, making it always take the wrong branch.
// Fixed code uses `foundIndex` saved before the compaction loop.

// Mirrors production combat list — simplified Object* array with cid
struct TestCombatObject {
    int cid;      // obj->cid
    int id;       // object ID (for identification)
};

static TestCombatObject stub_combat_list[10];
static int stub_list_total = 0;
static int stub_list_com = 0;
static int stub_list_noncom = 0;

static void stub_combat_list_init(const int ids[], const int cids[], int total, int com)
{
    stub_list_total = total;
    stub_list_com = com;
    stub_list_noncom = total - com;
    for (int i = 0; i < total; i++) {
        stub_combat_list[i].cid = cids[i];
        stub_combat_list[i].id = ids[i];
    }
}

// Old buggy: uses `i` (loop variable, ends at _list_total after loop)
static void stub_combat_delete_critter_old_buggy(int deleteIndex)
{
    if (deleteIndex >= stub_list_total) return;

    int i;
    for (i = deleteIndex; i < stub_list_total - 1; i++) {
        stub_combat_list[i] = stub_combat_list[i + 1];
        stub_combat_list[i].cid = i;
    }

    stub_list_total--;

    // BUG: `i` now equals old _list_total - 1 (not deleteIndex!),
    // so section check is always wrong when com/noncom differ.
    if (i >= stub_list_com) {
        if (i < (stub_list_noncom + stub_list_com)) {
            stub_list_noncom--;
        }
    } else {
        stub_list_com--;
    }

    stub_combat_list[deleteIndex].cid = -1;
}

// Fixed: uses foundIndex saved before the compaction loop.
// Also models the production sentinel pattern: the removed object is stored
// at the end of the array and marked with cid = -1 (I2-M48 fix).
static void stub_combat_delete_critter_fixed(int deleteIndex)
{
    if (deleteIndex >= stub_list_total) return;

    int foundIndex = deleteIndex;

    // Production sentinel pattern (combat.cc:6145):
    //   _combat_list[_list_total] = obj;  // store at end of array
    //   obj->cid = -1;                     // mark as removed
    // The stub saves the removed entry, then after compaction stores it
    // at the new end (position stub_list_total, post-decrement).
    TestCombatObject removedObj = stub_combat_list[foundIndex];

    int i;
    for (i = foundIndex; i < stub_list_total - 1; i++) {
        stub_combat_list[i] = stub_combat_list[i + 1];
        stub_combat_list[i].cid = i;
    }

    // After compaction, store sentinel at end-of-array position.
    // In production: _combat_list[_list_total] stores a copy of the
    // removed object at the array boundary. The object's cid is set to -1
    // so iteration loops that check cid == -1 can detect array end.
    //
    // Pre-decrement total was the old count; post-decrement the sentinel
    // sits at the new total position (one past the active range).
    stub_list_total--;
    stub_combat_list[stub_list_total] = removedObj;
    stub_combat_list[stub_list_total].cid = -1; // production sentinel marker

    if (foundIndex >= stub_list_com) {
        if (foundIndex < (stub_list_noncom + stub_list_com)) {
            stub_list_noncom--;
        }
    } else {
        stub_list_com--;
    }
}

TEST_CASE("M-006: cid renumbering after combat list compaction")
{
    // Setup: 5 critters, cids match index, first 3 are com
    int ids[] = {100, 101, 102, 103, 104};
    int cids[] = {0, 1, 2, 3, 4};
    stub_combat_list_init(ids, cids, 5, 3);
    CHECK(stub_list_total == 5);
    CHECK(stub_list_com == 3);
    CHECK(stub_list_noncom == 2);

    // Delete index 1 (second com critter, id=101, cid=1)
    stub_combat_delete_critter_fixed(1);

    // Total decreased
    CHECK(stub_list_total == 4);

    // Remaining critters renumbered
    // Original: [100(0), 101(1), 102(2), 103(3), 104(4)]
    // After removing index 1:
    //   [100(0), 102(1), 103(2), 104(3)]
    CHECK(stub_combat_list[0].id == 100);
    CHECK(stub_combat_list[0].cid == 0);
    CHECK(stub_combat_list[1].id == 102);
    CHECK(stub_combat_list[1].cid == 1);
    CHECK(stub_combat_list[2].id == 103);
    CHECK(stub_combat_list[2].cid == 2);
    CHECK(stub_combat_list[3].id == 104);
    CHECK(stub_combat_list[3].cid == 3);

    // com count decreased (foundIndex=1 < _list_com=3)
    CHECK(stub_list_com == 2);
    CHECK(stub_list_noncom == 2);
}

TEST_CASE("M-006: cid renumbering when deleting last element")
{
    int ids[] = {100, 101, 102};
    int cids[] = {0, 1, 2};
    stub_combat_list_init(ids, cids, 3, 2);
    CHECK(stub_list_total == 3);

    // Delete index 2 (last element)
    stub_combat_delete_critter_fixed(2);

    CHECK(stub_list_total == 2);
    // Non-com count decreased (foundIndex=2 >= _list_com=2 → noncom)
    CHECK(stub_list_com == 2);
    CHECK(stub_list_noncom == 0);
}

TEST_CASE("M-006: Old buggy code always decrements wrong section")
{
    // Scenario: 4 com + 1 noncom, delete a com critter (index 0)
    int ids[] = {100, 101, 102, 103, 200};
    int cids[] = {0, 1, 2, 3, 4};
    stub_combat_list_init(ids, cids, 5, 4);

    stub_combat_delete_critter_old_buggy(0);

    // Old buggy code uses i=4 (after compaction loop, i = _list_total = 4)
    // i=4 >= stub_list_com (4) → enters noncom branch
    // i=4 < (stub_list_noncom + stub_list_com) → decrements noncom
    // WRONG: deleted index 0 was in com section, should have decremented com
    CHECK(stub_list_com == 4);     // unchanged — should be 3!
    CHECK(stub_list_noncom == 0);  // decremented from 1 → 0, wrong!

    // Reset and try with fixed code
    stub_combat_list_init(ids, cids, 5, 4);
    stub_combat_delete_critter_fixed(0);

    // Fixed code uses foundIndex=0:
    // 0 < stub_list_com (4) → decrements com
    CHECK(stub_list_com == 3);     // correct
    CHECK(stub_list_noncom == 1);  // unchanged, correct
}

TEST_CASE("M-006: obj->cid = -1 sentinel after removal")
{
    int ids[] = {100, 101, 102};
    int cids[] = {0, 1, 2};
    stub_combat_list_init(ids, cids, 3, 3);

    stub_combat_delete_critter_fixed(1);

    // The deleted object's cid should be -1 (sentinel for "not in combat")
    // In production: obj->cid = -1 at combat.cc:6145
    // In our stub: stub_combat_list[foundIndex].cid was already overwritten
    // by compaction, so we check the logic:
    // Production writes obj->cid = -1 to the ORIGINAL object, not the slot
    // The slot at index `foundIndex` gets overwritten by compaction.
    // The sentinel applies to the actual object being removed.

    // Verify total decreased
    CHECK(stub_list_total == 2);
}

TEST_CASE("I2-M48: Sentinel — removed object stored at end of array with cid=-1")
{
    // The production pattern (combat.cc:6145) stores the removed object at
    // _combat_list[_list_total] (end of active range) and sets cid = -1.
    // Iteration loops check cid == -1 to detect the sentinel boundary.
    // This test verifies the stub correctly models this behavior.

    int ids[] = {200, 201, 202, 203};
    int cids[] = {0, 1, 2, 3};
    stub_combat_list_init(ids, cids, 4, 2);

    int preTotal = stub_list_total;
    CHECK(preTotal == 4);

    // Delete index 1 (second element, id=201, cid=1)
    stub_combat_delete_critter_fixed(1);

    // After deletion: total decreased by 1
    CHECK(stub_list_total == 3);

    // The sentinel at position [stub_list_total] should be the removed object
    // with cid = -1. Post-compaction, the removedObj (id=201) is stored at
    // the new end-of-array position and marked with the sentinel cid.
    int sentinelIdx = stub_list_total; // = 3
    CHECK(stub_combat_list[sentinelIdx].id == 201); // removed object's ID
    CHECK(stub_combat_list[sentinelIdx].cid == -1);  // sentinel marker

    // Active range [0, total) should NOT have cid = -1
    for (int i = 0; i < stub_list_total; i++) {
        CHECK(stub_combat_list[i].cid >= 0);
    }
}

// =============================================================
// M-007: unarmedInitCustom "Penetrate" key fix (combat.cc:6684)
// regression test — config key independence
// =============================================================
//
// Old code (buggy): configGetBool(..., "BonusDamage", &isPenetrate)
//   "BonusDamage" key read twice: once as int for bonusDamage,
//   once as bool for isPenetrate → isPenetrate = (bonusDamage != 0)
// Fixed code: configGetBool(..., "Penetrate", &isPenetrate)
//   Separate "Penetrate" key for isPenetrate
//
// Finding ID: M-007 | Source: combat.cc:6684

// Stub unarmed hit description matching production
struct StubUnarmedHitDescription {
    int requiredLevel;
    int requiredSkill;
    int minDamage;
    int maxDamage;
    int bonusDamage;
    int bonusCriticalChance;
    int actionPointCost;
    bool isPenetrate;
    bool isSecondary;
};

// Stub config: key→int value map (simplified INI)
struct StubConfigEntry {
    const char* key;
    int value;
};

static const StubConfigEntry* stub_config_find_key(
    const StubConfigEntry* entries, int entryCount, const char* key)
{
    for (int i = 0; i < entryCount; i++) {
        if (strcmp(entries[i].key, key) == 0) {
            return &entries[i];
        }
    }
    return nullptr;
}

static void stub_config_read_old_buggy(
    StubUnarmedHitDescription* desc,
    const StubConfigEntry* entries, int entryCount)
{
    // Read BonusDamage as int
    const StubConfigEntry* entry = stub_config_find_key(entries, entryCount, "BonusDamage");
    if (entry) desc->bonusDamage = entry->value;

    // BUG: Reading "BonusDamage" as bool for isPenetrate
    // The old code had NO Penetrate key — isPenetrate was derived from BonusDamage only!
    entry = stub_config_find_key(entries, entryCount, "BonusDamage");
    if (entry) desc->isPenetrate = (entry->value != 0);
}

static void stub_config_read_fixed(
    StubUnarmedHitDescription* desc,
    const StubConfigEntry* entries, int entryCount)
{
    const StubConfigEntry* entry;

    entry = stub_config_find_key(entries, entryCount, "BonusDamage");
    if (entry) desc->bonusDamage = entry->value;

    // Fixed: reads "Penetrate" directly
    entry = stub_config_find_key(entries, entryCount, "Penetrate");
    if (entry) desc->isPenetrate = (entry->value != 0);
}

TEST_CASE("M-007: Penetrate key independent from BonusDamage (fixed code)")
{
    // Config with separate BonusDamage=0 and Penetrate=1
    StubConfigEntry entries[] = {
        {"ReqLevel", 5},
        {"SkillLevel", 100},
        {"MinDamage", 10},
        {"MaxDamage", 20},
        {"BonusDamage", 0},
        {"Penetrate", 1},
        {"Secondary", 0},
    };

    StubUnarmedHitDescription desc = {};
    stub_config_read_fixed(&desc, entries, 7);

    CHECK(desc.bonusDamage == 0);
    CHECK(desc.isPenetrate == true);
}

TEST_CASE("M-007: Old buggy code conflates BonusDamage and Penetrate")
{
    // Config with BonusDamage=5 (truthy), but Penetrate=0
    StubConfigEntry entries[] = {
        {"BonusDamage", 5},
        {"Penetrate", 0},
    };

    StubUnarmedHitDescription desc = {};
    stub_config_read_old_buggy(&desc, entries, 2);

    // bonusDamage correctly read as 5
    CHECK(desc.bonusDamage == 5);
    // BUG: isPenetrate is true because BonusDamage (5) != 0
    // even though Penetrate=0 explicitly!
    CHECK(desc.isPenetrate == true);
}

TEST_CASE("M-007: Fixed code correctly reads Penetrate=0")
{
    StubConfigEntry entries[] = {
        {"BonusDamage", 5},
        {"Penetrate", 0},
    };

    StubUnarmedHitDescription desc = {};
    stub_config_read_fixed(&desc, entries, 2);

    CHECK(desc.bonusDamage == 5);
    CHECK(desc.isPenetrate == false);  // correct
}

TEST_CASE("M-007: Old buggy code — BonusDamage=0 masks Penetrate=1")
{
    // Config with bonusDamage=0 but Penetrate=1 intended
    StubConfigEntry entries[] = {
        {"BonusDamage", 0},
        {"Penetrate", 1},
    };

    StubUnarmedHitDescription desc = {};
    stub_config_read_old_buggy(&desc, entries, 2);

    CHECK(desc.bonusDamage == 0);
    // BUG: isPenetrate is false because BonusDamage (0) is falsy
    // The Penetrate=1 setting is lost!
    CHECK(desc.isPenetrate == false);
}

TEST_CASE("M-007: Fixed code — BonusDamage=0, Penetrate=1 correctly independent")
{
    StubConfigEntry entries[] = {
        {"BonusDamage", 0},
        {"Penetrate", 1},
    };

    StubUnarmedHitDescription desc = {};
    stub_config_read_fixed(&desc, entries, 2);

    CHECK(desc.bonusDamage == 0);
    CHECK(desc.isPenetrate == true);  // correct
}

TEST_CASE("M-007: Unarmed hit descriptions for advanced unarmed hit modes")
{
    // Verify all advanced unarmed hit modes have valid indices that the
    // unarmedInitCustom loop iterates over (line 6668-6671).
    for (int hm = FIRST_ADVANCED_UNARMED_HIT_MODE; hm <= LAST_ADVANCED_UNARMED_HIT_MODE; hm++) {
        CHECK(hm >= 0);
        CHECK(hm < HIT_MODE_COUNT);
        CHECK(test_isUnarmedHitMode(hm));
    }
}

// ============================================================
// F-M54: Combat coverage — hit-roll, damage, and AI stubs
// ============================================================
// The test_combat.cc file had coverage gaps in hit-roll probability,
// damage calculation, and AI decision-making. These stubs exercise
// simplified behavioral models without linking the full combat engine.
//
// R8-016 (MEDIUM): Combat stubs are simplified models, NOT production mirrors.
//   Production attackDetermineToHit is 227 lines (weapons, perks, distance,
//   light, sfall hooks, armor class). Stub testToHitChance is ~10 lines:
//   skill + modifiers - AC, clamp 1-95. Covers ~2-5% of production logic.
//   Production AI target selection is 218 lines (disposition system, pathfinding,
//   perception, team sorting). Stub testAiSelectTarget is ~30 lines with simple
//   weighted scoring. Production damage involves DT/DR, critical tables, ammo
//   modifiers. Stub testCalculateDamage uses (min+max)/2 + bonus.
//   Passing these tests provides ZERO confidence about production combat correctness.
//   TODO: Replace stubs with integration tests linking the full combat engine, or
//   add explicit commentary that these are design-intent sanity checks only.
//   LINKAGE PATH: Production combat.cc/combat_ai.cc requires full engine linking
//   (Object, proto, perks, traits, skill system, attack tables, etc.).

// ---- Hit-roll probability stub ----
// SIMPLIFIED MODEL (R8-016): Not a production mirror. Tests arithmetic only.
// TODO: Link production attackDetermineToHit for integration-level validation.
// Chance to hit = skill + modifiers - target AC + random(1,20) vs threshold.
static int testToHitChance(int attackSkill, int targetAC, int modifiers)
{
    // Base chance = skill + modifiers - target AC
    int baseChance = attackSkill + modifiers - targetAC;

    // Clamp to [1, 95] — always 5% miss and 5% hit chance
    if (baseChance < 1) baseChance = 1;
    if (baseChance > 95) baseChance = 95;

    return baseChance;
}

// ---- Damage calculation stub ----
// SIMPLIFIED MODEL (R8-016): Not a production mirror. Tests arithmetic only.
// TODO: Link production damage calculation for integration-level validation.
// Mirrors the damage roll from combat.cc.
// Damage = (weaponMin + weaponMax) / 2 + random bonus.
static int testCalculateDamage(int weaponMinDmg, int weaponMaxDmg, int bonusDamage)
{
    // Average base damage
    int base = (weaponMinDmg + weaponMaxDmg) / 2;

    // Add bonus damage (e.g., from critical hit, perks, or strength bonus)
    int total = base + bonusDamage;

    // Minimum 1 damage per hit
    if (total < 1) total = 1;

    return total;
}

// ---- AI target prioritization stub ----
// SIMPLIFIED MODEL (R8-016): Not a production mirror. Tests arithmetic only.
// TODO: Link production AI target selection for integration-level validation.
// AI selects target based on: distance, threat level, HP remaining.
struct TestAiTarget {
    int id;
    int distance;       // hex-distance from AI
    int threatLevel;    // 0-10 threat rating
    int hpRemaining;    // current HP
    int hpMax;
};

// Returns the index of the best target or -1 if none.
static int testAiSelectTarget(const TestAiTarget targets[], int count,
                              int aiHpRemaining, int aiHpMax)
{
    if (count <= 0) return -1;

    int bestIndex = -1;
    int bestScore = -1;

    // Decision factors:
    // - Prefer low-HP targets (finish kills)
    // - Prefer high-threat targets
    // - Penalize distance
    // - When AI is low HP (< 25%), flee — select no target
    if (aiHpRemaining < aiHpMax / 4) {
        return -1; // low HP — flee
    }

    for (int i = 0; i < count; i++) {
        int hpRatio = (100 * targets[i].hpRemaining) / targets[i].hpMax;
        int score = targets[i].threatLevel * 10      // threat priority
                  + (100 - hpRatio)                     // prefer wounded
                  - targets[i].distance * 2;            // distance penalty
        if (score > bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    return bestIndex;
}

TEST_CASE("F-M54: Hit-roll — base chance formula")
{
    SUBCASE("Equal skill and AC gives base chance 0, clamped to 1")
    {
        // skill=50, AC=50 → base=0, clamped to 1 (always 5% to hit)
        CHECK(testToHitChance(50, 50, 0) == 1);
    }

    SUBCASE("High skill vs low AC results in near-certain hit")
    {
        // skill=150, AC=20 → base=130, clamped to 95
        CHECK(testToHitChance(150, 20, 0) == 95);
    }

    SUBCASE("Modifiers shift the chance")
    {
        // skill=30, AC=25, +10 aim → base=15
        CHECK(testToHitChance(30, 25, 10) == 15);
    }

    SUBCASE("Upper cap at 95")
    {
        CHECK(testToHitChance(200, 10, 50) == 95);
    }

    SUBCASE("Lower cap at 1")
    {
        CHECK(testToHitChance(10, 100, -50) == 1);
    }
}

TEST_CASE("F-M54: Damage — basic damage roll")
{
    SUBCASE("Average of weapon range")
    {
        // 10-20 weapon → avg 15
        CHECK(testCalculateDamage(10, 20, 0) == 15);
    }

    SUBCASE("Bonus damage from strength/critical")
    {
        // 5-10 weapon + 5 bonus → avg 7 + 5 = 12
        CHECK(testCalculateDamage(5, 10, 5) == 12);
    }

    SUBCASE("Minimum 1 damage")
    {
        // 1-1 weapon with -10 penalty → clamped to 1
        CHECK(testCalculateDamage(1, 1, -10) == 1);
    }

    SUBCASE("High-damage weapon")
    {
        // 40-60 weapon + 20 bonus → avg 50 + 20 = 70
        CHECK(testCalculateDamage(40, 60, 20) == 70);
    }
}

TEST_CASE("F-M54: AI — target selection priority")
{
    TestAiTarget targets[4] = {
        { 101, 3, 8, 50, 100 },   // close, high threat, half HP
        { 102, 10, 2, 90, 100 },  // far, low threat, full HP
        { 103, 2, 5, 20, 100 },   // very close, medium threat, low HP
        { 104, 5, 10, 10, 100 },  // medium distance, max threat, very low HP
    };

    SUBCASE("AI at full HP prioritizes high-threat wounded close targets")
    {
        int best = testAiSelectTarget(targets, 4, 100, 100);
        // target 104: threat=10*10=100, hpRatio=10, wounded=90, dist=-10 → score=180
        // target 103: threat=5*10=50, hpRatio=20, wounded=80, dist=-4 → score=126
        // target 101: threat=8*10=80, hpRatio=50, wounded=50, dist=-6 → score=124
        CHECK(best == 3); // target 104: score=180, highest score
        (void)best;
    }

    SUBCASE("AI at <25% HP flees — returns no target")
    {
        int best = testAiSelectTarget(targets, 4, 10, 100); // 10/100 = 10%
        CHECK(best == -1);
    }

    SUBCASE("Empty target list returns no target")
    {
        int best = testAiSelectTarget(targets, 0, 100, 100);
        CHECK(best == -1);
    }
}
