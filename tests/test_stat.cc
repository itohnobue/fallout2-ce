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
static void testStatSetMaxValue(int stat, int value)
{
    if (testStatIsValid(stat)) {
        gTestStatDescriptions[stat].maximumValue = value;
    }
}

// Mirror of stat.cc:751-756 statSetMinValue.
static void testStatSetMinValue(int stat, int value)
{
    if (testStatIsValid(stat)) {
        gTestStatDescriptions[stat].minimumValue = value;
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
    CHECK(TEST_STAT_COUNT == 41);       // 7 SPECIAL + 26 secondary + 3 current pseudostats
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
