// Unit tests for perk.h — perk description data accessor semantics.
//
// Tests the sfall-integration perk accessor functions that are standalone-
// testable (pure data reads/writes on static global arrays):
//   - perkSetMinLevel(int, int) — sets gPerkDescriptions[perk].minLevel
//   - perkGetMinLevel(int)      — reads gPerkDescriptions[perk].minLevel
//   - perkGetMaxRank(int)       — reads gPerkDescriptions[perk].maxRank
//   - perkIsValid(int)          — bounds check (inline in perk.h:28-31)
//
// These use test-local mirrors of the production PerkDescription struct
// since perk.cc links to 30+ engine files. The functions map exactly to
// the production code (perk.cc:570-594, perk.h:28-31).
//
// Production function references:
//   perkSetMinLevel() → perk.cc:570-576
//   perkGetMinLevel() → perk.cc:579-585
//   perkGetMaxRank()  → perk.cc:588-594
//   perkIsValid()     → perk.h:28-31
//   perkGetRankData() → perk.cc:284-299 (nullptr fix F-01)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

// ============================================================
// Test-local type definitions mirroring perk.cc types
// ============================================================
namespace fallout {

// Constants from perk_defs.h
enum {
    TEST_PERK_COUNT = 126,  // PERK_COUNT from perk_defs.h:126
};

// Constants from stat_defs.h
enum {
    TEST_PRIMARY_STAT_COUNT = 7,
};

// PerkDescription mirrors perk.cc:23-45 (simplified to tested fields)
typedef struct TestPerkDescription {
    char* name;
    char* description;
    int frmId;
    int maxRank;
    int minLevel;
    int stat;
    int statModifier;
    int param1;
    int value1;
    int paramMode;
    int param2;
    int value2;
    int stats[TEST_PRIMARY_STAT_COUNT];
} TestPerkDescription;

} // namespace fallout

using namespace fallout;

// ============================================================
// Test-local global state (mirrors perk.cc statics)
// ============================================================
static TestPerkDescription gPerkDescriptions[TEST_PERK_COUNT];

// ============================================================
// Test-local function mirrors (exact algorithms from production)
// ============================================================

// Mirrors perk.h:28-31 perkIsValid
static bool testPerkIsValid(int perk)
{
    return perk >= 0 && perk < TEST_PERK_COUNT;
}

// Mirrors perk.cc:570-576 perkSetMinLevel
static void testPerkSetMinLevel(int perk, int minLevel)
{
    if (!testPerkIsValid(perk)) {
        return;
    }
    gPerkDescriptions[perk].minLevel = minLevel;
}

// Mirrors perk.cc:579-585 perkGetMinLevel
static int testPerkGetMinLevel(int perk)
{
    if (!testPerkIsValid(perk)) {
        return 0;
    }
    return gPerkDescriptions[perk].minLevel;
}

// Mirrors perk.cc:588-594 perkGetMaxRank
static int testPerkGetMaxRank(int perk)
{
    if (!testPerkIsValid(perk)) {
        return -1;
    }
    return gPerkDescriptions[perk].maxRank;
}

// Mirrors perk.cc:302-369 perkCanAdd (simplified — full param1/param2 logic).
// M-023: Tests the null-guard chain for non-party-member critters.
// When perkGetRankData returns nullptr (critter not in party member list),
// the rank check is skipped, but level/stat/skill requirements still apply.

// Minimal Object type for critter identification
typedef struct TestCritterObj {
    int pid;
    int id;
} TestCritterObj;

// Mock of pcGetStat for level check (perk.cc:320).
static int testPcGetStatLevel = 1;
static int testPcGetStat(int stat)
{
    if (stat == 1) { // PC_STAT_LEVEL = 1 in our test enum
        return testPcGetStatLevel;
    }
    return 0;
}

// Mock skill lookup table for perk requirements.
static int gTestSkillValues[256] = {};

// Mock of skillGetValue.
static int testSkillGetValue(TestCritterObj* critter, int skill)
{
    if (skill >= 0 && skill < 256) {
        return gTestSkillValues[skill];
    }
    return 0;
}

// Mock global var table.
static int gTestGlobalVars[256] = {};

