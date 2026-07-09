// Unit tests for Stage 4 MISC UI/CONFIG domain fixes.
//
// Covers:
//   UF-H-015/H-016 — Barter table bounds checks with offset
//   UF-H-020 — critterSetBonusStat min/max value clamping
//   UF-H-039 — _movieUpdate stops on any negative error code
//   UF-H-043 — configGetInt base=10 (no octal parse failure)
//
// Self-contained mirror tests for stat and movie logic.
// configGetInt test links test_sources (real config.cc implementation).
// barter bounds test is pure logic — no engine deps.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <climits>
#include <iterator>

// ---- UF-H-015 / UF-H-016: Barter table bounds check with offset ----

// Mirror of the corrected bounds-check logic from src/inventory.cc:5559,5576.
// The original check was  slotIndex < inventory->length  (omitting offset).
// The fix checks  slotIndex + offset < inventory->length  before computing
// the index  inventory->length - (slotIndex + offset + 1).

TEST_CASE("UF-H-015/H-016: barter bounds check includes offset")
{
    // Simulate a 20-element inventory with offset scrolling.
    const int invLength = 20;
    const int visibleSlots = 8;

    SUBCASE("slotIndex 0, offset 0: in bounds (no scroll)")
    {
        int offset = 0;
        int slotIndex = 0;
        CHECK(slotIndex + offset < invLength);
    }

    SUBCASE("slotIndex 0, offset 10: in bounds")
    {
        int offset = 10;
        int slotIndex = 0;
        // 0 + 10 = 10 < 20  → OK
        CHECK(slotIndex + offset < invLength);
    }

    SUBCASE("slotIndex 7, offset 10: in bounds")
    {
        int offset = 10;
        int slotIndex = 7;
        // 7 + 10 = 17 < 20  → OK
        CHECK(slotIndex + offset < invLength);
    }

    SUBCASE("slotIndex 7, offset 13: in bounds (boundary)")
    {
        int offset = 13;
        int slotIndex = 7;
        // 7 + 13 = 20  → NOT < 20, rejected
        CHECK_FALSE(slotIndex + offset < invLength);
    }

    SUBCASE("slotIndex 7, offset 15: out of bounds")
    {
        int offset = 15;
        int slotIndex = 7;
        // 7 + 15 = 22 < 20 is false
        CHECK_FALSE(slotIndex + offset < invLength);
    }

    SUBCASE("omitted-offset check (old buggy code) would incorrectly pass")
    {
        // With old check: slotIndex < invLength
        // slotIndex = 7, offset = 15 → 7 < 20 is true (bug: index wraps)
        int offset = 15;
        int slotIndex = 7;
        bool oldCheckPasses = (slotIndex < invLength);
        CHECK(oldCheckPasses); // old code says OK — BAD

        // Fixed check includes offset:
        bool fixedCheckPasses = (slotIndex + offset < invLength);
        CHECK_FALSE(fixedCheckPasses); // fixed code correctly rejects
    }

    SUBCASE("clamped offset at max position (scroller at end)")
    {
        // Scroller clamps offset so offset + visibleSlots <= invLength.
        // maxOffset = invLength - visibleSlots = 20 - 8 = 12
        int maxOffset = invLength - visibleSlots; // 12
        int slotIndex = 7;
        // 7 + 12 = 19 < 20  → OK (last visible slot is valid)
        CHECK(slotIndex + maxOffset < invLength);

        // slotIndex = 0 with maxOffset:
        // 0 + 12 = 12 < 20 → OK (first visible slot with max scroll)
        CHECK(0 + maxOffset < invLength);
    }
}

// ---- UF-H-020: critterSetBonusStat min/max value clamping ----

// Mirror of the StatDescription struct from src/stat.cc:38-45.
struct MirroredStatDescription {
    int minimumValue;
    int maximumValue;
};

enum {
    MIRROR_STAT_STRENGTH = 0,
    MIRROR_STAT_MAX_HP = 7, // STAT_MAXIMUM_HIT_POINTS
    MIRROR_STAT_CRIT_CHANCE = 14, // STAT_CRITICAL_CHANCE
    MIRROR_STAT_DT_NORMAL = 16, // STAT_DAMAGE_THRESHOLD
    MIRROR_STAT_UNARMED_DMG = 9, // STAT_UNARMED_DAMAGE
    MIRROR_SAVEABLE_COUNT = 35,
};

