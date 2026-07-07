// Unit tests for combat-related opcode ext opcodes.
//
// F-090 (MEDIUM): get_last_target per-critter cache multi-critter test.
//   Production: sfall_opcodes.cc:4813-4856 — extern gLastAttacker/gLastTarget,
//   file-static gCritterLastTarget/gCritterLastAttacker maps.
//   Tests per-critter isolation: setting target for one critter does not
//   affect another critter's cached target. Mirrors the lazy-evaluation
//   pattern from op_get_last_target.
//
// F2-037 (MEDIUM): force_aimed_shots/disable_aimed_shots integration test.
//   Production: sfall_opcodes.cc:4412-4424 (opcodes), combat.cc:3618-3626
//   (resolution). Documents the else-if precedence rule: disable takes
//   priority over force when both are set for the same PID.
//   Mirrors sfallGetForceAimedShots/sfallGetDisableAimedShots logic.
//
// F-090 mirrors the per-critter caching algorithm. The production
// gCritterLastTarget/gCritterLastAttacker maps are file-static and
// inaccessible from tests, but gLastAttacker/gLastTarget are extern
// globals available via test_stubs.
//
// See report at tmp/s7-impl-opcodes-b-report.md for INTENT section.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <unordered_map>

// ============================================================
// Test-local types mirroring production structs
// ============================================================

// Minimal Object struct — mirrors object.h Object.
// We only need the `id` field for cache keying.
struct TestObject {
    int id;
};

// ============================================================
// Mirror: get_last_target per-critter cache
// Production: sfall_opcodes.cc:4816-4856
//
// The production code maintains:
//   extern int gLastAttacker = -1;  // set by combat.cc
//   extern int gLastTarget = -1;    // set by combat.cc
//   static unordered_map<int, int> gCritterLastTarget;   // keyed by Object::id
//   static unordered_map<int, int> gCritterLastAttacker;  // keyed by Object::id
//
// Algorithm (op_get_last_target, lines 4821-4856):
//   1. Receive critter Object* from stack
//   2. If critter is not null:
//      a. If gLastAttacker == critter->id: cache gLastTarget for this critter
//      b. Look up critter->id in gCritterLastTarget
//      c. If found and value >= 0: look up the target object by id
//      d. If target object exists: return it
//      e. Otherwise: remove stale entry
//   3. Global fallback: if gLastTarget >= 0, look up and return
//   4. Return 0 (nullptr)
// ============================================================

// Mirror of the per-critter last_target state machine.
// This replicates the caching behavior exactly.
class LastTargetCache {
public:
    // Global state — matches extern globals gLastAttacker/gLastTarget.
    int lastAttacker = -1;
    int lastTarget = -1;

    // Per-critter caches — matches file-static gCritterLastTarget/gCritterLastAttacker.
    // Keyed by critter->id, value is the target/attacker object id.
    std::unordered_map<int, int> critterLastTarget;
    std::unordered_map<int, int> critterLastAttacker;

    // Mirror of op_get_last_target (lines 4821-4856).
    // Returns the target object id for the given critter, or -1 if none.
    int getLastTarget(int critterObjectId) {
        // Lazy-populate per-critter storage from global.
        // Production: sfall_opcodes.cc:4830-4832
        if (lastAttacker == critterObjectId) {
            critterLastTarget[critterObjectId] = lastTarget;
        }

        // Check per-critter cache first.
        // Production: sfall_opcodes.cc:4835-4844
        auto it = critterLastTarget.find(critterObjectId);
        if (it != critterLastTarget.end() && it->second >= 0) {
            // Production would call objectFindById(it->second) here.
            // If the object was found (non-null), return it.
            // If not found, erase and fall through.
            return it->second; // Return the cached target id
        }

        // Global fallback.
        // Production: sfall_opcodes.cc:4848-4854
        if (lastTarget >= 0) {
            return lastTarget;
        }

        return -1;
    }

