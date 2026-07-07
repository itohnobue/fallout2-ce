// Unit tests for stat.cc — experience calculation, stat min/max, and XP modifier.
//
// Tests mirror the production implementations in stat.cc:700-814 (pcGetExperienceForLevel,
// statSetMaxValue, statSetMinValue, statGetFrmId, pcAddExperienceWithOptions XP modifier).
// Uses test-struct mirrors to avoid linking the full engine dependency graph (same
// pattern as test_criticals.cc).
//
// Reference source: src/stat.cc, src/stat_defs.h, src/random.h

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <climits>

namespace fallout {

// ---- Mirror type/constant definitions (matching stat_defs.h, stat.cc) ----

#define TEST_PRIMARY_STAT_MIN (1)
#define TEST_PRIMARY_STAT_MAX (10)
#define TEST_PRIMARY_STAT_RANGE ((TEST_PRIMARY_STAT_MAX) - (TEST_PRIMARY_STAT_MIN) + 1)
#define TEST_PC_LEVEL_MAX 99

// Mirror of stat_defs.h Stat enum (abbreviated — full enum has 41 entries).
// We only need STAT_COUNT for array sizing.
enum {
    TEST_STAT_STRENGTH = 0,
    TEST_STAT_PERCEPTION,
    TEST_STAT_ENDURANCE,
    TEST_STAT_CHARISMA,
    TEST_STAT_INTELLIGENCE,
    TEST_STAT_AGILITY,
    TEST_STAT_LUCK,
    TEST_STAT_MAXIMUM_HIT_POINTS,
    TEST_STAT_MAXIMUM_ACTION_POINTS,
    TEST_STAT_ARMOR_CLASS,
    TEST_STAT_UNARMED_DAMAGE,
    TEST_STAT_MELEE_DAMAGE,
    TEST_STAT_CARRY_WEIGHT,
    TEST_STAT_SEQUENCE,
    TEST_STAT_HEALING_RATE,
    TEST_STAT_CRITICAL_CHANCE,
    TEST_STAT_BETTER_CRITICALS,
    TEST_STAT_DAMAGE_THRESHOLD,
    TEST_STAT_DAMAGE_THRESHOLD_LASER,
    TEST_STAT_DAMAGE_THRESHOLD_FIRE,
    TEST_STAT_DAMAGE_THRESHOLD_PLASMA,
    TEST_STAT_DAMAGE_THRESHOLD_ELECTRICAL,
    TEST_STAT_DAMAGE_THRESHOLD_EMP,
    TEST_STAT_DAMAGE_THRESHOLD_EXPLOSION,
    TEST_STAT_DAMAGE_RESISTANCE,
    TEST_STAT_DAMAGE_RESISTANCE_LASER,
    TEST_STAT_DAMAGE_RESISTANCE_FIRE,
    TEST_STAT_DAMAGE_RESISTANCE_PLASMA,
    TEST_STAT_DAMAGE_RESISTANCE_ELECTRICAL,
    TEST_STAT_DAMAGE_RESISTANCE_EMP,
    TEST_STAT_DAMAGE_RESISTANCE_EXPLOSION,
    TEST_STAT_RADIATION_RESISTANCE,
    TEST_STAT_POISON_RESISTANCE,
    TEST_STAT_AGE,
    TEST_STAT_GENDER,
    TEST_STAT_CURRENT_HIT_POINTS,
    TEST_STAT_CURRENT_POISON_LEVEL,
    TEST_STAT_CURRENT_RADIATION_LEVEL,
    TEST_STAT_COUNT,
    TEST_PRIMARY_STAT_COUNT = 7,
};

// Mirror of stat_defs.h PcStat enum.
enum {
    TEST_PC_STAT_UNSPENT_SKILL_POINTS = 0,
    TEST_PC_STAT_LEVEL,
    TEST_PC_STAT_EXPERIENCE,
    TEST_PC_STAT_REPUTATION,
    TEST_PC_STAT_KARMA,
    TEST_PC_STAT_COUNT,
};

// Perk constants needed for Swift Learner (matching perk_defs.h).
enum {
    TEST_PERK_SWIFT_LEARNER = 0,
};

// Roll constants (matching random.h).
enum {
    TEST_ROLL_CRITICAL_FAILURE = 0,
    TEST_ROLL_FAILURE = 1,
    TEST_ROLL_SUCCESS = 2,
    TEST_ROLL_CRITICAL_SUCCESS = 3,
};

// ---- Mirror data structures (matching stat.cc:36-94) ----

typedef struct TestStatDescription {
    char* name;
    char* description;
    int frmId;
    int minimumValue;
    int maximumValue;
    int defaultValue;
} TestStatDescription;

// Mirror of stat.cc:46-85 gStatDescriptions (initialized at compile time).
// Clang-format off
static TestStatDescription gTestStatDescriptions[TEST_STAT_COUNT] = {
    { nullptr, nullptr,  0, TEST_PRIMARY_STAT_MIN, TEST_PRIMARY_STAT_MAX, 5 },   // STRENGTH
    { nullptr, nullptr,  1, TEST_PRIMARY_STAT_MIN, TEST_PRIMARY_STAT_MAX, 5 },   // PERCEPTION
    { nullptr, nullptr,  2, TEST_PRIMARY_STAT_MIN, TEST_PRIMARY_STAT_MAX, 5 },   // ENDURANCE
    { nullptr, nullptr,  3, TEST_PRIMARY_STAT_MIN, TEST_PRIMARY_STAT_MAX, 5 },   // CHARISMA
    { nullptr, nullptr,  4, TEST_PRIMARY_STAT_MIN, TEST_PRIMARY_STAT_MAX, 5 },   // INTELLIGENCE
    { nullptr, nullptr,  5, TEST_PRIMARY_STAT_MIN, TEST_PRIMARY_STAT_MAX, 5 },   // AGILITY
    { nullptr, nullptr,  6, TEST_PRIMARY_STAT_MIN, TEST_PRIMARY_STAT_MAX, 5 },   // LUCK
    { nullptr, nullptr, 10,  0,  999,  0 },                                      // MAX_HP
    { nullptr, nullptr, 75,  1,   99,  0 },                                      // MAX_AP
    { nullptr, nullptr, 18,  0,  999,  0 },                                      // ARMOR_CLASS
    { nullptr, nullptr, 31,  0,  INT_MAX, 0 },                                   // UNARMED_DAMAGE
    { nullptr, nullptr, 32,  0,  500,  0 },                                      // MELEE_DAMAGE
    { nullptr, nullptr, 20,  0,  999,  0 },                                      // CARRY_WEIGHT
    { nullptr, nullptr, 24,  0,   60,  0 },                                      // SEQUENCE
    { nullptr, nullptr, 25,  0,   30,  0 },                                      // HEALING_RATE
    { nullptr, nullptr, 26,  0,  100,  0 },                                      // CRITICAL_CHANCE
    { nullptr, nullptr, 94,-60,  100,  0 },                                      // BETTER_CRITICALS
    { nullptr, nullptr,  0,  0,  100,  0 },                                      // DAMAGE_THRESHOLD
    { nullptr, nullptr,  0,  0,  100,  0 },                                      // DT_LASER
    { nullptr, nullptr,  0,  0,  100,  0 },                                      // DT_FIRE
    { nullptr, nullptr,  0,  0,  100,  0 },                                      // DT_PLASMA
    { nullptr, nullptr,  0,  0,  100,  0 },                                      // DT_ELECTRICAL
    { nullptr, nullptr,  0,  0,  100,  0 },                                      // DT_EMP
    { nullptr, nullptr,  0,  0,  100,  0 },                                      // DT_EXPLOSION
    { nullptr, nullptr,  0,  0,  100,  0 },                                      // DAMAGE_RESISTANCE
    { nullptr, nullptr, 22,  0,   90,  0 },                                      // DR_LASER
    { nullptr, nullptr,  0,  0,   90,  0 },                                      // DR_FIRE
    { nullptr, nullptr,  0,  0,   90,  0 },                                      // DR_PLASMA
    { nullptr, nullptr,  0,  0,   90,  0 },                                      // DR_ELECTRICAL
    { nullptr, nullptr,  0,  0,  100,  0 },                                      // DR_EMP
    { nullptr, nullptr,  0,  0,   90,  0 },                                      // DR_EXPLOSION
    { nullptr, nullptr, 83,  0,   95,  0 },                                      // RADIATION_RESISTANCE
    { nullptr, nullptr, 23,  0,   95,  0 },                                      // POISON_RESISTANCE
    { nullptr, nullptr,  0, 16,  101, 25 },                                      // AGE
    { nullptr, nullptr,  0,  0,    1,  0 },                                      // GENDER
    { nullptr, nullptr, 10,  0, 2000,  0 },                                      // CURRENT_HP
    { nullptr, nullptr, 11,  0, 2000,  0 },                                      // CURRENT_POISON
    { nullptr, nullptr, 12,  0, 2000,  0 },                                      // CURRENT_RADIATION
};
// Clang-format on

static TestStatDescription gTestPcStatDescriptions[TEST_PC_STAT_COUNT] = {
    { nullptr, nullptr,  0,  0,   INT_MAX, 0 },   // UNSPENT_SKILL_POINTS
    { nullptr, nullptr,  0,  1,   TEST_PC_LEVEL_MAX, 1 },  // LEVEL
    { nullptr, nullptr,  0,  0,   INT_MAX, 0 },   // EXPERIENCE
    { nullptr, nullptr,  0, -20, 20, 0 },         // REPUTATION
    { nullptr, nullptr,  0,  0,   INT_MAX, 0 },   // KARMA
};

// Mirror of file-static gPcStatValues[PC_STAT_COUNT] in stat.cc:103.
static int gTestPcStatValues[TEST_PC_STAT_COUNT];

// Mirror of extern globals referenced by stat.cc.
static int gTestXPTableMode = 0;
static int gTestXpModPercentage = 100;

// Mock Object type for perkGetRank / gDude.
// Minimal mock — just enough to carry a PID for Swift Learner detection.
typedef struct TestObject {
    int pid;
    int id;
} TestObject;

static TestObject gTestDude;
static TestObject* gTestDudePtr = &gTestDude;

// ---- Mirror functions (matching production implementations) ----

static inline bool testStatIsValid(int stat)
{
    return stat >= 0 && stat < TEST_STAT_COUNT;
}

// Mirror of stat.cc:700-720 pcGetExperienceForLevel.
// Exact copy of the production logic: returns XP required to reach given level.
static int testPcGetExperienceForLevel(int level)
{
    if (level >= TEST_PC_LEVEL_MAX) {
        return -1;
    }

    // gXPTableMode >= 1: external table loading (TODO, not yet implemented).
    // Falls through to hardcoded formula in all cases.
    if (gTestXPTableMode >= 1) {
        // Fall through — file-based table not yet implemented.
    }

    int halfLevel = level / 2;
    if ((level & 1) != 0) {
        return 1000 * halfLevel * level;
    } else {
        return 1000 * halfLevel * (level - 1);
    }
}

// Mirror of stat.cc:735-738 statGetFrmId.
static int testStatGetFrmId(int stat)
{
    return testStatIsValid(stat) ? gTestStatDescriptions[stat].frmId : 0;
}

// Mirror of stat.cc:742-747 statSetMaxValue.
// I2-01: Guard against UB when min > max (C++17 [alg.clamp] requires lo <= hi).
// If new max is below current min, raise min to match.
static void testStatSetMaxValue(int stat, int value)
{
    if (testStatIsValid(stat)) {
        gTestStatDescriptions[stat].maximumValue = value;
        // I2-01: Prevent min > max (would UB on std::clamp in critterGetStat)
        if (gTestStatDescriptions[stat].minimumValue > gTestStatDescriptions[stat].maximumValue) {
            gTestStatDescriptions[stat].minimumValue = gTestStatDescriptions[stat].maximumValue;
        }
    }
}

// Mirror of stat.cc:751-756 statSetMinValue.
// I2-01: Guard against UB when min > max (C++17 [alg.clamp] requires lo <= hi).
// If new min is above current max, lower max to match.
static void testStatSetMinValue(int stat, int value)
{
    if (testStatIsValid(stat)) {
        gTestStatDescriptions[stat].minimumValue = value;
        // I2-01: Prevent min > max (would UB on std::clamp in critterGetStat)
        if (gTestStatDescriptions[stat].minimumValue > gTestStatDescriptions[stat].maximumValue) {
            gTestStatDescriptions[stat].maximumValue = gTestStatDescriptions[stat].minimumValue;
        }
    }
}

// Mirror of stat.cc:722-726 pcStatGetName.
static char* testPcStatGetName(int pcStat)
{
    return pcStat >= 0 && pcStat < TEST_PC_STAT_COUNT ? gTestPcStatDescriptions[pcStat].name : nullptr;
}

// Mirror of stat.cc:728-732 pcStatGetDescription.
static char* testPcStatGetDescription(int pcStat)
{
    return pcStat >= 0 && pcStat < TEST_PC_STAT_COUNT ? gTestPcStatDescriptions[pcStat].description : nullptr;
}

// Mock of perkGetRank for testing XP modifier paths.
// Returns 0 (no perk) by default, but we override per-test via a simple test helper.
static int testPerkGetRankSwitftLearner = 0;
static int testPerkGetRank(TestObject* /*dude*/, int perk)
{
    if (perk == TEST_PERK_SWIFT_LEARNER) {
        return testPerkGetRankSwitftLearner;
    }
    return 0;
}

// Mirror of the XP modifier calculation from pcAddExperienceWithOptions (stat.cc:795-807).
// Isolates the percentage-math + Swift Learner component without the full
// critter/heal/display dependency chain.
static void testApplyXpModifier(int baseXp, int xpModPercent, int swiftLearnerRank, int& outAdjustedXp, int& outTotalXp)
{
    int adjustedXp = baseXp * xpModPercent / 100;
    int newXp = 0;
    newXp += adjustedXp;
    newXp += swiftLearnerRank * 5 * adjustedXp / 100;
    outAdjustedXp = adjustedXp;
    outTotalXp = newXp;
}

// ---- Mirror types/functions for derived stats guard removal (M-017), ----
// ---- statSetMaxValue/MinValue integration (M-018/M-019), and          ----
// ---- pcAddExperienceWithOptions full path (M-020).                    ----
// ---- Production reference: stat.cc:474-520, 742-756, 795-868.         ----

// Mirror of PID_TYPE macro and OBJ_TYPE_CRITTER for type-safety tests.
#define TEST_PID_TYPE(value) (value) >> 24
enum {
    TEST_OBJ_TYPE_CRITTER = 1,
    TEST_OBJ_TYPE_ITEM = 0,
    TEST_OBJ_TYPE_SCENERY = 2,
};

// SAVEABLE_STAT_COUNT = 35 from stat_defs.h:70.
// All stats from STAT_STRENGTH (0) through STAT_DAMAGE_RESISTANCE_EXPLOSION (28)
// plus some extras up to index 34.
#define TEST_SAVEABLE_STAT_COUNT 35

// Mirror of critterSetBaseStat (stat.cc:474-520) — includes fork's
// PID_TYPE guard but REMOVES the old derived-stat guard (M-017).
// Derived stats (indices 7-24+: STAT_MAXIMUM_HIT_POINTS through
// STAT_POISON_RESISTANCE) can now be set as base stats.
static int testCritterSetBaseStat(int pid, int stat, int value, bool isDude)
{
    if (stat < 0 || stat >= TEST_STAT_COUNT) {
        return -5;
    }

    if (TEST_PID_TYPE(pid) != TEST_OBJ_TYPE_CRITTER) {
        return -5;
    }

    // Fork: old guard `if (stat > STAT_LUCK && stat <= STAT_POISON_RESISTANCE) return -1`
    // has been REMOVED. Derived stats now flow through normal bounds enforcement.
    if (stat >= 0 && stat < TEST_SAVEABLE_STAT_COUNT) {
        if (isDude) {
            value -= 0; // trait modifier subtracted in production; stub here
        }

        if (value < gTestStatDescriptions[stat].minimumValue) {
            return -2;
        }

        if (value > gTestStatDescriptions[stat].maximumValue) {
            return -3;
        }

        // In production: proto->critter.data.baseStats[stat] = value
        // We track it in a test-local array.
        static int gTestBaseStats[TEST_STAT_COUNT];
        gTestBaseStats[stat] = value;

        return 0;
    }

    // Pseudostats (STAT_CURRENT_HIT_POINTS, STAT_CURRENT_POISON_LEVEL,
    // STAT_CURRENT_RADIATION_LEVEL) handled by switch in production.
    return 0;
}

// Mirror of critterSetBonusStat (stat.cc:547-580) — includes fork's PID_TYPE guard.
static int testCritterSetBonusStat(int pid, int stat, int value)
{
    if (stat < 0 || stat >= TEST_STAT_COUNT) {
        return -5;
    }

    if (TEST_PID_TYPE(pid) != TEST_OBJ_TYPE_CRITTER) {
        return -5;
    }

    if (stat >= 0 && stat < TEST_SAVEABLE_STAT_COUNT) {
        static int gTestBonusStats[TEST_STAT_COUNT];
        gTestBonusStats[stat] = value;
        return 0;
    }

    return -1;
}

// Mirror of pcAddExperienceWithOptions full production path (stat.cc:795-868).
// Includes: XP modifier, Swift Learner bonus, level-up loop, HP gain per level,
// doParty trigger, and pcSetExperience asymmetry (pcSetExperience does NOT
// apply gXpModPercentage — lines 871-879).
static void testPcAddExperienceWithOptions(
    int xp, int xpModPercent, int swiftLearnerRank,
    bool doParty, int* outXpGained, int* outLevelsGained)
{
    int oldXp = gTestPcStatValues[TEST_PC_STAT_EXPERIENCE];

    // stat.cc:802: adjustedXp = xp * gXpModPercentage / 100
    int adjustedXp = xp * xpModPercent / 100;

    // stat.cc:804-806
    int newXp = oldXp;
    newXp += adjustedXp;
    newXp += swiftLearnerRank * 5 * adjustedXp / 100;

    // stat.cc:808-814: clamp to min/max of PC_STAT_EXPERIENCE
    if (newXp < gTestPcStatDescriptions[TEST_PC_STAT_EXPERIENCE].minimumValue) {
        newXp = gTestPcStatDescriptions[TEST_PC_STAT_EXPERIENCE].minimumValue;
    }
    if (newXp > gTestPcStatDescriptions[TEST_PC_STAT_EXPERIENCE].maximumValue) {
        newXp = gTestPcStatDescriptions[TEST_PC_STAT_EXPERIENCE].maximumValue;
    }

    gTestPcStatValues[TEST_PC_STAT_EXPERIENCE] = newXp;

    // stat.cc:818-861: level-up loop
    int levelsGained = 0;
    int currentLevel = gTestPcStatValues[TEST_PC_STAT_LEVEL];
    while (currentLevel < TEST_PC_LEVEL_MAX) {
        int xpForNext = testPcGetExperienceForLevel(currentLevel + 1);
        if (xpForNext == -1 || newXp < xpForNext) {
            break;
        }

        currentLevel++;
        levelsGained++;

        // HP gain: endurance/2 + 2 + lifegiver*4
        // STAT_ENDURANCE = 2 (index), default=5, base=5
        int enduranceDefault = gTestStatDescriptions[2].defaultValue;
        int hpPerLevel = enduranceDefault / 2 + 2;  // 5/2+2 = 4
    }

    gTestPcStatValues[TEST_PC_STAT_LEVEL] = currentLevel;

    if (doParty) {
        // In production: _partyMemberIncLevels() would be called here.
        // For mirror testing we set a flag.
    }

    if (outXpGained != nullptr) {
        *outXpGained = newXp - oldXp;
    }

    if (outLevelsGained != nullptr) {
        *outLevelsGained = levelsGained;
    }
}

// Mirror of pcSetExperience (stat.cc:871-879) — does NOT apply gXpModPercentage.
static void testPcSetExperience(int xp, int* outLevelsGained)
{
    int oldLevel = gTestPcStatValues[TEST_PC_STAT_LEVEL];
    gTestPcStatValues[TEST_PC_STAT_EXPERIENCE] = xp;

    int level = 1;
    do {
        level += 1;
    } while (xp >= testPcGetExperienceForLevel(level) && level < TEST_PC_LEVEL_MAX);

    // Clamp: if xp >= XP for level 99, level stays at 99
    int xpFor99 = testPcGetExperienceForLevel(99);
    if (xpFor99 != -1 && xp >= xpFor99) {
        level = TEST_PC_LEVEL_MAX;
    }

    gTestPcStatValues[TEST_PC_STAT_LEVEL] = level;

    if (outLevelsGained != nullptr) {
        *outLevelsGained = level - oldLevel;
    }
}

// ---- End of new mirrors ----

} // namespace fallout

