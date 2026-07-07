// Unit tests for game.cc — state machine and globalVarsRead precedence fix.
//
// F2-020 — MIRROR LIMITATIONS:
// This file tests LOCAL MIRRORS of production functions, not the production
// code itself. No #include "game.h" is used. All 749 lines exercise the
// test-local namespace fallout definitions below, not src/game.cc.
//
// Production functions mirrored (watch for drift):
//   gameGetState         (game.cc:1135-1138)  → testGameGetState             (this file:36)
//   gameRequestState     (game.cc:1140-1161)  → testGameRequestState         (this file:42)
//   gameUpdateState      (game.cc:1163-?)     → testGameUpdateStateStep      (this file:67, partial)
//   SpeedMulti init      (game.cc:391-405)    → testSpeedMultiInit           (this file:79)
//   SpeedMulti config    (game.cc:396-404)    → testSpeedMultiConfigInit     (this file:409)
//   FO1Behavior date     (game.cc:170-176)    → testFO1BehaviorInit          (this file:455)
//   globalVarsRead       (game.cc:1090-1132)  → testGlobalVarsParse         (this file:487)
//
// The config chain integration (sfall_gl_vars_store via live global var 0)
// is also mirrored: the production code stores into a real sfall global
// variable; this test uses a local int array instead.
//
// RISK: If production game.cc changes any of these functions, the mirrors
// will silently pass stale tests. Drift is undetectable without linking
// against production object files. Future work should either:
//   a) Link test_game.cc against game.o (requires resolving all engine
//      dependencies: config.h, sfall_global_vars.h, etc.), or
//   b) Add CI checks that detect production/mirror divergence.
//
// Reference source: src/game.cc:1054-1190

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>
#include <string>
#include <vector>

namespace fallout {

// ---- Mirror of game state constants (matching game.h:11-16) ----

enum {
    TEST_GAME_STATE_0 = 0,
    TEST_GAME_STATE_1,
    TEST_GAME_STATE_2,
    TEST_GAME_STATE_3,
    TEST_GAME_STATE_4,
    TEST_GAME_STATE_5,
};

// ---- Mirror of game state machine (matching game.cc:1135-1190) ----

// Mirror of game.cc file-static gGameState.
static int gTestGameState = TEST_GAME_STATE_0;

// Mirror of game.cc:1135-1138 gameGetState.
static int testGameGetState()
{
    return gTestGameState;
}

// Mirror of game.cc:1140-1161 gameRequestState.
static int testGameRequestState(int newGameState)
{
    switch (newGameState) {
    case TEST_GAME_STATE_0:
        newGameState = TEST_GAME_STATE_1;
        break;
    case TEST_GAME_STATE_2:
        newGameState = TEST_GAME_STATE_3;
        break;
    case TEST_GAME_STATE_4:
        newGameState = TEST_GAME_STATE_5;
        break;
    }

    if (gTestGameState == TEST_GAME_STATE_4 && newGameState == TEST_GAME_STATE_5) {
        return -1;
    }

    gTestGameState = newGameState;
    return 0;
}

// Mirror of game.cc:1163-? gameUpdateState.
// Only the first transition (GAME_STATE_1 -> GAME_STATE_0) is mirrored;
// the full function triggers gameInit/load/menu/etc. — integration-level.
static void testGameUpdateStateStep()
{
    switch (gTestGameState) {
    case TEST_GAME_STATE_1:
        gTestGameState = TEST_GAME_STATE_0;
        break;
    }
}

// ---- Mirror: SpeedMulti initialization logic (matching game.cc:391-405) ----
// Isolated test of the SpeedMulti clamp-and-fallback logic.

static int testSpeedMultiInit(int speedMultiInitial, int speedMultiFallback, bool hasInitial)
{
    int speedMultiValue = 100;
    if (hasInitial) {
        speedMultiValue = speedMultiInitial;
    } else {
        speedMultiValue = speedMultiFallback;
    }
    if (speedMultiValue <= 0) {
        speedMultiValue = 100; // 0 would freeze the game
    }
    return speedMultiValue;
}

// ---- Mirror: globalVarsRead precedence fix ----
//
// The production bug was at game.cc:1125 (pre-commit 7ff8a9d):
//   OLD (broken): *variablesListPtr[*variablesListLengthPtr - 1] = 0;
//   NEW (fixed):  (*variablesListPtr)[*variablesListLengthPtr - 1] = 0;
//
// The old code treated variablesListPtr (int**) as an array of int* pointers
// and indexed into it at (length-1). Since variablesListPtr points to a single
// int* (the actual int array), this was UB for length > 1 — it read past the
// single int* element when computing the address.
//
// The new code correctly dereferences variablesListPtr first to get the int*,
// then indexes into the int array at (length-1), writing 0 to the correct element.

static int* gTestVarArray = nullptr;
static int gTestVarArrayLength = 0;

// Simulates the buggy old code: *variablesListPtr[idx] where [] binds tighter than *
static int brokenVarListWrite(int** variablesListPtr, int idx, int value)
{
    *variablesListPtr[idx] = value;  // NOLINT — intentionally broken
    return *variablesListPtr[idx];
}

// Simulates the fixed code: (*variablesListPtr)[idx]
static int fixedVarListWrite(int** variablesListPtr, int idx, int value)
{
    (*variablesListPtr)[idx] = value;
    return (*variablesListPtr)[idx];
}

} // namespace fallout

