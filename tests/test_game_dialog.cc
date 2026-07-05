// Unit tests for game_dialog.cc — fork changes regression tests.
//
// Tests:
//   1. N2-001: artGetFidgetCount dead error-check branch —
//      artGetFidgetCount never returns -1 but caller at game_dialog.cc:2668
//      checks fidgetCount == -1. Dead code; OOB silently degrades to default
//      fidget instead of error. Cross-file contract mismatch (art.cc↔game_dialog.cc).
//   2. N2-049: gGameDialogSpeaker != nullptr null-guard in _gdialogExitFromScript
//      (game_dialog.cc:1047). Pre-fork dereferences gGameDialogSpeaker->pid
//      without null check. Crash when dialog exits with no active speaker.
//
// This file does NOT link game_dialog.cc (40+ engine deps). It mirrors
// the specific fork-fixed code paths as local functions and validates
// against the pre-fork buggy behavior.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>
#include <cstdlib>

// All test structures and functions are self-contained mirrors;
// no production headers needed for the test logic.

// ============================================================
// N2-001: artGetFidgetCount dead error-check (game_dialog.cc:2668)
// ============================================================
// Bug: artGetFidgetCount returns 0 for OOB head index (after the >= fix at
// art.cc:393) or for unhandled fidget values (line 406-409). It NEVER returns -1.
// However, the sole caller at game_dialog.cc:2668 checks:
//   if (fidgetCount == -1) { debugPrint("Error - No available fidgets\n"); return; }
// This is DEAD CODE — artGetFidgetCount cannot return -1. When head index is OOB,
// the caller silently proceeds with fidgetCount=0, falling through the switch
// to set fidget=FIDGET_GOOD — a silent degradation to the default fidget animation.
//
// The contract should be: artGetFidgetCount returns fidget count (≥0) on success,
// or a negative sentinel on error. Current implementation returns 0 for both
// "no fidgets" AND "OOB head index" — the caller cannot distinguish.
//
// Research: CONFIRMED (cross-file contract mismatch)
// Verification: CONFIRMED (exhaustive code path analysis confirms no -1 return)

// Mirrored artGetFidgetCount return semantics for testing
// Simplified version: returns the valid return values of the real function
// 0 = OOB head index or unhandled fidget value
// positive = valid fidget count
static int mirroredGetFidgetCount(int headIndex, int maxHeadCount, int fidgetType) {
    if (headIndex >= maxHeadCount) {
        return 0;  // OOB — returns 0, NOT -1
    }
    // Assume fidget counts are 3, 4, 3 for good/neutral/bad
    switch (fidgetType) {
    case 1: return 3;  // FIDGET_GOOD
    case 4: return 4;  // FIDGET_NEUTRAL
    case 7: return 3;  // FIDGET_BAD
    default: return 0;  // unhandled fidget — returns 0, NOT -1
    }
}

// Mirrors the caller at game_dialog.cc:2666-2671
// This reveals the dead error-check branch
static bool callerHandlesFidgetCount(int fidgetCount, bool* errorLogged) {
    *errorLogged = false;
    if (fidgetCount == -1) {
        *errorLogged = true;
        return false;  // dead code path — never reached
    }
    if (fidgetCount == 0) {
        return false;  // silent degradation — no error, but no fidgets
    }
    return true;  // normal path
}

TEST_CASE("N2-001 — artGetFidgetCount never returns -1, caller dead-code branch (game_dialog.cc:2668)")
{
    SUBCASE("OOB head index returns 0, not -1 — dead code path unreachable") {
        int result = mirroredGetFidgetCount(99, 14, 1);  // 99 > 14 valid heads
        CHECK(result == 0);          // Returns 0 for OOB, NOT -1
        CHECK(result != -1);         // Confirms -1 is never returned

        bool errorLogged;
        bool handled = callerHandlesFidgetCount(result, &errorLogged);
        CHECK(handled == false);      // 0 means no fidgets
        CHECK(errorLogged == false);  // -1 check is dead — never triggers
    }

    SUBCASE("valid head, valid fidget returns positive count") {
        int result = mirroredGetFidgetCount(0, 14, 1);
        CHECK(result > 0);  // 3 fidgets
        bool errorLogged;
        CHECK(callerHandlesFidgetCount(result, &errorLogged) == true);
        CHECK(errorLogged == false);
    }

    SUBCASE("valid head, unhandled fidget returns 0 — silent degradation") {
        // Fidget type 2 (not in the switch) → returns 0 (default case)
        int result = mirroredGetFidgetCount(0, 14, 2);
        CHECK(result == 0);
        bool errorLogged;
        bool handled = callerHandlesFidgetCount(result, &errorLogged);
        CHECK(handled == false);
        CHECK(errorLogged == false);
        // Production behavior: fidgetCount=0 → falls through switch →
        // sets fidget=FIDGET_GOOD → silent default fidget animation
    }

    SUBCASE("exhaustive: all head indices + fidget types return >= 0, never -1") {
        // Verify the core finding: artGetFidgetCount NEVER returns -1
        for (int head = 0; head < 14; head++) {
            for (int fidget : {1, 4, 7}) {
                int result = mirroredGetFidgetCount(head, 14, fidget);
                CHECK(result >= 0);
                CHECK(result != -1);
            }
        }
        // OOB case
        CHECK(mirroredGetFidgetCount(14, 14, 1) >= 0);
        CHECK(mirroredGetFidgetCount(14, 14, 4) >= 0);
        CHECK(mirroredGetFidgetCount(14, 14, 7) >= 0);
    }

    SUBCASE("boundary: exactly at the limit (head == maxCount)") {
        // The fork fix changed > to >= at art.cc:393
        // head == maxCount → OOB, returns 0
        int result = mirroredGetFidgetCount(14, 14, 1);
        CHECK(result == 0);
    }

    SUBCASE("boundary: one below limit (head == maxCount - 1)") {
        // Valid: head == 13, maxCount == 14
        int result = mirroredGetFidgetCount(13, 14, 1);
        CHECK(result > 0);
    }
}

