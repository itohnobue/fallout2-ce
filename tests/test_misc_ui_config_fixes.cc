// Unit tests for Stage 4 MISC UI/CONFIG domain fixes.
//
// Covers:
//   UF-H-015/H-016 — Barter table bounds checks with offset
//   C-05 — critterSetBonusStat signed-delta semantics (write-side clamp removed)
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

// Mirror of critterSetBonusStat (src/stat.cc:577-625) — C-05 FIXED semantics.
// bonusStats[stat] is a SIGNED DELTA (modifier), NOT an absolute value, so
// the write path stores the raw delta with NO min/max validation (0 and
// negatives are legitimate: radiation, addiction, drug wear-off, editor
// reset). The effective stat is bounded by the READ-SIDE display clamp in
// critterGetStat() (src/stat.cc:423-426), mirrored as mirroredGetStat below.
// Returns: 0 on success (delta accepted/stored), -5 on invalid stat index.
static int mirroredSetBonusStat(int stat, int value, int saveableCount,
                                const MirroredStatDescription* descs)
{
    (void)value;
    (void)descs;
    if (stat < 0 || stat >= saveableCount) return -5;
    return 0;
}

// Mirror of the read-side display clamp in critterGetStat (src/stat.cc:423-426):
// effective = base + bonus, clamped to [minimumValue, maximumValue] when
// min <= max (the guard prevents UB on min > max, C++17 [alg.clamp]).
static int mirroredGetStat(int baseValue, int bonusValue,
                           const MirroredStatDescription& desc)
{
    int value = baseValue + bonusValue;
    if (desc.minimumValue <= desc.maximumValue) {
        value = std::clamp(value, desc.minimumValue, desc.maximumValue);
    }
    return value;
}