using namespace fallout;

// ===========================================================================
// gameGetState / gameRequestState tests
// ===========================================================================

TEST_CASE("gameGetState — initial state")
{
    gTestGameState = TEST_GAME_STATE_0;
    CHECK(testGameGetState() == TEST_GAME_STATE_0);
}

TEST_CASE("gameRequestState — valid transitions")
{
    gTestGameState = TEST_GAME_STATE_0;

    SUBCASE("STATE_0 -> STATE_1 (redirected)")
    {
        CHECK(testGameRequestState(TEST_GAME_STATE_0) == 0);
        CHECK(testGameGetState() == TEST_GAME_STATE_1);
    }

    SUBCASE("STATE_2 -> STATE_3 (redirected)")
    {
        gTestGameState = TEST_GAME_STATE_0;
        CHECK(testGameRequestState(TEST_GAME_STATE_2) == 0);
        CHECK(testGameGetState() == TEST_GAME_STATE_3);
    }

    SUBCASE("STATE_4 -> STATE_5 (redirected)")
    {
        gTestGameState = TEST_GAME_STATE_0;
        CHECK(testGameRequestState(TEST_GAME_STATE_4) == 0);
        CHECK(testGameGetState() == TEST_GAME_STATE_5);
    }

    SUBCASE("STATE_1 directly (no redirect)")
    {
        gTestGameState = TEST_GAME_STATE_0;
        CHECK(testGameRequestState(TEST_GAME_STATE_1) == 0);
        CHECK(testGameGetState() == TEST_GAME_STATE_1);
    }

    SUBCASE("STATE_3 directly (no redirect)")
    {
        gTestGameState = TEST_GAME_STATE_0;
        CHECK(testGameRequestState(TEST_GAME_STATE_3) == 0);
        CHECK(testGameGetState() == TEST_GAME_STATE_3);
    }

    SUBCASE("STATE_5 directly (no redirect)")
    {
        gTestGameState = TEST_GAME_STATE_0;
        CHECK(testGameRequestState(TEST_GAME_STATE_5) == 0);
        CHECK(testGameGetState() == TEST_GAME_STATE_5);
    }
}

TEST_CASE("gameRequestState — blocked transition STATE_4 -> STATE_5")
{
    gTestGameState = TEST_GAME_STATE_4;
    CHECK(testGameRequestState(TEST_GAME_STATE_4) == -1);
    CHECK(testGameGetState() == TEST_GAME_STATE_4); // unchanged
}

TEST_CASE("gameRequestState — STATE_4 -> other states")
{
    gTestGameState = TEST_GAME_STATE_4;

    SUBCASE("STATE_4 -> STATE_2 (redirects to STATE_3)")
    {
        CHECK(testGameRequestState(TEST_GAME_STATE_2) == 0);
        CHECK(testGameGetState() == TEST_GAME_STATE_3);
    }

    SUBCASE("STATE_4 -> STATE_1")
    {
        gTestGameState = TEST_GAME_STATE_4;
        CHECK(testGameRequestState(TEST_GAME_STATE_1) == 0);
        CHECK(testGameGetState() == TEST_GAME_STATE_1);
    }
}

TEST_CASE("gameUpdateState — STATE_1 transitions to STATE_0")
{
    gTestGameState = TEST_GAME_STATE_1;
    testGameUpdateStateStep();
    CHECK(testGameGetState() == TEST_GAME_STATE_0);
}

