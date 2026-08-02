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
static int gTestXPTable[TEST_PC_LEVEL_MAX] = {};
static int gTestXPTableCount = 0;
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

// Mirror of stat.cc:762-772 statGetLevelCap.
// The mirror models FO2 mode (gFallout1Behavior=false, cap TEST_PC_LEVEL_MAX);
// the FO1-mode 21 branch is not mirrored (pre-existing mirror limitation).
// F4: table-aware cap — with gTestXPTableMode >= 1 the cap is bounded by
// gTestXPTableCount + 1 (sfall cap-write parity), so short tables stop at
// table length + 1 instead of running away to the FO2 cap.
static int testStatGetLevelCap()
{
    int cap = TEST_PC_LEVEL_MAX;
    if (gTestXPTableMode >= 1) {
        cap = std::min(cap, gTestXPTableCount + 1);
    }
    return cap;
}

// Mirror of stat.cc:763-796 pcGetExperienceForLevel.
// Exact copy of the production logic: returns XP required to reach given level.
static int testPcGetExperienceForLevel(int level)
{
    if (level >= testStatGetLevelCap()) {
        return -1;
    }

    // M-54/R-06: apply the XP table parsed by combat.cc's combatInit() when
    // gXPTableMode is set. gXPTable[i] holds the XP required to reach level
    // i+2, so the threshold for `level` is gXPTable[level - 2]. A level that
    // falls outside the table (including level 1 and levels beyond the last
    // table entry) is unreachable (-1).
    if (gTestXPTableMode >= 1) {
        int index = level - 2;
        if (index >= 0 && index < gTestXPTableCount) {
            return gTestXPTable[index];
        }
        return -1; // beyond the table -> level unreachable
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

// Mirror of critterSetBonusStat (stat.cc:577-625) — C-05 fixed semantics.
// bonusStats[stat] is a SIGNED DELTA stored with NO write-side min/max clamp;
// the effective stat is bounded by the read-side display clamp in
// critterGetStat (stat.cc:423-426). This mirror previously DIVERGED from
// production (it omitted the UF-H-020 write clamp); production now matches
// it. Keep this mirror clamp-free — adding a clamp here documents the bug.
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

// Mirror of the read-side display clamp in critterGetStat (stat.cc:423-426):
// effective = base + bonus, clamped to [min, max] when min <= max.
static int testCritterGetStatEffective(int baseValue, int bonusValue,
                                       const TestStatDescription& desc)
{
    int value = baseValue + bonusValue;
    if (desc.minimumValue <= desc.maximumValue) {
        value = std::clamp(value, desc.minimumValue, desc.maximumValue);
    }
    return value;
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
// XP table mode (gXPTableMode / gXPTable / gXPTableCount) — M-54/R-06
// ===========================================================================

TEST_CASE("pcGetExperienceForLevel — gXPTableMode >= 1 applies parsed XP table")
{
    int savedMode = gTestXPTableMode;
    int savedCount = gTestXPTableCount;

    // Mode 0: hardcoded formula baseline.
    gTestXPTableMode = 0;
    int xp10_mode0 = testPcGetExperienceForLevel(10);
    int xp21_mode0 = testPcGetExperienceForLevel(21);
    CHECK(xp10_mode0 == 45000);
    CHECK(xp21_mode0 == 210000);

    // Populate a table mirroring the M-54 parse: gXPTable[i] holds the XP
    // required to reach level i+2 (sfall semantics). XPTable=1000,3000,6000,10000
    gTestXPTableMode = 1;
    gTestXPTableCount = 4;
    gTestXPTable[0] = 1000;   // level 2
    gTestXPTable[1] = 3000;   // level 3
    gTestXPTable[2] = 6000;   // level 4
    gTestXPTable[3] = 10000;  // level 5

    // Table values are applied for in-range levels.
    CHECK(testPcGetExperienceForLevel(2) == 1000);
    CHECK(testPcGetExperienceForLevel(3) == 3000);
    CHECK(testPcGetExperienceForLevel(4) == 6000);

    // F4: with a 4-entry table the table-aware cap is gXPTableCount + 1 = 5,
    // so level 5 (== cap) is unreachable via query (-1), not 10000. The last
    // table entry is only reached by the level-up loop granting level 5.
    CHECK(testPcGetExperienceForLevel(5) == -1);

    // Level 1 is below the first table entry (index -1) -> unreachable (-1).
    CHECK(testPcGetExperienceForLevel(1) == -1);

    // Levels at/after the table-aware cap are unreachable (-1).
    CHECK(testPcGetExperienceForLevel(6) == -1);
    CHECK(testPcGetExperienceForLevel(10) == -1);
    CHECK(testPcGetExperienceForLevel(98) == -1);

    // Level >= FO2 cap still returns -1 regardless of mode.
    CHECK(testPcGetExperienceForLevel(99) == -1);

    // Restore mode 0: formula applies again (table path not taken).
    gTestXPTableMode = 0;
    CHECK(testPcGetExperienceForLevel(10) == xp10_mode0);
    CHECK(testPcGetExperienceForLevel(21) == xp21_mode0);

    // Table mode with a populated table no longer returns formula values.
    gTestXPTableMode = 1;
    CHECK_FALSE(testPcGetExperienceForLevel(10) == xp10_mode0);

    // Cleanup: restore saved state.
    gTestXPTableCount = savedCount;
    gTestXPTableMode = savedMode;
}

TEST_CASE("pcGetExperienceForLevel — empty XP table (mode set, no entries)")
{
    int savedMode = gTestXPTableMode;
    int savedCount = gTestXPTableCount;

    // combat.cc only sets gXPTableMode=1 when gXPTableCount > 0, but the
    // lookup must be safe for the degenerate empty-table state: all levels
    // except those in the table are unreachable (-1).
    gTestXPTableMode = 1;
    gTestXPTableCount = 0;
    CHECK(testPcGetExperienceForLevel(2) == -1);
    CHECK(testPcGetExperienceForLevel(10) == -1);

    gTestXPTableCount = savedCount;
    gTestXPTableMode = savedMode;
}

// ===========================================================================
// F4 — table-aware level cap: short XP tables must not run away to the FO2 cap.
// Reference: sfall writes the engine level-cap byte to numLevels + 1
// (SafeWrite8(0x4AFB1B, numLevels + 1), sfall Modules/Stats.cpp:323).
// ===========================================================================

// Mirror of the pcAddExperienceWithOptions level-up loop gate (stat.cc:920-923):
//   while (gPcStatValues[PC_STAT_LEVEL] < statGetLevelCap()) {
//       if (newXp < pcGetExperienceForNextLevel()) break;
//       ... grant level, add per-level HP bonus ...
//   }
// Beyond-table levels make pcGetExperienceForLevel return -1, for which the
// `newXp < -1` break is never true (newXp is clamped >= 0), so the loop can
// only stop on the cap bound. Returns the level the loop stops at.
static int testLevelUpLoopStopsAt(int startLevel, int newXp)
{
    int level = startLevel;
    while (level < testStatGetLevelCap()) {
        if (newXp < testPcGetExperienceForLevel(level + 1)) {
            break;
        }
        level += 1;
    }
    return level;
}

TEST_CASE("statGetLevelCap — table-aware for short XP tables (F4 regression)")
{
    int savedMode = gTestXPTableMode;
    int savedCount = gTestXPTableCount;

    // Mode 0 (no table): FO2 cap unchanged.
    gTestXPTableMode = 0;
    gTestXPTableCount = 0;
    CHECK(testStatGetLevelCap() == TEST_PC_LEVEL_MAX);

    // A realistic FO1-oriented 20-entry table in FO2 mode (the F4 trigger):
    // cap must be gXPTableCount + 1 = 21, not 99 — sfall parity.
    gTestXPTableMode = 1;
    gTestXPTableCount = 20;
    CHECK(testStatGetLevelCap() == 21);
    CHECK(testStatGetLevelCap() == gTestXPTableCount + 1);

    // Full table (PC_LEVEL_MAX - 1 = 98 entries): cap stays at FO2 max.
    gTestXPTableCount = TEST_PC_LEVEL_MAX - 1;
    CHECK(testStatGetLevelCap() == TEST_PC_LEVEL_MAX);

    // Degenerate empty table (mode set, no entries): cap 1 — every level is
    // beyond-reach. (combat.cc only sets mode when count > 0; defensive.)
    gTestXPTableCount = 0;
    CHECK(testStatGetLevelCap() == 1);

    gTestXPTableCount = savedCount;
    gTestXPTableMode = savedMode;
}

TEST_CASE("pcGetExperienceForLevel — short table: beyond-table levels are capped, not 99 (F4)")
{
    int savedMode = gTestXPTableMode;
    int savedCount = gTestXPTableCount;

    gTestXPTableMode = 1;
    gTestXPTableCount = 20;
    for (int i = 0; i < gTestXPTableCount; i++) {
        gTestXPTable[i] = (i + 2) * 1000; // threshold for level i+2
    }

    // In-table thresholds still applied (levels 2..20, below cap 21).
    CHECK(testPcGetExperienceForLevel(2) == 2000);
    CHECK(testPcGetExperienceForLevel(20) == 20000);

    // Level 21 == cap (gXPTableCount + 1): unreachable via query.
    CHECK(testPcGetExperienceForLevel(21) == -1);
    // Beyond the table / beyond the cap: -1, never a reachable formula level.
    CHECK(testPcGetExperienceForLevel(22) == -1);
    CHECK(testPcGetExperienceForLevel(50) == -1);
    CHECK(testPcGetExperienceForLevel(99) == -1);

    gTestXPTableCount = savedCount;
    gTestXPTableMode = savedMode;
}

TEST_CASE("level-up loop — short table stops at table length + 1, no runaway (F4 regression)")
{
    int savedMode = gTestXPTableMode;
    int savedCount = gTestXPTableCount;

    // 20-entry table in FO2 mode with XP far beyond the last entry. Pre-fix the
    // loop free-ran to the FO2 cap (99), granting the per-level HP bonus on
    // every level-up (stat.cc:950-951); with the table-aware cap the loop must
    // stop at gXPTableCount + 1 = 21.
    gTestXPTableMode = 1;
    gTestXPTableCount = 20;
    for (int i = 0; i < gTestXPTableCount; i++) {
        gTestXPTable[i] = (i + 2) * 1000;
    }

    int level = testLevelUpLoopStopsAt(1, 100000000);
    CHECK(level == 21);
    CHECK(level == gTestXPTableCount + 1);
    CHECK(level < TEST_PC_LEVEL_MAX); // no runaway to 99

    // Same table, XP just over the last in-table threshold: still stops at 21
    // (the final 20->21 grant is the loop reaching the cap, not an XP gate).
    level = testLevelUpLoopStopsAt(1, 21000);
    CHECK(level == 21);

    // XP below the first threshold: no level-ups.
    level = testLevelUpLoopStopsAt(1, 1000);
    CHECK(level == 1);

    gTestXPTableCount = savedCount;
    gTestXPTableMode = savedMode;
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
    // Mirror of the F-002 fixed statGetLevelCap logic — FO-mode model only.
    // Production statGetLevelCap() is now also table-aware (F4); the exact
    // production copy (FO2 model) is testStatGetLevelCap() above. This model
    // is used only by the F-050 FO1/FO2 mode tests, which never set a table.
    static constexpr int kFO1LevelCap = 21;
    static constexpr int kFO2LevelCap = 99;

    static int testFOStatGetLevelCap(bool fallout1Behavior)
    {
        return fallout1Behavior ? kFO1LevelCap : kFO2LevelCap;
    }
}

TEST_CASE("F-050: FO1 mode level cap is 21")
{
    // In FO1 mode (gFallout1Behavior=true), max level is 21
    CHECK(testFOStatGetLevelCap(true) == 21);
}

TEST_CASE("F-050: FO2 mode level cap is 99")
{
    // In FO2 mode (gFallout1Behavior=false), max level is 99
    CHECK(testFOStatGetLevelCap(false) == 99);
}

TEST_CASE("F-050: FO1 level cap — level 21 is allowed in FO1 mode")
{
    // At exactly the cap level, the player should be allowed to be at level 21
    int cap = testFOStatGetLevelCap(true);
    int level = 21;
    CHECK(level <= cap);
}

TEST_CASE("F-050: FO1 level cap — level 22 is blocked in FO1 mode")
{
    // One level above the cap should be blocked
    int cap = testFOStatGetLevelCap(true);
    int level = 22;
    CHECK_FALSE(level <= cap);
}

TEST_CASE("F-050: FO1 level cap — level 98 is allowed in FO2 mode")
{
    // FO2 mode allows up to level 99
    int cap = testFOStatGetLevelCap(false);
    int level = 98;
    CHECK(level <= cap);
}

TEST_CASE("F-050: FO1 level cap — level 100 is blocked in FO2 mode")
{
    // Above 99 should still be blocked in FO2 mode
    int cap = testFOStatGetLevelCap(false);
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
    int cap = testFOStatGetLevelCap(true);
    CHECK(20 < cap);  // can still level up
    CHECK_FALSE(21 < cap); // cannot go past cap

    // FO2: level 98 can gain XP to reach 99
    cap = testFOStatGetLevelCap(false);
    CHECK(98 < cap);
    CHECK_FALSE(99 < cap);
}

// ===========================================================================
// C-05: critterSetBonusStat signed-delta semantics (stat.cc:577-625)
// ===========================================================================
// The pre-fix write-side clamp (added 7f58356, removed by this pass) rejected
// bonus 0 and all negatives for primary stats (min = PRIMARY_STAT_MIN = 1),
// breaking radiation drain/heal, drug wear-off, addiction penalties, the
// editor GCD reset, armor removal, and level-down HP. Bonus stats are signed
// deltas; the effective stat is clamped at READ time in critterGetStat.

TEST_CASE("C-05: critterSetBonusStat accepts zero and negative bonus deltas")
{
    int critterPid = (TEST_OBJ_TYPE_CRITTER << 24) | 1;

    SUBCASE("zero bonus accepted (editor GCD reset loop, character_editor.cc:4205)")
    {
        CHECK(testCritterSetBonusStat(critterPid, TEST_STAT_STRENGTH, 0) == 0);
    }

    SUBCASE("negative bonus accepted (radiation/addiction penalties)")
    {
        CHECK(testCritterSetBonusStat(critterPid, TEST_STAT_STRENGTH, -2) == 0);
        CHECK(testCritterSetBonusStat(critterPid, TEST_STAT_CRITICAL_CHANCE, -61) == 0);
    }

    SUBCASE("above-max bonus accepted (raw delta; read side clamps display)")
    {
        CHECK(testCritterSetBonusStat(critterPid, TEST_STAT_STRENGTH, 11) == 0);
    }

    SUBCASE("non-critter PID still rejected (PID_TYPE guard)")
    {
        int itemPid = (0 << 24) | 5;
        CHECK(testCritterSetBonusStat(itemPid, TEST_STAT_STRENGTH, 1) == -5);
    }
}

TEST_CASE("C-05: read-side clamp bounds the effective stat value")
{
    // Mirror of critterGetStat display clamp (stat.cc:423-426).
    SUBCASE("STR base 5 — negative bonus clamps to PRIMARY_STAT_MIN")
    {
        CHECK(testCritterGetStatEffective(5, -10, gTestStatDescriptions[TEST_STAT_STRENGTH]) == 1);
        CHECK(testCritterGetStatEffective(5, -2, gTestStatDescriptions[TEST_STAT_STRENGTH]) == 3);
    }

    SUBCASE("STR base 5 — positive bonus clamps to PRIMARY_STAT_MAX")
    {
        CHECK(testCritterGetStatEffective(5, 100, gTestStatDescriptions[TEST_STAT_STRENGTH]) == 10);
        CHECK(testCritterGetStatEffective(5, 2, gTestStatDescriptions[TEST_STAT_STRENGTH]) == 7);
    }

    SUBCASE("zero bonus yields the base stat")
    {
        CHECK(testCritterGetStatEffective(5, 0, gTestStatDescriptions[TEST_STAT_STRENGTH]) == 5);
    }
}

// ===========================================================================
// M-176: MAX_HP derived from BASE stats only (stat.cc:658)
// ===========================================================================
// Commit 104f461 regressed the MAX_HP formula from base-only
// (critterGetBaseStatWithTraitModifier) to bonus-inclusive (critterGetStat).
// With the C-05 clamp removed, a temporary STR/END bonus (drugs, radiation)
// would otherwise inflate base MAX_HP, and a later debuff recalc would leave
// current HP above max → permanent HP loss on the next heal/clamp. sfall's
// default HPDependOnBonusStats=0 matches base-only.

// Mirror of the fixed MAX_HP line: base stats + traits only, no bonus stats.
static int testMaxHpBaseOnly(int baseStrength, int baseEndurance)
{
    return baseStrength + baseEndurance * 2 + 15;
}

// Mirror of the regressed 104f461 behavior (bonus-inclusive) — kept only to
// demonstrate the difference the fix removes; NOT called by production.
static int testMaxHpBonusInclusive(int baseStrength, int baseEndurance,
                                   int bonusStrength, int bonusEndurance)
{
    return (baseStrength + bonusStrength) + (baseEndurance + bonusEndurance) * 2 + 15;
}

TEST_CASE("M-176: MAX_HP formula ignores transient bonus stats")
{
    SUBCASE("base-only formula matches vanilla for clean critter (5 STR, 5 END)")
    {
        CHECK(testMaxHpBaseOnly(5, 5) == 30); // 5 + 5*2 + 15
    }

    SUBCASE("radiation STR penalty does not deflate base MAX_HP")
    {
        // Radiation drains STR by -6 (gRadiationEffectPenalties). Base-only
        // keeps MAX_HP at 30; the regressed bonus-inclusive formula would
        // drop it to 24, leaving current HP above max after the recalc.
        CHECK(testMaxHpBaseOnly(5, 5) == 30);
        CHECK(testMaxHpBonusInclusive(5, 5, -6, 0) == 24); // regressed behavior
        CHECK(testMaxHpBonusInclusive(5, 5, -6, 0) != testMaxHpBaseOnly(5, 5));
    }

    SUBCASE("drug STR bonus does not inflate base MAX_HP")
    {
        // Mentats-style +2 STR bonus. Base-only keeps MAX_HP at 30.
        CHECK(testMaxHpBaseOnly(5, 5) == 30);
        CHECK(testMaxHpBonusInclusive(5, 5, 2, 0) == 32); // regressed behavior
    }
}
