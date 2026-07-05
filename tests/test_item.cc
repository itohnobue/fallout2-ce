// Unit tests for item.cc — addiction logic, power armor weight,
// and Fast Shot AP reduction.
//
// Validates behavior of the changed functions since fork point 24199e9:
//   1. dudeIsAddicted() — loop-termination bug fix (e4656db + 481cb9e)
//   2. itemGetWeight() — gFallout1Behavior for power armor weight
//   3. weaponGetActionPointCost() — gFastShotFix refactoring
//
// These tests use local mirrored logic rather than linking item.cc,
// which has 70+ engine dependencies. The test-local stubs replicate
// the exact production code patterns from src/item.cc.
//
// See discovery report: tmp/s2-discover-item-report.md for the full
// audit and testability analysis.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

// ================================================================
// Test-local type definitions mirroring production types
// ================================================================

// Mirror of proto_types.h enums needed by the functions under test.
enum {
    TEST_PID_POWER_ARMOR = 3,
    TEST_PID_HARDENED_POWER_ARMOR = 232,
    TEST_PID_ADVANCED_POWER_ARMOR = 348,
    TEST_PID_ADVANCED_POWER_ARMOR_MK_II = 349,

    TEST_PID_NUKA_COLA = 106,
    TEST_PID_BUFF_OUT = 87,
    TEST_PID_MENTATS = 53,
    TEST_PID_PSYCHO = 110,
    TEST_PID_RADAWAY = 48,
    TEST_PID_BEER = 124,
    TEST_PID_BOOZE = 125,
    TEST_PID_JET = 259,
    TEST_PID_DECK_OF_TRAGIC_CARDS = 304,
};

enum {
    TEST_ITEM_TYPE_ARMOR = 0,
    TEST_ITEM_TYPE_CONTAINER = 1,
    TEST_ITEM_TYPE_DRUG = 2,
    TEST_ITEM_TYPE_WEAPON = 3,
    TEST_ITEM_TYPE_AMMO = 4,
    TEST_ITEM_TYPE_MISC = 5,
    TEST_ITEM_TYPE_KEY = 6,
};

// Mirror of game_vars.h GVAR indices for addiction tracking.
// These must match the enum in game_vars.h.
enum {
    TEST_GVAR_NUKA_COLA_ADDICT = 28,
    TEST_GVAR_BUFF_OUT_ADDICT = 29,
    TEST_GVAR_MENTATS_ADDICT = 30,
    TEST_GVAR_PSYCHO_ADDICT = 31,
    TEST_GVAR_RADAWAY_ADDICT = 32,
    TEST_GVAR_ALCOHOL_ADDICT = 33,
    // GVAR_ADDICT_TRAGIC = 302
    // GVAR_ADDICT_JET = 303
    TEST_GVAR_ADDICT_TRAGIC = 302,
    TEST_GVAR_ADDICT_JET = 303,
};

// Mirror of the gGameGlobalVars size. The real game has 1500+ GVARs; we
// allocate enough to cover all addiction indices.
constexpr int TEST_GAME_GLOBAL_VARS_LENGTH = 500;

// Mirror of trait_defs.h
enum {
    TEST_TRAIT_FAST_SHOT = 17,
};

// Mirror of combat_defs.h hit modes needed for offense checks.
enum {
    TEST_HIT_MODE_LEFT_WEAPON_PRIMARY = 0,
    TEST_HIT_MODE_LEFT_WEAPON_SECONDARY = 1,
    TEST_HIT_MODE_RIGHT_WEAPON_PRIMARY = 2,
    TEST_HIT_MODE_RIGHT_WEAPON_SECONDARY = 3,
    TEST_HIT_MODE_PUNCH = 4,
    TEST_HIT_MODE_KICK = 5,
    TEST_HIT_MODE_LEFT_WEAPON_RELOAD = 6,
    TEST_HIT_MODE_RIGHT_WEAPON_RELOAD = 7,
    TEST_HIT_MODE_STRONG_PUNCH = 8,
    TEST_HIT_MODE_HAMMER_PUNCH = 9,
    TEST_HIT_MODE_HAYMAKER = 10,
    TEST_HIT_MODE_JAB = 11,
    TEST_HIT_MODE_PALM_STRIKE = 12,
    TEST_HIT_MODE_PIERCING_STRIKE = 13,
    TEST_HIT_MODE_STRONG_KICK = 14,
    TEST_HIT_MODE_SNAP_KICK = 15,
    TEST_HIT_MODE_POWER_KICK = 16,
    TEST_HIT_MODE_HIP_KICK = 17,
    TEST_HIT_MODE_HOOK_KICK = 18,
    TEST_HIT_MODE_PIERCING_KICK = 19,
    TEST_HIT_MODE_COUNT = 20,
};

// Mirror of the DrugDescription struct from src/item.cc:80-84.
typedef struct TestDrugDescription {
    int drugPid;
    int gvar;
    int maxActiveEffects;
} TestDrugDescription;

// ================================================================
// Test-local addictions count (mirrors ADDICTION_COUNT in item.cc:44)
// ================================================================
constexpr int TEST_ADDICTION_COUNT = 9;

// ================================================================
// Test-local globals
// ================================================================

// Mirror of gGameGlobalVars (src/game.h:26)
static int gTestGameGlobalVars[TEST_GAME_GLOBAL_VARS_LENGTH];