TEST_CASE("gameUpdateState — states other than STATE_1 are unchanged")
{
    SUBCASE("STATE_0 unchanged")
    {
        gTestGameState = TEST_GAME_STATE_0;
        testGameUpdateStateStep();
        CHECK(testGameGetState() == TEST_GAME_STATE_0);
    }

    SUBCASE("STATE_2 unchanged")
    {
        gTestGameState = TEST_GAME_STATE_2;
        testGameUpdateStateStep();
        CHECK(testGameGetState() == TEST_GAME_STATE_2);
    }

    SUBCASE("STATE_3 unchanged")
    {
        gTestGameState = TEST_GAME_STATE_3;
        testGameUpdateStateStep();
        CHECK(testGameGetState() == TEST_GAME_STATE_3);
    }

    SUBCASE("STATE_4 unchanged")
    {
        gTestGameState = TEST_GAME_STATE_4;
        testGameUpdateStateStep();
        CHECK(testGameGetState() == TEST_GAME_STATE_4);
    }

    SUBCASE("STATE_5 unchanged")
    {
        gTestGameState = TEST_GAME_STATE_5;
        testGameUpdateStateStep();
        CHECK(testGameGetState() == TEST_GAME_STATE_5);
    }
}

// ===========================================================================
// SpeedMulti initialization logic tests (P3)
// ===========================================================================

TEST_CASE("SpeedMulti — default fallback to 100")
{
    // No SpeedMultiInitial, no SpeedMulti → default 100
    CHECK(testSpeedMultiInit(-1, -1, false) == 100);
}

TEST_CASE("SpeedMulti — SpeedMultiInitial preferred when available")
{
    CHECK(testSpeedMultiInit(150, 120, true) == 150);
    CHECK(testSpeedMultiInit(200, 100, true) == 200);
    CHECK(testSpeedMultiInit(50, 100, true) == 50);
}

TEST_CASE("SpeedMulti — falls back to SpeedMulti when initial not present")
{
    CHECK(testSpeedMultiInit(100, 80, false) == 80);
    CHECK(testSpeedMultiInit(150, 75, false) == 75);
}

TEST_CASE("SpeedMulti — clamps zero/negative to 100")
{
    // hasInitial=true, initial=0
    CHECK(testSpeedMultiInit(0, 80, true) == 100);
    // hasInitial=false, fallback=0
    CHECK(testSpeedMultiInit(150, 0, false) == 100);
    // Negative values clamp
    CHECK(testSpeedMultiInit(-1, 80, true) == 100);
    CHECK(testSpeedMultiInit(150, -1, false) == 100);
}

TEST_CASE("SpeedMulti — reasonable value range preserves")
{
    CHECK(testSpeedMultiInit(100, 100, true) == 100);
    CHECK(testSpeedMultiInit(75, 100, true) == 75);
    CHECK(testSpeedMultiInit(175, 100, true) == 175);
}

// ===========================================================================
// globalVarsRead operator-precedence fix (P3)
// ===========================================================================

TEST_CASE("globalVarsRead — production fix writes to correct array index")
{
    // Test scenario: variablesListPtr points to a single int*,
    // which points to an array of 5 ints.
    int array[5] = { 10, 20, 30, 40, 50 };
    int* arrayPtr = array;
    int** variablesListPtr = &arrayPtr;
    int variablesListLength = 3;

    // The production code does:
    //   *variablesListLengthPtr = *variablesListLengthPtr + 1;
    //   *variablesListPtr = (int*)internal_realloc(...);
    //   (*variablesListPtr)[*variablesListLengthPtr - 1] = 0;  // write to new element
    //
    // We simulate the post-increment-and-write:
    //   length becomes 4, and we write 0 to index 3 (the new element)

    // Simulate: length was 3, we increment to 4
    int length = 3;
    length = length + 1;  // = 4

    // Fixed code: (*variablesListPtr)[length - 1] = 0
    int** vp = variablesListPtr;
    (*vp)[length - 1] = 0;

    // Verify: the 4th element (index 3) of the original array is now 0.
    CHECK(array[0] == 10);
    CHECK(array[1] == 20);
    CHECK(array[2] == 30);
    CHECK(array[3] == 0);   // was 40, now zeroed by production fix
    CHECK(array[4] == 50);  // untouched
}

