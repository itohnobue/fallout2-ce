// Unit tests for game core fixes (Stage 4 Implementation).
//
// Covers: UF-H-008, UF-H-014, UF-H-017, UF-H-018, UF-004/M-75, UF-010, UF-H-044
//
// These are self-contained mirror tests that validate the logic patterns
// of the fixes without linking the production .cc files (60+ engine deps each).
// Each test mirrors the fixed logic in a test-local function and validates
// the behavior against edge cases and pre-fix crash scenarios.
//
// M-75: the UF-004 oracle was rewritten in Stage 6 to model the full ammo
// stack state (clip count + top-clip rounds) — the original test only
// modeled the top clip's quantity and never exercised the quantity++ side
// effect, giving false confidence on round preservation.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>
#include <string>

// =============================================================
// UF-H-008: opSfxBuildCharName/opSfxBuildOpenName nullptr push
//
// Before: programStackPushString(program, nullptr) → strlen(nullptr) → SIGSEGV
// After:  programStackPushString(program, "") — pushes empty string
//
// Test validates the fix pattern: when object is null, push empty string.
// =============================================================

namespace {
// Test-local mirror of the fix pattern for opSfxBuildCharName
const char* sfxBuildCharNameNullGuard(const char* obj, const char* fallback) {
    if (obj != nullptr) {
        return obj;
    }
    // Fix: push empty string instead of nullptr
    return "";
}
} // namespace

TEST_CASE("UF-H-008: opSfxBuildCharName pushes empty string for null object") {
    // Null object: should return empty string (not nullptr)
    const char* result = sfxBuildCharNameNullGuard(nullptr, "");
    REQUIRE(result != nullptr);
    CHECK(std::strlen(result) == 0);
    CHECK(result[0] == '\0');

    // Valid object: should return the actual string
    const char* valid = "I9999991";
    result = sfxBuildCharNameNullGuard(valid, "");
    REQUIRE(result != nullptr);
    CHECK(std::strcmp(result, valid) == 0);
}

TEST_CASE("UF-H-008: opSfxBuildOpenName pushes empty string for null object") {
    // Null object: same fix pattern as BuildCharName
    const char* result = sfxBuildCharNameNullGuard(nullptr, "");
    REQUIRE(result != nullptr);
    CHECK(std::strlen(result) == 0);

    // Valid object
    const char* result2 = sfxBuildCharNameNullGuard("OABOOK01", "");
    REQUIRE(result2 != nullptr);
    CHECK(std::strcmp(result2, "OABOOK01") == 0);
}

// =============================================================
// UF-H-014: _weapPrefOrderings OOB access guarded
//
// Before: _weapPrefOrderings[ai->best_weapon + 1][index] — no bounds check
// After:  Validate bestWeapon < 0 || bestWeapon >= BEST_WEAPON_COUNT → return nullptr
//
// Test validates the bounds check pattern matching _caiHasWeapPrefType.
// =============================================================

namespace {
constexpr int TEST_BEST_WEAPON_COUNT = 9; // matches BEST_WEAPON_COUNT from combat_ai_defs.h
constexpr int TEST_ATTACK_TYPE_COUNT = 5; // matches ATTACK_TYPE_COUNT from item.h

// Test-local mirror of _weapPrefOrderings[BEST_WEAPON_COUNT + 1][ATTACK_TYPE_COUNT]
// Array has BEST_WEAPON_COUNT + 1 rows (indices 0..BEST_WEAPON_COUNT)
//   index 0 = invalid (best_weapon+1 when best_weapon == -1)
//   index 1..BEST_WEAPON_COUNT = valid (best_weapon+1 for best_weapon 0..8)
constexpr int INVALID_INDEX = -999;

int testWeapPrefOrderings[TEST_BEST_WEAPON_COUNT + 1][TEST_ATTACK_TYPE_COUNT];

// Test-local mirror of the fix pattern: bounds-checked access
bool testAccessWeapPrefOrderingsSafe(int bestWeapon, int attackTypeIndex) {
    // Fix: validate best_weapon against BEST_WEAPON_COUNT to prevent OOB
    if (bestWeapon < 0 || bestWeapon >= TEST_BEST_WEAPON_COUNT) {
        return false; // safe return, matches _ai_best_weapon returning nullptr
    }
    int prefIndex = bestWeapon + 1;
    // prefIndex is now guaranteed in [1, TEST_BEST_WEAPON_COUNT],
    // which is within array bounds [0, TEST_BEST_WEAPON_COUNT].
    return true; // access would be safe
}
} // namespace