// Mock of gameGetGlobalVar.
static int testGameGetGlobalVar(int varNum)
{
    if (varNum >= 0 && varNum < 256) {
        return gTestGlobalVars[varNum];
    }
    return 0;
}

// Mirrors the nullptr-guard + rank/level/skill check chain from perk.cc:302-369.
// Simplified to test the 3 key branch patterns:
//   (a) ranksData==nullptr → rank check skipped (lines 314-317)
//   (b) critter==gDude → level check enforced (lines 319-323)
//   (c) non-dude → level check SKIPPED (line 319 condition)
static bool testPerkCanAdd(TestCritterObj* critter, int perk,
    bool isPartyMember, int currentRank, int minLevel)
{
    if (!testPerkIsValid(perk)) {
        return false;
    }

    int maxRank = gPerkDescriptions[perk].maxRank;
    if (maxRank == -1) {
        return false;
    }

    // M-023: Null-guard on perkGetRankData.
    // For non-party-member critters, perkGetRankData returns nullptr.
    // The null check skips the rank cap enforcement entirely.
    if (isPartyMember) {
        if (currentRank >= maxRank) {
            return false;
        }
    }
    // else: ranksData==nullptr, rank check skipped — any rank passes.

    // Level check only for gDude (perk.cc:319-322).
    // Non-dude critters skip this check entirely.
    bool isDude = (critter->id == 0); // id 0 = gDude in test convention
    if (isDude) {
        if (testPcGetStat(1 /* PC_STAT_LEVEL */) < minLevel) {
            return false;
        }
    }

    // Stat/skill requirements (simplified from perk.cc:325-369).
    // For M-023, we verify the requirement paths work for both
    // dude and non-dude critters.
    int param1 = gPerkDescriptions[perk].param1;
    if (param1 != -1) {
        int reqSkill = param1;  // simplified: no GVAR flag handling
        int reqValue = gPerkDescriptions[perk].value1;
        if (reqValue > 0) {
            if (testSkillGetValue(critter, reqSkill) < reqValue) {
                return false;
            }
        }
    }

    return true;
}

static void resetPerkCanAddState()
{
    memset(gPerkDescriptions, 0, sizeof(gPerkDescriptions));
    testPcGetStatLevel = 1;
    memset(gTestSkillValues, 0, sizeof(gTestSkillValues));
    memset(gTestGlobalVars, 0, sizeof(gTestGlobalVars));
}

// ============================================================
// perkIsValid tests
// ============================================================
TEST_CASE("perkIsValid — bounds check")
{
    SUBCASE("valid perks: 0 to PERK_COUNT-1")
    {
        CHECK(testPerkIsValid(0));
        CHECK(testPerkIsValid(1));
        CHECK(testPerkIsValid(TEST_PERK_COUNT - 1));
    }

    SUBCASE("negative perk returns false")
    {
        CHECK_FALSE(testPerkIsValid(-1));
        CHECK_FALSE(testPerkIsValid(-100));
    }

    SUBCASE("perk >= PERK_COUNT returns false")
    {
        CHECK_FALSE(testPerkIsValid(TEST_PERK_COUNT));
        CHECK_FALSE(testPerkIsValid(TEST_PERK_COUNT + 1));
        CHECK_FALSE(testPerkIsValid(9999));
    }

    SUBCASE("boundary values")
    {
        CHECK(testPerkIsValid(TEST_PERK_COUNT - 1));
        CHECK_FALSE(testPerkIsValid(TEST_PERK_COUNT));
    }
}