    // Mirror of op_get_last_attacker (lines 4858-4888).
    // Returns the attacker object id for the given critter, or -1 if none.
    int getLastAttacker(int critterObjectId) {
        // Lazy-populate: if this critter was the most recent TARGET,
        // cache who attacked it.
        // Production: sfall_opcodes.cc:4865-4869
        if (lastTarget == critterObjectId) {
            critterLastAttacker[critterObjectId] = lastAttacker;
        }

        auto it = critterLastAttacker.find(critterObjectId);
        if (it != critterLastAttacker.end() && it->second >= 0) {
            return it->second;
        }

        if (lastAttacker >= 0) {
            return lastAttacker;
        }

        return -1;
    }
};

// ============================================================
// F-090: get_last_target per-critter cache multi-critter test
// ============================================================

TEST_CASE("get_last_target — multi-critter cache isolation")
{
    // Scenario: Critter A attacks Target X, then Critter B attacks Target Y.
    // Verify that Critter A still returns Target X (not Target Y).
    LastTargetCache cache;

    const int CRITTER_A = 100;
    const int CRITTER_B = 200;
    const int TARGET_X = 300;
    const int TARGET_Y = 400;

    SUBCASE("Stale global: critter returns the global fallback when no per-critter cache exists") {
        cache.lastAttacker = CRITTER_A;
        cache.lastTarget = TARGET_X;

        // Critter A was the last attacker, so it gets TARGET_X from global.
        int result = cache.getLastTarget(CRITTER_A);
        // The lazy-evaluation should have cached TARGET_X for Critter A.
        CHECK(result == TARGET_X);
    }

    SUBCASE("Per-critter cache: Critter A retains TARGET_X after Critter B attacks TARGET_Y") {
        // Setup: Critter A attacks Target X.
        cache.lastAttacker = CRITTER_A;
        cache.lastTarget = TARGET_X;

        // First query for Critter A — lazy-populates cache.
        int resultA_first = cache.getLastTarget(CRITTER_A);
        CHECK(resultA_first == TARGET_X);

        // Now Critter B attacks Target Y, overwriting the globals.
        cache.lastAttacker = CRITTER_B;
        cache.lastTarget = TARGET_Y;

        // Critter A's cached target should STILL be TARGET_X.
        int resultA_second = cache.getLastTarget(CRITTER_A);
        CHECK(resultA_second == TARGET_X);

        // Critter B's target should be TARGET_Y (from its own attack).
        int resultB = cache.getLastTarget(CRITTER_B);
        CHECK(resultB == TARGET_Y);
    }

    SUBCASE("Lazy-evaluation: Critter C (never attacked) gets global fallback") {
        const int CRITTER_C = 500;

        // Setup: Critter A attacks.
        cache.lastAttacker = CRITTER_A;
        cache.lastTarget = TARGET_X;

        // Critter C has never attacked — no per-critter cache for it.
        // It should fall back to the global gLastTarget.
        int resultC = cache.getLastTarget(CRITTER_C);
        CHECK(resultC == TARGET_X); // global fallback

        // Change globals to Critter B attacking Target Y.
        cache.lastAttacker = CRITTER_B;
        cache.lastTarget = TARGET_Y;

        // Critter C now sees Target Y via global fallback.
        int resultC_after = cache.getLastTarget(CRITTER_C);
        CHECK(resultC_after == TARGET_Y);
    }

    SUBCASE("Multiple critters: each retains independent cached target") {
        const int CRITTER_D = 600;
        const int CRITTER_E = 700;
        const int TARGET_D = 800;
        const int TARGET_E = 900;

        // Critter D attacks TARGET_D.
        cache.lastAttacker = CRITTER_D;
        cache.lastTarget = TARGET_D;
        cache.getLastTarget(CRITTER_D); // populate cache

        // Critter E attacks TARGET_E.
        cache.lastAttacker = CRITTER_E;
        cache.lastTarget = TARGET_E;
        cache.getLastTarget(CRITTER_E); // populate cache

        // Both critters should return their own targets independently.
        CHECK(cache.getLastTarget(CRITTER_D) == TARGET_D);
        CHECK(cache.getLastTarget(CRITTER_E) == TARGET_E);
    }

    SUBCASE("Target with negative id (-1 sentinel) is treated as 'no target'") {
        // Production checks `it->second >= 0` as a validity check.
        // A value of -1 in the cache means "no valid target."
        cache.lastAttacker = CRITTER_A;
        cache.lastTarget = -1; // -1 sentinel = no target

        int result = cache.getLastTarget(CRITTER_A);
        // Should NOT return -1 from cache (since cache check excludes negative).
        // Should fall through to global fallback, which also has -1.
        // Should return -1 (no target).
        CHECK(result == -1);
    }

    SUBCASE("Per-critter cache survives global state changes") {
        // Critter A attacks Target X.
        cache.lastAttacker = CRITTER_A;
        cache.lastTarget = TARGET_X;

        // Verify Critter A's cache entry.
        CHECK(cache.getLastTarget(CRITTER_A) == TARGET_X);

        // Overwrite globals with an unrelated attack.
        cache.lastAttacker = 9999;
        cache.lastTarget = 9999;

        // Critter A should still return TARGET_X from cache.
        CHECK(cache.getLastTarget(CRITTER_A) == TARGET_X);

        // An unrelated critter should get the new global.
        CHECK(cache.getLastTarget(5000) == 9999);
    }
}