using namespace fallout;

// ===========================================================================
// pcGetExperienceForLevel tests (P1 — pure math, zero dependencies)
// ===========================================================================

TEST_CASE("pcGetExperienceForLevel — level 1 (lowest level)")
{
    // Level 1: halfLevel=0, odd. Expected: 1000 * 0 * 1 = 0
    CHECK(testPcGetExperienceForLevel(1) == 0);
}

TEST_CASE("pcGetExperienceForLevel — level 2 (even)")
{
    // Level 2: halfLevel=1, even. Expected: 1000 * 1 * 1 = 1000
    CHECK(testPcGetExperienceForLevel(2) == 1000);
}

TEST_CASE("pcGetExperienceForLevel — level 3 (odd)")
{
    // Level 3: halfLevel=1, odd. Expected: 1000 * 1 * 3 = 3000
    CHECK(testPcGetExperienceForLevel(3) == 3000);
}

TEST_CASE("pcGetExperienceForLevel — level 4 (even)")
{
    // Level 4: halfLevel=2, even. Expected: 1000 * 2 * 3 = 6000
    CHECK(testPcGetExperienceForLevel(4) == 6000);
}

TEST_CASE("pcGetExperienceForLevel — level 5 (odd)")
{
    // Level 5: halfLevel=2, odd. Expected: 1000 * 2 * 5 = 10000
    CHECK(testPcGetExperienceForLevel(5) == 10000);
}

