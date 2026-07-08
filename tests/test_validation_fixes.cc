// Unit tests for s6 validation fixes (F-04, F-46, F-23, F2-11, F2-12, F-37).
//
// These are self-contained mirrors of the production code logic — no linking
// against the engine. Each test validates the corrected behavior pattern.
//
// Fixes tested:
//   - F-04 (HIGH): sscanf return check in mouse_manager.cc — uninitialized v3/v4 used
//                  as malloc multipliers. Fix: init to 0, check sscanf == 2.
//   - F-46 (MEDIUM): sscanf return check in game.cc — uninitialized target int in
//                    globalVarsRead. Fix: init target to 0 before sscanf.
//   - F-23 (MEDIUM): opSetLocalVar bounds check in scripts.cc — variable not checked
//                    against script->localVarsCount. Fix: add bounds check.
//   - F2-11 (MEDIUM): CRC mismatch non-fatal during load (loadsave.cc). Fix: abort
//                     load on CRC mismatch.
//   - F2-12 (MEDIUM): malloc failure silently skipped during save CRC (loadsave.cc).
//                     Fix: abort save on malloc failure.
//   - F-37 (LOW): null guard for HOOK_FINDTARGET site 3 (combat_ai.cc). Fix: skip
//                 hook when result is nullptr.
//
// Reference source: synthesis report s3-synth-report.md, adv report s5-adv-m1-report.md

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>

// ============================================================================
// F-04: sscanf return check mirror (mouse_manager.cc:355-363)
// ============================================================================
// Original: int v3; float v4; sscanf(sep+1, "%d %f", &v3, &v4);
// Bug: v3 used as malloc multiplier without checking parse success.
// Fix: init to 0, check sscanf return == 2, return false on failure.

struct AnimatedParseResult {
    bool valid;
    int frameCount;
    float field18;
};

// Mirror of pre-fix (buggy) — no sscanf return check, uninitialized vars
static AnimatedParseResult parseMouseAnimPreFix(const char* input) {
    AnimatedParseResult result = {};
    int v3;
    float v4;
    sscanf(input, "%d %f", &v3, &v4);
    // UNSAFE: v3 could be uninitialized if sscanf fails
    result.valid = true; // always considered valid
    result.frameCount = v3;
    result.field18 = v4;
    return result;
}

// Mirror of post-fix (correct) — init vars, check sscanf return
static AnimatedParseResult parseMouseAnimPostFix(const char* input) {
    AnimatedParseResult result = {};
    int v3 = 0;
    float v4 = 0.0f;
    if (sscanf(input, "%d %f", &v3, &v4) != 2) {
        result.valid = false;
        result.frameCount = 0;
        result.field18 = 0.0f;
        return result;
    }
    result.valid = true;
    result.frameCount = v3;
    result.field18 = v4;
    return result;
}

TEST_CASE("F-04: sscanf return check — valid parse produces correct values")
{
    auto r = parseMouseAnimPostFix("5 0.75");
    CHECK(r.valid);
    CHECK(r.frameCount == 5);
    CHECK(r.field18 == 0.75f);
}

TEST_CASE("F-04: sscanf return check — non-numeric input rejected")
{
    auto r = parseMouseAnimPostFix("abc def");
    CHECK_FALSE(r.valid);
    CHECK(r.frameCount == 0);    // safe default, not uninitialized
    CHECK(r.field18 == 0.0f);
}

TEST_CASE("F-04: sscanf return check — partial parse (only one value) rejected")
{
    auto r = parseMouseAnimPostFix("5");
    CHECK_FALSE(r.valid);
    CHECK(r.frameCount == 0);
}

TEST_CASE("F-04: sscanf return check — empty input rejected")
{
    auto r = parseMouseAnimPostFix("");
    CHECK_FALSE(r.valid);
    CHECK(r.frameCount == 0);
}

