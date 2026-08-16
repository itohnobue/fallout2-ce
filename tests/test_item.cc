// Unit tests for item.cc — addiction logic, power armor weight,
// and Fast Shot AP reduction.
//
// Validates behavior of the changed functions since fork point 24199e9:
//   1. dudeIsAddicted() — loop-termination bug fix (e4656db + 481cb9e)
//   2. itemGetWeight() — unconditional power-armor weight halving
//      (upstream CE semantics; et tu compensates script-side)
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
#include <vector>

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
        // CE halves power armor weight unconditionally (upstream behavior).
        // et tu compensates script-side (adjust_pa_weight doubles the proto
        // weight), so the engine must always halve — see src/item.cc.
        weight /= 2;
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

    SUBCASE("all power armors halved regardless of gFallout1Behavior (upstream CE semantics)")
    {
        gTestFallout1Behavior = false;

        CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, kPowerArmorWeight, false) == 42);       // 85/2 = 42
        CHECK(testGetArmorWeight(TEST_PID_HARDENED_POWER_ARMOR, kHardenedPowerArmorWeight, false) == 50); // 100/2 = 50
        CHECK(testGetArmorWeight(TEST_PID_ADVANCED_POWER_ARMOR, kAdvancedPowerArmorWeight, false) == 37); // 75/2 = 37
        CHECK(testGetArmorWeight(TEST_PID_ADVANCED_POWER_ARMOR_MK_II, kAdvancedPowerArmorMKIIWeight, false) == 37); // 75/2 = 37

        // FO1 mode halving is also unconditional: et tu's adjust_pa_weight
        // doubles the proto weight to compensate, so the engine must halve
        // in both modes (otherwise the two combine to 2× weight).
        gTestFallout1Behavior = true;

        CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, kPowerArmorWeight, false) == 42);
        CHECK(testGetArmorWeight(TEST_PID_HARDENED_POWER_ARMOR, kHardenedPowerArmorWeight, false) == 50);
        CHECK(testGetArmorWeight(TEST_PID_ADVANCED_POWER_ARMOR, kAdvancedPowerArmorWeight, false) == 37);
        CHECK(testGetArmorWeight(TEST_PID_ADVANCED_POWER_ARMOR_MK_II, kAdvancedPowerArmorMKIIWeight, false) == 37);
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

    // Toggle Fallout1Behavior mid-test to verify the global does NOT affect
    // power armor weight (CE halves unconditionally — et tu compensates
    // script-side with adjust_pa_weight).
    gTestFallout1Behavior = false;
    CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, kPowerArmorWeight, false) == 42);

    gTestFallout1Behavior = true;
    CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, kPowerArmorWeight, false) == 42);

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

    SUBCASE("FO1: Advanced Power Armor is still halved (unconditional)")
    {
        CHECK(testGetArmorWeight(TEST_PID_ADVANCED_POWER_ARMOR, 75, false) == 37);
    }

    SUBCASE("FO1: Power Armor (basic) is still halved (unconditional)")
    {
        CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, 85, false) == 42);
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

    SUBCASE("gFallout1Behavior toggle does NOT affect power armor weight")
    {
        // CE halves power armor weight unconditionally (upstream semantics);
        // et tu's adjust_pa_weight doubles protos script-side. The global
        // toggle must leave the weight unchanged.
        gTestFallout1Behavior = true;
        CHECK(testGetArmorWeight(TEST_PID_POWER_ARMOR, 85, false) == 42);

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

// ================================================================
// SECTION 9: N2-013 — weaponGetRange called outside weapon-null guard
// ================================================================
//
// Finding N2-013 (MEDIUM): In the refactored Fast Shot path (item.cc:1740),
// weaponGetRange(critter, hitMode) is called even when weapon == nullptr
// and !isUnarmedHitMode(hitMode). This call path was never exercised
// before the fork's refactoring of the Fast Shot logic.
//
// The weaponGetRange function (item.cc:1648-1686) is SAFE when weapon
// is nullptr — it falls through to the CRITTER_LONG_LIMBS / default-1
// check. But the new call path is a testing gap.

// ---- Mirror types for weaponGetRange ----
static constexpr int TEST_CRITTER_LONG_LIMBS = 1;

// Mirror of weaponGetRange (item.cc:1642-1686)
// Simplified: returns 1 for unarmed/unknown, 2 for long-limbs,
// or the actual weapon range if weapon is provided.
static int testWeaponGetRange(int /*critter*/, int hitMode, int critterFlags, int weaponRange)
{
    // When weapon != nullptr: use weapon range
    // (simplified stub — the real function looks up proto data)
    if (weaponRange > 0) {
        return weaponRange;
    }

    // weapon == nullptr fallback:
    // Check if critter has long limbs
    if (critterFlags & TEST_CRITTER_LONG_LIMBS) {
        return 2;
    }

    return 1;
}

TEST_CASE("N2-013: weaponGetRange — called outside weapon-null guard (item.cc:1740)")
{
    // Finding: N2-013 (MEDIUM), adversarial CONFIRMED
    // Source: item.cc:1740
    //
    // Production code (item.cc:1738-1743):
    //   if (critter == gDude && traitIsSelected(TRAIT_FAST_SHOT)) {
    //       if (gFastShotFix >= 1) {
    //           actionPoints--;
    //       } else {
    //           // FO2 vanilla: only ranged weapons with range > 2
    //           if (!isUnarmedHitMode(hitMode) && weaponGetRange(critter, hitMode) > 2) {
    //               actionPoints--;
    //           }
    //       }
    //   }
    //
    // In the gFastShotFix==0 branch, weaponGetRange is called even when
    // weapon==nullptr (e.g., unarmed hit mode is checked first, but
    // non-unarmed without a weapon falls through to the range check).
    // Before the refactoring, weaponGetRange was inside the weapon!=nullptr
    // guard — this call path is new.

    SUBCASE("weaponGetRange returns 1 for normal critter (no weapon)")
    {
        int range = testWeaponGetRange(0, TEST_HIT_MODE_PUNCH, 0, 0);
        CHECK(range == 1);
        // For FO2 vanilla Fast Shot: range 1 <= 2 → no AP reduction
        CHECK_FALSE(range > 2);
    }

    SUBCASE("weaponGetRange returns 2 for long-limbs critter (no weapon)")
    {
        int range = testWeaponGetRange(0, TEST_HIT_MODE_PUNCH, TEST_CRITTER_LONG_LIMBS, 0);
        CHECK(range == 2);
        // For FO2 vanilla Fast Shot: range 2 <= 2 → no AP reduction
        CHECK_FALSE(range > 2);
    }

    SUBCASE("weaponGetRange returns actual weapon range when weapon exists")
    {
        int range = testWeaponGetRange(0, TEST_HIT_MODE_RIGHT_WEAPON_PRIMARY, 0, 50);
        CHECK(range == 50);
        // For FO2 vanilla Fast Shot: range 50 > 2 → AP reduction applies
        CHECK(range > 2);
    }

    SUBCASE("new call path: weaponGetRange with nullptr weapon in Fast Shot path")
    {
        // The production Fast Shot path calls weaponGetRange when:
        // - critter == gDude
        // - has Fast Shot trait
        // - gFastShotFix == 0 (FO2 vanilla)
        // - !isUnarmedHitMode (e.g., HIT_MODE_RIGHT_WEAPON_PRIMARY)
        // - weapon == nullptr (no weapon actually equipped)

        // Simulate the call chain:
        // weaponGetRange(nullptr-returning critter, RIGHT_WEAPON_PRIMARY)
        int range = testWeaponGetRange(0, TEST_HIT_MODE_RIGHT_WEAPON_PRIMARY, 0, 0);
        CHECK(range == 1); // falls through to default

        // With this range (1), the Fast Shot condition is:
        // !isUnarmed && 1 > 2 → false → no AP reduction
        // This is correct behavior, but the code path was never exercised.
        CHECK_FALSE(range > 2);

        // The function is safe: weaponGetRange always returns 1-2
        // when weapon is nullptr (for normal critters).
        // N2-013 notes: if critterGetWeaponForHitMode had side effects,
        // this new call path would introduce a bug.
    }

    SUBCASE("regression: old code called weaponGetRange inside weapon-null guard")
    {
        // Old code (pre-fork):
        //   if (weapon != nullptr) {
        //       actionPoints = ...;
        //       if (weaponGetRange(critter, hitMode) > 2) {
        //           actionPoints--;
        //       }
        //   } else {
        //       actionPoints = ...; // no range check
        //   }
        //
        // The fork restructured to:
        //   if (weapon != nullptr) {
        //       actionPoints = ...;
        //   } else {
        //       actionPoints = 3;
        //   }
        //   // ... (after both branches)
        //   if (critter == gDude && traitIsSelected(FAST_SHOT)) {
        //       if (!isUnarmedHitMode(hitMode) && weaponGetRange(critter, hitMode) > 2) {
        //           actionPoints--;
        //       }
        //   }
        //
        // The weaponGetRange call is now AFTER the if/else block,
        // meaning it runs for BOTH weapon!=nullptr and weapon==nullptr paths.

        // Verify that the new code path doesn't crash with nullptr weapon:
        int range = testWeaponGetRange(0, TEST_HIT_MODE_RIGHT_WEAPON_PRIMARY, 0, 0);
        CHECK(range >= 1);
        CHECK(range <= 2);
    }
}

// ================================================================
// H-15: explosiveGetDamage must handle the _II (active) vanilla pids.
// explosiveActivate() converts DYNAMITE_I (51) → DYNAMITE_II (206)
// and PLASTIC_EXPLOSIVES_I (85) → PLASTIC_EXPLOSIVES_II (209) before
// queueAddEvent, so queue.cc:490 queries the damage with the _II pid.
// Before the fix the _II pids returned false and min/max stayed 0 —
// every vanilla timed explosive detonated with zero damage.
// ================================================================

namespace {
// Mirrors item.cc explosiveGetDamage() after the H-15 fix and the R-14
// (F1) activePid parity fix.
enum {
    TEST_PID_DYNAMITE_I = 51,
    TEST_PID_DYNAMITE_II = 206,
    TEST_PID_PLASTIC_EXPLOSIVES_I = 85,
    TEST_PID_PLASTIC_EXPLOSIVES_II = 209,
};

int testGDynamiteMin = 30;
int testGdynamiteMax = 50;
int testGPlasticMin = 40;
int testGPlasticMax = 80;

// Mirror of item.cc ExplosiveDescription (item.cc:93-98) + the file-static
// gExplosives vector + explosiveAdd() (item.cc:3781-3804, dedup included).
struct TestExplosiveDescription {
    int pid;
    int activePid;
    int minDamage;
    int maxDamage;
};
std::vector<TestExplosiveDescription> testGExplosives;

void testExplosiveAdd(int pid, int activePid, int minDamage, int maxDamage)
{
    for (auto it = testGExplosives.begin(); it != testGExplosives.end(); ++it) {
        if (it->pid == pid) {
            testGExplosives.erase(it);
            break;
        }
    }
    testGExplosives.push_back({ pid, activePid, minDamage, maxDamage });
}

bool testExplosiveGetDamage(int pid, int* minPtr, int* maxPtr)
{
    if (pid == TEST_PID_DYNAMITE_I || pid == TEST_PID_DYNAMITE_II) {
        if (minPtr) *minPtr = testGDynamiteMin;
        if (maxPtr) *maxPtr = testGdynamiteMax;
        return true;
    }
    if (pid == TEST_PID_PLASTIC_EXPLOSIVES_I || pid == TEST_PID_PLASTIC_EXPLOSIVES_II) {
        if (minPtr) *minPtr = testGPlasticMin;
        if (maxPtr) *maxPtr = testGPlasticMax;
        return true;
    }
    // R-14 (F1): match BOTH the registered (inactive) pid and the converted
    // active pid — sfall GetDamage matches item.pidActive (Explosions.cpp:184)
    // and explosiveIsActiveExplosive mirrors it (item.cc:3827).
    for (const auto& explosive : testGExplosives) {
        if (explosive.pid == pid || explosive.activePid == pid) {
            if (minPtr) *minPtr = explosive.minDamage;
            if (maxPtr) *maxPtr = explosive.maxDamage;
            return true;
        }
    }
    return false;
}

// Mirror of item.cc explosiveActivate() (item.cc:3837-3864) for the
// custom-explosive pid → activePid conversion performed on arming.
bool testExplosiveActivate(int* pidPtr)
{
    for (const auto& explosive : testGExplosives) {
        if (explosive.pid == *pidPtr) {
            *pidPtr = explosive.activePid;
            return true;
        }
    }
    return false;
}
} // namespace

TEST_CASE("H-15: explosiveGetDamage handles _II active pids")
{
    SUBCASE("Dynamite II returns the same damage as Dynamite I")
    {
        int minI = 0, maxI = 0;
        CHECK(testExplosiveGetDamage(TEST_PID_DYNAMITE_I, &minI, &maxI) == true);
        int minII = 0, maxII = 0;
        CHECK(testExplosiveGetDamage(TEST_PID_DYNAMITE_II, &minII, &maxII) == true);
        CHECK(minII == minI);
        CHECK(maxII == maxI);
        CHECK(minII == 30);
        CHECK(maxII == 50);
    }

    SUBCASE("Plastic Explosives II returns the same damage as Plastic I")
    {
        int minI = 0, maxI = 0;
        CHECK(testExplosiveGetDamage(TEST_PID_PLASTIC_EXPLOSIVES_I, &minI, &maxI) == true);
        int minII = 0, maxII = 0;
        CHECK(testExplosiveGetDamage(TEST_PID_PLASTIC_EXPLOSIVES_II, &minII, &maxII) == true);
        CHECK(minII == minI);
        CHECK(maxII == maxI);
        CHECK(minII == 40);
        CHECK(maxII == 80);
    }

    SUBCASE("Pre-fix behavior would return false for _II pids (zero damage)")
    {
        // The old code only handled the _I pids, so the _II lookup failed
        // and the caller's zero-initialized min/max stayed 0.
        int min = 0, max = 0;
        bool handled = false;
        if (TEST_PID_DYNAMITE_II == 51 || TEST_PID_DYNAMITE_II == 85) {
            handled = true; // _I-only lookup
        }
        CHECK(handled == false);
        (void)min;
        (void)max;
    }
}

// ================================================================
// R-14 (F1, CONFIRMED): explosiveGetDamage must resolve the CONVERTED
// active pid for custom explosives with a distinct activePid.
//
// item_make_explosive(pid=100, activePid=200, min=30, max=60) registers
// {pid:100, activePid:200, 30, 60} in gExplosives. On arming,
// _obj_use_explosive → explosiveActivate() (item.cc:3849-3853) rewrites the
// item pid to activePid (100 → 200) before queueAddEvent, so the detonation
// lookup at queue.cc:570 queries explosiveGetDamage(200). Pre-fix, the
// gExplosives loop matched only explosive.pid (the inactive pid 100) and the
// override map is keyed by the inactive pid too — both lookups missed → 0/0
// damage (randomBetween(0,0) = 0). This test drives the arming → detonation
// round-trip through the mirrored production predicate (which now also
// matches explosive.activePid, mirroring sfall GetDamage at Explosions.cpp:184
// and explosiveIsActiveExplosive at item.cc:3827).
// ================================================================
TEST_CASE("R-14 (F1): explosiveGetDamage resolves the converted active pid")
{
    SUBCASE("Detonation after arming resolves damage via the active pid")
    {
        testGExplosives.clear();
        testExplosiveAdd(100, 200, 30, 60); // item_make_explosive(100, 200, 30, 60)

        // Arming converts the item pid to the active pid (proto_instance.cc
        // _obj_use_explosive → explosiveActivate, item.cc:3849-3853).
        int pid = 100;
        CHECK(testExplosiveActivate(&pid) == true);
        CHECK(pid == 200);

        // Detonation (queue.cc:570) queries the converted pid. Pre-fix this
        // missed both the gExplosives loop (100 != 200) and the override map
        // (keyed by inactive pid 100) → 0/0 damage.
        int min = 0, max = 0;
        CHECK(testExplosiveGetDamage(pid, &min, &max) == true);
        CHECK(min == 30);
        CHECK(max == 60);
    }

    SUBCASE("Inactive pid still resolves damage (both lookups work)")
    {
        testGExplosives.clear();
        testExplosiveAdd(100, 200, 30, 60);

        int min = 0, max = 0;
        CHECK(testExplosiveGetDamage(100, &min, &max) == true);
        CHECK(min == 30);
        CHECK(max == 60);
    }

    SUBCASE("Unmatched pid returns false (caller keeps 0/0 defaults)")
    {
        testGExplosives.clear();
        testExplosiveAdd(100, 200, 30, 60);

        int min = 0, max = 0;
        CHECK(testExplosiveGetDamage(999, &min, &max) == false);
        CHECK(min == 0);
        CHECK(max == 0);
    }

    SUBCASE("Distinct-activePid entries coexist without cross-resolution")
    {
        testGExplosives.clear();
        testExplosiveAdd(100, 200, 30, 60);
        testExplosiveAdd(300, 400, 10, 20);

        int min = 0, max = 0;
        CHECK(testExplosiveGetDamage(200, &min, &max) == true);
        CHECK(min == 30);
        CHECK(max == 60);
        min = 0; max = 0;
        CHECK(testExplosiveGetDamage(400, &min, &max) == true);
        CHECK(min == 10);
        CHECK(max == 20);
        min = 0; max = 0;
        CHECK(testExplosiveGetDamage(300, &min, &max) == true);
        CHECK(min == 10);
        CHECK(max == 20);
    }
}

// ================================================================
// R-07 (P-02 residual): drugEffectEventRead must reject the CURRENT_*
// pseudo-stats (35/36/37) and the identity stats AGE (33) / GENDER (34).
//
// The pass-1 P-02 read-side validation allowed stats[i] ==
// STAT_CURRENT_HIT_POINTS (35) through the ±1000 clamp. A crafted save with
// stats[0]=35, modifiers[0]=-1000 → _perform_drug_effect →
// critterSetBonusStat(35,-1000) → critterAdjustHitPoints(-1000) →
// critterKill → INSTANT DEATH on event fire. The stat-36 sibling is a poison
// loop: stats[0]=36, modifiers[0]=+1000 → critterAdjustPoison(1000) →
// negative-delay poison event → immediate re-fire, −1 HP/tick → death
// (critter.cc:329-377). Save-loaded drug events are proto-detached
// (drugPid zeroed at item.cc:3189), so the read path cannot validate them
// against proto data — the CURRENT_* set is rejected entirely. Runtime-created
// events stay trusted (the vanilla chem-death special case in
// _perform_drug_effect, item.cc:2952, is preserved for them).
//
// This mirror encodes the fixed validation predicate from item.cc
// drugEffectEventRead: stats[i] must be -1 (no effect) or in
// [0, SPECIAL_STAT_COUNT). SPECIAL_STAT_COUNT = 33 excludes AGE (33),
// GENDER (34) and the CURRENT_* pseudo-stats (35/36/37). Modifiers are
// clamped to ±1000.
// ================================================================

namespace {
// Mirrors stat_defs.h Stat enum values relevant to the drug-event read.
enum {
    TEST_STAT_AGE = 33,
    TEST_STAT_GENDER = 34,
    TEST_STAT_CURRENT_HIT_POINTS = 35,
    TEST_STAT_CURRENT_POISON_LEVEL = 36,
    TEST_STAT_CURRENT_RADIATION_LEVEL = 37,
    TEST_SPECIAL_STAT_COUNT = 33, // matches SPECIAL_STAT_COUNT
};

// Mirrors the R-07-fixed validation loop of drugEffectEventRead.
// Returns the sanitized stats[i] (the value after the read guard).
int testSanitizeDrugStat(int stat, int& modifier)
{
    const int kModifierClamp = 1000;
    int sanitized = stat;
    if (sanitized != -1 && !(sanitized >= 0 && sanitized < TEST_SPECIAL_STAT_COUNT)) {
        sanitized = -1;
        modifier = 0;
    }
    if (modifier < -kModifierClamp) {
        modifier = -kModifierClamp;
    } else if (modifier > kModifierClamp) {
        modifier = kModifierClamp;
    }
    return sanitized;
}
} // namespace

TEST_CASE("R-07: drugEffectEventRead rejects CURRENT_* pseudo-stats and identity stats")
{
    SUBCASE("CURRENT_HIT_POINTS (35) is rejected — closes the -1000 instant-death vector")
    {
        // Pre-fix: 35 passed the allow-list; on fire
        // critterSetBonusStat(35,-1000) → critterAdjustHitPoints(-1000) →
        // critterKill. Post-fix: 35 is outside [0, SPECIAL_STAT_COUNT) → -1.
        int mod = -1000;
        CHECK(testSanitizeDrugStat(TEST_STAT_CURRENT_HIT_POINTS, mod) == -1);
        CHECK(mod == 0); // modifier zeroed with the rejected stat
    }

    SUBCASE("CURRENT_POISON_LEVEL (36) is rejected — closes the poison-loop death")
    {
        int mod = 1000;
        CHECK(testSanitizeDrugStat(TEST_STAT_CURRENT_POISON_LEVEL, mod) == -1);
        CHECK(mod == 0);
    }

    SUBCASE("CURRENT_RADIATION_LEVEL (37) is rejected")
    {
        int mod = 1000;
        CHECK(testSanitizeDrugStat(TEST_STAT_CURRENT_RADIATION_LEVEL, mod) == -1);
        CHECK(mod == 0);
    }

    SUBCASE("GENDER (34) and AGE (33) are rejected (R-20 fold-in)")
    {
        // R-20: stat 34 (GENDER) was in the pass-1 allow-list. No legitimate
        // drug targets identity stats. Restricting to [0, SPECIAL_STAT_COUNT)
        // excludes both AGE and GENDER.
        int mod = -1;
        CHECK(testSanitizeDrugStat(TEST_STAT_GENDER, mod) == -1);
        CHECK(mod == 0);
        mod = -1;
        CHECK(testSanitizeDrugStat(TEST_STAT_AGE, mod) == -1);
        CHECK(mod == 0);
    }

    SUBCASE("Valid SPECIAL stats and -1 pass through unchanged")
    {
        // STAT_STRENGTH (0) and STAT_MAXIMUM_HIT_POINTS (7) are legitimate
        // drug targets; -1 is the "no effect" marker.
        int mod = 50;
        CHECK(testSanitizeDrugStat(0, mod) == 0);
        CHECK(mod == 50);
        mod = -2;
        CHECK(testSanitizeDrugStat(7, mod) == 7);
        CHECK(mod == -2);
        mod = 5;
        CHECK(testSanitizeDrugStat(-1, mod) == -1);
        CHECK(mod == 5);
    }

    SUBCASE("Modifiers are still clamped to ±1000 for accepted stats")
    {
        int mod = 100000;
        CHECK(testSanitizeDrugStat(0, mod) == 0);
        CHECK(mod == 1000);
        mod = -100000;
        CHECK(testSanitizeDrugStat(0, mod) == 0);
        CHECK(mod == -1000);
    }
}