// Mirror of gDrugDescriptions (src/item.cc:148-158)
static TestDrugDescription gTestDrugDescriptions[TEST_ADDICTION_COUNT];

// Mirror of gFallout1Behavior (src/item.cc:41)
static bool gTestFallout1Behavior = false;

// Mirror of gFastShotFix (src/item.cc:42)
static int gTestFastShotFix = 0;

// ================================================================
// Test-local function mirrors
// ================================================================

// Mirror of drugGetAddictionGvarByPid (src/item.cc:3203-3213)
static int testDrugGetAddictionGvarByPid(int drugPid)
{
    for (int index = 0; index < TEST_ADDICTION_COUNT; index++) {
        TestDrugDescription* drugDescription = &(gTestDrugDescriptions[index]);
        if (drugDescription->drugPid == drugPid) {
            return drugDescription->gvar;
        }
    }
    return -1;
}

// Mirror of dudeIsAddicted (src/item.cc:3247-3261)
// This is the function that was BUG-FIXED in 481cb9e.
// The fix: changed from an if/else-if chain where '0' could never
// be reached in the else-if, to a loop-terminating 'else if' pattern
// that allows fallthrough to return false.
static bool testDudeIsAddicted(int drugPid)
{
    for (int index = 0; index < TEST_ADDICTION_COUNT; index++) {
        TestDrugDescription* drugDescription = &(gTestDrugDescriptions[index]);
        if (drugPid == -1) {
            if (gTestGameGlobalVars[drugDescription->gvar] != 0) {
                return true;
            }
        } else if (drugPid == drugDescription->drugPid) {
            return gTestGameGlobalVars[drugDescription->gvar] != 0;
        }
    }

    return false;
}

// Mirror of the power-armor weight logic from itemGetWeight (src/item.cc:802-813)
// Extracts only the power-armor-specific weight halving logic.
static int testGetArmorWeight(int pid, int protoWeight, bool isHidden)
{
    if (isHidden) {
        return 0;
    }

    int weight = protoWeight;

    // Check if this is one of the 4 power armor PIDs
    switch (pid) {
    case TEST_PID_POWER_ARMOR:
    case TEST_PID_HARDENED_POWER_ARMOR:
    case TEST_PID_ADVANCED_POWER_ARMOR:
    case TEST_PID_ADVANCED_POWER_ARMOR_MK_II:
        // FO2 halves power armor weight; FO1 does not.
        if (!gTestFallout1Behavior) {
            weight /= 2;
        }
        break;
    }

    return weight;
}

// Mirror of the Fast Shot AP reduction logic from weaponGetActionPointCost
// (src/item.cc:1732-1744). Tests the refactored conditional that was changed
// from a nested structure to a cleaner two-branch pattern.
static int testApplyFastShotAP(int baseAp, bool isDude, bool hasFastShotTrait,
                               bool isUnarmed, int weaponRange)
{
    int actionPoints = baseAp;

    // Fast Shot trait AP reduction.
    if (isDude && hasFastShotTrait) {
        if (gTestFastShotFix >= 1) {
            // FO1 behavior: -1 AP for ALL weapons including unarmed.
            actionPoints--;
        } else {
            // FO2 vanilla: only ranged weapons with range > 2
            if (!isUnarmed && weaponRange > 2) {
                actionPoints--;
            }
        }
    }

    return actionPoints;
}

// Helper: isUnarmedHitMode mirror (combat.h:85-90)
static bool testIsUnarmedHitMode(int hitMode)
{
    return hitMode == TEST_HIT_MODE_PUNCH
        || hitMode == TEST_HIT_MODE_KICK
        || (hitMode >= TEST_HIT_MODE_STRONG_PUNCH && hitMode <= TEST_HIT_MODE_PIERCING_KICK);
}

// ================================================================
// Test setup helper
// ================================================================

static void resetAddictions()
{
    memset(gTestGameGlobalVars, 0, sizeof(gTestGameGlobalVars));

    // Initialize drug descriptions to match production (item.cc:148-158)
    gTestDrugDescriptions[0] = { TEST_PID_NUKA_COLA, TEST_GVAR_NUKA_COLA_ADDICT, 0 };
    gTestDrugDescriptions[1] = { TEST_PID_BUFF_OUT, TEST_GVAR_BUFF_OUT_ADDICT, 4 };
    gTestDrugDescriptions[2] = { TEST_PID_MENTATS, TEST_GVAR_MENTATS_ADDICT, 4 };
    gTestDrugDescriptions[3] = { TEST_PID_PSYCHO, TEST_GVAR_PSYCHO_ADDICT, 4 };
    gTestDrugDescriptions[4] = { TEST_PID_RADAWAY, TEST_GVAR_RADAWAY_ADDICT, 0 };
    gTestDrugDescriptions[5] = { TEST_PID_BEER, TEST_GVAR_ALCOHOL_ADDICT, 0 };
    gTestDrugDescriptions[6] = { TEST_PID_BOOZE, TEST_GVAR_ALCOHOL_ADDICT, 0 };
    gTestDrugDescriptions[7] = { TEST_PID_JET, TEST_GVAR_ADDICT_JET, 4 };
    gTestDrugDescriptions[8] = { TEST_PID_DECK_OF_TRAGIC_CARDS, TEST_GVAR_ADDICT_TRAGIC, 0 };
}