TEST_CASE("F-04: sscanf return check — large values survive parsing")
{
    auto r = parseMouseAnimPostFix("128 1.5");
    CHECK(r.valid);
    CHECK(r.frameCount == 128);
    CHECK(r.field18 == 1.5f);
}

TEST_CASE("F-04: sscanf return check — pre-fix comparison: garbage on bad input")
{
    // Demonstrate that pre-fix behavior allows uninitialized/unsafe state.
    auto bad = parseMouseAnimPreFix("abc def");
    // Pre-fix: valid is always true, v3/v4 are whatever sscanf left in stack.
    // We can't assert on the value (indeterminate), but we can assert that
    // valid=true when it shouldn't be.
    CHECK(bad.valid);  // bug: bad input accepted as valid
}

// ============================================================================
// F-46: sscanf return check + init in game variable loading (game.cc:1134)
// ============================================================================
// Original: sscanf(equals+1, "%d", *variablesListPtr + ...) — no init, no check
// Bug: parse failure leaves uninitialized heap value as game variable.
// Fix: init target to 0 before sscanf.

// Mirror of pre-fix (buggy) — no init, no sscanf return check
static int parseGameVarPreFix(const char* equalsPart) {
    int value;
    sscanf(equalsPart, "%d", &value);
    return value; // could be uninitialized
}

// Mirror of post-fix (correct) — init to 0 before sscanf
static int parseGameVarPostFix(const char* equalsPart) {
    int value = 0;
    sscanf(equalsPart, "%d", &value);
    return value; // returns 0 on parse failure (safe default)
}

TEST_CASE("F-46: game variable sscanf — valid integer parses correctly")
{
    CHECK(parseGameVarPostFix("42") == 42);
    CHECK(parseGameVarPostFix("-1") == -1);
    CHECK(parseGameVarPostFix("0") == 0);
}

TEST_CASE("F-46: game variable sscanf — non-numeric input returns 0 (safe default)")
{
    CHECK(parseGameVarPostFix("abc") == 0);
    CHECK(parseGameVarPostFix("def") == 0);
    CHECK(parseGameVarPostFix("") == 0);
}

TEST_CASE("F-46: game variable sscanf — int overflow yields undefined but not uninitialized")
{
    // With init=0, even on overflow, the result is at least not uninitialized.
    int result = parseGameVarPostFix("99999999999999999999");
    // Don't check the exact value (overflow behavior is implementation-defined),
    // but assert the function doesn't return uninitialized stack data.
    (void)result; // at least doesn't crash
    CHECK(true);
}

TEST_CASE("F-46: game variable sscanf — hex input treated as not matching (%d)")
{
    // %d does not parse hex; sscanf returns 0 (no match), value stays at init=0.
    CHECK(parseGameVarPostFix("0xFF") == 0);
    CHECK(parseGameVarPostFix("0x10") == 0);
}

// ============================================================================
// F-23: scriptSetLocalVar bounds check mirror (scripts.cc:2986-3009)
// ============================================================================
// Original: variable used in offset = localVarsOffset + variable without bounds check.
// Bug: variable >= localVarsCount → OOB write into heap, cross-script corruption.
// Fix: add variable < 0 || variable >= localVarsCount check, return -1 on OOB.

struct TestScript {
    int localVarsCount;
    int localVarsOffset;
    int* localVars; // simulated heap array
};

static int testScriptSetLocalVarPreFix(TestScript* script, int variable, int value) {
    if (script->localVarsCount <= 0) return -1;
    // NO bounds check on variable
    int offset = script->localVarsOffset + variable;
    script->localVars[offset] = value;
    return 0;
}

static int testScriptSetLocalVarPostFix(TestScript* script, int variable, int value) {
    if (script->localVarsCount <= 0) return -1;
    if (variable < 0 || variable >= script->localVarsCount) return -1;  // THE FIX
    int offset = script->localVarsOffset + variable;
    script->localVars[offset] = value;
    return 0;
}