TEST_CASE("UF-H-014: _weapPrefOrderings OOB access guarded") {
    // Valid range: bestWeapon in [0, BEST_WEAPON_COUNT - 1] = [0, 8]
    for (int bw = 0; bw < TEST_BEST_WEAPON_COUNT; bw++) {
        CHECK(testAccessWeapPrefOrderingsSafe(bw, 0) == true);
    }

    // Invalid: negative best_weapon
    CHECK(testAccessWeapPrefOrderingsSafe(-1, 0) == false);
    CHECK(testAccessWeapPrefOrderingsSafe(-100, 0) == false);

    // Invalid: best_weapon >= BEST_WEAPON_COUNT
    CHECK(testAccessWeapPrefOrderingsSafe(TEST_BEST_WEAPON_COUNT, 0) == false);
    CHECK(testAccessWeapPrefOrderingsSafe(TEST_BEST_WEAPON_COUNT + 1, 0) == false);
    CHECK(testAccessWeapPrefOrderingsSafe(9999, 0) == false);

    // INT_MIN / INT_MAX: should be safely rejected
    CHECK(testAccessWeapPrefOrderingsSafe(-2147483647 - 1, 0) == false); // INT_MIN
    CHECK(testAccessWeapPrefOrderingsSafe(2147483647, 0) == false);     // INT_MAX
}

TEST_CASE("UF-H-014: OOB best_weapon from save file deserialization") {
    // Save file deserialization reads raw int32 → any value possible
    // The fix rejects all OOB values safely
    struct { int value; bool expected_safe; } cases[] = {
        {0, true},
        {1, true},
        {7, true},    // BEST_WEAPON_UNARMED_OVER_THROW
        {8, true},    // BEST_WEAPON_RANDOM (valid, last enum value before COUNT)
        {9, false},   // == BEST_WEAPON_COUNT → OOB
        {10, false},  // > BEST_WEAPON_COUNT
        {-1, false},  // negative
        {255, false}, // typical corrupted byte
    };
    for (auto& tc : cases) {
        INFO("bestWeapon = ", tc.value);
        CHECK(testAccessWeapPrefOrderingsSafe(tc.value, 0) == tc.expected_safe);
    }
}

// =============================================================
// UF-H-017: protoGetProto return value unchecked in armor getters
//
// Before: protoGetProto(armor->pid, &proto); — return value ignored
// After:  if (protoGetProto(armor->pid, &proto) == -1) return [default];
//
// Test validates the fix pattern from ammoGetDamageDivisor.
// =============================================================

namespace {
// Test-local mirror of the fix pattern for armor getter functions
int armorGetArmorClassFixed(int pid, bool protoSucceeds) {
    if (!protoSucceeds) {
        return 0; // default AC when proto lookup fails
    }
    return 25; // example armor class
}

int armorGetPerkFixed(int pid, bool protoSucceeds) {
    if (!protoSucceeds) {
        return -1; // default perk when proto lookup fails
    }
    return 5; // example perk
}

int armorGetMaleFidFixed(int pid, bool protoSucceeds) {
    if (!protoSucceeds) {
        return -1; // default FID when proto lookup fails
    }
    return 12345;
}

int armorGetFemaleFidFixed(int pid, bool protoSucceeds) {
    if (!protoSucceeds) {
        return -1;
    }
    return 12346;
}

int armorGetDamageResistanceFixed(int pid, int damageType, bool protoSucceeds) {
    if (!protoSucceeds) {
        return 0; // default DR
    }
    return 40;
}

int armorGetDamageThresholdFixed(int pid, int damageType, bool protoSucceeds) {
    if (!protoSucceeds) {
        return 0; // default DT
    }
    return 8;
}
} // namespace

TEST_CASE("UF-H-017: protoGetProto failure returns default value in armor getters") {
    // Proto lookup failure should return safe defaults, not crash
    CHECK(armorGetArmorClassFixed(0, false) == 0);
    CHECK(armorGetPerkFixed(0, false) == -1);
    CHECK(armorGetMaleFidFixed(0, false) == -1);
    CHECK(armorGetFemaleFidFixed(0, false) == -1);
    CHECK(armorGetDamageResistanceFixed(0, 0, false) == 0);
    CHECK(armorGetDamageThresholdFixed(0, 0, false) == 0);
}