// ============================================================
// perkSetMinLevel / perkGetMinLevel tests
// ============================================================
TEST_CASE("perkSetMinLevel / perkGetMinLevel — roundtrip")
{
    SUBCASE("set and get a min level")
    {
        resetPerkCanAddState();
        testPerkSetMinLevel(0, 3);
        CHECK(testPerkGetMinLevel(0) == 3);
    }

    SUBCASE("set multiple perks independently")
    {
        resetPerkCanAddState();
        testPerkSetMinLevel(0, 3);
        testPerkSetMinLevel(1, 6);
        testPerkSetMinLevel(2, 9);

        CHECK(testPerkGetMinLevel(0) == 3);
        CHECK(testPerkGetMinLevel(1) == 6);
        CHECK(testPerkGetMinLevel(2) == 9);
    }

    SUBCASE("overwrite existing value")
    {
        resetPerkCanAddState();
        testPerkSetMinLevel(0, 3);
        CHECK(testPerkGetMinLevel(0) == 3);

        testPerkSetMinLevel(0, 12);
        CHECK(testPerkGetMinLevel(0) == 12);
    }

    SUBCASE("invalid perk is silently ignored for set")
    {
        resetPerkCanAddState();
        testPerkSetMinLevel(TEST_PERK_COUNT, 42);  // out of bounds
        // Should not crash; minLevel of valid perks unchanged
        CHECK(testPerkGetMinLevel(0) == 0);  // default from reset
    }

    SUBCASE("invalid perk returns 0 for get")
    {
        resetPerkCanAddState();
        CHECK(testPerkGetMinLevel(-1) == 0);
        CHECK(testPerkGetMinLevel(TEST_PERK_COUNT) == 0);
    }

    SUBCASE("non-standard min levels (high values)")
    {
        resetPerkCanAddState();
        testPerkSetMinLevel(10, 24);    // Sniper perk has level 24 in vanilla
        testPerkSetMinLevel(24, 310);   // Special perk level (used by SPECIAL perks)
        testPerkSetMinLevel(53, 12);    // Mysterious Stranger (level 12 in some mods)

        CHECK(testPerkGetMinLevel(10) == 24);
        CHECK(testPerkGetMinLevel(24) == 310);
        CHECK(testPerkGetMinLevel(53) == 12);
    }

    SUBCASE("sfall opcode use case: set_perk_level (0x817A)")
    {
        // Simulates what the sfall opcode does: change min level at runtime.
        // This is used by mods to adjust perk availability.
        resetPerkCanAddState();

        // Initial state: PERK_AWARENESS has vanilla minLevel=3
        testPerkSetMinLevel(0, 3);
        CHECK(testPerkGetMinLevel(0) == 3);

        // Mod changes it to level 1
        testPerkSetMinLevel(0, 1);
        CHECK(testPerkGetMinLevel(0) == 1);

        // Mod changes PERK_BONUS_HTH_ATTACKS from 3 to 6
        testPerkSetMinLevel(1, 6);
        CHECK(testPerkGetMinLevel(1) == 6);
        CHECK(testPerkGetMinLevel(0) == 1); // unchanged
    }
}

// ============================================================
// perkGetMaxRank tests
// ============================================================
TEST_CASE("perkGetMaxRank — rank accessor")
{
    SUBCASE("returns maxRank for known perks")
    {
        resetPerkCanAddState();

        // Set up some known perks with their vanilla maxRanks
        gPerkDescriptions[0].maxRank = 1;   // Awareness
        gPerkDescriptions[1].maxRank = 1;   // Bonus HtH Attacks
        gPerkDescriptions[2].maxRank = 3;   // Bonus HtH Damage (3 ranks)
        gPerkDescriptions[5].maxRank = 1;   // Bonus RoF
        gPerkDescriptions[6].maxRank = 3;   // Earlier Sequence (3 ranks)

        CHECK(testPerkGetMaxRank(0) == 1);
        CHECK(testPerkGetMaxRank(1) == 1);
        CHECK(testPerkGetMaxRank(2) == 3);
        CHECK(testPerkGetMaxRank(5) == 1);
        CHECK(testPerkGetMaxRank(6) == 3);
    }

    SUBCASE("returns -1 for invalid perks")
    {
        resetPerkCanAddState();
        CHECK(testPerkGetMaxRank(-1) == -1);
        CHECK(testPerkGetMaxRank(TEST_PERK_COUNT) == -1);
    }

    SUBCASE("maxRank=-1 means perk has no ranks (unavailable)")
    {
        resetPerkCanAddState();
        // Several perks in the vanilla game have maxRank=-1, meaning they
        // cannot be taken normally (e.g., addictions, special perks)
        gPerkDescriptions[50].maxRank = -1;  // Nuka-Cola Addiction
        gPerkDescriptions[51].maxRank = -1;  // Buffout Addiction

        CHECK(testPerkGetMaxRank(50) == -1);
        CHECK(testPerkGetMaxRank(51) == -1);
    }

    SUBCASE("maxRank=0 edge case (not used in vanilla but valid)")
    {
        resetPerkCanAddState();
        gPerkDescriptions[0].maxRank = 0;
        CHECK(testPerkGetMaxRank(0) == 0);
    }

    SUBCASE("all perks accessible")
    {
        resetPerkCanAddState();
        // Set all perks to have maxRank = 1
        for (int i = 0; i < TEST_PERK_COUNT; i++) {
            gPerkDescriptions[i].maxRank = 1;
        }

        // Verify all entries are accessible
        for (int i = 0; i < TEST_PERK_COUNT; i++) {
            CHECK(testPerkGetMaxRank(i) == 1);
        }
    }
}