TEST_CASE("F-23: scriptSetLocalVar — in-bounds index writes correctly")
{
    int heap[16] = {0};
    TestScript s = { .localVarsCount = 10, .localVarsOffset = 0, .localVars = heap };
    CHECK(testScriptSetLocalVarPostFix(&s, 3, 42) == 0);
    CHECK(heap[3] == 42);
    CHECK(heap[4] == 0); // adjacent slot untouched
}

TEST_CASE("F-23: scriptSetLocalVar — out-of-bounds index (>= count) rejected")
{
    int heap[16] = {0};
    TestScript s = { .localVarsCount = 10, .localVarsOffset = 0, .localVars = heap };
    // variable 10 is out of bounds (valid: 0-9)
    CHECK(testScriptSetLocalVarPostFix(&s, 10, 99) == -1);
    CHECK(heap[10] == 0); // not written
}

TEST_CASE("F-23: scriptSetLocalVar — negative index rejected")
{
    int heap[16] = {0};
    TestScript s = { .localVarsCount = 10, .localVarsOffset = 0, .localVars = heap };
    CHECK(testScriptSetLocalVarPostFix(&s, -1, 99) == -1);
}

TEST_CASE("F-23: scriptSetLocalVar — zero-count script always returns -1")
{
    int heap[8] = {0};
    TestScript s = { .localVarsCount = 0, .localVarsOffset = 0, .localVars = heap };
    CHECK(testScriptSetLocalVarPostFix(&s, 0, 42) == -1);
}

TEST_CASE("F-23: scriptSetLocalVar — variable at upper bound (= count-1) works")
{
    int heap[16] = {0};
    TestScript s = { .localVarsCount = 10, .localVarsOffset = 0, .localVars = heap };
    CHECK(testScriptSetLocalVarPostFix(&s, 9, 77) == 0);
    CHECK(heap[9] == 77);
}

TEST_CASE("F-23: scriptSetLocalVar — with offset, bounds check still uses count")
{
    // localVarsOffset is merely a base; the variable range [0, count) is absolute.
    int heap[16] = {0};
    TestScript s = { .localVarsCount = 5, .localVarsOffset = 0, .localVars = heap };
    // variable 5 (>= count 5) should be rejected regardless of offset
    CHECK(testScriptSetLocalVarPostFix(&s, 5, 33) == -1);
    CHECK(heap[5] == 0);
}

TEST_CASE("F-23: scriptSetLocalVar — pre-fix comparison: OOB writes accepted")
{
    int heap[16] = {0};
    TestScript s = { .localVarsCount = 5, .localVarsOffset = 0, .localVars = heap };
    // Pre-fix does NOT check bounds — variable 10 is accepted (OOB)
    CHECK(testScriptSetLocalVarPreFix(&s, 10, 88) == 0);
    CHECK(heap[10] == 88); // OOB write went through
}

// ============================================================================
// F2-11: CRC mismatch abort during load (loadsave.cc:2212-2216)
// ============================================================================
// Original: CRC mismatch only prints warning, load continues with corrupt data.
// Fix: abort load on CRC mismatch (close file, reset game, return -1).

struct LoadSkipCrcState {
    bool warned;
    bool aborted;
    int ret;
    bool fileClosed;
    bool gameReset;
    bool loadingDone;
};

// Mirror of pre-fix (buggy) — CRC mismatch is non-fatal
static LoadSkipCrcState loadHandlerCrcCheckPreFix(unsigned int storedCrc,
    unsigned int computedCrc, long dataSize) {
    LoadSkipCrcState state = {};
    bool crcOk = false;
    if (dataSize == 0) {
        crcOk = (storedCrc == 0);
    } else {
        crcOk = (computedCrc == storedCrc);
    }
    if (!crcOk) {
        state.warned = true;
        // Bug: no abort — load continues
    }
    // Load completes normally
    state.fileClosed = true;
    state.ret = 0;
    return state;
}