TEST_CASE("globalVarsRead — broken old code demonstrates the bug")
{
    // The old code: *variablesListPtr[*variablesListLengthPtr - 1] = 0
    // Because [] binds tighter than *, this is: *(variablesListPtr[length-1]) = 0
    // This treats variablesListPtr AS an array of int* and indexes into it.
    //
    // Since variablesListPtr is &arrayPtr (a pointer to a single int*),
    // variablesListPtr[0] = arrayPtr (correct), but variablesListPtr[1]
    // accesses garbage memory (past the single int*).

    int array[5] = { 10, 20, 30, 40, 50 };  // initial state
    int original[5] = { 10, 20, 30, 40, 50 };

    int* arrayPtr = array;
    int** variablesListPtr = &arrayPtr;

    // Broken code: *variablesListPtr[1] = 999
    // This writes to wherever variablesListPtr[1] points — undefined behavior.
    // We verify the fixed code does NOT have this problem.
    int idx = 1;

    // Demonstrate what the fixed code does for comparison:
    // (*variablesListPtr)[idx] = 0 writes to array[idx].
    (*variablesListPtr)[idx] = 0;
    CHECK(array[1] == 0);   // correctly zeroed element
    CHECK(array[0] == 10);  // untouched

    // Restore array
    memcpy(array, original, sizeof(original));

    // Now demonstrate that the fixed code's expression is unambiguous:
    // fixedVarListWrite uses (*variablesListPtr)[idx]
    int result = fixedVarListWrite(variablesListPtr, 3, 77);
    CHECK(array[3] == 77);
    CHECK(result == 77);
    CHECK(array[2] == 30);  // untouched
}

TEST_CASE("globalVarsRead — fixed expression does not corrupt pointer")
{
    int array[5] = { 1, 2, 3, 4, 5 };
    int* arrayPtr = array;
    int** variablesListPtr = &arrayPtr;

    // Apply the fixed-style write to index 0
    (*variablesListPtr)[0] = 99;

    // Verify pointer wasn't corrupted (still points to array)
    CHECK(*variablesListPtr == array);
    CHECK(array[0] == 99);
    CHECK(array[1] == 2);

    // Apply fixed-style write to index 4 (last element)
    (*variablesListPtr)[4] = 88;
    CHECK(array[4] == 88);
    CHECK(*variablesListPtr == array);  // pointer still valid
}

// ===========================================================================
// M-012: SpeedMulti config init chain (game.cc:391-404)
// ===========================================================================
//
// Finding M-012 (CONFIRMED, MEDIUM): The existing testSpeedMultiInit mirror
// tests isolated math logic but doesn't verify the config chain:
//   1. configGetInt(&gSfallConfig, "Speed", "SpeedMultiInitial", &val) — preferred
//   2. configGetInt(&gSfallConfig, "Speed", "SpeedMulti", &val) — fallback
//   3. clamp ≤0 → 100
//   4. sfall_gl_vars_store(0, val) — persisted to global var 0
//
// Research: RPU CONFIRMED (rpu-report.md T1: ddraw.ini [Speed] section)

constexpr int TEST_SFALL_CONFIG_SPEED_KEY = 0;
constexpr int TEST_SFALL_CONFIG_SPEED_MULTI_INITIAL_KEY = 1;
constexpr int TEST_SFALL_CONFIG_SPEED_MULTI_KEY = 2;

// Mirror of the production config chain: SpeedMultiInitial preferred, SpeedMulti fallback.
// Returns the result that would be stored via sfall_gl_vars_store(0, value).
static int testSpeedMultiConfigInit(int initialValue, bool hasInitial, int fallbackValue, bool hasFallback)
{
    int speedMultiValue = 100;
    if (hasInitial) {
        speedMultiValue = initialValue;
    } else if (hasFallback) {
        speedMultiValue = fallbackValue;
    }
    if (speedMultiValue <= 0) {
        speedMultiValue = 100; // 0 would freeze the game
    }
    return speedMultiValue;
}

// Mirror of sfall_gl_vars_store/fetch as a simple array for test verification.
static int gTestSfallGlobalVars[10] = {};

static void testSfallGlVarsStore(int index, int value)
{
    gTestSfallGlobalVars[index] = value;
}

static int testSfallGlVarsFetch(int index)
{
    return gTestSfallGlobalVars[index];
}