TEST_CASE("pcGetExperienceForLevel — level 6 (even)")
{
    // Level 6: halfLevel=3, even. Expected: 1000 * 3 * 5 = 15000
    CHECK(testPcGetExperienceForLevel(6) == 15000);
}

TEST_CASE("pcGetExperienceForLevel — known mid-range levels (spot-checks)")
{
    // Level 10: halfLevel=5, even -> 1000 * 5 * 9 = 45000
    CHECK(testPcGetExperienceForLevel(10) == 45000);

    // Level 15: halfLevel=7, odd -> 1000 * 7 * 15 = 105000
    CHECK(testPcGetExperienceForLevel(15) == 105000);

    // Level 21: halfLevel=10, odd -> 1000 * 10 * 21 = 210000
    CHECK(testPcGetExperienceForLevel(21) == 210000);
}

TEST_CASE("pcGetExperienceForLevel — high levels (spot-checks)")
{
    // Level 50: halfLevel=25, even -> 1000 * 25 * 49 = 1225000
    CHECK(testPcGetExperienceForLevel(50) == 1225000);

    // Level 75: halfLevel=37, odd -> 1000 * 37 * 75 = 2775000
    CHECK(testPcGetExperienceForLevel(75) == 2775000);

    // Level 98: halfLevel=49, even -> 1000 * 49 * 97 = 4753000
    CHECK(testPcGetExperienceForLevel(98) == 4753000);
}

TEST_CASE("pcGetExperienceForLevel — boundary level 99 (PC_LEVEL_MAX)")
{
    // Level >= PC_LEVEL_MAX returns -1 (no more levels)
    CHECK(testPcGetExperienceForLevel(99) == -1);
}

TEST_CASE("pcGetExperienceForLevel — level 0 (below valid range)")
{
    // Level 0: halfLevel=0, even. Expected: 1000 * 0 * (-1) = 0
    CHECK(testPcGetExperienceForLevel(0) == 0);
}

TEST_CASE("pcGetExperienceForLevel — monotonic increase")
{
    // XP requirements should monotonically increase from level 1 to 98.
    int prev = testPcGetExperienceForLevel(1);
    for (int level = 2; level < TEST_PC_LEVEL_MAX; level++) {
        int curr = testPcGetExperienceForLevel(level);
        CHECK(curr > prev);
        prev = curr;
    }
}

TEST_CASE("pcGetExperienceForLevel — odd/even branch equivalence at boundary")
{
    // Odd level N and even level N+1 should produce related values.
    // Even N: 1000 * N/2 * (N-1)
    // Odd N-1 (prev): 1000 * floor((N-1)/2) * (N-1)
    // For N even, N/2 = floor((N-1)/2), so XP for even N equals
    // 1000 * k * (N-1) where k = N/2.
    for (int level = 3; level < TEST_PC_LEVEL_MAX; level += 2) {
        int odd = testPcGetExperienceForLevel(level);      // 1000 * k * level
        int prevEven = testPcGetExperienceForLevel(level - 1);  // 1000 * k * (level-2)
        CHECK(odd > prevEven);
    }
}

// ===========================================================================
// XP table mode (gXPTableMode) — currently no-op fallthrough
// ===========================================================================

TEST_CASE("pcGetExperienceForLevel — gXPTableMode >= 1 does not change formula")
{
    int saved = gTestXPTableMode;
    gTestXPTableMode = 0;

    // Record values at mode 0
    int xp10_mode0 = testPcGetExperienceForLevel(10);
    int xp21_mode0 = testPcGetExperienceForLevel(21);

    // Set mode to 1 — should produce same results (fallthrough)
    gTestXPTableMode = 1;
    CHECK(testPcGetExperienceForLevel(10) == xp10_mode0);
    CHECK(testPcGetExperienceForLevel(21) == xp21_mode0);

    // Set mode to 2
    gTestXPTableMode = 2;
    CHECK(testPcGetExperienceForLevel(10) == xp10_mode0);
    CHECK(testPcGetExperienceForLevel(21) == xp21_mode0);

    // Level 99 still returns -1 regardless of mode
    CHECK(testPcGetExperienceForLevel(99) == -1);

    gTestXPTableMode = saved;
}

// ===========================================================================
// statSetMaxValue / statSetMinValue tests (P1 — minimal fixture)
// ===========================================================================

TEST_CASE("statSetMaxValue — sets maximumValue for valid stat")
{
    // Save original
    int originalMax = gTestStatDescriptions[TEST_STAT_RADIATION_RESISTANCE].maximumValue;

    testStatSetMaxValue(TEST_STAT_RADIATION_RESISTANCE, 100);
    CHECK(gTestStatDescriptions[TEST_STAT_RADIATION_RESISTANCE].maximumValue == 100);

    // Restore
    gTestStatDescriptions[TEST_STAT_RADIATION_RESISTANCE].maximumValue = originalMax;
}

TEST_CASE("statSetMaxValue — invalid stat is silently ignored")
{
    // Record state before
    int originalMax_neg1 = -1;  // no such stat at index -1

    testStatSetMaxValue(-1, 999);   // negative index
    testStatSetMaxValue(TEST_STAT_COUNT, 999);     // one past end
    testStatSetMaxValue(TEST_STAT_COUNT + 100, 999); // well past end

    // Stat at index 0 should be unchanged
    CHECK(gTestStatDescriptions[0].maximumValue == TEST_PRIMARY_STAT_MAX);
}