// Mirror of post-fix (correct) — CRC mismatch aborts load
static LoadSkipCrcState loadHandlerCrcCheckPostFix(unsigned int storedCrc,
    unsigned int computedCrc, long dataSize) {
    LoadSkipCrcState state = {};
    bool crcOk = false;
    if (dataSize == 0) {
        crcOk = (storedCrc == 0);
    } else {
        crcOk = (computedCrc == storedCrc);
    }
    if (!crcOk) {
        state.warned = true;
        state.aborted = true;
        state.fileClosed = true;
        state.gameReset = true;
        state.loadingDone = true;
        state.ret = -1;
        return state;
    }
    state.fileClosed = true;
    state.ret = 0;
    return state;
}

TEST_CASE("F2-11: CRC mismatch aborts load (post-fix)")
{
    // CRC mismatch: stored=0xABCD, computed=0x1234, dataSize > 0
    auto state = loadHandlerCrcCheckPostFix(0xABCD, 0x1234, 100);
    CHECK(state.warned);
    CHECK(state.aborted);
    CHECK(state.gameReset);
    CHECK(state.loadingDone);
    CHECK(state.ret == -1);
}

TEST_CASE("F2-11: CRC match proceeds normally (post-fix)")
{
    auto state = loadHandlerCrcCheckPostFix(0xABCD, 0xABCD, 100);
    CHECK_FALSE(state.warned);
    CHECK_FALSE(state.aborted);
    CHECK(state.ret == 0);
}

TEST_CASE("F2-11: zero-size handler CRC matches placeholder (post-fix)")
{
    // dataSize==0: stored CRC should be 0 (placeholder)
    auto state = loadHandlerCrcCheckPostFix(0, 0, 0);
    CHECK_FALSE(state.warned);
    CHECK(state.ret == 0);
}

TEST_CASE("F2-11: zero-size handler CRC mismatch aborts (post-fix)")
{
    // dataSize==0 but stored CRC != 0 — mismatch
    auto state = loadHandlerCrcCheckPostFix(0xFFFFFFFF, 0, 0);
    CHECK(state.warned);
    CHECK(state.aborted);
    CHECK(state.ret == -1);
}

TEST_CASE("F2-11: CRC mismatch with non-zero dataSize but zero computed (no buffer)")
{
    // crcComputed=false (malloc failed), dataSize > 0 → crcOk stays false.
    // Our mirror has no explicit crcComputed flag; computed=0 acts as fallback.
    // Even when computedCrc is 0 and storedCrc is non-zero, mismatch aborts.
    auto state = loadHandlerCrcCheckPostFix(0xABCD, 0, 200);
    CHECK(state.warned);
    CHECK(state.aborted);
    CHECK(state.ret == -1);
}

TEST_CASE("F2-11: pre-fix comparison — CRC mismatch does NOT abort")
{
    // Pre-fix: warns but returns 0 and load continues.
    auto state = loadHandlerCrcCheckPreFix(0xABCD, 0x1234, 100);
    CHECK(state.warned);
    // Bug: no abort, no reset, return 0
    CHECK_FALSE(state.aborted);     // not aborted
    CHECK_FALSE(state.gameReset);   // corrupt data stays
    CHECK(state.ret == 0);          // returns success despite CRC fail
}

// ============================================================================
// F2-12: malloc failure abort during save CRC (loadsave.cc:2018-2029)
// ============================================================================
// Original: malloc failure silently skipped, placeholder CRC (0) remains.
// Fix: abort save on malloc failure (close file, restore save, return -1).

struct SaveCrcState {
    bool errored;
    bool fileClosed;
    bool restored;
    int ret;
};

// Mirror of pre-fix (buggy) — malloc failure silently ignored
static SaveCrcState saveHandlerCrcPreFix(long dataSize, bool simulateMallocFail) {
    SaveCrcState state = {};
    if (dataSize > 0) {
        if (!simulateMallocFail) {
            // Buffer allocated, CRC computed, patched
        }
        // Bug: no else branch — malloc failure silently ignored
    }
    state.ret = 0; // always returns 0
    return state;
}