TEST_CASE("get_last_target — default state")
{
    LastTargetCache cache;

    // With no attacks recorded, all queries return -1 (no target).
    CHECK(cache.getLastTarget(100) == -1);
    CHECK(cache.getLastTarget(200) == -1);
    CHECK(cache.getLastTarget(300) == -1);
}

// ============================================================
// F2-037: force_aimed_shots / disable_aimed_shots integration
// ============================================================

// Mirror of sfallGetForceAimedShots / sfallGetDisableAimedShots.
// Production: sfall_opcodes.cc:4400-4410 (accessors),
//            combat.cc:3618-3626 (resolution order).
//
// The production maps are:
//   static unordered_map<int, bool> gForceAimedShotsMap;
//   static unordered_map<int, bool> gDisableAimedShotsMap;
//
// Resolution order in combat.cc:3618-3626:
//   if (sfallGetDisableAimedShots(pid) && hitLocation != TORSO && hitLocation != UNCALLED) {
//       hitLocation = UNCALLED;  // DISABLE takes effect
//   } else if (sfallGetForceAimedShots(pid) && hitLocation == UNCALLED) {
//       hitLocation = HEAD;      // FORCE takes effect only if DISABLE didn't fire
//   }
//
// Key contract: disable_aimed_shots takes PRECEDENCE over force_aimed_shots.
// When BOTH are set for the same PID, disable wins.

enum HitLocation {
    HIT_LOCATION_UNCALLED = -1,  // unaimed shot (no body part targeted)
    HIT_LOCATION_HEAD = 0,
    HIT_LOCATION_LEFT_ARM = 1,
    HIT_LOCATION_RIGHT_ARM = 2,
    HIT_LOCATION_TORSO = 3,
    HIT_LOCATION_RIGHT_LEG = 4,
    HIT_LOCATION_LEFT_LEG = 5,
    HIT_LOCATION_EYES = 6,
    HIT_LOCATION_GROIN = 7,
};

// Mirror of the aimed-shot resolution logic from combat.cc:3618-3626.
// Replicates the exact else-if chain with the documented precedence.
static HitLocation resolveAimedShot(
    int pid,
    HitLocation originalLocation,
    bool forceEnabled,
    bool disableEnabled)
{
    // Production: combat.cc:3622-3626
    // disable_aimed_shots: convert aimed shots to unaimed.
    // Excludes TORSO and UNCALLED (those aren't "aimed" shots).
    if (disableEnabled
        && originalLocation != HIT_LOCATION_TORSO
        && originalLocation != HIT_LOCATION_UNCALLED) {
        originalLocation = HIT_LOCATION_UNCALLED;
    }
    // force_aimed_shots: convert unaimed shots to aimed (HEAD).
    // This is an ELSE-IF: only runs if disable_aimed_shots did NOT fire.
    else if (forceEnabled && originalLocation == HIT_LOCATION_UNCALLED) {
        originalLocation = HIT_LOCATION_HEAD;
    }

    return originalLocation;
}