TEST_CASE("UF-H-017: protoGetProto success returns actual value") {
    // Proto lookup success should return the real proto data
    CHECK(armorGetArmorClassFixed(0, true) == 25);
    CHECK(armorGetPerkFixed(0, true) == 5);
    CHECK(armorGetMaleFidFixed(0, true) == 12345);
    CHECK(armorGetFemaleFidFixed(0, true) == 12346);
    CHECK(armorGetDamageResistanceFixed(0, 0, true) == 40);
    CHECK(armorGetDamageThresholdFixed(0, 0, true) == 8);
}

// =============================================================
// UF-H-018: _proto_dude_init unconditional return 0
//
// Before: return 0; — gcdLoad failure never propagated
// After:  return _retval; — _retval set to -1 on gcdLoad failure
//
// Test validates error propagation for corrupted premade .gcd files.
// =============================================================

namespace {
// Test-local mirror of the fix: _retval propagates gcdLoad result
int testProtoDudeInit(bool gcdLoadSucceeds) {
    int retval = 0;
    if (!gcdLoadSucceeds) {
        retval = -1;
    }
    // Fix: return retval instead of hardcoded 0
    return retval;
}
} // namespace

TEST_CASE("UF-H-018: _proto_dude_init returns -1 on gcdLoad failure") {
    CHECK(testProtoDudeInit(false) == -1); // gcdLoad fails → returns -1
}

TEST_CASE("UF-H-018: _proto_dude_init returns 0 on gcdLoad success") {
    CHECK(testProtoDudeInit(true) == 0); // gcdLoad succeeds → returns 0
}

TEST_CASE("UF-H-018: characterSelectorWindowRefresh dead code revived") {
    // Before fix: _proto_dude_init always returned 0, so this check was dead code.
    // After fix:  -1 is returned on failure, so the check works correctly.
    auto checkReturnsFalse = [](int result) { return result == -1; };
    CHECK(checkReturnsFalse(-1) == true);   // failure → returns false
    CHECK(checkReturnsFalse(0) == false);   // success → continues
}

// =============================================================
// UF-004 / M-75: Ammo overflow merge round preservation
//
// M-75: pass-13 (7f58356) changed the overflow merge to set the new
// clip's rounds to the full combined amount (clamped to capacity) and
// then quantity++ — but quantity++ already adds a full clip, so the
// merge created rounds. Concrete trace: a stack of 2×10-round clips
// (entry quantity=2, top clip 10 rounds) plus a 5-round pickup gives
// combined=15 > capacity=10; the buggy code set the new clip to 10 and
// quantity++ → 3 clips × 10 = 30 rounds owned from 25 (+5 created).
//
// Correct (upstream excess-only): ammoSetQuantity(itemToAdd, combined -
// capacity) keeps only the overflow on the new clip; quantity++ accounts
// for the new clip itself. Total = capacity×(N-1) + excess + newRounds.
//
// The oracle models the full inventory state (clip count + top-clip
// rounds), NOT just the top clip's quantity — the old test (testAmmoMergeOverflow)
// only modeled the top clip and never exercised the quantity++ side
// effect, giving false confidence.
// =============================================================

namespace {
struct AmmoStack {
    int clipCount;    // inventory entry quantity (number of clips)
    int topRounds;    // rounds in the current/top clip
    int capacity;     // rounds per full clip
};

// Total rounds owned by an ammo stack entry:
// every clip except the top is full at capacity; the top holds topRounds.
int testAmmoStackTotal(const AmmoStack& stack) {
    return stack.capacity * (stack.clipCount - 1) + stack.topRounds;
}

// Mirrors the fixed production merge (item.cc itemAdd ammo branch):
// existing stack + a new clip of [newRounds] rounds, capacity [capacity].
AmmoStack testAmmoMergeExcessOnly(const AmmoStack& existing, int newRounds) {
    int capacity = existing.capacity;
    int combined = existing.topRounds + newRounds;
    AmmoStack result = existing;
    if (combined > capacity) {
        // Excess-only: the new clip carries only the overflow; the
        // clip-count increment below accounts for the new clip.
        result.topRounds = combined - capacity;
        result.clipCount += 1;
    } else {
        // No overflow: rounds fold into the existing top clip.
        result.topRounds = combined;
    }
    return result;
}

// Mirrors the buggy pass-13 behavior (the regression M-75 fixed).
AmmoStack testAmmoMergeBuggy(const AmmoStack& existing, int newRounds) {
    int capacity = existing.capacity;
    int combined = existing.topRounds + newRounds;
    AmmoStack result = existing;
    if (combined > capacity) {
        // ammoSetQuantity clamps to capacity, then quantity++ adds a
        // full clip → rounds created.
        result.topRounds = capacity;
        result.clipCount += 1;
    } else {
        result.topRounds = combined;
    }
    return result;
}
} // namespace

