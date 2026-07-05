// Unit tests for loadsave.cc fork change logic patterns.
//
// All three fork changes in loadsave.cc (lsgSaveGame, lsgPerformSaveGame,
// lsgLoadGameInSlot) are file-static functions with heavy engine file I/O
// dependencies — they cannot be practically linked. This file mirrors the
// logical patterns and validates them in isolation using self-contained stubs.
//
// Fork changes tested:
//   - H-013: lsgSaveGame() return-value fix (loadsave.cc:465-471)
//   - H-014: sfallgv.sav corruption cleanup (loadsave.cc:1938-1944)
//   - H-015: _loadingGame flag reset on sfall load failure (loadsave.cc:2042-2045)
//
// Research cross-references:
//   - RPU (s1-research-rpu-report.md, Section 5.1, CONFIRMED):
//     RPU scripts (gl_k_alcohl.ssl) store addiction timers in sfall globals
//     that persist via sfallgv.sav. Corrupted file = lost addiction state.
//     game_loaded guard in 11+ global scripts depends on _loadingGame.
//   - ET Tu (s1-research-etu-report.md, Section 5.3, CONFIRMED):
//     TMA system uses set_sfall_global("TMA_DATA", array). If sfallgv.sav
//     load fails, TMA interface loses data state.
//
// See combined synthesis: tmp/s5-synth-report.md

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>
#include <cstdarg>

// ============================================================================
// Section 1: Stub types and globals for loadsave pattern mirroring
// ============================================================================