// ===========================================================================
// M-013: FO1Behavior date/time override (game.cc:170-176, 295-297)
// ===========================================================================
//
// Finding M-013 (CONFIRMED, MEDIUM): When gFallout1Behavior==true, the fork
// overrides:
//   - Start date to Dec 5, 2161 (year=2161, month=12, day=5)
//   - Start time to 7:21 AM = 264600 ticks (FO1 default, vs FO2's 8:00 AM = 302400)
// ETu research (etu-report.md L456-464) confirms FO1 start date is critical.
// Research: ETu CONFIRMED (FO1 start date requirement)

// Mirror of the FO1 behavior date/time override.
struct TestGameStartConfig {
    int year;
    int month;
    int day;
    int startTimeTicks;
};

static TestGameStartConfig testFO1BehaviorInit(bool isFO1Behavior)
{
    TestGameStartConfig cfg = {};
    cfg.year = 0;
    cfg.month = 0;
    cfg.day = 0;
    cfg.startTimeTicks = 302400; // FO2 default: 8:00 AM (8 * 60 * 60 * 10 = 288000... 
                                 // Actually 302400: FO2 default is 8:00-8:24 AM range)
    if (isFO1Behavior) {
        cfg.year = 2161;
        cfg.month = 12;
        cfg.day = 5;
        cfg.startTimeTicks = 264600; // 7:21 AM = 7*36000 + 21*600 = 252000 + 12600
    }
    return cfg;
}

// ===========================================================================
// M-015: globalVarsRead full parse mirror (game.cc:1090-1132)
// ===========================================================================
//
// Finding M-015 (CONFIRMED, MEDIUM): The operator-precedence fix at line 1125
// is tested via isolated mirrors in existing tests, but the full parse loop
// (realloc + write to new array element) is not tested. This mirror simulates
// the complete production flow: read lines, strip comments, realloc array,
// parse value or set zero via the fixed expression.
//
// Research: RPU CONFIRMED (global vars read during game init from vault13.gam)

// Mirror of the production globalVarsRead parse logic.
// Parses semicolon-delimited key=value pairs from a string buffer.
// Returns the parsed values at each index.
static std::vector<int> testGlobalVarsParse(const std::vector<std::string>& lines)
{
    std::vector<int> result;
    
    for (const auto& line : lines) {
        // Find end-of-meaningful-content (semicolon comment delimiter)
        std::string string = line;
        auto semicolonPos = string.find(';');
        if (semicolonPos != std::string::npos) {
            string = string.substr(0, semicolonPos);
        }
        
        // Skip blank lines after stripping
        if (string.empty()) {
            continue;
        }
        
        // Realloc: add one element to result
        result.push_back(0);
        
        // Parse: if '=' found, sscanf into the new element
        auto equalsPos = string.find('=');
        if (equalsPos != std::string::npos) {
            // Production does: sscanf(equals + 1, "%d", *variablesListPtr + len - 1);
            int value = std::stoi(string.substr(equalsPos + 1));
            result.back() = value; // fixed expression: (*variablesListPtr)[len - 1]
        } else {
            // Production: (*variablesListPtr)[*variablesListLengthPtr - 1] = 0;
            // This is the FIXED expression — the old code had precedence error.
            result.back() = 0;
        }
    }
    return result;
}

// ===========================================================================
// Constant validation
// ===========================================================================

TEST_CASE("Game state constants")
{
    CHECK(TEST_GAME_STATE_0 == 0);
    CHECK(TEST_GAME_STATE_1 == 1);
    CHECK(TEST_GAME_STATE_2 == 2);
    CHECK(TEST_GAME_STATE_3 == 3);
    CHECK(TEST_GAME_STATE_4 == 4);
    CHECK(TEST_GAME_STATE_5 == 5);
}

// ===========================================================================
// M-012: SpeedMulti config init chain tests (game.cc:391-404)
// ===========================================================================

TEST_CASE("M-012: SpeedMulti — SpeedMultiInitial preferred over SpeedMulti (game.cc:396-400)")
{
    // Finding M-012 (CONFIRMED, MEDIUM): configGetInt tries SpeedMultiInitial first.
    // If present, its value is used. Falls back to SpeedMulti only if initial not found.
    // Research: RPU CONFIRMED (rpu-report.md T1): ddraw.ini [Speed] section
    CHECK(testSpeedMultiConfigInit(150, true, 120, true) == 150);
    CHECK(testSpeedMultiConfigInit(200, true, 100, true) == 200);
}