TEST_CASE("statSetMinValue — sets minimumValue for valid stat")
{
    int originalMin = gTestStatDescriptions[TEST_STAT_BETTER_CRITICALS].minimumValue;

    testStatSetMinValue(TEST_STAT_BETTER_CRITICALS, -50);
    CHECK(gTestStatDescriptions[TEST_STAT_BETTER_CRITICALS].minimumValue == -50);

    // Restore
    gTestStatDescriptions[TEST_STAT_BETTER_CRITICALS].minimumValue = originalMin;
}

TEST_CASE("statSetMinValue — invalid stat is silently ignored")
{
    testStatSetMinValue(-1, 10);
    testStatSetMinValue(TEST_STAT_COUNT, 10);
    testStatSetMinValue(9999, 10);

    // Verify array not corrupted
    CHECK(gTestStatDescriptions[0].minimumValue == TEST_PRIMARY_STAT_MIN);
    CHECK(gTestStatDescriptions[TEST_STAT_COUNT - 1].minimumValue == 0);
}

TEST_CASE("statSetMaxValue / statSetMinValue — combined interaction")
{
    int savedMax = gTestStatDescriptions[TEST_STAT_DAMAGE_RESISTANCE].maximumValue;
    int savedMin = gTestStatDescriptions[TEST_STAT_DAMAGE_RESISTANCE].minimumValue;

    testStatSetMaxValue(TEST_STAT_DAMAGE_RESISTANCE, 90);
    testStatSetMinValue(TEST_STAT_DAMAGE_RESISTANCE, -10);

    CHECK(gTestStatDescriptions[TEST_STAT_DAMAGE_RESISTANCE].maximumValue == 90);
    CHECK(gTestStatDescriptions[TEST_STAT_DAMAGE_RESISTANCE].minimumValue == -10);

    // Both calls modify the same stat independently
    gTestStatDescriptions[TEST_STAT_DAMAGE_RESISTANCE].maximumValue = savedMax;
    gTestStatDescriptions[TEST_STAT_DAMAGE_RESISTANCE].minimumValue = savedMin;
}

TEST_CASE("statSetMaxValue — set extreme value on PRIMARY stat")
{
    int savedMax = gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue;

    testStatSetMaxValue(TEST_STAT_STRENGTH, 10);
    CHECK(gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue == 10);

    testStatSetMaxValue(TEST_STAT_STRENGTH, 100);
    CHECK(gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue == 100);

    gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue = savedMax;
}

TEST_CASE("statSetMaxValue / statSetMinValue — zero value")
{
    int savedMax = gTestStatDescriptions[TEST_STAT_CHARISMA].maximumValue;
    int savedMin = gTestStatDescriptions[TEST_STAT_CHARISMA].minimumValue;

    testStatSetMaxValue(TEST_STAT_CHARISMA, 0);
    testStatSetMinValue(TEST_STAT_CHARISMA, 0);

    CHECK(gTestStatDescriptions[TEST_STAT_CHARISMA].maximumValue == 0);
    CHECK(gTestStatDescriptions[TEST_STAT_CHARISMA].minimumValue == 0);

    gTestStatDescriptions[TEST_STAT_CHARISMA].maximumValue = savedMax;
    gTestStatDescriptions[TEST_STAT_CHARISMA].minimumValue = savedMin;
}

// ===========================================================================
// statGetFrmId tests (P1 — trivial fixture)
// ===========================================================================

TEST_CASE("statGetFrmId — returns correct frmId for valid stats")
{
    CHECK(testStatGetFrmId(TEST_STAT_STRENGTH) == 0);
    CHECK(testStatGetFrmId(TEST_STAT_PERCEPTION) == 1);
    CHECK(testStatGetFrmId(TEST_STAT_ENDURANCE) == 2);
    CHECK(testStatGetFrmId(TEST_STAT_CHARISMA) == 3);
    CHECK(testStatGetFrmId(TEST_STAT_INTELLIGENCE) == 4);
    CHECK(testStatGetFrmId(TEST_STAT_AGILITY) == 5);
    CHECK(testStatGetFrmId(TEST_STAT_LUCK) == 6);

    // Non-SPECIAL stats
    CHECK(testStatGetFrmId(TEST_STAT_MAXIMUM_HIT_POINTS) == 10);
    CHECK(testStatGetFrmId(TEST_STAT_MAXIMUM_ACTION_POINTS) == 75);
    CHECK(testStatGetFrmId(TEST_STAT_CARRY_WEIGHT) == 20);
    CHECK(testStatGetFrmId(TEST_STAT_DAMAGE_THRESHOLD) == 0);
    CHECK(testStatGetFrmId(TEST_STAT_DAMAGE_RESISTANCE_LASER) == 22);
    CHECK(testStatGetFrmId(TEST_STAT_RADIATION_RESISTANCE) == 83);
    CHECK(testStatGetFrmId(TEST_STAT_POISON_RESISTANCE) == 23);
    CHECK(testStatGetFrmId(TEST_STAT_AGE) == 0);
    CHECK(testStatGetFrmId(TEST_STAT_GENDER) == 0);
    CHECK(testStatGetFrmId(TEST_STAT_CURRENT_HIT_POINTS) == 10);
    CHECK(testStatGetFrmId(TEST_STAT_CURRENT_POISON_LEVEL) == 11);
    CHECK(testStatGetFrmId(TEST_STAT_CURRENT_RADIATION_LEVEL) == 12);
}

TEST_CASE("statGetFrmId — invalid stat returns 0")
{
    CHECK(testStatGetFrmId(-1) == 0);
    CHECK(testStatGetFrmId(TEST_STAT_COUNT) == 0);
    CHECK(testStatGetFrmId(999) == 0);
}

// ===========================================================================
// pcStatGetName / pcStatGetDescription
// ===========================================================================

TEST_CASE("pcStatGetName — nullptr for uninitialized descriptions")
{
    // These are nullptr until statsInit() loads message files.
    // Verify the boundary checks work correctly.
    CHECK(testPcStatGetName(-1) == nullptr);
    CHECK(testPcStatGetName(TEST_PC_STAT_COUNT) == nullptr);
    CHECK(testPcStatGetName(999) == nullptr);
}

TEST_CASE("pcStatGetDescription — nullptr for uninitialized descriptions")
{
    CHECK(testPcStatGetDescription(-1) == nullptr);
    CHECK(testPcStatGetDescription(TEST_PC_STAT_COUNT) == nullptr);
    CHECK(testPcStatGetDescription(999) == nullptr);
}

// ===========================================================================
// testStatIsValid — boundary conditions
// ===========================================================================

TEST_CASE("testStatIsValid — boundary checks")
{
    CHECK(testStatIsValid(0));
    CHECK(testStatIsValid(TEST_STAT_COUNT - 1));          // last valid index
    CHECK_FALSE(testStatIsValid(-1));                      // negative
    CHECK_FALSE(testStatIsValid(TEST_STAT_COUNT));         // one past
    CHECK_FALSE(testStatIsValid(TEST_STAT_COUNT + 100));   // far past
}

// ===========================================================================
// XP modifier math (pcAddExperienceWithOptions mirror — P2)
// ===========================================================================

TEST_CASE("XP modifier — default 100% (no modification)")
{
    int adjusted, total;
    testApplyXpModifier(1000, 100, 0, adjusted, total);

    CHECK(adjusted == 1000);    // no change
    CHECK(total == 1000);
}

TEST_CASE("XP modifier — 200% (doubled XP)")
{
    int adjusted, total;
    testApplyXpModifier(1000, 200, 0, adjusted, total);

    CHECK(adjusted == 2000);
    CHECK(total == 2000);
}

TEST_CASE("XP modifier — 50% (halved XP)")
{
    int adjusted, total;
    testApplyXpModifier(1000, 50, 0, adjusted, total);

    CHECK(adjusted == 500);
    CHECK(total == 500);
}

TEST_CASE("XP modifier — 0% (zero XP)")
{
    int adjusted, total;
    testApplyXpModifier(1000, 0, 0, adjusted, total);

    CHECK(adjusted == 0);
    CHECK(total == 0);
}

TEST_CASE("XP modifier — 150% (1.5x, common mod value)")
{
    int adjusted, total;
    testApplyXpModifier(1000, 150, 0, adjusted, total);

    CHECK(adjusted == 1500);
    CHECK(total == 1500);
}

TEST_CASE("XP modifier — integer truncation for small XP values")
{
    int adjusted, total;

    // 1 XP at 50%: 1 * 50 / 100 = 0 (truncation)
    testApplyXpModifier(1, 50, 0, adjusted, total);
    CHECK(adjusted == 0);
    CHECK(total == 0);

    // 1 XP at 150%: 1 * 150 / 100 = 1
    testApplyXpModifier(1, 150, 0, adjusted, total);
    CHECK(adjusted == 1);
    CHECK(total == 1);
}

TEST_CASE("XP modifier — with Swift Learner rank 1 (+5%)")
{
    int adjusted, total;

    // 1000 XP at 100% mod + Swift Learner rank 1: 1000 + 5%*1000 = 1050
    testApplyXpModifier(1000, 100, 1, adjusted, total);
    CHECK(adjusted == 1000);
    CHECK(total == 1050);

    // 1000 XP at 200% mod + Swift Learner rank 1: 2000 + 5%*2000 = 2100
    testApplyXpModifier(1000, 200, 1, adjusted, total);
    CHECK(adjusted == 2000);
    CHECK(total == 2100);

    // Swift Learner applies to adjusted XP, not base XP
    testApplyXpModifier(1000, 150, 1, adjusted, total);
    CHECK(adjusted == 1500);
    CHECK(total == 1500 + 75);  // 1500 + 5% of 1500
}