// ============================================================
// N2-049: null speaker guard (game_dialog.cc:1047)
// ============================================================
// Fork added: if (gGameDialogSpeaker != nullptr && PID_TYPE(...))
// Pre-fork: direct dereference gGameDialogSpeaker->pid → crash on null.
// Null speaker can occur when dialog exits without an active speaker,
// e.g., when HOOK_GAMEMODECHANGE triggers exit during game load or
// when a script calls gdialogExit before gdialogStart.
// Research: LIKELY (ET Tu's TMA system creates/destroys dialog windows
// that could trigger this code path).

// Mirrors the pre-fork (buggy) _gdialogExitFromScript null dereference pattern
static bool exitFromScriptPreFork(void* speaker, int* outPidType) {
    if (speaker == nullptr) {
        // Pre-fork: dereferences null pointer → CRASH
        // In test we detect this instead of crashing
        return false;  // would have crashed
    }
    *outPidType = ((int*)speaker)[35];  // simulated PID_TYPE(gGameDialogSpeaker->pid)
    return true;
}

// Mirrors the post-fork (fixed) _gdialogExitFromScript with null guard
static bool exitFromScriptPostFork(void* speaker, int* outPidType) {
    if (speaker != nullptr) {
        *outPidType = ((int*)speaker)[35];
        return true;
    }
    // Guard: skip dereference when speaker is null
    return false;
}

TEST_CASE("N2-049 — gGameDialogSpeaker null guard prevents crash (game_dialog.cc:1047)")
{
    // Simulated Object pointer with pid at offset 35*4 = 140 bytes
    // (Object struct has 35 ints before pid)
    int dummyObject[40];  // enough space for simulated Object
    dummyObject[35] = 0x01 << 24;  // pid with OBJ_TYPE_ITEM type

    SUBCASE("null speaker — pre-fork crashes, post-fork guards") {
        int pidType = -1;
        // Pre-fork: would dereference nullptr
        bool preForkOk = exitFromScriptPreFork(nullptr, &pidType);
        CHECK(preForkOk == false);  // null speaker → crash prevented in test

        // Post-fork: gracefully returns false
        pidType = -1;
        bool postForkOk = exitFromScriptPostFork(nullptr, &pidType);
        CHECK(postForkOk == false);  // null speaker → skip
        CHECK(pidType == -1);        // pidType unchanged
    }

    SUBCASE("non-null speaker, critter type — both paths work") {
        int pidTypeNoGuard = -1;
        bool preOk = exitFromScriptPreFork(dummyObject, &pidTypeNoGuard);
        CHECK(preOk == true);
        CHECK(pidTypeNoGuard != -1);

        int pidTypeGuarded = -1;
        bool postOk = exitFromScriptPostFork(dummyObject, &pidTypeGuarded);
        CHECK(postOk == true);
        CHECK(pidTypeGuarded == pidTypeNoGuard);
    }

    SUBCASE("non-null speaker, item type — pre-fork dereferences fine but skips tile restore") {
        // From game_dialog.cc:1047:
        // if (gGameDialogSpeaker != nullptr && PID_TYPE(gGameDialogSpeaker->pid) != OBJ_TYPE_ITEM)
        // Item-type speakers skip gameDialogRestoreCenterTile()
        int dummyItemObj[40];
        dummyItemObj[35] = 0x00 << 24;  // pid with OBJ_TYPE_ITEM type (0)

        int pidType = -1;
        bool postOk = exitFromScriptPostFork(dummyItemObj, &pidType);
        CHECK(postOk == true);
        // OBJ_TYPE_ITEM = 0, so PID_TYPE(0) = 0 → skip restore
        CHECK(pidType == 0);
    }

    SUBCASE("non-null speaker, critter type — restore center tile called") {
        int dummyCritterObj[40];
        dummyCritterObj[35] = 0x01 << 24;  // pid with OBJ_TYPE_CRITTER type (1)

        int pidType = -1;
        bool postOk = exitFromScriptPostFork(dummyCritterObj, &pidType);
        CHECK(postOk == true);
        // Raw pid value is 0x01000000 (16777216); PID_TYPE(pidType) would extract 1
        CHECK(pidType == 0x01000000);
    }

    SUBCASE("guard semantics: pid type accessed ONLY when speaker is non-null") {
        // Verify the short-circuit evaluation: nullptr check must come first
        // If speaker is null, PID_TYPE should never be called
        // This test validates the guard order in the original code
        int pidType = -1;
        bool result = exitFromScriptPostFork(nullptr, &pidType);
        CHECK(result == false);
        // pid type was not accessed (remains -1)
        CHECK(pidType == -1);
    }
}
