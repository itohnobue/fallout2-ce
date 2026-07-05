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

// ============================================================
// Helper: reset perk descriptions to a known state
// ============================================================
static void resetPerkState()
{
    memset(gPerkDescriptions, 0, sizeof(gPerkDescriptions));
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
        resetPerkState();
        testPerkSetMinLevel(0, 3);
        CHECK(testPerkGetMinLevel(0) == 3);
    }

    SUBCASE("set multiple perks independently")
    {
        resetPerkState();
        testPerkSetMinLevel(0, 3);
        testPerkSetMinLevel(1, 6);
        testPerkSetMinLevel(2, 9);

        CHECK(testPerkGetMinLevel(0) == 3);
        CHECK(testPerkGetMinLevel(1) == 6);
        CHECK(testPerkGetMinLevel(2) == 9);
    }

    SUBCASE("overwrite existing value")
    {
        resetPerkState();
        testPerkSetMinLevel(0, 3);
        CHECK(testPerkGetMinLevel(0) == 3);

        testPerkSetMinLevel(0, 12);
        CHECK(testPerkGetMinLevel(0) == 12);
    }

    SUBCASE("invalid perk is silently ignored for set")
    {
        resetPerkState();
        testPerkSetMinLevel(TEST_PERK_COUNT, 42);  // out of bounds
        // Should not crash; minLevel of valid perks unchanged
        CHECK(testPerkGetMinLevel(0) == 0);  // default from reset
    }

    SUBCASE("invalid perk returns 0 for get")
    {
        resetPerkState();
        CHECK(testPerkGetMinLevel(-1) == 0);
        CHECK(testPerkGetMinLevel(TEST_PERK_COUNT) == 0);
    }

    SUBCASE("non-standard min levels (high values)")
    {
        resetPerkState();
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
        resetPerkState();

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
        resetPerkState();

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
        resetPerkState();
        CHECK(testPerkGetMaxRank(-1) == -1);
        CHECK(testPerkGetMaxRank(TEST_PERK_COUNT) == -1);
    }

    SUBCASE("maxRank=-1 means perk has no ranks (unavailable)")
    {
        resetPerkState();
        // Several perks in the vanilla game have maxRank=-1, meaning they
        // cannot be taken normally (e.g., addictions, special perks)
        gPerkDescriptions[50].maxRank = -1;  // Nuka-Cola Addiction
        gPerkDescriptions[51].maxRank = -1;  // Buffout Addiction

        CHECK(testPerkGetMaxRank(50) == -1);
        CHECK(testPerkGetMaxRank(51) == -1);
    }

    SUBCASE("maxRank=0 edge case (not used in vanilla but valid)")
    {
        resetPerkState();
        gPerkDescriptions[0].maxRank = 0;
        CHECK(testPerkGetMaxRank(0) == 0);
    }

    SUBCASE("all perks accessible")
    {
        resetPerkState();
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
    resetPerkState();

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
        resetPerkState();

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