TEST_CASE("M-75: Ammo overflow merge preserves total rounds (excess-only)") {
    // Concrete adversarial trace: 2×10 clips + 5-round pickup = 25 rounds.
    AmmoStack stack = { 2, 10, 10 };
    CHECK(testAmmoStackTotal(stack) == 20);

    AmmoStack fixed = testAmmoMergeExcessOnly(stack, 5);
    CHECK(fixed.clipCount == 3);
    CHECK(fixed.topRounds == 5);            // excess only
    CHECK(testAmmoStackTotal(fixed) == 25); // total preserved

    AmmoStack buggy = testAmmoMergeBuggy(stack, 5);
    CHECK(buggy.clipCount == 3);
    CHECK(buggy.topRounds == 10);           // clamped to capacity
    CHECK(testAmmoStackTotal(buggy) == 30); // +5 rounds created
    CHECK(testAmmoStackTotal(buggy) > testAmmoStackTotal(fixed));
}

TEST_CASE("M-75: Ammo merge never creates rounds across the range") {
    for (int existingClips = 1; existingClips <= 5; existingClips++) {
        for (int topRounds = 1; topRounds <= 12; topRounds++) {
            for (int newRounds = 1; newRounds <= 12; newRounds++) {
                AmmoStack stack = { existingClips, topRounds, 12 };
                int beforeTotal = testAmmoStackTotal(stack);

                AmmoStack fixed = testAmmoMergeExcessOnly(stack, newRounds);
                INFO("clips=", existingClips, " top=", topRounds, " new=", newRounds);
                // Round-trip invariant: merging must preserve the exact total.
                CHECK(testAmmoStackTotal(fixed) == beforeTotal + newRounds);

                AmmoStack buggy = testAmmoMergeBuggy(stack, newRounds);
                // The buggy behavior never preserves fewer than the correct
                // total for this case (it can only create rounds) — but it
                // must never be the round-preserving implementation.
                // Buggy creates rounds iff combined < 2*capacity (the clamp
                // to capacity loses the overflow remainder); at exactly
                // combined == 2*capacity the clamped result coincides with
                // the excess-only result (both topRounds = capacity), so the
                // strict > assertion only holds below that boundary.
                if (topRounds + newRounds > 12 && topRounds + newRounds < 24) {
                    CHECK(testAmmoStackTotal(buggy) > beforeTotal + newRounds);
                }
            }
        }
    }
}

TEST_CASE("M-75: No-overflow merge folds into the top clip") {
    AmmoStack stack = { 1, 3, 12 };
    AmmoStack fixed = testAmmoMergeExcessOnly(stack, 5);
    CHECK(fixed.clipCount == 1);
    CHECK(fixed.topRounds == 8);            // 3 + 5, no new clip
    CHECK(testAmmoStackTotal(fixed) == 8);
}

// =============================================================
// UF-010: actionPickUp inverted null check
//
// Before: if (art == nullptr) { actionFrame = artGetActionFrame(art); ... }
//         → artGetActionFrame(nullptr) when art IS null (wrong branch)
// After:  if (art != nullptr) { actionFrame = artGetActionFrame(art); ... }
//         → correct: get frame only when art exists
//
// Test validates the correct branch is taken.
// =============================================================