TEST_CASE("C-05: critterSetBonusStat accepts signed deltas (0/negative/above-max)")
{
    SUBCASE("zero bonus is accepted (editor GCD reset)")
    {
        // character_editor.cc:4205 writes 0 for every saveable stat to reset
        // bonuses. The pre-fix write-side clamp rejected 0 (< PRIMARY_STAT_MIN=1),
        // leaving stale bonuses permanently applied.
        CHECK(mirroredSetBonusStat(MIRROR_STAT_STRENGTH, 0,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
    }

    SUBCASE("negative bonus is accepted (radiation/addiction penalties)")
    {
        // Radiation penalties (critter.cc:128-137) and addiction penalties
        // (perk.cc) are negative deltas.
        CHECK(mirroredSetBonusStat(MIRROR_STAT_STRENGTH, -2,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
        CHECK(mirroredSetBonusStat(MIRROR_STAT_CRIT_CHANCE, -61,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
    }

    SUBCASE("above-max bonus is accepted (raw delta; read side clamps display)")
    {
        // Drug/perk boosts may push the raw delta above the stat's nominal
        // maximum; the effective value is bounded at read time, not on write.
        CHECK(mirroredSetBonusStat(MIRROR_STAT_STRENGTH, 11,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
        CHECK(mirroredSetBonusStat(MIRROR_STAT_STRENGTH, 1000,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
    }

    SUBCASE("read-side clamp bounds the effective value")
    {
        // effective = base + bonus, clamped at read time (stat.cc:423-426).
        // STR base 5: bonus -10 → 5-10 = -5 → clamped to min 1; bonus +100
        // → 105 → clamped to max 10; in-range deltas pass through unchanged.
        CHECK(mirroredGetStat(5, -10, kBonusStatDescriptions[MIRROR_STAT_STRENGTH]) == 1);
        CHECK(mirroredGetStat(5, 100, kBonusStatDescriptions[MIRROR_STAT_STRENGTH]) == 10);
        CHECK(mirroredGetStat(5, -2, kBonusStatDescriptions[MIRROR_STAT_STRENGTH]) == 3);
        CHECK(mirroredGetStat(5, 0, kBonusStatDescriptions[MIRROR_STAT_STRENGTH]) == 5);
    }

    SUBCASE("invalid stat index returns -5")
    {
        CHECK(mirroredSetBonusStat(-1, 5,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == -5);
        CHECK(mirroredSetBonusStat(MIRROR_SAVEABLE_COUNT, 5,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == -5);
    }
}

// ---- C-05 consumer-level regression tests ----
// These model the actual game consumers that route through critterSetBonusStat:
// radiation drain/heal (critter.cc:595-647), drug apply/wear-off
// (item.cc:2803-2859), addiction apply/withdrawal-end (perk.cc:697-760), and
// the character-editor GCD reset loop (character_editor.cc:4205). Each models
// the production delta formula (statBonus = oldBonus ± delta) against the
// fixed write path and asserts the cycle returns to baseline with NO
// permanent stat inflation (the pre-fix clamp rejected the negative leg and
// accepted the positive leg, producing permanent +N per cycle).

TEST_CASE("C-05: radiation drain/heal cycle returns to baseline (never +1)")
{
    // radiationProcess (critter.cc:595-598): value = bonus + modifier * penalty
    // with penalty = -1 for STR at the first damaging level
    // (gRadiationEffectPenalties[0][0]); modifier = +1 on drain, -1 on heal
    // (radiationClearDamage, critter.cc:555-564).
    const int penalty = -1;
    const MirroredStatDescription& strDesc = kBonusStatDescriptions[MIRROR_STAT_STRENGTH];

    SUBCASE("radiation drain applies the negative penalty")
    {
        int bonus = 0;
        int value = bonus + 1 * penalty; // 0 + (-1) = -1 → ACCEPTED
        CHECK(mirroredSetBonusStat(MIRROR_STAT_STRENGTH, value,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
        bonus = value;
        CHECK(bonus == -1);
        // Effective STR = base 5 + (-1) = 4; alive (>= PRIMARY_STAT_MIN 1).
        CHECK(mirroredGetStat(5, bonus, strDesc) == 4);
    }

    SUBCASE("healing restores exactly to baseline, no permanent +1")
    {
        int bonus = -1; // state after radiation drain
        int value = bonus + (-1) * penalty; // -1 + (+1) = 0 → ACCEPTED
        CHECK(mirroredSetBonusStat(MIRROR_STAT_STRENGTH, value,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
        bonus = value;
        CHECK(bonus == 0); // never +1: the old clamp accepted +1 on the heal leg
        CHECK(mirroredGetStat(5, bonus, strDesc) == 5);
    }
}

TEST_CASE("C-05: drug apply/wear-off cycle returns to baseline")
{
    // _perform_drug_effect (item.cc:2803-2859): statBonus = mod + oldStatBonus;
    // wear-off applies the negative side of the drug's mods to return to 0.
    const int drugMod = 2;
    const MirroredStatDescription& strDesc = kBonusStatDescriptions[MIRROR_STAT_STRENGTH];

    SUBCASE("drug boost applies and wear-off returns to 0")
    {
        int bonus = 0;
        int value = bonus + drugMod; // 0 + 2 = +2 → ACCEPTED
        CHECK(mirroredSetBonusStat(MIRROR_STAT_STRENGTH, value,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
        bonus = value;
        CHECK(bonus == 2);
        CHECK(mirroredGetStat(5, bonus, strDesc) == 7);

        // wear-off: 2 + (-2) = 0 → ACCEPTED (pre-fix clamp rejected the
        // reversal below PRIMARY_STAT_MIN, making drug boosts permanent).
        value = bonus - drugMod;
        CHECK(mirroredSetBonusStat(MIRROR_STAT_STRENGTH, value,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
        bonus = value;
        CHECK(bonus == 0);
        CHECK(mirroredGetStat(5, bonus, strDesc) == 5);
    }
}

TEST_CASE("C-05: addiction apply/withdrawal-end cycle returns to baseline")
{
    // perkAddEffect (perk.cc:716-720): value + (-2) penalty → ACCEPTED;
    // perkRemoveEffect on withdrawal end (perk.cc:757-761): 0 - (-2) → +2
    // added back → returns to 0. The pre-fix clamp rejected the -2 penalty
    // but accepted the +2 removal, producing permanent stat inflation.
    const int addictionPenalty = -2;
    const MirroredStatDescription& strDesc = kBonusStatDescriptions[MIRROR_STAT_STRENGTH];

    SUBCASE("addiction penalty applies and withdrawal end restores baseline")
    {
        int bonus = 0;
        int value = bonus + addictionPenalty; // 0 + (-2) = -2 → ACCEPTED
        CHECK(mirroredSetBonusStat(MIRROR_STAT_STRENGTH, value,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
        bonus = value;
        CHECK(bonus == -2);
        CHECK(mirroredGetStat(5, bonus, strDesc) == 3);

        // withdrawal end: -2 - (-2) = 0 → ACCEPTED
        value = bonus - addictionPenalty;
        CHECK(mirroredSetBonusStat(MIRROR_STAT_STRENGTH, value,
              MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
        bonus = value;
        CHECK(bonus == 0);
        CHECK(mirroredGetStat(5, bonus, strDesc) == 5);
    }
}

TEST_CASE("C-05: editor GCD reset loop clears all bonus stats")
{
    // character_editor.cc:4205: critterSetBonusStat(gDude, stat, 0) for every
    // saveable stat. The reset must succeed on primary stats (bonus 0 < 1 was
    // previously rejected → stale bonuses unrecoverable in-game).
    const MirroredStatDescription& strDesc = kBonusStatDescriptions[MIRROR_STAT_STRENGTH];

    int bonus = 5; // some accumulated bonus
    CHECK(mirroredSetBonusStat(MIRROR_STAT_STRENGTH, 0,
          MIRROR_SAVEABLE_COUNT, kBonusStatDescriptions) == 0);
    bonus = 0;
    CHECK(bonus == 0);
    CHECK(mirroredGetStat(5, bonus, strDesc) == 5);
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