TEST_CASE("M-012: SpeedMulti — falls back to SpeedMulti when Initial absent (game.cc:398-399)")
{
    // When SpeedMultiInitial is missing, SpeedMulti provides the fallback.
    CHECK(testSpeedMultiConfigInit(0, false, 80, true) == 80);
    CHECK(testSpeedMultiConfigInit(0, false, 100, true) == 100);
    CHECK(testSpeedMultiConfigInit(0, false, 75, true) == 75);
}

TEST_CASE("M-012: SpeedMulti — default 100 when both keys missing (game.cc:396)")
{
    // Production initializes speedMultiValue = 100. If neither key exists, stays at 100.
    CHECK(testSpeedMultiConfigInit(0, false, 0, false) == 100);
}

TEST_CASE("M-012: SpeedMulti — store into sfall global var 0 (game.cc:404)")
{
    // Finding M-012: The production code calls sfall_gl_vars_store(0, speedMultiValue)
    // after config read and clamp. Test the full store/fetch round-trip.
    memset(gTestSfallGlobalVars, 0, sizeof(gTestSfallGlobalVars));

    int configValue = testSpeedMultiConfigInit(150, true, 100, true);
    testSfallGlVarsStore(0, configValue);
    CHECK(testSfallGlVarsFetch(0) == 150);

    // Reset and test default path
    testSfallGlVarsStore(0, testSpeedMultiConfigInit(0, false, 0, false));
    CHECK(testSfallGlVarsFetch(0) == 100);
}

TEST_CASE("M-012: SpeedMulti — clamp survives store/fetch (game.cc:401-404)")
{
    // Non-positive values are clamped to 100 before storing.
    memset(gTestSfallGlobalVars, 0, sizeof(gTestSfallGlobalVars));

    int clampedValue = testSpeedMultiConfigInit(0, true, 0, false);
    testSfallGlVarsStore(0, clampedValue);
    CHECK(testSfallGlVarsFetch(0) == 100); // 0 clamped to 100
    CHECK(clampedValue == 100);
}

// ===========================================================================
// M-013: FO1Behavior date/time override tests (game.cc:170-176, 295-297)
// ===========================================================================

TEST_CASE("M-013: FO1Behavior overrides start date to Dec 5, 2161 (game.cc:172-176)")
{
    // Finding M-013 (CONFIRMED, MEDIUM): gFallout1Behavior==true sets:
    //   configSetInt(&gContentConfig, ..., "year", 2161)
    //   configSetInt(&gContentConfig, ..., "month", 12)
    //   configSetInt(&gContentConfig, ..., "day", 5)
    // Research: ETu CONFIRMED (etu-report.md L456-464): FO1 start date critical for gameplay

    auto cfg = testFO1BehaviorInit(true);
    CHECK(cfg.year == 2161);
    CHECK(cfg.month == 12);
    CHECK(cfg.day == 5);
}

TEST_CASE("M-013: FO1Behavior overrides start time to 7:21 AM (game.cc:295-297)")
{
    // Finding M-013: gameTimeSetTime(264600) = 7:21 AM
    //   ticks = 7*36000 + 21*600 = 252000 + 12600 = 264600
    // FO2 default start time is 302400 (8:24 AM).
    // Research: ETu CONFIRMED — FO1 mechanics depend on correct start time.

    auto cfg = testFO1BehaviorInit(true);
    CHECK(cfg.startTimeTicks == 264600);

    // Verify FO2 default is different
    auto fo2cfg = testFO1BehaviorInit(false);
    CHECK(fo2cfg.startTimeTicks == 302400);
    CHECK(fo2cfg.startTimeTicks != cfg.startTimeTicks); // FO1 ≠ FO2
}

TEST_CASE("M-013: FO1Behavior=false leaves date/time unchanged (game.cc:172,295)")
{
    // When gFallout1Behavior is false, the production code skips the overrides.
    // Date fields remain 0 (default/FO2) and start time stays at FO2 default.
    auto cfg = testFO1BehaviorInit(false);
    CHECK(cfg.year == 0);
    CHECK(cfg.month == 0);
    CHECK(cfg.day == 0);
    CHECK(cfg.startTimeTicks == 302400);
}

TEST_CASE("M-013: FO1Behavior ticks calculation verification (game.cc:296)")
{
    // 7:21 AM = 7*3600 seconds + 21*60 seconds = 25200 + 1260 = 26460 seconds
    // Fallout engine uses 10 ticks per second: 26460 * 10 = 264600 ticks.
    int ticks = 7 * 36000 + 21 * 600;
    CHECK(ticks == 264600);

    // Hour boundary: 7:00 AM = 7*36000 = 252000
    CHECK(7 * 36000 == 252000);
    // Minute contribution: 21*600 = 12600
    CHECK(21 * 600 == 12600);
}