// ============================================================
// PerkDescription struct layout tests
// ============================================================
TEST_CASE("PerkDescription struct layout")
{
    resetPerkCanAddState();

    SUBCASE("size and field count match (PERK_COUNT=126)")
    {
        CHECK(TEST_PERK_COUNT == 126);
        CHECK(sizeof(gPerkDescriptions) == TEST_PERK_COUNT * sizeof(TestPerkDescription));
    }

    SUBCASE("name pointer field")
    {
        char testName[] = "Awareness";
        gPerkDescriptions[0].name = testName;
        CHECK(gPerkDescriptions[0].name == testName);
    }

    SUBCASE("description pointer field")
    {
        char testDesc[] = "You are always aware of...";
        gPerkDescriptions[0].description = testDesc;
        CHECK(gPerkDescriptions[0].description == testDesc);
    }

    SUBCASE("frmId field")
    {
        gPerkDescriptions[0].frmId = 72;
        CHECK(gPerkDescriptions[0].frmId == 72);
    }

    SUBCASE("stat modifier fields")
    {
        gPerkDescriptions[1].stat = 11;
        gPerkDescriptions[1].statModifier = 2;
        CHECK(gPerkDescriptions[1].stat == 11);
        CHECK(gPerkDescriptions[1].statModifier == 2);
    }

    SUBCASE("stats array boundary check")
    {
        // Verify PRIMARY_STAT_COUNT fits in the stats array
        CHECK(TEST_PRIMARY_STAT_COUNT == 7);

        for (int i = 0; i < TEST_PRIMARY_STAT_COUNT; i++) {
            gPerkDescriptions[0].stats[i] = i * 10;
        }
        for (int i = 0; i < TEST_PRIMARY_STAT_COUNT; i++) {
            CHECK(gPerkDescriptions[0].stats[i] == i * 10);
        }
    }
}

// ============================================================
// Regression: F-01 — perkGetRankData returns nullptr for non-members
// ============================================================
TEST_CASE("Regression: F-01 — perkGetRankData nullptr safety")
{
    // The original bug: perkGetRankData returned gPartyMemberPerkRanks
    // (index 0 = dude's data) when no PID match was found. This meant
    // any critter not in the party member list could read/write dude's
    // perks. The fix returns nullptr and guards all call sites.
    //
    // This test validates the *pattern*: when data is not found, return
    // a safe sentinel (0 or -1) rather than leaking another entity's data.

    SUBCASE("perkGetMinLevel returns 0 for invalid — safe default")
    {
        // Shows that the invalid-perk path returns a safe default (0),
        // not some other perk's data.
        CHECK(testPerkGetMinLevel(-1) == 0);
        CHECK(testPerkGetMinLevel(TEST_PERK_COUNT) == 0);
    }

    SUBCASE("perkGetMaxRank returns -1 for invalid — safe sentinel")
    {
        // Shows that invalid perks return -1 (meaning "no ranks"),
        // not some other perk's maxRank.
        CHECK(testPerkGetMaxRank(-1) == -1);
        CHECK(testPerkGetMaxRank(TEST_PERK_COUNT) == -1);
    }

    SUBCASE("perkSetMinLevel silently ignores invalid — no corruption")
    {
        resetPerkCanAddState();

        // Set a known valid value
        testPerkSetMinLevel(0, 3);
        CHECK(testPerkGetMinLevel(0) == 3);

        // Try to set an invalid perk — should be a no-op
        testPerkSetMinLevel(TEST_PERK_COUNT, 99);

        // Valid data unchanged
        CHECK(testPerkGetMinLevel(0) == 3);
    }

    SUBCASE("nullptr guard pattern: all accessors validate before access")
    {
        // The production bug fix (F-01) guards every call site of
        // perkGetRankData with nullptr checks. This test validates
        // that our accessor mirrors also safely handle out-of-bounds.
        //
        // In the real code:
        //   perkAdd (perk.cc:445-447): if (ranksData == nullptr) return -1;
        //   perkAddForce (perk.cc:465-467): if (ranksData == nullptr) return -1;
        //   perkRemove (perk.cc:493-495): if (ranksData == nullptr) return -1;
        //   perkGetRank (perk.cc:534-536): if (ranksData == nullptr) return 0;
        //   perkAddEffect (perk.cc:618-620): if (ranksData == nullptr) return;

        // The bounds check in our accessors is the equivalent guard pattern
        CHECK(testPerkGetMinLevel(-1) == 0);   // safe default, not a crash
        CHECK(testPerkGetMaxRank(-1) == -1);   // safe sentinel
    }
}