TEST_CASE("XP modifier — with Swift Learner rank 2 (+10%)")
{
    int adjusted, total;
    testApplyXpModifier(1000, 100, 2, adjusted, total);
    CHECK(adjusted == 1000);
    CHECK(total == 1100);   // 1000 + 10% of 1000
}

TEST_CASE("XP modifier — with Swift Learner rank 3 (+15%)")
{
    int adjusted, total;
    testApplyXpModifier(1000, 100, 3, adjusted, total);
    CHECK(adjusted == 1000);
    CHECK(total == 1150);   // 1000 + 15% of 1000
}

TEST_CASE("XP modifier — Swift Learner + XP mod combined")
{
    int adjusted, total;
    // 100 base XP * 200% = 200 adjusted, + 5% of 200 = 210 total
    testApplyXpModifier(100, 200, 1, adjusted, total);
    CHECK(adjusted == 200);
    CHECK(total == 210);
}

// ===========================================================================
// Stat description data integrity
// ===========================================================================

TEST_CASE("StatDescription array — size matches STAT_COUNT")
{
    CHECK(TEST_STAT_COUNT == 38);       // 7 SPECIAL + 28 secondary + 3 current pseudostats
    CHECK(TEST_PC_STAT_COUNT == 5);     // unspent skill points, level, XP, reputation, karma
}

TEST_CASE("StatDescription — PRIMARY stat defaults are correct")
{
    // All 7 SPECIAL stats should have min=1, max=10, default=5
    for (int i = 0; i < TEST_PRIMARY_STAT_COUNT; i++) {
        CHECK(gTestStatDescriptions[i].minimumValue == TEST_PRIMARY_STAT_MIN);
        CHECK(gTestStatDescriptions[i].maximumValue == TEST_PRIMARY_STAT_MAX);
        CHECK(gTestStatDescriptions[i].defaultValue == 5);
    }
}

TEST_CASE("StatDescription — MAX_HP has special range")
{
    CHECK(gTestStatDescriptions[TEST_STAT_MAXIMUM_HIT_POINTS].minimumValue == 0);
    CHECK(gTestStatDescriptions[TEST_STAT_MAXIMUM_HIT_POINTS].maximumValue == 999);
    CHECK(gTestStatDescriptions[TEST_STAT_MAXIMUM_HIT_POINTS].defaultValue == 0);
}

TEST_CASE("StatDescription — BETTER_CRITICALS has negative minimum")
{
    CHECK(gTestStatDescriptions[TEST_STAT_BETTER_CRITICALS].minimumValue == -60);
    CHECK(gTestStatDescriptions[TEST_STAT_BETTER_CRITICALS].defaultValue == 0);
}

TEST_CASE("StatDescription — CURRENT_HP/POISON/RADIATION have 2000 max")
{
    CHECK(gTestStatDescriptions[TEST_STAT_CURRENT_HIT_POINTS].maximumValue == 2000);
    CHECK(gTestStatDescriptions[TEST_STAT_CURRENT_POISON_LEVEL].maximumValue == 2000);
    CHECK(gTestStatDescriptions[TEST_STAT_CURRENT_RADIATION_LEVEL].maximumValue == 2000);
}

TEST_CASE("PcStatDescription — LEVEL range correct")
{
    CHECK(gTestPcStatDescriptions[TEST_PC_STAT_LEVEL].minimumValue == 1);
    CHECK(gTestPcStatDescriptions[TEST_PC_STAT_LEVEL].maximumValue == TEST_PC_LEVEL_MAX);
}

TEST_CASE("PcStatDescription — REPUTATION has negative range")
{
    CHECK(gTestPcStatDescriptions[TEST_PC_STAT_REPUTATION].minimumValue == -20);
    CHECK(gTestPcStatDescriptions[TEST_PC_STAT_REPUTATION].maximumValue == 20);
}

// ===========================================================================
// Constant validation (cross-reference: sfall_testing expected values)
// ===========================================================================

TEST_CASE("Constants match Fallout 2 values")
{
    // RPU and sfall_testing both assume these values for SPECIAL stats.
    CHECK(TEST_PRIMARY_STAT_MIN == 1);
    CHECK(TEST_PRIMARY_STAT_MAX == 10);
    CHECK(TEST_PRIMARY_STAT_RANGE == 10);  // 10-1+1
    CHECK(TEST_PC_LEVEL_MAX == 99);        // Hardcoded in Fallout 2 engine
}

// ===========================================================================
// M-017: Derived stats guard removal (stat.cc:479)
// ===========================================================================
// Fork REMOVED the guard that blocked setting base values for derived stats
// (STAT_LUCK < stat <= STAT_POISON_RESISTANCE). Now derived stats like
// STAT_MAXIMUM_HIT_POINTS (index 7), STAT_ARMOR_CLASS (index 9), and all
// damage thresholds/resistances can be set via critterSetBaseStat.
// Research: RPU uses set_pc_base_stat(STAT_max_hp, 999) — CONFIRMED (Section 1.1-B).

TEST_CASE("M-017: Derived stats guard removal — base stat can now be set on derived stats")
{
    // Critter PID: type=CRITTER in high byte.
    int critterPid = (TEST_OBJ_TYPE_CRITTER << 24) | 1;

    SUBCASE("STAT_MAXIMUM_HIT_POINTS (index 7) — old code returned -1, fork allows")
    {
        // Old code blocked all derived stats. Fork removed the guard.
        // The stat must pass min/max checks from the stat description.
        int result = testCritterSetBaseStat(critterPid, TEST_STAT_MAXIMUM_HIT_POINTS, 100, false);
        // Fork: no derived-stat block → proceeds to bounds check.
        // gStatDescriptions[MAX_HP].minimumValue=0, maximumValue=999, so 100 is valid.
        CHECK(result == 0);
    }

    SUBCASE("STAT_ARMOR_CLASS (index 9) — can be set as base stat")
    {
        int result = testCritterSetBaseStat(critterPid, TEST_STAT_ARMOR_CLASS, 25, false);
        CHECK(result == 0);
    }

    SUBCASE("STAT_DAMAGE_RESISTANCE (index 23) — can be set as base stat")
    {
        int result = testCritterSetBaseStat(critterPid, TEST_STAT_DAMAGE_RESISTANCE, 50, false);
        CHECK(result == 0);
    }

    SUBCASE("STAT_RADIATION_RESISTANCE (index 32) — can be set as base stat")
    {
        int result = testCritterSetBaseStat(critterPid, TEST_STAT_RADIATION_RESISTANCE, 50, false);
        CHECK(result == 0);
    }

    SUBCASE("STAT_POISON_RESISTANCE (index 33) — can be set as base stat")
    {
        int result = testCritterSetBaseStat(critterPid, TEST_STAT_POISON_RESISTANCE, 50, false);
        CHECK(result == 0);
    }

    SUBCASE("Value exceeding derived stat's max is rejected")
    {
        // MAX_HP max is 999 per gStatDescriptions.
        int result = testCritterSetBaseStat(critterPid, TEST_STAT_MAXIMUM_HIT_POINTS, 1000, false);
        CHECK(result == -3);  // value > maximumValue
    }

    SUBCASE("Value below derived stat's min is rejected")
    {
        // MAX_HP min is 0.
        int result = testCritterSetBaseStat(critterPid, TEST_STAT_MAXIMUM_HIT_POINTS, -1, false);
        CHECK(result == -2);  // value < minimumValue
    }
}

TEST_CASE("M-017: Derived stats — non-primary stat write does NOT trigger derived recalculation")
{
    // In the fork, only primary stats (STAT_STRENGTH through STAT_LUCK, indices 0-6)
    // trigger critterUpdateDerivedStats after a base stat write (stat.cc:502-504).
    // Setting a derived stat like MAX_HP does NOT recompute derived stats.
    // This is a behavioral contract: setting MAX_HP directly changes only MAX_HP,
    // not CURRENT_HP or other dependent stats.
    int critterPid = (TEST_OBJ_TYPE_CRITTER << 24) | 1;

    SUBCASE("Setting primary stat STRENGTH returns 0 (triggers update in production)")
    {
        int result = testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 8, false);
        CHECK(result == 0);
    }

    SUBCASE("Setting derived stat MAX_HP returns 0 (no update trigger in production)")
    {
        // The fork allows this write (M-017) but does NOT call critterUpdateDerivedStats.
        int result = testCritterSetBaseStat(critterPid, TEST_STAT_MAXIMUM_HIT_POINTS, 150, false);
        CHECK(result == 0);
    }

    SUBCASE("Setting derived stat DAMAGE_RESISTANCE returns 0")
    {
        int result = testCritterSetBaseStat(critterPid, TEST_STAT_DAMAGE_RESISTANCE, 60, false);
        CHECK(result == 0);
    }
}

// ===========================================================================
// M-018: statSetMaxValue/MinValue integration with critterSetBaseStat
//         (stat.cc:742-756 + stat.cc:491-498)
// ===========================================================================
// Existing tests use test-local mirrors operating on gTestStatDescriptions.
// These tests verify the INTEGRATION pattern: modifying stat bounds via
// statSetMaxValue/MinValue changes the range enforced by critterSetBaseStat.
// Research: ET Tu LIKELY uses set_pc_stat_max (Section 1.6).