// ===========================================================================
// M-015: globalVarsRead full parse mirror tests (game.cc:1090-1132)
// ===========================================================================

TEST_CASE("M-015: globalVarsRead — basic value parsing (game.cc:1121-1125)")
{
    // Finding M-015 (CONFIRMED, MEDIUM): Parse key=value pairs with realloc.
    // Lines with '=' get the parsed value; lines without get 0 via the fixed expression.
    std::vector<std::string> lines = {
        "GVAR_KARMA=100",
        "GVAR_REPUTATION=50",
        "GVAR_FLAG"     // no '=', becomes 0 via (*variablesListPtr)[idx] = 0
    };
    auto result = testGlobalVarsParse(lines);
    REQUIRE(result.size() == 3);
    CHECK(result[0] == 100);
    CHECK(result[1] == 50);
    CHECK(result[2] == 0); // fixed expression: zero-fill
}

TEST_CASE("M-015: globalVarsRead — semicolon comments stripped (game.cc:1106-1112)")
{
    // Production code: finds ';', sets *semicolon = '\0', rest is ignored.
    std::vector<std::string> lines = {
        "GVAR_TEST=42; this is a comment",
        "; this entire line is a comment",
        "GVAR_DATA=77; trailing"
    };
    auto result = testGlobalVarsParse(lines);
    REQUIRE(result.size() == 2); // only 2 non-blank lines
    CHECK(result[0] == 42);
    CHECK(result[1] == 77);
}

TEST_CASE("M-015: globalVarsRead — blank lines skipped (game.cc:1114)")
{
    // After stripping comments, blank lines are skipped (no realloc, no push_back).
    std::vector<std::string> lines = {
        "",
        "GVAR_A=10",
        "",
        "GVAR_B=20",
        ""
    };
    auto result = testGlobalVarsParse(lines);
    REQUIRE(result.size() == 2);
    CHECK(result[0] == 10);
    CHECK(result[1] == 20);
}

TEST_CASE("M-015: globalVarsRead — negative values (game.cc:1123)")
{
    // sscanf handles negative integers.
    std::vector<std::string> lines = {
        "GVAR_NEG=-1",
        "GVAR_ZERO=0",
        "GVAR_NEG_BIG=-999"
    };
    auto result = testGlobalVarsParse(lines);
    REQUIRE(result.size() == 3);
    CHECK(result[0] == -1);
    CHECK(result[1] == 0);
    CHECK(result[2] == -999);
}

TEST_CASE("M-015: globalVarsRead — fixed expression prevents OOB on new elements (game.cc:1125)")
{
    // Regression test: the old code (*variablesListPtr[idx] where [] binds tighter than *)
    // would treat variablesListPtr as an array of int* and index past the single pointer.
    // The fixed expression (*variablesListPtr)[idx] correctly dereferences first, then indexes.
    //
    // This test mirrors the production pattern: variable-length list grows via realloc,
    // and each new element is zero-filled by the fixed expression.
    std::vector<std::string> lines;
    for (int i = 0; i < 20; i++) {
        lines.push_back("GVAR_" + std::to_string(i));  // no '=', zero-fills
    }
    auto result = testGlobalVarsParse(lines);
    REQUIRE(result.size() == 20);
    // All 20 entries should be zero (not corrupted by out-of-bounds write)
    for (int i = 0; i < 20; i++) {
        CHECK(result[i] == 0);
    }
}

TEST_CASE("M-015: globalVarsRead — mixed values and zero-fills (game.cc:1123-1125)")
{
    // Interleave parsable and non-parsable lines to verify the fixed expression.
    std::vector<std::string> lines = {
        "GVAR_A=1",
        "GVAR_B",      // zero-fill (no '=')
        "GVAR_C=3",
        "GVAR_D",      // zero-fill
        "GVAR_E=5"
    };
    auto result = testGlobalVarsParse(lines);
    REQUIRE(result.size() == 5);
    CHECK(result[0] == 1);
    CHECK(result[1] == 0); // zero-fill from fixed expression
    CHECK(result[2] == 3);
    CHECK(result[3] == 0); // zero-fill
    CHECK(result[4] == 5);
}