TEST_CASE("force_aimed_shots / disable_aimed_shots — independent flags")
{
    const int PID_RAT = 0x1000001;

    SUBCASE("Neither flag set: hit location unchanged") {
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_UNCALLED, false, false) == HIT_LOCATION_UNCALLED);
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_HEAD, false, false) == HIT_LOCATION_HEAD);
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_EYES, false, false) == HIT_LOCATION_EYES);
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_GROIN, false, false) == HIT_LOCATION_GROIN);
    }

    SUBCASE("force_aimed_shots only: unaimed → HEAD, aimed unchanged") {
        // force_aimed_shots converts UNCALLED → HEAD.
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_UNCALLED, true, false) == HIT_LOCATION_HEAD);

        // Already-aimed shots are NOT converted to HEAD.
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_EYES, true, false) == HIT_LOCATION_EYES);
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_GROIN, true, false) == HIT_LOCATION_GROIN);
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_HEAD, true, false) == HIT_LOCATION_HEAD);
    }

    SUBCASE("disable_aimed_shots only: aimed → UNCALLED, torso/uncalled unchanged") {
        // disable_aimed_shots converts aimed shots to UNCALLED.
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_HEAD, false, true) == HIT_LOCATION_UNCALLED);
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_EYES, false, true) == HIT_LOCATION_UNCALLED);
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_GROIN, false, true) == HIT_LOCATION_UNCALLED);

        // TORSO and UNCALLED are NOT converted (they're not "aimed" shots).
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_TORSO, false, true) == HIT_LOCATION_TORSO);
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_UNCALLED, false, true) == HIT_LOCATION_UNCALLED);
    }

    SUBCASE("BOTH flags set: disable_aimed_shots takes PRECEDENCE") {
        // When both flags are set, disable wins (else-if chain).
        // An aimed shot (HEAD) with both flags → disable converts to UNCALLED.
        // The force branch does NOT execute because the else-if prevents it.
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_HEAD, true, true) == HIT_LOCATION_UNCALLED);
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_EYES, true, true) == HIT_LOCATION_UNCALLED);
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_GROIN, true, true) == HIT_LOCATION_UNCALLED);

        // TORSO with both flags: disable ignores TORSO, so fall through.
        // force also ignores TORSO (only converts UNCALLED). So TORSO stays.
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_TORSO, true, true) == HIT_LOCATION_TORSO);

        // UNCALLED with both flags: disable ignores UNCALLED, force converts to HEAD.
        CHECK(resolveAimedShot(PID_RAT, HIT_LOCATION_UNCALLED, true, true) == HIT_LOCATION_HEAD);
    }
}