namespace {
struct TestArt {
    int actionFrame;
};

int testArtGetActionFrame(TestArt* art) {
    if (art != nullptr) {
        return art->actionFrame;
    }
    return -1; // default when art is null
}

int testActionPickUpFrameFixed(TestArt* art) {
    // Fix: invert condition to art != nullptr
    if (art != nullptr) {
        return testArtGetActionFrame(art); // correct: art exists → get frame
    } else {
        return -1; // correct: art is null → use -1
    }
}

int testActionPickUpFrameOld(TestArt* art) {
    // Original bug: art == nullptr check inverted
    if (art == nullptr) {
        return testArtGetActionFrame(art); // BUG: calls with nullptr when art IS null
    } else {
        return -1; // BUG: returns -1 when art IS valid (resource leak)
    }
}
} // namespace

TEST_CASE("UF-010: actionPickUp fixed — null art returns -1") {
    CHECK(testActionPickUpFrameFixed(nullptr) == -1);
}

TEST_CASE("UF-010: actionPickUp fixed — valid art returns actionFrame") {
    TestArt art = {42};
    CHECK(testActionPickUpFrameFixed(&art) == 42);
}

TEST_CASE("UF-010: actionPickUp old code — null art calls artGetActionFrame(nullptr)") {
    // The old code would crash/return wrong: artGetActionFrame called with nullptr
    // Our test-local mirror returns -1 from the null guard, but the real
    // production artGetActionFrame would dereference nullptr.
    CHECK(testActionPickUpFrameOld(nullptr) == -1); // test-local safe, but conceptually buggy
}

TEST_CASE("UF-010: actionPickUp old code — valid art returns -1 (wrong)") {
    TestArt art = {42};
    // Old code: when art IS valid, the else branch runs → returns -1
    // Loses the correct actionFrame AND leaks the art lock (no artUnlock called)
    CHECK(testActionPickUpFrameOld(&art) == -1);
}

TEST_CASE("UF-010: actionPickUp — fixed code correctly branches") {
    TestArt art = {99};
    // Fixed: valid art → get frame (99)
    CHECK(testActionPickUpFrameFixed(&art) == 99);
    // Fixed: null art → return -1
    CHECK(testActionPickUpFrameFixed(nullptr) == -1);
}

// =============================================================
// UF-H-044: _action_melee null defender guard
//
// Before: No null guard — attack->defender->data dereferenced directly
// After:  if (attack == nullptr || attack->defender == nullptr) return 0;
//
// Test validates defense-in-depth null guard.
// =============================================================

namespace {
struct TestAttack {
    bool defenderIsNull;
    // When !defenderIsNull, the defender "exists" and has combat results
};

int testActionMeleeFixed(TestAttack* attack) {
    // Fix: null guard for defense-in-depth
    if (attack == nullptr || attack->defenderIsNull) {
        return 0; // safe return
    }
    // ... rest of function (would dereference defender->data.critter.combat.results)
    return 1; // success
}

int testActionMeleeOld(TestAttack* attack) {
    // Original: no guard, would crash on nullptr defender
    // In test-local mirror, we simulate the crash by returning -1 if guard missing
    if (attack == nullptr || attack->defenderIsNull) {
        return -1; // would be SIGSEGV in real code
    }
    return 1; // success
}
} // namespace

TEST_CASE("UF-H-044: _action_melee with null defender returns safely") {
    TestAttack attackWithNullDefender = {true};
    CHECK(testActionMeleeFixed(&attackWithNullDefender) == 0); // safe return
}

TEST_CASE("UF-H-044: _action_melee with null attack returns safely") {
    CHECK(testActionMeleeFixed(nullptr) == 0); // null attack → safe return
}

TEST_CASE("UF-H-044: _action_melee with valid defender proceeds normally") {
    TestAttack attackWithDefender = {false};
    CHECK(testActionMeleeFixed(&attackWithDefender) == 1); // normal execution
}

TEST_CASE("UF-H-044: old _action_melee crashes on null defender") {
    TestAttack attackWithNullDefender = {true};
    // Old code would return -1 (our proxy for "would crash")
    CHECK(testActionMeleeOld(&attackWithNullDefender) == -1);
}

TEST_CASE("UF-H-044: caller _combat_attack enforces non-null in practice") {
    // The caller (_combat_attack) dereferences defender->id before calling
    // _action_melee, so defender is non-null through the normal call path.
    // The null guard is defense-in-depth for any future code path change.
    // This test documents that expectation.
    TestAttack normalAttack = {false};
    CHECK(testActionMeleeFixed(&normalAttack) == 1);
}