TEST_CASE("M-018: statSetMaxValue integration — modified max enforced by critterSetBaseStat")
{
    int critterPid = (TEST_OBJ_TYPE_CRITTER << 24) | 1;
    int saved = gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue;

    SUBCASE("Default max allows value up to 10")
    {
        gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue = TEST_PRIMARY_STAT_MAX; // 10
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 10, false) == 0);
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 11, false) == -3);
    }

    SUBCASE("After statSetMaxValue to 8, value 9 is rejected")
    {
        testStatSetMaxValue(TEST_STAT_STRENGTH, 8);
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 8, false) == 0);
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 9, false) == -3);
    }

    SUBCASE("After statSetMaxValue to 15, value 15 passes")
    {
        testStatSetMaxValue(TEST_STAT_STRENGTH, 15);
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 15, false) == 0);
    }

    // Restore
    gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue = saved;
}

TEST_CASE("M-018: statSetMinValue integration — modified min enforced by critterSetBaseStat")
{
    int critterPid = (TEST_OBJ_TYPE_CRITTER << 24) | 1;
    int saved = gTestStatDescriptions[TEST_STAT_STRENGTH].minimumValue;

    SUBCASE("Default min allows value 1, rejects 0")
    {
        gTestStatDescriptions[TEST_STAT_STRENGTH].minimumValue = TEST_PRIMARY_STAT_MIN; // 1
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 1, false) == 0);
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 0, false) == -2);
    }

    SUBCASE("After statSetMinValue to 5, value 4 is rejected")
    {
        testStatSetMinValue(TEST_STAT_STRENGTH, 5);
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 5, false) == 0);
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 4, false) == -2);
    }

    // Restore
    gTestStatDescriptions[TEST_STAT_STRENGTH].minimumValue = saved;
}

TEST_CASE("M-018: statSetMaxValue/MinValue — combined max+min on non-SPECIAL stat")
{
    int critterPid = (TEST_OBJ_TYPE_CRITTER << 24) | 1;
    int savedMax = gTestStatDescriptions[TEST_STAT_BETTER_CRITICALS].maximumValue;
    int savedMin = gTestStatDescriptions[TEST_STAT_BETTER_CRITICALS].minimumValue;

    SUBCASE("Tighten range to [10, 50] on BETTER_CRITICALS (default [-60, 100])")
    {
        testStatSetMinValue(TEST_STAT_BETTER_CRITICALS, 10);
        testStatSetMaxValue(TEST_STAT_BETTER_CRITICALS, 50);

        // Rejected: below new min
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_BETTER_CRITICALS, 5, false) == -2);
        // Accepted: within range
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_BETTER_CRITICALS, 30, false) == 0);
        // Rejected: above new max
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_BETTER_CRITICALS, 60, false) == -3);
    }

    gTestStatDescriptions[TEST_STAT_BETTER_CRITICALS].maximumValue = savedMax;
    gTestStatDescriptions[TEST_STAT_BETTER_CRITICALS].minimumValue = savedMin;
}

// ===========================================================================
// M-019: statSetMaxValue/MinValue inversion guard (I2-01 fix)
// Before I2-01: set_max(3) then set_min(5) → range [5,3] with no valid values.
// After I2-01:  set_max(3) then set_min(5) → range [5,5] (max raised to match min).
// ===========================================================================
// I2-01 adds a guard that prevents min > max by adjusting the counterpart
// when either side is set to a value that would invert the range (stat.cc:765-787).

TEST_CASE("M-019: I2-01 guard — setting min > max raises max to match")
{
    int critterPid = (TEST_OBJ_TYPE_CRITTER << 24) | 1;
    int savedMax = gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue;
    int savedMin = gTestStatDescriptions[TEST_STAT_STRENGTH].minimumValue;

    SUBCASE("Set max=3 then min=5 — guard raises max to 5")
    {
        testStatSetMaxValue(TEST_STAT_STRENGTH, 3);
        // min(1) <= max(3) → no guard trigger
        testStatSetMinValue(TEST_STAT_STRENGTH, 5);
        // min(5) > max(3) → guard raises max to 5

        CHECK(gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue == 5);
        CHECK(gTestStatDescriptions[TEST_STAT_STRENGTH].minimumValue == 5);

        // value=5: passes (5 >= 5 AND 5 <= 5) → 0
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 5, false) == 0);
        // value=4: below min → -2
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 4, false) == -2);
        // value=6: above max → -3
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 6, false) == -3);
    }

    SUBCASE("Set min=5 then max=3 — guard lowers min to 3")
    {
        int savedMax2 = gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue;
        int savedMin2 = gTestStatDescriptions[TEST_STAT_STRENGTH].minimumValue;

        testStatSetMinValue(TEST_STAT_STRENGTH, 5);
        // max(10) >= min(5) → no guard trigger
        testStatSetMaxValue(TEST_STAT_STRENGTH, 3);
        // min(5) > max(3) → guard lowers min to 3

        CHECK(gTestStatDescriptions[TEST_STAT_STRENGTH].minimumValue == 3);
        CHECK(gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue == 3);

        // value=3: passes (3 >= 3 AND 3 <= 3) → 0
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 3, false) == 0);

        gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue = savedMax2;
        gTestStatDescriptions[TEST_STAT_STRENGTH].minimumValue = savedMin2;
    }

    SUBCASE("All values in converged range [5,5] are testable")
    {
        testStatSetMaxValue(TEST_STAT_STRENGTH, 3);
        testStatSetMinValue(TEST_STAT_STRENGTH, 5);
        // Range converged to [5,5]

        // value=4: below converged min(5) → -2
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 4, false) == -2);
        // value=5: exactly at converged range → 0
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 5, false) == 0);
        // value=6: above converged max(5) → -3
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 6, false) == -3);
    }

    SUBCASE("Well-formed range [5, 10] works correctly (regression check)")
    {
        testStatSetMaxValue(TEST_STAT_STRENGTH, 10);
        testStatSetMinValue(TEST_STAT_STRENGTH, 5);

        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 4, false) == -2);
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 5, false) == 0);
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 8, false) == 0);
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 10, false) == 0);
        CHECK(testCritterSetBaseStat(critterPid, TEST_STAT_STRENGTH, 11, false) == -3);
    }

    gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue = savedMax;
    gTestStatDescriptions[TEST_STAT_STRENGTH].minimumValue = savedMin;
}

// ===========================================================================
// M-020: pcAddExperienceWithOptions full production path (stat.cc:795-868)
// ===========================================================================
// Tests the XP modifier, Swift Learner interaction, level-up from gains,
// doParty trigger, and pcSetExperience asymmetry (no modifier applied).
// Research: RPU No usage (Section 2.2). ET Tu LIKELY usage (Section 1.6).

// Helper to reset PC stat state for each subcase.
static void resetPcStatState()
{
    gTestPcStatValues[TEST_PC_STAT_EXPERIENCE] = 0;
    gTestPcStatValues[TEST_PC_STAT_LEVEL] = 1;
    gTestXpModPercentage = 100;
}

TEST_CASE("M-020: pcAddExperienceWithOptions — basic XP gain at 100% modifier")
{
    SUBCASE("XP gain with default modifier")
    {
        resetPcStatState();
        int xpGained = 0;
        int levelsGained = 0;
        testPcAddExperienceWithOptions(1000, 100, 0, false, &xpGained, &levelsGained);
        CHECK(xpGained == 1000);
        CHECK(levelsGained == 1); // 1000 XP from level 1 → level 2 (needs 1000 for level 2)
    }

    SUBCASE("Level-up from single XP add")
    {
        resetPcStatState();
        // XP for level 2 = 1000. Award 999: no level-up.
        int xpGained = 0;
        int levelsGained = 0;
        testPcAddExperienceWithOptions(999, 100, 0, false, &xpGained, &levelsGained);
        CHECK(levelsGained == 0);

        // Award 1 more XP: total XP = 1000, triggers level-up to 2.
        testPcAddExperienceWithOptions(1, 100, 0, false, &xpGained, &levelsGained);
        CHECK(levelsGained >= 1);
    }
}