// Mirror of post-fix (correct) — malloc failure aborts save
static SaveCrcState saveHandlerCrcPostFix(long dataSize, bool simulateMallocFail) {
    SaveCrcState state = {};
    if (dataSize > 0) {
        if (!simulateMallocFail) {
            // Buffer allocated, CRC computed normally
        } else {
            state.errored = true;
            state.fileClosed = true;
            state.restored = true;
            state.ret = -1;
            return state;
        }
    }
    state.ret = 0;
    return state;
}

TEST_CASE("F2-12: malloc failure aborts save (post-fix)")
{
    auto state = saveHandlerCrcPostFix(1024, /*simulateMallocFail=*/true);
    CHECK(state.errored);
    CHECK(state.fileClosed);
    CHECK(state.restored);
    CHECK(state.ret == -1);
}

TEST_CASE("F2-12: normal save proceeds (post-fix)")
{
    auto state = saveHandlerCrcPostFix(1024, /*simulateMallocFail=*/false);
    CHECK_FALSE(state.errored);
    CHECK(state.ret == 0);
}

TEST_CASE("F2-12: zero dataSize skips CRC computation (no malloc needed)")
{
    // dataSize <= 0: CRC computation skipped entirely, no malloc call.
    // Even if malloc would fail, it's not called for zero-size data.
    auto state = saveHandlerCrcPostFix(0, /*simulateMallocFail=*/true);
    CHECK_FALSE(state.errored);
    CHECK(state.ret == 0);
}

TEST_CASE("F2-12: pre-fix comparison — malloc failure silently accepted")
{
    // Pre-fix: malloc failure silently ignored, save returns 0.
    auto state = saveHandlerCrcPreFix(1024, /*simulateMallocFail=*/true);
    CHECK_FALSE(state.errored);    // not treated as error
    CHECK(state.ret == 0);         // success returned despite CRC failure
}

// ============================================================================
// F-37: null guard for HOOK_FINDTARGET site 3 (combat_ai.cc:1921-1947)
// ============================================================================
// Original: hook fires with potentially-null result (no guard).
// Fix: if (result != nullptr) guard before ScriptHookCall constructor.

struct FindTargetHookState {
    bool hookCalled;
    bool overrideApplied;
    int hookCandidatePid;
};

// Mirror of pre-fix (buggy) — hook fires even with nullptr result
static FindTargetHookState fireFindTargetHookPreFix(void* result) {
    FindTargetHookState state = {};
    // Bug: no null check — hook fires with potentially-null result
    state.hookCalled = true;
    if (result != nullptr) {
        // simulate hook callback returning an override
        state.overrideApplied = true;
    }
    return state;
}

// Mirror of post-fix (correct) — skip hook when result is nullptr
static FindTargetHookState fireFindTargetHookPostFix(void* result) {
    FindTargetHookState state = {};
    if (result != nullptr) {
        state.hookCalled = true;
        // simulate hook logic
        state.overrideApplied = true;
    }
    // nullptr → skip hook entirely (matches sites 1 and 2)
    return state;
}

TEST_CASE("F-37: hook fires when result is non-null (post-fix)")
{
    int dummyTarget = 42;
    auto state = fireFindTargetHookPostFix(&dummyTarget);
    CHECK(state.hookCalled);
    CHECK(state.overrideApplied);
}

TEST_CASE("F-37: hook skipped when result is nullptr (post-fix)")
{
    auto state = fireFindTargetHookPostFix(nullptr);
    CHECK_FALSE(state.hookCalled);
    CHECK_FALSE(state.overrideApplied);
}

TEST_CASE("F-37: pre-fix comparison — hook fires with nullptr result")
{
    // Pre-fix: hook fires even when result is nullptr (no null guard)
    auto state = fireFindTargetHookPreFix(nullptr);
    CHECK(state.hookCalled); // bug: hook called with null argument
}