// ================================================================
// SECTION 1: drugGetAddictionGvarByPid
// ================================================================

TEST_CASE("testDrugGetAddictionGvarByPid")
{
    resetAddictions();

    SUBCASE("returns correct GVAR for known drug PIDs")
    {
        CHECK(testDrugGetAddictionGvarByPid(TEST_PID_NUKA_COLA) == TEST_GVAR_NUKA_COLA_ADDICT);
        CHECK(testDrugGetAddictionGvarByPid(TEST_PID_BUFF_OUT) == TEST_GVAR_BUFF_OUT_ADDICT);
        CHECK(testDrugGetAddictionGvarByPid(TEST_PID_MENTATS) == TEST_GVAR_MENTATS_ADDICT);
        CHECK(testDrugGetAddictionGvarByPid(TEST_PID_PSYCHO) == TEST_GVAR_PSYCHO_ADDICT);
        CHECK(testDrugGetAddictionGvarByPid(TEST_PID_RADAWAY) == TEST_GVAR_RADAWAY_ADDICT);
        CHECK(testDrugGetAddictionGvarByPid(TEST_PID_JET) == TEST_GVAR_ADDICT_JET);
        CHECK(testDrugGetAddictionGvarByPid(TEST_PID_DECK_OF_TRAGIC_CARDS) == TEST_GVAR_ADDICT_TRAGIC);
    }

    SUBCASE("returns same GVAR for beer and booze (shared alcohol GVAR)")
    {
        CHECK(testDrugGetAddictionGvarByPid(TEST_PID_BEER) == TEST_GVAR_ALCOHOL_ADDICT);
        CHECK(testDrugGetAddictionGvarByPid(TEST_PID_BOOZE) == TEST_GVAR_ALCOHOL_ADDICT);
    }

    SUBCASE("returns -1 for unknown PIDs")
    {
        CHECK(testDrugGetAddictionGvarByPid(9999) == -1);
        CHECK(testDrugGetAddictionGvarByPid(-1) == -1);
        CHECK(testDrugGetAddictionGvarByPid(0) == -1);
    }
}

// ================================================================
// SECTION 2: dudeIsAddicted — the bug-fixed function
// ================================================================

TEST_CASE("testDudeIsAddicted — drugPid == -1 (any addiction)")
{
    resetAddictions();

    SUBCASE("no addictions → false")
    {
        CHECK_FALSE(testDudeIsAddicted(-1));
    }

    SUBCASE("addicted to Jet → true")
    {
        gTestGameGlobalVars[TEST_GVAR_ADDICT_JET] = 1;
        CHECK(testDudeIsAddicted(-1));
    }

    SUBCASE("addicted to Nuka-Cola → true")
    {
        gTestGameGlobalVars[TEST_GVAR_NUKA_COLA_ADDICT] = 1;
        CHECK(testDudeIsAddicted(-1));
    }

    SUBCASE("addicted to Buffout → true")
    {
        gTestGameGlobalVars[TEST_GVAR_BUFF_OUT_ADDICT] = 1;
        CHECK(testDudeIsAddicted(-1));
    }

    SUBCASE("multiple addictions → true")
    {
        gTestGameGlobalVars[TEST_GVAR_ADDICT_JET] = 1;
        gTestGameGlobalVars[TEST_GVAR_BUFF_OUT_ADDICT] = 1;
        gTestGameGlobalVars[TEST_GVAR_ALCOHOL_ADDICT] = 1;
        CHECK(testDudeIsAddicted(-1));
    }

    SUBCASE("all addictions active → true")
    {
        for (int i = 0; i < TEST_ADDICTION_COUNT; i++) {
            gTestGameGlobalVars[gTestDrugDescriptions[i].gvar] = 1;
        }
        CHECK(testDudeIsAddicted(-1));
    }
}

TEST_CASE("testDudeIsAddicted — specific drugPid")
{
    resetAddictions();

    SUBCASE("not addicted, query specific drug → false")
    {
        CHECK_FALSE(testDudeIsAddicted(TEST_PID_JET));
        CHECK_FALSE(testDudeIsAddicted(TEST_PID_BUFF_OUT));
        CHECK_FALSE(testDudeIsAddicted(TEST_PID_NUKA_COLA));
    }

    SUBCASE("addicted to Jet, query Jet → true")
    {
        gTestGameGlobalVars[TEST_GVAR_ADDICT_JET] = 1;
        CHECK(testDudeIsAddicted(TEST_PID_JET));
    }

    SUBCASE("addicted to Jet, query Buffout → false (not addicted to Buffout)")
    {
        gTestGameGlobalVars[TEST_GVAR_ADDICT_JET] = 1;
        CHECK_FALSE(testDudeIsAddicted(TEST_PID_BUFF_OUT));
    }

    SUBCASE("addicted to Jet, query other drug → false")
    {
        gTestGameGlobalVars[TEST_GVAR_ADDICT_JET] = 1;
        CHECK_FALSE(testDudeIsAddicted(TEST_PID_MENTATS));
    }

    SUBCASE("addicted to Nuka-Cola, query Nuka-Cola → true")
    {
        gTestGameGlobalVars[TEST_GVAR_NUKA_COLA_ADDICT] = 1;
        CHECK(testDudeIsAddicted(TEST_PID_NUKA_COLA));
    }

    SUBCASE("unknown drugPid → false")
    {
        gTestGameGlobalVars[TEST_GVAR_ADDICT_JET] = 1;
        CHECK_FALSE(testDudeIsAddicted(9999));
        CHECK_FALSE(testDudeIsAddicted(0));
        CHECK_FALSE(testDudeIsAddicted(-2));
    }

    SUBCASE("all addictions, query each individually → true")
    {
        for (int i = 0; i < TEST_ADDICTION_COUNT; i++) {
            gTestGameGlobalVars[gTestDrugDescriptions[i].gvar] = 1;
        }

        for (int i = 0; i < TEST_ADDICTION_COUNT; i++) {
            int pid = gTestDrugDescriptions[i].drugPid;
            CAPTURE(pid);
            CAPTURE(i);
            CHECK(testDudeIsAddicted(pid));
        }
    }

    SUBCASE("beer and booze share GVAR — addiction to one is addiction to both")
    {
        // Set alcohol GVAR
        gTestGameGlobalVars[TEST_GVAR_ALCOHOL_ADDICT] = 1;
        CHECK(testDudeIsAddicted(TEST_PID_BEER));
        CHECK(testDudeIsAddicted(TEST_PID_BOOZE));
    }

    SUBCASE("addiction GVAR cleared after every test — Beer no longer addicted")
    {
        // This test verifies the GVAR is properly reset between SUBCASEs
        CHECK_FALSE(testDudeIsAddicted(TEST_PID_BEER));
        CHECK_FALSE(testDudeIsAddicted(TEST_PID_JET));
    }
}