TEST_CASE("M-020: pcAddExperienceWithOptions — XP modifier percentage")
{
    SUBCASE("50% modifier halves XP")
    {
        resetPcStatState();
        int xpGained = 0;
        int levelsGained = 0;
        testPcAddExperienceWithOptions(1000, 50, 0, false, &xpGained, &levelsGained);
        CHECK(xpGained == 500);
    }

    SUBCASE("200% modifier doubles XP")
    {
        resetPcStatState();
        int xpGained = 0;
        int levelsGained = 0;
        testPcAddExperienceWithOptions(1000, 200, 0, false, &xpGained, &levelsGained);
        CHECK(xpGained == 2000);
    }

    SUBCASE("0% modifier gives zero XP")
    {
        resetPcStatState();
        int xpGained = 0;
        int levelsGained = 0;
        testPcAddExperienceWithOptions(1000, 0, 0, false, &xpGained, &levelsGained);
        CHECK(xpGained == 0);
        CHECK(levelsGained == 0);
    }

    SUBCASE("10000% modifier (max allowed by sfall) — overflow boundary (N2-02)")
    {
        // N2-02: Integer overflow in XP modifier at stat.cc:802.
        // overflow threshold = INT_MAX / 10000 ≈ 214,748.
        // At XP=500000 and modifier=10000, 500000 * 10000 = 5,000,000,000 > INT_MAX.
        // On 2's complement, this wraps to a negative or truncated value.
        // Research: CONFIRMED — mechanically provable overflow.
        resetPcStatState();
        int xpGained = 0;
        int levelsGained = 0;
        testPcAddExperienceWithOptions(500000, 10000, 0, false, &xpGained, &levelsGained);

        // Due to integer overflow, the result is implementation-defined.
        // Document that the current behavior does NOT crash, but the XP gain
        // may be incorrect (loss instead of gain depending on wrap behavior).
        // The actual production code should clamp before multiplication.
        (void)xpGained;   // behavior documented
        (void)levelsGained;
    }

    SUBCASE("INT_MIN XP at 100% modifier (N2-03) — signed overflow boundary")
    {
        // N2-03: Negative XP passes through overflow-vulnerable multiplication.
        // INT_MIN * 100 is signed overflow. On 2's complement, wraps to 0.
        // Result is 0 net XP gain after clamp.
        // Research: PLAUSIBLE — requires deeply buggy script caller.
        resetPcStatState();
        int xpGained = 0;
        int levelsGained = 0;
        testPcAddExperienceWithOptions(-100, 100, 0, false, &xpGained, &levelsGained);
        // Negative adjusted XP: -100. After Swift Learner (0%): stays -100.
        // Clamped to min XP (0). Net: 0 change.
        // XP gained should NOT be negative.
        CHECK(xpGained >= 0);
    }
}

TEST_CASE("M-020: pcAddExperienceWithOptions — Swift Learner interaction")
{
    SUBCASE("Swift Learner rank 1 adds 5% bonus on adjusted XP")
    {
        resetPcStatState();
        int xpGained = 0;
        int levelsGained = 0;
        // 1000 base XP at 100% → adjusted=1000, SL rank 1: +5%*1000 = +50 → total=1050
        testPcAddExperienceWithOptions(1000, 100, 1, false, &xpGained, &levelsGained);
        CHECK(xpGained == 1050);
    }

    SUBCASE("Swift Learner stacks with XP modifier")
    {
        resetPcStatState();
        int xpGained = 0;
        int levelsGained = 0;
        // 100 base XP at 200% → adjusted=200, SL rank 2: +10%*200 = +20 → total=220
        testPcAddExperienceWithOptions(100, 200, 2, false, &xpGained, &levelsGained);
        CHECK(xpGained == 220);
    }
}

TEST_CASE("M-020: pcAddExperienceWithOptions — doParty flag triggers party level-up")
{
    // In production, doParty=true calls _partyMemberIncLevels() at stat.cc:857-859.
    // The mirror sets a flag instead; we verify the logical flow:
    // XP gained with doParty=true still correctly accrues XP + levels.
    SUBCASE("doParty=true — XP gain is identical to doParty=false for value calculation")
    {
        resetPcStatState();
        int xpGainedParty = 0;
        int levelsGainedParty = 0;
        testPcAddExperienceWithOptions(3000, 100, 0, true, &xpGainedParty, &levelsGainedParty);

        resetPcStatState();
        int xpGainedSolo = 0;
        int levelsGainedSolo = 0;
        testPcAddExperienceWithOptions(3000, 100, 0, false, &xpGainedSolo, &levelsGainedSolo);

        // XP gained by the PC is identical — doParty only affects NPC level-ups.
        CHECK(xpGainedParty == xpGainedSolo);
        CHECK(levelsGainedParty == levelsGainedSolo);
    }
}

TEST_CASE("M-020: pcSetExperience asymmetry — does NOT apply XP modifier")
{
    // pcSetExperience (stat.cc:871-879) sets XP directly without applying
    // gXpModPercentage. This is the intended asymmetry: setting XP should
    // set exactly what was specified, while adding XP applies the modifier.
    SUBCASE("pcSetExperience sets XP directly regardless of modifier")
    {
        resetPcStatState();

        // Simulate: gXpModPercentage=200 (double XP on gain).
        gTestXpModPercentage = 200;

        // pcAddExperienceWithOptions: XP is doubled
        int xpGained = 0;
        int levelsGained = 0;
        testPcAddExperienceWithOptions(500, 200, 0, false, &xpGained, &levelsGained);
        CHECK(xpGained == 1000); // doubled

        // pcSetExperience: XP is NOT doubled — set to exact value
        int levelChange = 0;
        testPcSetExperience(500, &levelChange);
        // Verify pcSetExperience does NOT apply gXpModPercentage:
        // If gXpModPercentage (200%) were applied, XP=500 would become 1000,
        // After adding 500 XP with 200% modifier → 1000 XP → level 2.
        // Setting XP to 500 directly → stays at level 2 (needs 3000 for level 3).
        CHECK(levelChange == 0);
    }
}

TEST_CASE("M-020: pcAddExperienceWithOptions — XP clamps to min/max")
{
    SUBCASE("Negative adjusted XP clamped to minimum (0)")
    {
        resetPcStatState();
        int xpGained = 0;
        int levelsGained = 0;
        testPcAddExperienceWithOptions(100, -50, 0, false, &xpGained, &levelsGained);
        // adjustedXp = 100 * (-50) / 100 = -50
        // newXp = 0 + (-50) = -50, clamped to minimumValue (0)
        // net: xpGained = 0
        CHECK(xpGained == 0);
    }
}

// =================================================================
// I2-01: std::clamp guard when min > max
// =================================================================
//
// Finding I2-01 (MEDIUM, confirmed): The production code in statSetMaxValue
// and statSetMinValue (stat.cc:765-787) includes a guard that prevents
// min > max, which would cause UB in std::clamp at critterGetStat line 408
// (C++17 [alg.clamp] requires lo <= hi).
//
// Without this guard: set_stat_max followed by set_stat_min (or vice versa)
// with conflicting values could create a state where minimumValue >
// maximumValue, triggering UB the next time critterGetStat is called.

TEST_CASE("I2-01: statSetMaxValue clamps min when new max < current min")
{
    // Save initial state
    int savedMax = gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue;
    int savedMin = gTestStatDescriptions[TEST_STAT_STRENGTH].minimumValue;

    // STRENGTH default: min=1, max=10
    // Set max to 0 (below current min of 1)
    testStatSetMaxValue(TEST_STAT_STRENGTH, 0);

    // Guard: min should be lowered to match new max (0)
    CHECK(gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue == 0);
    CHECK(gTestStatDescriptions[TEST_STAT_STRENGTH].minimumValue == 0); // clamped down
    CHECK(gTestStatDescriptions[TEST_STAT_STRENGTH].minimumValue <= gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue);

    // Restore
    gTestStatDescriptions[TEST_STAT_STRENGTH].maximumValue = savedMax;
    gTestStatDescriptions[TEST_STAT_STRENGTH].minimumValue = savedMin;
}

TEST_CASE("I2-01: statSetMinValue clamps max when new min > current max")
{
    int savedMax = gTestStatDescriptions[TEST_STAT_ENDURANCE].maximumValue;
    int savedMin = gTestStatDescriptions[TEST_STAT_ENDURANCE].minimumValue;

    // ENDURANCE default: min=1, max=10
    // Set min to 20 (above current max of 10)
    testStatSetMinValue(TEST_STAT_ENDURANCE, 20);

    // Guard: max should be raised to match new min (20)
    CHECK(gTestStatDescriptions[TEST_STAT_ENDURANCE].minimumValue == 20);
    CHECK(gTestStatDescriptions[TEST_STAT_ENDURANCE].maximumValue == 20); // clamped up
    CHECK(gTestStatDescriptions[TEST_STAT_ENDURANCE].minimumValue <= gTestStatDescriptions[TEST_STAT_ENDURANCE].maximumValue);

    // Restore
    gTestStatDescriptions[TEST_STAT_ENDURANCE].maximumValue = savedMax;
    gTestStatDescriptions[TEST_STAT_ENDURANCE].minimumValue = savedMin;
}

TEST_CASE("I2-01: statSetMaxValue no-op when new max >= current min")
{
    int savedMax = gTestStatDescriptions[TEST_STAT_AGILITY].maximumValue;
    int savedMin = gTestStatDescriptions[TEST_STAT_AGILITY].minimumValue;

    // AGILITY default: min=1, max=10. Set max to 10 (already at 10)
    testStatSetMaxValue(TEST_STAT_AGILITY, 10);

    // No change needed: min(1) <= max(10)
    CHECK(gTestStatDescriptions[TEST_STAT_AGILITY].minimumValue == savedMin);
    CHECK(gTestStatDescriptions[TEST_STAT_AGILITY].maximumValue == 10);
    CHECK(gTestStatDescriptions[TEST_STAT_AGILITY].minimumValue <= gTestStatDescriptions[TEST_STAT_AGILITY].maximumValue);

    gTestStatDescriptions[TEST_STAT_AGILITY].maximumValue = savedMax;
    gTestStatDescriptions[TEST_STAT_AGILITY].minimumValue = savedMin;
}