// ============================================================
// M-023: perkCanAdd null-guard chain (perk.cc:302-369)
// ============================================================
// The F-01 fix added a nullptr check on perkGetRankData's return value.
// For non-party-member critters, perkGetRankData returns nullptr, meaning:
// (a) maxRank check is SKIPPED (any rank passes for non-party critters)
// (b) Level check only applies to gDude (perk.cc:319: if (critter == gDude))
// (c) Skill/GVAR/stat requirements still apply to ALL critters
// These tests verify each branch of the interaction.
// Research: RPU No usage. Fork: F-01 fix, partially tested.

TEST_CASE("M-023: perkCanAdd — non-party-member critter skips rank check")
{
    TestCritterObj npcCritter = {};
    npcCritter.pid = 0x01000010;  // arbitrary NPC, not gDude
    npcCritter.id = 42;           // id != 0, so NOT gDude

    SUBCASE("Non-party-member critter: nullptr ranksData → rank cap skipped")
    {
        resetPerkCanAddState();
        // Set up a perk with maxRank=1 and no other requirements.
        gPerkDescriptions[0].maxRank = 1;
        gPerkDescriptions[0].minLevel = 3;
        gPerkDescriptions[0].param1 = -1;  // no stat/skill requirement

        // Even though the NPC is at "rank 0" (just testing add eligibility)
        // and maxRank is 1, the nullptr path skips the rank check.
        // The perk should be available (no rank cap, no level check for non-dude).
        CHECK(testPerkCanAdd(&npcCritter, 0, false /*not party member*/, 0, 3));
    }

    SUBCASE("Party-member critter: rank check IS enforced")
    {
        resetPerkCanAddState();
        gPerkDescriptions[0].maxRank = 1;
        gPerkDescriptions[0].minLevel = 3;
        gPerkDescriptions[0].param1 = -1;

        // Party member at rank 1 (already at max) — perk CANNOT be added.
        CHECK_FALSE(testPerkCanAdd(&npcCritter, 0, true /*party member*/, 1 /*current rank*/, 3));

        // Party member at rank 0 — perk CAN be added.
        CHECK(testPerkCanAdd(&npcCritter, 0, true /*party member*/, 0 /*current rank*/, 3));
    }
}