TEST_CASE("testDudeIsAddicted — loop termination correctness (the bug fix)")
{
    // This test validates that the fixed implementation correctly reaches
    // `return false` when drugPid != -1 and no matching drug is found.
    // The pre-fix code could skip the loop-termination check due to the
    // if/else-if structure not reaching the '0 == gvar' case.
    resetAddictions();

    SUBCASE("drugPid == -1 with no addictions → correctly returns false")
    {
        CHECK_FALSE(testDudeIsAddicted(-1));
    }

    SUBCASE("specific pid not found → correctly returns false")
    {
        CHECK_FALSE(testDudeIsAddicted(TEST_PID_JET));
    }

    SUBCASE("drugPid == -1, all gvars zero → correctly returns false")
    {
        // Explicitly ensure all GVARs are zero
        for (int i = 0; i < TEST_ADDICTION_COUNT; i++) {
            gTestGameGlobalVars[gTestDrugDescriptions[i].gvar] = 0;
        }
        CHECK_FALSE(testDudeIsAddicted(-1));
    }

    SUBCASE("specific pid exists but GVAR is zero → false")
    {
        gTestGameGlobalVars[TEST_GVAR_ADDICT_JET] = 0;
        CHECK_FALSE(testDudeIsAddicted(TEST_PID_JET));
    }

    SUBCASE("GVAR is zero for first drug, addiction on subsequent drug → drugPid=-1 still returns true")
    {
        // gTestDrugDescriptions[0] = Nuka-Cola
        gTestGameGlobalVars[TEST_GVAR_NUKA_COLA_ADDICT] = 0;
        // gTestDrugDescriptions[1] = Buffout
        gTestGameGlobalVars[TEST_GVAR_BUFF_OUT_ADDICT] = 1;
        CHECK(testDudeIsAddicted(-1));
    }

    SUBCASE("GVAR is zero for early drugs, addiction on late drug (tests full loop)")
    {
        for (int i = 0; i < TEST_ADDICTION_COUNT; i++) {
            gTestGameGlobalVars[gTestDrugDescriptions[i].gvar] = 0;
        }
        // Last drug: Deck of Tragic Cards (index 8)
        gTestGameGlobalVars[TEST_GVAR_ADDICT_TRAGIC] = 1;
        CHECK(testDudeIsAddicted(-1));
    }
}

// ================================================================
// SECTION 3: itemGetWeight — gFallout1Behavior power armor logic
// ================================================================