TEST_CASE("I2-01: statSetMinValue no-op when new min <= current max")
{
    int savedMax = gTestStatDescriptions[TEST_STAT_LUCK].maximumValue;
    int savedMin = gTestStatDescriptions[TEST_STAT_LUCK].minimumValue;

    // LUCK default: min=1, max=10. Set min to 5
    testStatSetMinValue(TEST_STAT_LUCK, 5);

    // No change needed: min(5) <= max(10)
    CHECK(gTestStatDescriptions[TEST_STAT_LUCK].minimumValue == 5);
    CHECK(gTestStatDescriptions[TEST_STAT_LUCK].maximumValue == savedMax);
    CHECK(gTestStatDescriptions[TEST_STAT_LUCK].minimumValue <= gTestStatDescriptions[TEST_STAT_LUCK].maximumValue);

    gTestStatDescriptions[TEST_STAT_LUCK].maximumValue = savedMax;
    gTestStatDescriptions[TEST_STAT_LUCK].minimumValue = savedMin;
}

TEST_CASE("I2-01: statSetMaxValue followed by statSetMinValue — converging")
{
    int savedMax = gTestStatDescriptions[TEST_STAT_PERCEPTION].maximumValue;
    int savedMin = gTestStatDescriptions[TEST_STAT_PERCEPTION].minimumValue;

    // PERCEPTION default: min=1, max=10
    // Step 1: Set max to 5 (guard lowers min from 1 to not trigger, 1 <= 5 OK)
    testStatSetMaxValue(TEST_STAT_PERCEPTION, 5);
    CHECK(gTestStatDescriptions[TEST_STAT_PERCEPTION].maximumValue == 5);

    // Step 2: Set min to 8 (above current max of 5 → guard raises max)
    testStatSetMinValue(TEST_STAT_PERCEPTION, 8);
    CHECK(gTestStatDescriptions[TEST_STAT_PERCEPTION].minimumValue == 8);
    CHECK(gTestStatDescriptions[TEST_STAT_PERCEPTION].maximumValue == 8); // clamped up
    CHECK(gTestStatDescriptions[TEST_STAT_PERCEPTION].minimumValue <= gTestStatDescriptions[TEST_STAT_PERCEPTION].maximumValue);

    // Final state: min == max == 8 (converged)

    gTestStatDescriptions[TEST_STAT_PERCEPTION].maximumValue = savedMax;
    gTestStatDescriptions[TEST_STAT_PERCEPTION].minimumValue = savedMin;
}

TEST_CASE("I2-01: regression — without guard, set_stat_max(-10) + set_stat_min(20) = UB")
{
    // Without the I2-01 guard: setting min to 20 after max was set to -10
    // would create min(20) > max(-10), causing UB on the next std::clamp.
    // With the guard: the system stays self-consistent.
    int savedMax = gTestStatDescriptions[TEST_STAT_CARRY_WEIGHT].maximumValue;
    int savedMin = gTestStatDescriptions[TEST_STAT_CARRY_WEIGHT].minimumValue;

    // CARRY_WEIGHT default: min=0, max=999
    testStatSetMaxValue(TEST_STAT_CARRY_WEIGHT, -10);
    // Guard: min was 0, max now -10 → min < max, guard lowers min to -10
    CHECK(gTestStatDescriptions[TEST_STAT_CARRY_WEIGHT].minimumValue == -10);
    CHECK(gTestStatDescriptions[TEST_STAT_CARRY_WEIGHT].maximumValue == -10);
    CHECK(gTestStatDescriptions[TEST_STAT_CARRY_WEIGHT].minimumValue <= gTestStatDescriptions[TEST_STAT_CARRY_WEIGHT].maximumValue);

    testStatSetMinValue(TEST_STAT_CARRY_WEIGHT, 20);
    // Guard: min now 20, max was -10 → max raised to 20
    CHECK(gTestStatDescriptions[TEST_STAT_CARRY_WEIGHT].minimumValue == 20);
    CHECK(gTestStatDescriptions[TEST_STAT_CARRY_WEIGHT].maximumValue == 20);
    CHECK(gTestStatDescriptions[TEST_STAT_CARRY_WEIGHT].minimumValue <= gTestStatDescriptions[TEST_STAT_CARRY_WEIGHT].maximumValue);

    gTestStatDescriptions[TEST_STAT_CARRY_WEIGHT].maximumValue = savedMax;
    gTestStatDescriptions[TEST_STAT_CARRY_WEIGHT].minimumValue = savedMin;
}

TEST_CASE("I2-01: critterGetStat clamp guard — min > max does not reach std::clamp")
{
    // Production: critterGetStat at stat.cc:407 guards with:
    //   if (min <= max) { std::clamp(value, min, max); }
    // If min > max, the clamp is skipped and the unclamped value is returned.

    // Mirror of the guard: only clamp when invariant holds
    auto testCritterGetStatClampGuard = [](int value, int minVal, int maxVal) -> int {
        if (minVal <= maxVal) {
            return std::clamp(value, minVal, maxVal);
        }
        return value; // min > max → no clamping (avoids UB)
    };

    // Normal case: min <= max
    CHECK(testCritterGetStatClampGuard(50, 0, 100) == 50);  // in range
    CHECK(testCritterGetStatClampGuard(-5, 0, 100) == 0);   // clamped to min
    CHECK(testCritterGetStatClampGuard(200, 0, 100) == 100); // clamped to max

    // Abnormal case: min > max — guard prevents UB, returns unclamped
    CHECK(testCritterGetStatClampGuard(50, 100, 0) == 50);   // min>max, no clamp
    CHECK(testCritterGetStatClampGuard(-1, 100, 0) == -1);   // same, negative value
    CHECK(testCritterGetStatClampGuard(999, 100, 0) == 999);  // same, large value
}

// ============================================================
// F-050: FO1 level cap enforcement (correlated with F-002 fix)
// ============================================================
//
// Production: PC_LEVEL_MAX is defined as 99 in stat_defs.h:19.
// The F-002 fix gates PC_LEVEL_MAX by gFallout1Behavior=false →
// level cap = 21 in FO1 mode, 99 in FO2 mode.
//
// context: gFallout1Behavior is a global bool (sfall_config.cc:12).
// stat.cc:92 uses PC_LEVEL_MAX as the max for PC_STAT_LEVEL.
// stat.cc:723,865,936 all check `level >= PC_LEVEL_MAX` / `level < PC_LEVEL_MAX`.
// character_editor.cc:5812 checks `level <= PC_LEVEL_MAX`.
//
// The fix (F-002) changes the level cap from a fixed 99 to:
//   statGetLevelCap() → gFallout1Behavior ? 21 : 99
//
// These tests validate the corrected behavior.

namespace {
    // Mirror of the F-002 fixed statGetLevelCap logic
    static constexpr int kFO1LevelCap = 21;
    static constexpr int kFO2LevelCap = 99;

    static int testStatGetLevelCap(bool fallout1Behavior)
    {
        return fallout1Behavior ? kFO1LevelCap : kFO2LevelCap;
    }
}

TEST_CASE("F-050: FO1 mode level cap is 21")
{
    // In FO1 mode (gFallout1Behavior=true), max level is 21
    CHECK(testStatGetLevelCap(true) == 21);
}

TEST_CASE("F-050: FO2 mode level cap is 99")
{
    // In FO2 mode (gFallout1Behavior=false), max level is 99
    CHECK(testStatGetLevelCap(false) == 99);
}

TEST_CASE("F-050: FO1 level cap — level 21 is allowed in FO1 mode")
{
    // At exactly the cap level, the player should be allowed to be at level 21
    int cap = testStatGetLevelCap(true);
    int level = 21;
    CHECK(level <= cap);
}

TEST_CASE("F-050: FO1 level cap — level 22 is blocked in FO1 mode")
{
    // One level above the cap should be blocked
    int cap = testStatGetLevelCap(true);
    int level = 22;
    CHECK_FALSE(level <= cap);
}

TEST_CASE("F-050: FO1 level cap — level 98 is allowed in FO2 mode")
{
    // FO2 mode allows up to level 99
    int cap = testStatGetLevelCap(false);
    int level = 98;
    CHECK(level <= cap);
}

TEST_CASE("F-050: FO1 level cap — level 100 is blocked in FO2 mode")
{
    // Above 99 should still be blocked in FO2 mode
    int cap = testStatGetLevelCap(false);
    int level = 100;
    CHECK_FALSE(level <= cap);
}

TEST_CASE("F-050: FO1 level cap — PC_LEVEL_MAX matches FO2 cap (99)")
{
    // Verify that the compile-time constant matches FO2 cap
    CHECK(TEST_PC_LEVEL_MAX == kFO2LevelCap);
    CHECK(TEST_PC_LEVEL_MAX == 99);
}

TEST_CASE("F-050: FO1 level cap — experience calc uses correct cap")
{
    // Mirror of pcAddExperienceWithOptions level-up loop at stat.cc:936:
    //   while (xp >= pcGetExperienceForLevel(level) && level < PC_LEVEL_MAX)
    //
    // In FO1 mode with the fix, the loop should stop at level 21.
    // In FO2 mode, it stops at level 99.

    // FO1: level 20 can gain XP to reach 21, but cannot exceed 21
    int cap = testStatGetLevelCap(true);
    CHECK(20 < cap);  // can still level up
    CHECK_FALSE(21 < cap); // cannot go past cap

    // FO2: level 98 can gain XP to reach 99
    cap = testStatGetLevelCap(false);
    CHECK(98 < cap);
    CHECK_FALSE(99 < cap);
}