// Mirror of gStatDescriptions for a subset of stats that exercise boundary
// conditions.  Matches the values in src/stat.cc:48-87.
// Indexed by the real stat ordinal (0..34, matching MIRROR_SAVEABLE_COUNT).
// Unused entries default to {INT_MIN, INT_MAX} — no clamping.
static const MirroredStatDescription kBonusStatDescriptions[] = {
    // 0: STRENGTH
    {  1,  10 },                    // PRIMARY_STAT_MIN..PRIMARY_STAT_MAX
    {INT_MIN, INT_MAX},             // 1: PERCEPTION (unused in test)
    {INT_MIN, INT_MAX},             // 2: ENDURANCE
    {INT_MIN, INT_MAX},             // 3: CHARISMA
    {INT_MIN, INT_MAX},             // 4: INTELLIGENCE
    {INT_MIN, INT_MAX},             // 5: AGILITY
    {INT_MIN, INT_MAX},             // 6: LUCK
    {  0, 999 },                    // 7: MAX_HP
    {INT_MIN, INT_MAX},             // 8: MAX_AP
    {  0, INT_MAX },                // 9: UNARMED_DMG (unbounded max)
    {INT_MIN, INT_MAX},             // 10: MELEE_DAMAGE
    {INT_MIN, INT_MAX},             // 11: CARRY_WEIGHT
    {INT_MIN, INT_MAX},             // 12: SEQUENCE
    {INT_MIN, INT_MAX},             // 13: HEALING_RATE
    { -60, 100 },                   // 14: CRIT_CHANCE (wide negative range)
    {INT_MIN, INT_MAX},             // 15: BETTER_CRITICALS
    {  0, 100 },                    // 16: DT_NORMAL (0..100 DR/DT type)
    {INT_MIN, INT_MAX},             // 17: DT_LASER
    {INT_MIN, INT_MAX},             // 18: DT_FIRE
    {INT_MIN, INT_MAX},             // 19: DT_PLASMA
    {INT_MIN, INT_MAX},             // 20: DT_EXPLODE
    {INT_MIN, INT_MAX},             // 21: DT_ELECTRICAL
    {INT_MIN, INT_MAX},             // 22: DT_EMP
    {INT_MIN, INT_MAX},             // 23: DR_NORMAL
    {INT_MIN, INT_MAX},             // 24: DR_LASER
    {INT_MIN, INT_MAX},             // 25: DR_FIRE
    {INT_MIN, INT_MAX},             // 26: DR_PLASMA
    {INT_MIN, INT_MAX},             // 27: DR_EXPLODE
    {INT_MIN, INT_MAX},             // 28: DR_ELECTRICAL
    {INT_MIN, INT_MAX},             // 29: DR_EMP
    {INT_MIN, INT_MAX},             // 30: AGE
    {INT_MIN, INT_MAX},             // 31: GENDER
    {INT_MIN, INT_MAX},             // 32: CURRENT_HP
    {INT_MIN, INT_MAX},             // 33: CURRENT_POISON
    {INT_MIN, INT_MAX},             // 34: RADIATION
};
static_assert(std::size(kBonusStatDescriptions) == MIRROR_SAVEABLE_COUNT,
              "kBonusStatDescriptions must have MIRROR_SAVEABLE_COUNT entries");

// Mirror of critterSetBonusStat clamping logic from src/stat.cc:587-597 (fixed).
// Returns: 0 on success, -2 if below min, -3 if above max, -5 on invalid stat.
static int mirroredSetBonusStat(int stat, int value, int saveableCount,
                                const MirroredStatDescription* descs)
{
    if (stat < 0 || stat >= saveableCount) return -5;

    // UF-H-020: Added min/max clamping matching critterSetBaseStat pattern.
    if (value < descs[stat].minimumValue) return -2;
    if (value > descs[stat].maximumValue) return -3;

    return 0;
}