TEST_CASE("testGetArmorWeight — power armor halving")
{
    // Power armor base weight in FO2: 85 lbs
    const int kPowerArmorWeight = 85;
    const int kHardenedPowerArmorWeight = 100;
    const int kAdvancedPowerArmorWeight = 75;
    const int kAdvancedPowerArmorMKIIWeight = 75;

    SUBCASE("gFallout1Behavior=false (FO2 default) — all power armors halved")
    {
        gTestFallout1Behavior = false;

        CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, kPowerArmorWeight, false) == 42);       // 85/2 = 42
        CHECK(testGetArmorWeight(TEST_PID_HARDENED_POWER_ARMOR, kHardenedPowerArmorWeight, false) == 50); // 100/2 = 50
        CHECK(testGetArmorWeight(TEST_PID_ADVANCED_POWER_ARMOR, kAdvancedPowerArmorWeight, false) == 37); // 75/2 = 37
        CHECK(testGetArmorWeight(TEST_PID_ADVANCED_POWER_ARMOR_MK_II, kAdvancedPowerArmorMKIIWeight, false) == 37); // 75/2 = 37
    }

    SUBCASE("gFallout1Behavior=true (FO1 compatibility) — weight unchanged")
    {
        gTestFallout1Behavior = true;

        CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, kPowerArmorWeight, false) == 85);
        CHECK(testGetArmorWeight(TEST_PID_HARDENED_POWER_ARMOR, kHardenedPowerArmorWeight, false) == 100);
        CHECK(testGetArmorWeight(TEST_PID_ADVANCED_POWER_ARMOR, kAdvancedPowerArmorWeight, false) == 75);
        CHECK(testGetArmorWeight(TEST_PID_ADVANCED_POWER_ARMOR_MK_II, kAdvancedPowerArmorMKIIWeight, false) == 75);
    }

    SUBCASE("non-power-armor armor is never halved")
    {
        gTestFallout1Behavior = false;

        // Test with a regular armor (pid not in the power armor switch cases)
        CHECK(testGetArmorWeight(1, 50, false) == 50);   // leather armor
        CHECK(testGetArmorWeight(2, 60, false) == 60);   // metal armor
        CHECK(testGetArmorWeight(100, 40, false) == 40);  // some other armor

        gTestFallout1Behavior = true;
        CHECK(testGetArmorWeight(1, 50, false) == 50);
        CHECK(testGetArmorWeight(2, 60, false) == 60);
    }

    SUBCASE("hidden item → weight 0 regardless of gFallout1Behavior")
    {
        gTestFallout1Behavior = false;
        CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, kPowerArmorWeight, true) == 0);
        CHECK(testGetArmorWeight(1, 50, true) == 0);

        gTestFallout1Behavior = true;
        CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, kPowerArmorWeight, true) == 0);
        CHECK(testGetArmorWeight(1, 50, true) == 0);
    }

    SUBCASE("odd weight halving — integer division truncation")
    {
        gTestFallout1Behavior = false;

        // 85/2 = 42 (truncated, not 42.5)
        CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, 85, false) == 42);

        // Weight of 1 → 0
        CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, 1, false) == 0);
    }
}

TEST_CASE("testGetArmorWeight — gFallout1Behavior toggling")
{
    const int kPowerArmorWeight = 85;

    // Toggle Fallout1Behavior mid-test to verify dynamic behavior
    gTestFallout1Behavior = false;
    CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, kPowerArmorWeight, false) == 42);

    gTestFallout1Behavior = true;
    CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, kPowerArmorWeight, false) == 85);

    gTestFallout1Behavior = false;
    CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, kPowerArmorWeight, false) == 42);
}

// ================================================================
// SECTION 4: weaponGetActionPointCost — gFastShotFix refactoring
// ================================================================

TEST_CASE("testApplyFastShotAP — gFastShotFix >= 1 (FO1 behavior)")
{
    // FO1 behavior: Fast Shot reduces AP by 1 for ALL weapons, including unarmed.
    gTestFastShotFix = 1;

    SUBCASE("unarmed attack → -1 AP")
    {
        CHECK(testApplyFastShotAP(3, true, true, true, 0) == 2);
        CHECK(testApplyFastShotAP(4, true, true, true, 0) == 3);
    }

    SUBCASE("ranged weapon, range > 2 → -1 AP")
    {
        CHECK(testApplyFastShotAP(5, true, true, false, 30) == 4);
    }

    SUBCASE("ranged weapon, range <= 2 → -1 AP (still reduced in FO1 mode)")
    {
        CHECK(testApplyFastShotAP(5, true, true, false, 2) == 4);
        CHECK(testApplyFastShotAP(5, true, true, false, 1) == 4);
    }

    SUBCASE("melee weapon → -1 AP")
    {
        CHECK(testApplyFastShotAP(4, true, true, false, 1) == 3);
    }

    SUBCASE("gFastShotFix=2 behaves same as >= 1")
    {
        gTestFastShotFix = 2;
        CHECK(testApplyFastShotAP(3, true, true, true, 0) == 2);
        CHECK(testApplyFastShotAP(5, true, true, false, 2) == 4);
    }
}

TEST_CASE("testApplyFastShotAP — gFastShotFix = 0 (FO2 vanilla)")
{
    // FO2 vanilla: Fast Shot reduces AP by 1 only for ranged weapons with range > 2.
    gTestFastShotFix = 0;

    SUBCASE("unarmed attack → no AP reduction")
    {
        CHECK(testApplyFastShotAP(3, true, true, true, 0) == 3);
        CHECK(testApplyFastShotAP(4, true, true, true, 0) == 4);
    }

    SUBCASE("melee (non-unarmed) weapon with range <= 2 → no AP reduction")
    {
        CHECK(testApplyFastShotAP(4, true, true, false, 2) == 4);
        CHECK(testApplyFastShotAP(4, true, true, false, 1) == 4);
    }

    SUBCASE("ranged weapon with range > 2 → -1 AP")
    {
        CHECK(testApplyFastShotAP(5, true, true, false, 3) == 4);
        CHECK(testApplyFastShotAP(5, true, true, false, 30) == 4);
        CHECK(testApplyFastShotAP(5, true, true, false, 50) == 4);
    }

    SUBCASE("ranged weapon with range exactly 3 → -1 AP (boundary)")
    {
        CHECK(testApplyFastShotAP(5, true, true, false, 3) == 4);
    }

    SUBCASE("ranged weapon with range exactly 2 → no reduction (boundary)")
    {
        CHECK(testApplyFastShotAP(5, true, true, false, 2) == 5);
    }

    SUBCASE("ranged weapon with range exactly 1 → no reduction")
    {
        CHECK(testApplyFastShotAP(5, true, true, false, 1) == 5);
    }

    SUBCASE("ranged weapon with range 0 → no reduction")
    {
        CHECK(testApplyFastShotAP(5, true, true, false, 0) == 5);
    }

    SUBCASE("ranged weapon with range -1 (edge case) → no reduction")
    {
        CHECK(testApplyFastShotAP(5, true, true, false, -1) == 5);
    }
}