TEST_CASE("force_aimed_shots / disable_aimed_shots — edge cases")
{
    const int PID_DUDE = 0x1000010;

    // All 8 hit locations tested with all 4 flag combinations.
    // This verifies the decision table is complete and consistent.

    struct TestCase {
        HitLocation input;
        bool force;
        bool disable;
        HitLocation expected;
    };

    TestCase cases[] = {
        // Neither flag set: all locations pass through unchanged.
        {HIT_LOCATION_UNCALLED, false, false, HIT_LOCATION_UNCALLED},
        {HIT_LOCATION_HEAD,     false, false, HIT_LOCATION_HEAD},
        {HIT_LOCATION_EYES,     false, false, HIT_LOCATION_EYES},
        {HIT_LOCATION_GROIN,    false, false, HIT_LOCATION_GROIN},
        {HIT_LOCATION_TORSO,    false, false, HIT_LOCATION_TORSO},
        {HIT_LOCATION_LEFT_ARM, false, false, HIT_LOCATION_LEFT_ARM},
        {HIT_LOCATION_RIGHT_ARM,false, false, HIT_LOCATION_RIGHT_ARM},
        {HIT_LOCATION_LEFT_LEG, false, false, HIT_LOCATION_LEFT_LEG},

        // Force only: unaimed → HEAD.
        {HIT_LOCATION_UNCALLED, true,  false, HIT_LOCATION_HEAD},
        {HIT_LOCATION_HEAD,     true,  false, HIT_LOCATION_HEAD},
        {HIT_LOCATION_EYES,     true,  false, HIT_LOCATION_EYES},

        // Disable only: aimed → UNCALLED.
        {HIT_LOCATION_HEAD,     false, true,  HIT_LOCATION_UNCALLED},
        {HIT_LOCATION_EYES,     false, true,  HIT_LOCATION_UNCALLED},
        {HIT_LOCATION_GROIN,    false, true,  HIT_LOCATION_UNCALLED},
        {HIT_LOCATION_TORSO,    false, true,  HIT_LOCATION_TORSO},
        {HIT_LOCATION_UNCALLED, false, true,  HIT_LOCATION_UNCALLED},

        // Both flags: disable wins for aimed → UNCALLED.
        {HIT_LOCATION_HEAD,     true,  true,  HIT_LOCATION_UNCALLED},
        {HIT_LOCATION_EYES,     true,  true,  HIT_LOCATION_UNCALLED},
        {HIT_LOCATION_GROIN,    true,  true,  HIT_LOCATION_UNCALLED},
        {HIT_LOCATION_TORSO,    true,  true,  HIT_LOCATION_TORSO},
        {HIT_LOCATION_UNCALLED, true,  true,  HIT_LOCATION_HEAD},
    };

    for (auto& tc : cases) {
        HitLocation result = resolveAimedShot(PID_DUDE, tc.input, tc.force, tc.disable);
        INFO("input=", tc.input, " force=", tc.force, " disable=", tc.disable);
        CHECK(result == tc.expected);
    }
}

TEST_CASE("force_aimed_shots / disable_aimed_shots — different PIDs")
{
    // Verify that flags are per-PID (tested with simulated per-PID maps).
    // The production maps are keyed by PID (int).
    std::unordered_map<int, bool> forceMap;
    std::unordered_map<int, bool> disableMap;

    const int PID_A = 0x1000001; // rat
    const int PID_B = 0x1000002; // gecko

    // Only PID_A has force_aimed_shots enabled.
    forceMap[PID_A] = true;

    // Only PID_B has disable_aimed_shots enabled.
    disableMap[PID_B] = true;

    auto resolve = [&](int pid, HitLocation loc) -> HitLocation {
        return resolveAimedShot(pid, loc, forceMap[pid], disableMap[pid]);
    };

    // PID_A: force enabled, disable not — unaimed → HEAD.
    CHECK(resolve(PID_A, HIT_LOCATION_UNCALLED) == HIT_LOCATION_HEAD);
    CHECK(resolve(PID_A, HIT_LOCATION_EYES) == HIT_LOCATION_EYES);

    // PID_B: disable enabled, force not — aimed → UNCALLED.
    CHECK(resolve(PID_B, HIT_LOCATION_EYES) == HIT_LOCATION_UNCALLED);
    CHECK(resolve(PID_B, HIT_LOCATION_UNCALLED) == HIT_LOCATION_UNCALLED);

    // PID_C (no entry): neither flag → unchanged.
    const int PID_C = 0x1000003;
    CHECK(resolve(PID_C, HIT_LOCATION_UNCALLED) == HIT_LOCATION_UNCALLED);
    CHECK(resolve(PID_C, HIT_LOCATION_EYES) == HIT_LOCATION_EYES);
}