TEST_CASE("M-023: perkCanAdd — gDude level check enforced")
{
    TestCritterObj dude = {};
    dude.pid = 0x01000001;
    dude.id = 0;  // id=0 = gDude convention

    TestCritterObj npc = {};
    npc.pid = 0x01000010;
    npc.id = 42;  // NOT gDude

    SUBCASE("gDude below minLevel — perk rejected")
    {
        resetPerkCanAddState();
        testPcGetStatLevel = 3;  // dude at level 3

        gPerkDescriptions[0].maxRank = 1;
        gPerkDescriptions[0].minLevel = 6;
        gPerkDescriptions[0].param1 = -1;

        // Dude level 3 < minLevel 6 → rejected
        CHECK_FALSE(testPerkCanAdd(&dude, 0, true, 0, gPerkDescriptions[0].minLevel));
    }

    SUBCASE("gDude at or above minLevel — level check passes")
    {
        resetPerkCanAddState();
        testPcGetStatLevel = 6;  // dude at level 6

        gPerkDescriptions[0].maxRank = 1;
        gPerkDescriptions[0].minLevel = 6;
        gPerkDescriptions[0].param1 = -1;

        CHECK(testPerkCanAdd(&dude, 0, true, 0, gPerkDescriptions[0].minLevel));
    }

    SUBCASE("Non-dude critter: level check is SKIPPED regardless of level")
    {
        resetPerkCanAddState();
        // Set NPC's "level" via pcGetStat — but perkCanAdd never calls
        // pcGetStat for non-dude critters (perk.cc:319 condition).
        testPcGetStatLevel = 1;  // "level 1" — but this won't be checked for NPC

        gPerkDescriptions[0].maxRank = 1;
        gPerkDescriptions[0].minLevel = 24;  // Sniper perk level
        gPerkDescriptions[0].param1 = -1;

        // NPC at "level 1" can still take a level-24 perk because
        // the level check only runs for gDude.
        CHECK(testPerkCanAdd(&npc, 0, true, 0, gPerkDescriptions[0].minLevel));
    }
}

TEST_CASE("M-023: perkCanAdd — skill requirements enforced for all critters")
{
    TestCritterObj dude = {};
    dude.pid = 0x01000001;
    dude.id = 0;

    TestCritterObj npc = {};
    npc.pid = 0x01000010;
    npc.id = 42;

    SUBCASE("Skill requirement met — dude can add perk")
    {
        resetPerkCanAddState();
        testPcGetStatLevel = 3;
        gTestSkillValues[3] = 75;  // skill 3 = Small Guns

        gPerkDescriptions[0].maxRank = 1;
        gPerkDescriptions[0].minLevel = 3;
        gPerkDescriptions[0].param1 = 3;    // requires Small Guns (skill 3)
        gPerkDescriptions[0].value1 = 75;    // value1 > 0, so requires skill >= value1
        gPerkDescriptions[0].paramMode = 0;  // PERK_PARAM_MODE_FIRST_ONLY

        CHECK(testPerkCanAdd(&dude, 0, true, 0, gPerkDescriptions[0].minLevel));
    }

    SUBCASE("Skill requirement not met — dude rejected")
    {
        resetPerkCanAddState();
        testPcGetStatLevel = 3;
        gTestSkillValues[3] = 50;  // only 50 Small Guns

        gPerkDescriptions[0].maxRank = 1;
        gPerkDescriptions[0].minLevel = 3;
        gPerkDescriptions[0].param1 = 3;    // requires Small Guns (skill 3)
        gPerkDescriptions[0].value1 = 75;    // requires >= 75
        gPerkDescriptions[0].paramMode = 0;

        CHECK_FALSE(testPerkCanAdd(&dude, 0, true, 0, gPerkDescriptions[0].minLevel));
    }

    SUBCASE("Skill requirement enforced for non-party NPC too")
    {
        resetPerkCanAddState();
        gTestSkillValues[3] = 80;  // skill 3 = Small Guns at 80

        gPerkDescriptions[0].maxRank = 1;
        gPerkDescriptions[0].minLevel = 3;
        gPerkDescriptions[0].param1 = 3;
        gPerkDescriptions[0].value1 = 75;

        // NPC with skill 80 meets the 75 requirement.
        CHECK(testPerkCanAdd(&npc, 0, false /*not party member*/, 0, gPerkDescriptions[0].minLevel));
    }
}

TEST_CASE("M-023: perkCanAdd — maxRank=-1 blocks perk entirely")
{
    TestCritterObj dude = {};
    dude.pid = 0x01000001;
    dude.id = 0;

    SUBCASE("maxRank=-1 means perk unavailable regardless of everything else")
    {
        resetPerkCanAddState();
        testPcGetStatLevel = 99;

        gPerkDescriptions[0].maxRank = -1;  // unavailable (addiction perks, etc.)
        gPerkDescriptions[0].minLevel = 1;
        gPerkDescriptions[0].param1 = -1;

        // Even at level 99 with no skill requirements, the perk is blocked.
        CHECK_FALSE(testPerkCanAdd(&dude, 0, true, 0, gPerkDescriptions[0].minLevel));
    }
}