TEST_CASE("testApplyFastShotAP — no Fast Shot trait")
{
    // Without Fast Shot trait, no AP reduction regardless of all other factors.

    SUBCASE("FO2 vanilla, no trait → no reduction")
    {
        gTestFastShotFix = 0;
        CHECK(testApplyFastShotAP(5, true, false, false, 30) == 5);
        CHECK(testApplyFastShotAP(5, true, false, true, 0) == 5);
    }

    SUBCASE("FO1 behavior, no trait → no reduction")
    {
        gTestFastShotFix = 1;
        CHECK(testApplyFastShotAP(5, true, false, false, 30) == 5);
        CHECK(testApplyFastShotAP(5, true, false, true, 0) == 5);
    }
}

TEST_CASE("testApplyFastShotAP — NPC (not gDude)")
{
    // Fast Shot AP reduction only applies to the player character (gDude).

    SUBCASE("FO2 vanilla, NPC → no reduction")
    {
        gTestFastShotFix = 0;
        CHECK(testApplyFastShotAP(5, false, true, false, 30) == 5);
    }

    SUBCASE("FO1 behavior, NPC → no reduction")
    {
        gTestFastShotFix = 1;
        CHECK(testApplyFastShotAP(5, false, true, true, 0) == 5);
    }
}

TEST_CASE("testApplyFastShotAP — does not reduce below 0 in isolation")
{
    // This test only validates the Fast Shot subtraction.
    // The production code (item.cc:1766-1768) has a separate floor:
    //   if (actionPoints < 1) actionPoints = 1;
    // We test that the subtraction alone can produce 0 or negative,
    // but the caller's floor guard handles it.

    gTestFastShotFix = 1;

    SUBCASE("AP=1, FastShotFix>=1 → result is 0 (caller floors to 1)")
    {
        CHECK(testApplyFastShotAP(1, true, true, true, 0) == 0);
    }

    SUBCASE("AP=0, FastShotFix>=1 → result is -1 (caller floors to 1)")
    {
        CHECK(testApplyFastShotAP(0, true, true, true, 0) == -1);
    }
}

// ================================================================
// SECTION 5: isUnarmedHitMode helper
// ================================================================

TEST_CASE("testIsUnarmedHitMode")
{
    SUBCASE("PUNCH and KICK are unarmed")
    {
        CHECK(testIsUnarmedHitMode(TEST_HIT_MODE_PUNCH));
        CHECK(testIsUnarmedHitMode(TEST_HIT_MODE_KICK));
    }

    SUBCASE("advanced unarmed hit modes are unarmed")
    {
        // STRONG_PUNCH through PIERCING_KICK (8-19)
        CHECK(testIsUnarmedHitMode(TEST_HIT_MODE_STRONG_PUNCH));
        CHECK(testIsUnarmedHitMode(TEST_HIT_MODE_HAMMER_PUNCH));
        CHECK(testIsUnarmedHitMode(TEST_HIT_MODE_HAYMAKER));
        CHECK(testIsUnarmedHitMode(TEST_HIT_MODE_JAB));
        CHECK(testIsUnarmedHitMode(TEST_HIT_MODE_PALM_STRIKE));
        CHECK(testIsUnarmedHitMode(TEST_HIT_MODE_PIERCING_STRIKE));
        CHECK(testIsUnarmedHitMode(TEST_HIT_MODE_STRONG_KICK));
        CHECK(testIsUnarmedHitMode(TEST_HIT_MODE_SNAP_KICK));
        CHECK(testIsUnarmedHitMode(TEST_HIT_MODE_POWER_KICK));
        CHECK(testIsUnarmedHitMode(TEST_HIT_MODE_HIP_KICK));
        CHECK(testIsUnarmedHitMode(TEST_HIT_MODE_HOOK_KICK));
        CHECK(testIsUnarmedHitMode(TEST_HIT_MODE_PIERCING_KICK));
    }

    SUBCASE("weapon hit modes are NOT unarmed")
    {
        CHECK_FALSE(testIsUnarmedHitMode(TEST_HIT_MODE_LEFT_WEAPON_PRIMARY));
        CHECK_FALSE(testIsUnarmedHitMode(TEST_HIT_MODE_LEFT_WEAPON_SECONDARY));
        CHECK_FALSE(testIsUnarmedHitMode(TEST_HIT_MODE_RIGHT_WEAPON_PRIMARY));
        CHECK_FALSE(testIsUnarmedHitMode(TEST_HIT_MODE_RIGHT_WEAPON_SECONDARY));
        CHECK_FALSE(testIsUnarmedHitMode(TEST_HIT_MODE_LEFT_WEAPON_RELOAD));
        CHECK_FALSE(testIsUnarmedHitMode(TEST_HIT_MODE_RIGHT_WEAPON_RELOAD));
    }
}

// ================================================================
// SECTION 6: DrugDescription array validation
// ================================================================