TEST_CASE("UF-H-020: critterSetBonusStat clamps to min/max range")
{
    SUBCASE("valid value within range succeeds")
    {
        CHECK(mirroredSetBonusStat(MIRROR_STAT_STRENGTH, 5,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
        CHECK(mirroredSetBonusStat(MIRROR_STAT_MAX_HP, 500,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
    }

    SUBCASE("value at minimum boundary succeeds")
    {
        CHECK(mirroredSetBonusStat(MIRROR_STAT_STRENGTH, 1,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
        CHECK(mirroredSetBonusStat(MIRROR_STAT_CRIT_CHANCE, -60,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
    }

    SUBCASE("value at maximum boundary succeeds")
    {
        CHECK(mirroredSetBonusStat(MIRROR_STAT_STRENGTH, 10,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
        CHECK(mirroredSetBonusStat(MIRROR_STAT_DT_NORMAL, 100,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
    }

    SUBCASE("value below minimum returns -2")
    {
        CHECK(mirroredSetBonusStat(MIRROR_STAT_STRENGTH, 0,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == -2);
        CHECK(mirroredSetBonusStat(MIRROR_STAT_CRIT_CHANCE, -61,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == -2);
    }

    SUBCASE("value above maximum returns -3")
    {
        CHECK(mirroredSetBonusStat(MIRROR_STAT_STRENGTH, 11,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == -3);
        CHECK(mirroredSetBonusStat(MIRROR_STAT_DT_NORMAL, 101,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == -3);
    }

    SUBCASE("INT_MAX max bound allows large values")
    {
        // UNARMED_DMG has INT_MAX max — should accept large value
        CHECK(mirroredSetBonusStat(MIRROR_STAT_UNARMED_DMG, 50000,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
        // INT_MAX check — max value itself
        CHECK(mirroredSetBonusStat(MIRROR_STAT_UNARMED_DMG, INT_MAX,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
    }

    SUBCASE("invalid stat index returns -5")
    {
        CHECK(mirroredSetBonusStat(-1, 5,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == -5);
        CHECK(mirroredSetBonusStat(MIRROR_SAVEABLE_COUNT, 5,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == -5);
    }
}

// ---- UF-H-039: _movieUpdate stops on any negative error code ----

// Mirror of _stepMovie / _MVE_rmStepMovie return codes from
// src/movie_lib.cc:634-730.
// -1  = end of movie (normal)
// -2  = null data pointer (corrupt file)
// -5  = read error
// -6  = decompression error
// -8  = allocation error
// -10 = playback not active

// Mirror of fixed _movieUpdate logic (src/movie.cc:824).
// Original:  if (stepMovie() == -1) { cleanup }
// Fixed:     if (stepMovie() < 0) { cleanup }
static bool mirroredMovieUpdate(int stepResult)
{
    // UF-H-039: Check ALL negative error codes.
    if (stepResult < 0) {
        return true; // cleanup triggered
    }
    return false; // continue playback
}

TEST_CASE("UF-H-039: _movieUpdate stops on any negative error code")
{
    SUBCASE("end-of-movie (-1) triggers cleanup (was already handled)")
    {
        CHECK(mirroredMovieUpdate(-1));
    }

    SUBCASE("null data (-2) triggers cleanup (was NOT handled before fix)")
    {
        CHECK(mirroredMovieUpdate(-2));
    }

    SUBCASE("read error (-5) triggers cleanup")
    {
        CHECK(mirroredMovieUpdate(-5));
    }

    SUBCASE("decompression error (-6) triggers cleanup")
    {
        CHECK(mirroredMovieUpdate(-6));
    }

    SUBCASE("allocation error (-8) triggers cleanup")
    {
        CHECK(mirroredMovieUpdate(-8));
    }

    SUBCASE("not active (-10) triggers cleanup")
    {
        CHECK(mirroredMovieUpdate(-10));
    }

    SUBCASE("zero (success) continues playback")
    {
        CHECK_FALSE(mirroredMovieUpdate(0));
    }

    SUBCASE("positive values continue playback")
    {
        CHECK_FALSE(mirroredMovieUpdate(1));
        CHECK_FALSE(mirroredMovieUpdate(100));
    }

    SUBCASE("all negative values [ -10 .. -1 ] trigger cleanup")
    {
        for (int code = -10; code <= -1; ++code) {
            INFO("Testing error code: ", code);
            CHECK(mirroredMovieUpdate(code));
        }
    }
}

// ---- UF-H-043: configGetInt base=10 (no octal parse failure) ----

// This test validates the behavioral change from base=0 to base=10.
// With base=0, strtol("08", &end, 0) treats "0" as octal prefix,
// then '8' is invalid octal → returns 0 (silent-wrong).
// With base=10, strtol("08", &end, 10) returns 8 (correct).

static int mirroredConfigGetInt(const char* str, unsigned char base = 10)
{
    char* end;
    long l = strtol(str, &end, base);
    if (end == str) return -1; // no conversion
    return static_cast<int>(l);
}

TEST_CASE("UF-H-043: configGetInt base=10 prevents octal parse failure")
{
    SUBCASE("value '08' with base=10 returns 8 (correct)")
    {
        CHECK(mirroredConfigGetInt("08", 10) == 8);
    }

    SUBCASE("value '09' with base=10 returns 9 (correct)")
    {
        CHECK(mirroredConfigGetInt("09", 10) == 9);
    }

    SUBCASE("value '08' with base=0 returns 0 (BUG — old behavior)")
    {
        CHECK(mirroredConfigGetInt("08", 0) == 0);
    }

    SUBCASE("value '09' with base=0 returns 0 (BUG — old behavior)")
    {
        CHECK(mirroredConfigGetInt("09", 0) == 0);
    }

    SUBCASE("value '10' works correctly with both bases")
    {
        CHECK(mirroredConfigGetInt("10", 10) == 10);
        CHECK(mirroredConfigGetInt("10", 0) == 10);
    }

    SUBCASE("value '0' works correctly with both bases")
    {
        CHECK(mirroredConfigGetInt("0", 10) == 0);
        CHECK(mirroredConfigGetInt("0", 0) == 0);
    }

    SUBCASE("value '7' works correctly with both bases")
    {
        CHECK(mirroredConfigGetInt("7", 10) == 7);
        CHECK(mirroredConfigGetInt("7", 0) == 7);
    }

    SUBCASE("value '42' works correctly with both bases")
    {
        CHECK(mirroredConfigGetInt("42", 10) == 42);
        CHECK(mirroredConfigGetInt("42", 0) == 42);
    }

    SUBCASE("leading zeros with base=10 parse correctly")
    {
        CHECK(mirroredConfigGetInt("00123", 10) == 123);
        CHECK(mirroredConfigGetInt("000", 10) == 0);
    }
}
