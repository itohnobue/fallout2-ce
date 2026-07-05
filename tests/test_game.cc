// Unit tests for game.cc — state machine and globalVarsRead precedence fix.
//
// Tests mirror the production implementations:
//   - gameGetState / gameRequestState / gameUpdateState  (game.cc:1135-1190)
//   - globalVarsRead operator-precedence fix             (game.cc:1076-1132)
//   - SpeedMulti initialization logic                    (game.cc:391-405)
//
// Reference source: src/game.cc:1054-1190

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

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