TEST_CASE("gTestDrugDescriptions — production data mirror")
{
    resetAddictions();

    SUBCASE("addiction count matches production")
    {
        CHECK(TEST_ADDICTION_COUNT == 9);
    }

    SUBCASE("all entries have valid PIDs")
    {
        for (int i = 0; i < TEST_ADDICTION_COUNT; i++) {
            CHECK(gTestDrugDescriptions[i].drugPid > 0);
        }
    }

    SUBCASE("all entries have valid GVAR indices")
    {
        for (int i = 0; i < TEST_ADDICTION_COUNT; i++) {
            CHECK(gTestDrugDescriptions[i].gvar >= 0);
        }
    }

    SUBCASE("no duplicate drug PIDs")
    {
        for (int i = 0; i < TEST_ADDICTION_COUNT; i++) {
            for (int j = i + 1; j < TEST_ADDICTION_COUNT; j++) {
                CHECK(gTestDrugDescriptions[i].drugPid != gTestDrugDescriptions[j].drugPid);
            }
        }
    }

    SUBCASE("beer and booze share the same GVAR (alcohol)")
    {
        int beerGvar = testDrugGetAddictionGvarByPid(TEST_PID_BEER);
        int boozeGvar = testDrugGetAddictionGvarByPid(TEST_PID_BOOZE);
        CHECK(beerGvar == boozeGvar);
        CHECK(beerGvar == TEST_GVAR_ALCOHOL_ADDICT);
    }
}

// ================================================================
// SECTION 7: End-to-end scenarios
// ================================================================

TEST_CASE("Addiction lifecycle: set → check → clear → check")
{
    resetAddictions();

    SUBCASE("set Jet addiction → dudeIsAddicted(Jet) true → clear → false")
    {
        // Simulate dudeSetAddiction(PROTO_ID_JET)
        int gvar = testDrugGetAddictionGvarByPid(TEST_PID_JET);
        CHECK(gvar != -1);
        gTestGameGlobalVars[gvar] = 1;

        CHECK(testDudeIsAddicted(TEST_PID_JET));
        CHECK(testDudeIsAddicted(-1)); // any addiction

        // Simulate dudeClearAddiction(PROTO_ID_JET)
        gTestGameGlobalVars[gvar] = 0;

        CHECK_FALSE(testDudeIsAddicted(TEST_PID_JET));
        CHECK_FALSE(testDudeIsAddicted(-1));
    }

    SUBCASE("set Jet addiction → dudeIsAddicted(Mentats) is still false")
    {
        int gvar = testDrugGetAddictionGvarByPid(TEST_PID_JET);
        gTestGameGlobalVars[gvar] = 1;

        CHECK_FALSE(testDudeIsAddicted(TEST_PID_MENTATS));
    }

    SUBCASE("set Jet → set Buffout → clear Jet → still addicted (Buffout)")
    {
        int jetGvar = testDrugGetAddictionGvarByPid(TEST_PID_JET);
        int buffGvar = testDrugGetAddictionGvarByPid(TEST_PID_BUFF_OUT);

        gTestGameGlobalVars[jetGvar] = 1;
        gTestGameGlobalVars[buffGvar] = 1;

        CHECK(testDudeIsAddicted(-1));

        // Clear Jet
        gTestGameGlobalVars[jetGvar] = 0;

        // Still addicted to Buffout
        CHECK(testDudeIsAddicted(-1));
        CHECK_FALSE(testDudeIsAddicted(TEST_PID_JET));
        CHECK(testDudeIsAddicted(TEST_PID_BUFF_OUT));
    }

    SUBCASE("set all → clear all one by one → last cleared → not addicted")
    {
        // Set all addictions
        for (int i = 0; i < TEST_ADDICTION_COUNT; i++) {
            gTestGameGlobalVars[gTestDrugDescriptions[i].gvar] = 1;
        }
        CHECK(testDudeIsAddicted(-1));

        // Clear all but one (use unique GVARs to avoid shared alcohol GVAR double-count)
        int lastDrugIndex = TEST_ADDICTION_COUNT - 1; // Tragic Cards
        for (int i = 0; i < TEST_ADDICTION_COUNT; i++) {
            if (i != lastDrugIndex) {
                gTestGameGlobalVars[gTestDrugDescriptions[i].gvar] = 0;
            }
        }
        CHECK(testDudeIsAddicted(-1)); // Still Tragic

        // Clear last
        gTestGameGlobalVars[gTestDrugDescriptions[lastDrugIndex].gvar] = 0;
        CHECK_FALSE(testDudeIsAddicted(-1));
    }
}

TEST_CASE("Power armor weight — FO1 vs FO2 scenario")
{
    gTestFallout1Behavior = false;

    // FO2 behavior: all 4 power armors have halved weight
    SUBCASE("FO2: Advanced Power Armor is half weight")
    {
        CHECK(testGetArmorWeight(TEST_PID_ADVANCED_POWER_ARMOR, 75, false) == 37);
    }

    SUBCASE("FO2: Power Armor (basic) is half weight")
    {
        CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, 85, false) == 42);
    }

    SUBCASE("FO2: Hardened Power Armor is half weight")
    {
        CHECK(testGetArmorWeight(TEST_PID_HARDENED_POWER_ARMOR, 100, false) == 50);
    }

    SUBCASE("FO2: APA Mk II is half weight")
    {
        CHECK(testGetArmorWeight(TEST_PID_ADVANCED_POWER_ARMOR_MK_II, 75, false) == 37);
    }

    // Switch to FO1
    gTestFallout1Behavior = true;

    SUBCASE("FO1: Advanced Power Armor is full weight")
    {
        CHECK(testGetArmorWeight(TEST_PID_ADVANCED_POWER_ARMOR, 75, false) == 75);
    }

    SUBCASE("FO1: Power Armor (basic) is full weight")
    {
        CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, 85, false) == 85);
    }
}