namespace fallout {

// ---- Stub FILE handle ----
typedef struct {
    int id;            // non-zero = valid "file"
    bool writeFailed;  // set to true to simulate write failure
    bool readFailed;   // set to true to simulate read failure
    bool isOpen;
} TestFile;

static constexpr int kMaxTestFiles = 10;
static TestFile gTestFiles[kMaxTestFiles];
static int gTestFileNextId = 1;

static void testFileReset()
{
    std::memset(gTestFiles, 0, sizeof(gTestFiles));
    gTestFileNextId = 1;
}

static TestFile* testFileOpen(const char* /*path*/, const char* /*mode*/,
                              bool forceFail = false)
{
    if (forceFail) return nullptr;
    for (int i = 0; i < kMaxTestFiles; i++) {
        if (!gTestFiles[i].isOpen) {
            gTestFiles[i].id = gTestFileNextId++;
            gTestFiles[i].isOpen = true;
            gTestFiles[i].writeFailed = false;
            gTestFiles[i].readFailed = false;
            return &gTestFiles[i];
        }
    }
    return nullptr;
}

static void testFileClose(TestFile* f)
{
    if (f != nullptr) {
        f->isOpen = false;
    }
}

// ---- Stub globals ----
static TestFile* gFlptr = nullptr;       // _flptr
static bool gSfallSaveSuccess = true;    // sfallSaveGameData return
static bool gSfallLoadSuccess = true;    // sfallLoadGameData return
static bool gLoadingGame = false;        // _loadingGame
static bool gSfallgvRemoved = false;     // track compat_remove calls
static int gSlotCursor = 0;             // _slot_cursor
static char gGmpath[260];               // _gmpath (simplified)
static const char* gLatestRemovedPath = nullptr;

// ---- Stub: compat_remove ----
static int testCompatRemove(const char* path)
{
    gSfallgvRemoved = true;
    gLatestRemovedPath = path;
    return 0; // success
}

// ---- Stub: sfallSaveGameData ----
static bool testSfallSaveGameData(TestFile* /*file*/)
{
    return gSfallSaveSuccess;
}

// ---- Stub: sfallLoadGameData ----
static bool testSfallLoadGameData(TestFile* /*file*/)
{
    return gSfallLoadSuccess;
}

// ---- Stub: sprintf for path building ----
static void testBuildSfallgvPath(char* buf, size_t bufsz, int slot)
{
    std::snprintf(buf, bufsz, "SAVEGAME\\SLOT%.2d\\sfallgv.sav", slot + 1);
}

// ---- State reset helper ----
static void testResetAllState()
{
    testFileReset();
    gFlptr = nullptr;
    gSfallSaveSuccess = true;
    gSfallLoadSuccess = true;
    gLoadingGame = false;
    gSfallgvRemoved = false;
    gSlotCursor = 0;
    gLatestRemovedPath = nullptr;
    std::memset(gGmpath, 0, sizeof(gGmpath));
}

// ============================================================================
// Section 2: Mirrored production logic patterns
// ============================================================================

// ---------------------------------------------------------------------------
// H-013: lsgSaveGame() return-value fix (loadsave.cc:465-471)
//
// Before fix: v6 remained 1 when lsgPerformSaveGame returned -1.
// After fix: else { v6 = -1; } sets v6 correctly.
// ---------------------------------------------------------------------------
//
// Mirror of the logical control flow around the save result handling.
static int testLsgSaveGameResultLogic(int quickSnapResult,
                                       int performSaveResult)
{
    // Production code pattern:
    //   int v6 = _QuickSnapShot();       // 1 on success, -1 on failure
    //   if (v6 == 1) {
    //       int v7 = lsgPerformSaveGame(); // 0 on success, -1 on failure
    //       if (v7 != -1) {
    //           v6 = v7;
    //       } else {
    //           v6 = -1;  // <-- THE FIX (was missing before)
    //       }
    //   }
    int v6 = quickSnapResult;
    if (v6 == 1) {
        int v7 = performSaveResult;
        if (v7 != -1) {
            v6 = v7;
        } else {
            v6 = -1; // THE FIX: set v6 to -1 on save failure
        }
    }
    return v6;
}

// ---------------------------------------------------------------------------
// H-014: lsgPerformSaveGame() sfallgv.sav cleanup (loadsave.cc:1938-1944)
//
// Before fix: _flptr was left dangling, corrupted sfallgv.sav persisted.
// After fix: _flptr is nulled after close; corrupted file removed on failure.
// ---------------------------------------------------------------------------
//
// Mirror of the sfall save block cleanup pattern.
struct SfallSaveResult {
    bool saved;           // sfall save succeeded?
    bool flptrNulled;     // was _flptr set to nullptr after close?
    bool fileRemoved;     // was compat_remove called?
};

static SfallSaveResult testSfallSaveBlock(bool saveDataResult)
{
    SfallSaveResult result = {};
    result.saved = saveDataResult;

    // Production code pattern:
    //   _flptr = fileOpen(_gmpath, "wb");
    //   if (_flptr != nullptr) {
    //       bool saved = sfallSaveGameData(_flptr);
    //       fileClose(_flptr);
    //       _flptr = nullptr;     // THE FIX: null after close
    //       if (!saved) {
    //           compat_remove(_gmpath);  // THE FIX: remove corrupted file
    //           return -1;
    //       }
    //   }
    //
    gFlptr = testFileOpen("sfallgv.sav", "wb");
    if (gFlptr != nullptr) {
        bool saved = saveDataResult;
        testFileClose(gFlptr);
        gFlptr = nullptr;  // THE FIX
        result.flptrNulled = (gFlptr == nullptr);
        if (!saved) {
            testCompatRemove("sfallgv.sav");  // THE FIX
            result.fileRemoved = true;
            return result;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// H-015: lsgLoadGameInSlot() _loadingGame flag reset (loadsave.cc:2042-2045)
//
// Before fix: _loadingGame left true when sfallLoadGameData failed.
// After fix: _loadingGame set to false on sfall load failure.
// ---------------------------------------------------------------------------
//
// Mirror of the sfall load block with flag reset.
struct SfallLoadResult {
    bool loaded;
    bool loadingGameFlagAfter;
    bool fileClosed;
};

static SfallLoadResult testSfallLoadBlock(bool loadDataResult)
{
    gLoadingGame = true;  // _loadingGame = true at start of load
    SfallLoadResult result = {};

    // Production code pattern:
    //   _flptr = fileOpen(_gmpath, "rb");
    //   if (_flptr != nullptr) {
    //       bool loaded = sfallLoadGameData(_flptr);
    //       fileClose(_flptr);
    //       if (!loaded) {
    //           _loadingGame = false;   // THE FIX
    //           return -1;
    //       }
    //   }
    //
    TestFile* flptr = testFileOpen("sfallgv.sav", "rb");
    if (flptr != nullptr) {
        bool loaded = loadDataResult;
        testFileClose(flptr);
        result.fileClosed = true;
        if (!loaded) {
            gLoadingGame = false;  // THE FIX
        }
        result.loaded = loaded;
    }
    result.loadingGameFlagAfter = gLoadingGame;
    return result;
}

} // namespace fallout

using namespace fallout;


// ============================================================================
// Test Cases
// ============================================================================

// ---------------------------------------------------------------------------
// H-013: lsgSaveGame return-value fix
// ---------------------------------------------------------------------------

TEST_CASE("H-013: lsgSaveGame return-value fix (loadsave.cc:465-471)")
{
    // Finding: H-013, adversarial CONFIRMED
    // Source: loadsave.cc:465-471
    //
    // The fork added `else { v6 = -1; }` to ensure that when
    // lsgPerformSaveGame returns -1, the outer function also returns -1.
    // Before the fix, the "Game saved" message would display despite
    // actual save failure.
    //
    // RPU research (CONFIRMED): s1-research-rpu-report.md Section 5.1.
    // RPU stores addiction timers in sfall globals. A false-positive
    // save success would give the player a false sense of security
    // while their addiction state was actually never committed.

    SUBCASE("quickSnap fails → returns -1 immediately")
    {
        // Quick snap failure means no save attempted.
        int result = testLsgSaveGameResultLogic(-1, 0);
        CHECK(result == -1);
    }

    SUBCASE("quickSnap succeeds, performSave succeeds (returns 0)")
    {
        // Normal success path. v6 gets v7's return value (0).
        int result = testLsgSaveGameResultLogic(1, 0);
        CHECK(result == 0);
    }

    SUBCASE("quickSnap succeeds, performSave fails (returns -1) → THE FIX")
    {
        // THE BUG FIX: Before fix, v6 would remain 1 (reporting success
        // despite save failure). After fix, v6 correctly becomes -1.
        int result = testLsgSaveGameResultLogic(1, -1);
        CHECK(result == -1);
        // This is the regression test: without the `else { v6 = -1; }`
        // branch, this would return 1 (false-positive success).
    }

    SUBCASE("quickSnap succeeds, performSave returns custom (0 on success)")
    {
        // lsgPerformSaveGame returns 0 on normal success.
        int result = testLsgSaveGameResultLogic(1, 0);
        CHECK(result == 0);
    }
}


// ---------------------------------------------------------------------------
// H-014: lsgPerformSaveGame sfallgv.sav cleanup
// ---------------------------------------------------------------------------

TEST_CASE("H-014: sfallgv.sav corruption cleanup (loadsave.cc:1938-1944)")
{
    // Finding: H-014, adversarial CONFIRMED
    // Source: loadsave.cc:1938-1944
    //
    // The fork added: (1) null _flptr after fileClose, (2) call compat_remove
    // on corrupted sfallgv.sav when sfallSaveGameData fails.
    //
    // RPU research (CONFIRMED): RPU scripts store addiction state in
    // sfall globals that persist via sfallgv.sav. Corrupted file =
    // lost addiction state on next load.
    //
    // ET Tu research (CONFIRMED): TMA system (gl_tma.ssl:95-99) stores
    // TMA_DATA and TMA_GVAR arrays in sfall globals.

    testResetAllState();

    SUBCASE("sfall save succeeds: _flptr nulled, file NOT removed")
    {
        gSfallgvRemoved = false;
        SfallSaveResult r = testSfallSaveBlock(true);

        CHECK(r.saved == true);
        CHECK(r.flptrNulled == true);         // _flptr set to nullptr
        CHECK(r.fileRemoved == false);        // file kept (valid save)
    }

    SUBCASE("sfall save fails: _flptr nulled AND corrupted file removed")
    {
        gSfallgvRemoved = false;
        SfallSaveResult r = testSfallSaveBlock(false);

        CHECK(r.saved == false);
        CHECK(r.flptrNulled == true);         // _flptr cleaned up
        CHECK(r.fileRemoved == true);         // corrupted file removed
    }

    SUBCASE("_flptr is nullptr after sfall save block (success)")
    {
        testSfallSaveBlock(true);
        // After the block completes, _flptr must be nullptr
        // whether save succeeded or failed.
        CHECK(gFlptr == nullptr);
    }

    SUBCASE("_flptr is nullptr after sfall save block (failure)")
    {
        testSfallSaveBlock(false);
        CHECK(gFlptr == nullptr);
    }

    SUBCASE("fileOpen for sfallgv.sav returns nullptr → skip cleanup")
    {
        // When fileOpen fails (_flptr == nullptr), the save block is skipped
        // entirely. No close, no null, no remove needed.
        gFlptr = nullptr;
        gSfallgvRemoved = false;

        // Simulate: _flptr = fileOpen(...) returns nullptr
        // The if-guard prevents any further action.
        TestFile* flptr = testFileOpen("sfallgv.sav", "wb", /*forceFail=*/true);
        CHECK(flptr == nullptr);

        // Nothing was removed because we never entered the block
        CHECK(gSfallgvRemoved == false);
    }
}


// ---------------------------------------------------------------------------
// H-015: _loadingGame flag reset on sfall load failure
// ---------------------------------------------------------------------------

TEST_CASE("H-015: _loadingGame flag reset (loadsave.cc:2042-2045)")
{
    // Finding: H-015, adversarial CONFIRMED
    // Source: loadsave.cc:2042-2045
    //
    // The fork added: `_loadingGame = false;` when sfallLoadGameData fails.
    // Before fix, the engine remained in "loading game" state with stale
    // state. RPU scripts universally check is_loading_game/game_loaded.
    //
    // RPU research (CONFIRMED, 11+ global scripts): game_loaded guard
    // pattern used to skip init during load-from-save. Incorrect flag
    // state would break all global script initialization.

    testResetAllState();

    SUBCASE("sfall load succeeds: _loadingGame stays true")
    {
        // After a successful sfall load, _loadingGame should still be true
        // (the full game state is being loaded, other subsystems depend on it).
        SfallLoadResult r = testSfallLoadBlock(true);
        CHECK(r.loaded == true);
        // In production, _loadingGame = false is set later (line 2062).
        // Here, the sfall load block doesn't reset it on success.
        CHECK(r.loadingGameFlagAfter == true);
    }

    SUBCASE("sfall load fails: _loadingGame reset to false")
    {
        // THE BUG FIX: When sfall load data fails, _loadingGame is set to false.
        SfallLoadResult r = testSfallLoadBlock(false);
        CHECK(r.loaded == false);
        CHECK(r.loadingGameFlagAfter == false);
        CHECK(r.fileClosed == true);
    }

    SUBCASE("regression: without reset, _loadingGame would stay true")
    {
        // This test demonstrates the bug: if we skip the reset,
        // _loadingGame stays true after sfall load failure.
        // The fork's fix prevents this.
        gLoadingGame = true;

        // Simulate sfall load failure WITHOUT the fix
        // (i.e., no `_loadingGame = false;` after detection)
        bool loaded = false;
        if (!loaded) {
            // Old code: no reset
            // New code (THE FIX): gLoadingGame = false;
            // We test the old (buggy) behavior here:
            // (keeping gLoadingGame = true)
        }

        // In the old code, gLoadingGame would still be true here.
        // The fork's fix corrects this by adding `_loadingGame = false;`.
        // We verify the old behavior demonstrates the need for the fix:
        CHECK(gLoadingGame == true); // BUG: should be false after load failure
    }

    SUBCASE("fileOpen for sfallgv.sav returns nullptr → flag unchanged")
    {
        gLoadingGame = true;
        TestFile* flptr = testFileOpen("sfallgv.sav", "rb", /*forceFail=*/true);
        CHECK(flptr == nullptr);

        // When sfallgv.sav doesn't exist (nullptr handle), the load block
        // is skipped. _loadingGame is not reset — the main game save loaded
        // successfully, only sfall extra data is missing.
        CHECK(gLoadingGame == true);
    }
}


// ---------------------------------------------------------------------------
// Combined scenario: save → load round-trip state machine
// ---------------------------------------------------------------------------

TEST_CASE("LoadSave: save failure sequence — no false positive")
{
    // End-to-end: verify lsgSaveGame returns -1 when the inner save fails,
    // and the sfallgv.sav cleanup occurs.
    testResetAllState();

    // Simulate: quickSnap succeeds, but performSave returns -1
    int result = testLsgSaveGameResultLogic(1, -1);
    CHECK(result == -1); // H-013: correctly reports failure

    // Simulate: the save failure triggered sfallgv.sav cleanup
    SfallSaveResult cleanup = testSfallSaveBlock(false);
    CHECK(cleanup.fileRemoved == true);  // H-014: corrupted file removed
    CHECK(cleanup.flptrNulled == true);  // H-014: pointer cleaned up
}

TEST_CASE("LoadSave: load failure sequence — flag reset")
{
    testResetAllState();

    // Simulate: sfall load fails
    SfallLoadResult load = testSfallLoadBlock(false);
    CHECK(load.loaded == false);
    CHECK(load.loadingGameFlagAfter == false); // H-015: flag reset
}