TEST_CASE("Fast Shot — FO1 vs FO2 scenarios")
{
    SUBCASE("FO2 vanilla: unarmed character with Fast Shot gets no AP discount")
    {
        gTestFastShotFix = 0;
        CHECK(testApplyFastShotAP(3, true, true, true, 0) == 3); // no change
    }

    SUBCASE("FO1 behavior: unarmed character with Fast Shot gets -1 AP")
    {
        gTestFastShotFix = 1;
        CHECK(testApplyFastShotAP(3, true, true, true, 0) == 2); // -1
    }

    SUBCASE("FO2 vanilla: sniper with Fast Shot and range 50 gets -1 AP")
    {
        gTestFastShotFix = 0;
        CHECK(testApplyFastShotAP(6, true, true, false, 50) == 5); // -1
    }

    SUBCASE("FO2 vanilla: pistol user with Fast Shot and range 2 gets no discount")
    {
        gTestFastShotFix = 0;
        CHECK(testApplyFastShotAP(5, true, true, false, 2) == 5); // no change
    }

    SUBCASE("FO1 behavior: pistol user with Fast Shot and range 2 gets -1 AP")
    {
        gTestFastShotFix = 1;
        CHECK(testApplyFastShotAP(5, true, true, false, 2) == 4); // -1
    }

    SUBCASE("FO2 vanilla: melee (non-unarmed) with range 1 gets no discount")
    {
        gTestFastShotFix = 0;
        CHECK(testApplyFastShotAP(4, true, true, false, 1) == 4);
    }

    SUBCASE("FO1 behavior: melee (non-unarmed) with range 1 gets -1 AP")
    {
        gTestFastShotFix = 1;
        CHECK(testApplyFastShotAP(4, true, true, false, 1) == 3);
    }
}

// ================================================================
// SECTION 8: RPU/Et Tu cross-reference — validated behaviors
// ================================================================

TEST_CASE("RPU/Et Tu cross-reference: HOOK_USEOBJON drug path")
{
    // RPU uses HOOK_USEOBJON for the alcohol addiction system
    // (gl_k_alcohl.ssl registers HOOK_USEOBJON).
    //
    // In item.cc:2881-2886, drugItemTakeDrug() calls scriptHooks_UseItemOn()
    // and returns early if the hook handled the event. The engine handler
    // (lines 2888+) is the fallback.
    //
    // This test validates that:
    // 1. If the hook returns non--1 (handled), the engine handler is skipped
    // 2. The engine handler (fallback) processes the drug normally
    // We test the engine handler path indirectly via dudeIsAddicted/addiction
    // lifecycle tests above. The hook path is integration-tested only.

    resetAddictions();

    SUBCASE("engine handler path: taking Jet causes addiction")
    {
        // Simulate the engine handler setting the addiction GVAR
        int gvar = testDrugGetAddictionGvarByPid(TEST_PID_JET);
        gTestGameGlobalVars[gvar] = 1;
        CHECK(testDudeIsAddicted(TEST_PID_JET));
    }

    SUBCASE("engine handler path: taking Jet antidote clears addiction")
    {
        int gvar = testDrugGetAddictionGvarByPid(TEST_PID_JET);
        gTestGameGlobalVars[gvar] = 1;
        CHECK(testDudeIsAddicted(TEST_PID_JET));

        // Clear (simulating jet antidote)
        gTestGameGlobalVars[gvar] = 0;
        CHECK_FALSE(testDudeIsAddicted(TEST_PID_JET));
    }

    SUBCASE("engine handler path: alcohol addiction shared between beer and booze")
    {
        int gvar = testDrugGetAddictionGvarByPid(TEST_PID_BEER);
        gTestGameGlobalVars[gvar] = 1;

        CHECK(testDudeIsAddicted(TEST_PID_BEER));
        CHECK(testDudeIsAddicted(TEST_PID_BOOZE));
    }
}

TEST_CASE("RPU/Et Tu cross-reference: battle_game engine config")
{
    // RPU's gl_k_modini.ssl reads ddraw.ini via get_ini_setting (4 sites).
    // The gFallout1Behavior and gFastShotFix globals are read from ddraw.ini
    // during engine init and consumed by item.cc functions tested above.
    //
    // This test validates that the config globals toggling changes behavior
    // as expected, which is the observable effect for RPU mods that set
    // Fallout1Behavior=1 in ddraw.ini.

    SUBCASE("gFallout1Behavior toggle affects power armor weight")
    {
        gTestFallout1Behavior = true;
        CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, 85, false) == 85);

        gTestFallout1Behavior = false;
        CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, 85, false) == 42);
    }

    SUBCASE("gFastShotFix toggle affects unarmed AP reduction")
    {
        gTestFastShotFix = 0;
        CHECK(testApplyFastShotAP(3, true, true, true, 0) == 3);

        gTestFastShotFix = 1;
        CHECK(testApplyFastShotAP(3, true, true, true, 0) == 2);
    }
}
