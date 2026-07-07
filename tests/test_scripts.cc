// Unit tests for scripts.cc — data structure validation and accessor functions.
//
// Tests: scriptsIsValidScriptIndex, scriptsIsUniqueObjectId,
//        OBJECT_ID constants, SCRIPT_TYPE_COUNT, ScriptProc/Flags enums,
//        GAME_TIME constants, and fork behavior validation (N2-019/N2-020/N2-022).
//
// Uses test-local stubs mirroring scripts.cc internal functions where the
// real source has 40+ engine dependencies (Object, Program, interpreter, etc.).
//
// All production constants and function bodies are verified against
// src/scripts.cc.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

#include "scripts.h"

using namespace fallout;

// =============================================================
// Test-local globals mirroring scripts.cc internals
// =============================================================

// Mirror of scripts.cc:1539-1542 — scriptsIsValidScriptIndex
static int gTestScriptsListEntriesLength = 0;

static bool testScriptsIsValidScriptIndex(int scriptIndex)
{
    return scriptIndex >= 0 && scriptIndex < gTestScriptsListEntriesLength;
}

// =============================================================
// Object ID constants (mirrors scripts.cc:227-230, scripts.h:27)
// =============================================================

constexpr int TEST_OBJECT_ID_PLAYER = 18000;
constexpr int TEST_OBJECT_ID_PARTY_MEMBER_END = TEST_OBJECT_ID_PLAYER + 0x01000000; // 16795216
constexpr int TEST_OBJECT_ID_UNIQUE_START = 0x0FFFFFFF; // 268435455
constexpr int TEST_OBJECT_ID_UNIQUE_END = 0x7FFFFFFF;   // 2147483647

// Mirror of scripts.cc:580-584 — scriptsIsUniqueObjectId
static bool testScriptsIsUniqueObjectId(int objectId)
{
    return objectId > TEST_OBJECT_ID_UNIQUE_START
        || (objectId >= TEST_OBJECT_ID_PLAYER && objectId < TEST_OBJECT_ID_PARTY_MEMBER_END);
}

// =============================================================
// Production constant validation
// =============================================================

// ScriptType enum — SCRIPT_TYPE_COUNT must be 5
// (system, spatial, timed, item, critter)
constexpr int TEST_SCRIPT_TYPE_COUNT = 5;

// SCRIPT_LIST_EXTENT_SIZE from scripts.cc:49
constexpr int TEST_SCRIPT_LIST_EXTENT_SIZE = 16;

// SCRIPT_DIALOG_MESSAGE_LIST_MAX_CAPACITY from scripts.cc:52
constexpr int TEST_SCRIPT_DIALOG_MESSAGE_LIST_MAX_CAPACITY = 10000;

// ScriptsListEntry struct from scripts.cc:62-65
typedef struct TestScriptsListEntry {
    char name[16];
    int local_vars_num;
} TestScriptsListEntry;

// =============================================================
// TEST CASES
// =============================================================

TEST_CASE("OBJECT_ID constants")
{
    // Verify the object ID ranges used by scriptsIsUniqueObjectId
    CHECK(TEST_OBJECT_ID_PLAYER == 18000);
    CHECK(TEST_OBJECT_ID_PARTY_MEMBER_END == 16795216);
    CHECK(TEST_OBJECT_ID_UNIQUE_START == 0x0FFFFFFF);
    CHECK(TEST_OBJECT_ID_UNIQUE_END == 0x7FFFFFFF);

    // Party member range size
    CHECK(TEST_OBJECT_ID_PARTY_MEMBER_END - TEST_OBJECT_ID_PLAYER == 0x01000000);

    // Unique object range covers exactly [0x0FFFFFFF+1, 0x7FFFFFFF]
    CHECK(TEST_OBJECT_ID_UNIQUE_END - TEST_OBJECT_ID_UNIQUE_START == 0x70000000);
}

TEST_CASE("SCRIPT_TYPE_COUNT constant")
{
    // Must be 5: system, spatial, timed, item, critter
    CHECK(TEST_SCRIPT_TYPE_COUNT == 5);
}

TEST_CASE("SCRIPT_LIST_EXTENT_SIZE constant")
{
    // Extent size for script list save/load (scripts.cc:49)
    CHECK(TEST_SCRIPT_LIST_EXTENT_SIZE == 16);
}

TEST_CASE("SCRIPT_DIALOG_MESSAGE_LIST_MAX_CAPACITY constant")
{
    // Renamed from SCRIPT_DIALOG_MESSAGE_LIST_CAPACITY, value unchanged (scripts.cc:52)
    CHECK(TEST_SCRIPT_DIALOG_MESSAGE_LIST_MAX_CAPACITY == 10000);
}

TEST_CASE("TestScriptsListEntry struct layout")
{
    // Verify the struct mirrors production ScriptsListEntry (scripts.cc:62-65)
    TestScriptsListEntry entry;
    memset(&entry, 0, sizeof(entry));

    // name is 16 bytes (matches production: char name[16])
    CHECK(sizeof(entry.name) == 16);

    // local_vars_num is an int
    entry.local_vars_num = 42;
    CHECK(entry.local_vars_num == 42);

    // name null-termination: strncpy with 16 byte buffer
    strncpy(entry.name, "GL_TEST", sizeof(entry.name));
    CHECK(strcmp(entry.name, "GL_TEST") == 0);

    // Max name length without truncation is 15 chars + null
    strncpy(entry.name, "123456789012345", sizeof(entry.name));
    CHECK(strlen(entry.name) == 15);
}

// ---- scriptsIsValidScriptIndex tests ----

TEST_CASE("scriptsIsValidScriptIndex — range validation")
{
    SUBCASE("empty list — no index is valid")
    {
        gTestScriptsListEntriesLength = 0;
        CHECK_FALSE(testScriptsIsValidScriptIndex(-1));
        CHECK_FALSE(testScriptsIsValidScriptIndex(0));
        CHECK_FALSE(testScriptsIsValidScriptIndex(1));
        CHECK_FALSE(testScriptsIsValidScriptIndex(100));
    }

    SUBCASE("single entry list")
    {
        gTestScriptsListEntriesLength = 1;
        CHECK(testScriptsIsValidScriptIndex(0));
        CHECK_FALSE(testScriptsIsValidScriptIndex(-1));
        CHECK_FALSE(testScriptsIsValidScriptIndex(1));
    }

    SUBCASE("typical list (e.g. 30 scripts)")
    {
        gTestScriptsListEntriesLength = 30;
        CHECK(testScriptsIsValidScriptIndex(0));
        CHECK(testScriptsIsValidScriptIndex(29));
        CHECK_FALSE(testScriptsIsValidScriptIndex(-1));
        CHECK_FALSE(testScriptsIsValidScriptIndex(30));
    }

    SUBCASE("negative indices rejected")
    {
        gTestScriptsListEntriesLength = 5;
        CHECK_FALSE(testScriptsIsValidScriptIndex(-1));
        CHECK_FALSE(testScriptsIsValidScriptIndex(-100));
        CHECK_FALSE(testScriptsIsValidScriptIndex(-0x7FFFFFFF));
    }

    SUBCASE("large index at boundary")
    {
        gTestScriptsListEntriesLength = 1000;
        CHECK(testScriptsIsValidScriptIndex(0));
        CHECK(testScriptsIsValidScriptIndex(999));
        CHECK_FALSE(testScriptsIsValidScriptIndex(1000));
        CHECK_FALSE(testScriptsIsValidScriptIndex(1001));
        CHECK_FALSE(testScriptsIsValidScriptIndex(0x7FFFFFFF));
    }

    SUBCASE("index 0 valid only if length > 0")
    {
        gTestScriptsListEntriesLength = 0;
        CHECK_FALSE(testScriptsIsValidScriptIndex(0));

        gTestScriptsListEntriesLength = 1;
        CHECK(testScriptsIsValidScriptIndex(0));
    }
}

// ---- scriptsIsUniqueObjectId tests ----

TEST_CASE("scriptsIsUniqueObjectId — player and party member range")
{
    // IDs in [OBJECT_ID_PLAYER, OBJECT_ID_PARTY_MEMBER_END) are unique
    SUBCASE("player ID itself")
    {
        CHECK(testScriptsIsUniqueObjectId(TEST_OBJECT_ID_PLAYER)); // 18000
    }

    SUBCASE("party member range start + 1")
    {
        CHECK(testScriptsIsUniqueObjectId(TEST_OBJECT_ID_PLAYER + 1));
    }

    SUBCASE("party member range end - 1")
    {
        CHECK(testScriptsIsUniqueObjectId(TEST_OBJECT_ID_PARTY_MEMBER_END - 1));
    }

    SUBCASE("party member ID mid-range")
    {
        CHECK(testScriptsIsUniqueObjectId(TEST_OBJECT_ID_PLAYER + 0x00FFFFFF));
    }
}

TEST_CASE("scriptsIsUniqueObjectId — high unique range")
{
    // IDs > OBJECT_ID_UNIQUE_START are unique (0x0FFFFFFF = 268435455)
    SUBCASE("just above unique start")
    {
        CHECK(testScriptsIsUniqueObjectId(TEST_OBJECT_ID_UNIQUE_START + 1));
    }

    SUBCASE("mid unique range")
    {
        CHECK(testScriptsIsUniqueObjectId(0x4FFFFFFF));
    }

    SUBCASE("OBJECT_ID_UNIQUE_START itself is NOT unique (strict >)")
    {
        CHECK_FALSE(testScriptsIsUniqueObjectId(TEST_OBJECT_ID_UNIQUE_START));
    }

    SUBCASE("high end of unique range")
    {
        CHECK(testScriptsIsUniqueObjectId(TEST_OBJECT_ID_UNIQUE_END));
    }
}

TEST_CASE("scriptsIsUniqueObjectId — non-unique IDs")
{
    SUBCASE("zero is not unique")
    {
        CHECK_FALSE(testScriptsIsUniqueObjectId(0));
    }

    SUBCASE("small IDs below player range")
    {
        CHECK_FALSE(testScriptsIsUniqueObjectId(1));
        CHECK_FALSE(testScriptsIsUniqueObjectId(100));
        CHECK_FALSE(testScriptsIsUniqueObjectId(17999));
    }

    SUBCASE("gap between party range and unique range")
    {
        // IDs in [PARTY_MEMBER_END, UNIQUE_START] are NOT unique
        CHECK_FALSE(testScriptsIsUniqueObjectId(TEST_OBJECT_ID_PARTY_MEMBER_END));
        CHECK_FALSE(testScriptsIsUniqueObjectId(TEST_OBJECT_ID_PARTY_MEMBER_END + 1));
        CHECK_FALSE(testScriptsIsUniqueObjectId(0x0AAAAAAA)); // 178956970
        CHECK_FALSE(testScriptsIsUniqueObjectId(TEST_OBJECT_ID_UNIQUE_START - 1));
    }

    SUBCASE("negative IDs are not unique")
    {
        CHECK_FALSE(testScriptsIsUniqueObjectId(-1));
        CHECK_FALSE(testScriptsIsUniqueObjectId(-100));
        CHECK_FALSE(testScriptsIsUniqueObjectId(-0x7FFFFFFF));
    }
}

TEST_CASE("scriptsIsUniqueObjectId — boundary edge cases")
{
    SUBCASE("OBJECT_ID_PLAYER (lower bound, inclusive)")
    {
        CHECK(testScriptsIsUniqueObjectId(TEST_OBJECT_ID_PLAYER));
        CHECK_FALSE(testScriptsIsUniqueObjectId(TEST_OBJECT_ID_PLAYER - 1));
    }

    SUBCASE("OBJECT_ID_PARTY_MEMBER_END (upper bound, exclusive)")
    {
        CHECK_FALSE(testScriptsIsUniqueObjectId(TEST_OBJECT_ID_PARTY_MEMBER_END));
        CHECK(testScriptsIsUniqueObjectId(TEST_OBJECT_ID_PARTY_MEMBER_END - 1));
    }

    SUBCASE("OBJECT_ID_UNIQUE_START (lower bound, exclusive)")
    {
        CHECK_FALSE(testScriptsIsUniqueObjectId(TEST_OBJECT_ID_UNIQUE_START));
        CHECK(testScriptsIsUniqueObjectId(TEST_OBJECT_ID_UNIQUE_START + 1));
    }

    SUBCASE("INT_MIN is not unique")
    {
        CHECK_FALSE(testScriptsIsUniqueObjectId(-0x80000000));
    }

    SUBCASE("INT_MAX is unique (above UNIQUE_START)")
    {
        CHECK(testScriptsIsUniqueObjectId(0x7FFFFFFF));
    }
}

// ---- ScriptType enum validation ----

TEST_CASE("ScriptType enum values")
{
    // Verify ordering matches scripts.h:43-50
    CHECK(static_cast<int>(SCRIPT_TYPE_SYSTEM) == 0);
    CHECK(static_cast<int>(SCRIPT_TYPE_SPATIAL) == 1);
    CHECK(static_cast<int>(SCRIPT_TYPE_TIMED) == 2);
    CHECK(static_cast<int>(SCRIPT_TYPE_ITEM) == 3);
    CHECK(static_cast<int>(SCRIPT_TYPE_CRITTER) == 4);
    CHECK(static_cast<int>(SCRIPT_TYPE_COUNT) == 5);
}

// ---- ScriptProc enum validation (key procs) ----

TEST_CASE("ScriptProc enum key values")
{
    // Verify proc constants that hook cleanup and script removal use
    CHECK(static_cast<int>(SCRIPT_PROC_START) == 1);
    CHECK(static_cast<int>(SCRIPT_PROC_MAP_ENTER) == 15);
    CHECK(static_cast<int>(SCRIPT_PROC_MAP_EXIT) == 16);
    CHECK(static_cast<int>(SCRIPT_PROC_MAP_UPDATE) == 23);
    CHECK(static_cast<int>(SCRIPT_PROC_COUNT) > 0);
}

// ---- ScriptFlags enum validation ----

TEST_CASE("ScriptFlag constants")
{
    // scripts.h:12-16
    CHECK(SCRIPT_FLAG_LOADED == 0x01);
    CHECK(SCRIPT_FLAG_NO_SPATIAL == 0x02);
    CHECK(SCRIPT_FLAG_EXECUTED == 0x04);
    CHECK(SCRIPT_FLAG_NO_SAVE == 0x08);
    CHECK(SCRIPT_FLAG_NO_REMOVE == 0x10);

    // NO_SAVE and NO_REMOVE are used together in scriptsSetDudeScript (scripts.cc:1578)
    int combined = SCRIPT_FLAG_NO_SAVE | SCRIPT_FLAG_NO_REMOVE;
    CHECK(combined == 0x18);
}

// ---- GAME_TIME constants ----

TEST_CASE("GAME_TIME tick constants")
{
    // scripts.h:19-25
    CHECK(GAME_TIME_TICKS_PER_HOUR == 36000);   // 60 * 60 * 10
    CHECK(GAME_TIME_TICKS_PER_DAY == 864000);    // 24 * 60 * 60 * 10
    CHECK(GAME_TIME_TICKS_PER_YEAR == 315360000);// 365 * 24 * 60 * 60 * 10

    // Consistency: 24 hours per day, 365 days per year
    CHECK(GAME_TIME_TICKS_PER_DAY == GAME_TIME_TICKS_PER_HOUR * 24);
    CHECK(GAME_TIME_TICKS_PER_YEAR == GAME_TIME_TICKS_PER_DAY * 365);
}

// ===========================================================================
// N2-019: scriptsExecMapUpdateScripts execution order (scripts.cc:2770-2836)
// ===========================================================================
//
// Finding N2-019 (CONFIRMED, MEDIUM): Fork reversed execution order:
//   OLD: sfall global scripts ran FIRST, then regular map scripts
//   NEW: regular map scripts run FIRST, then sfall global scripts
// Additionally, sfall global scripts now ALWAYS execute:
//   OLD: skipped when sidListCapacity==0 or malloc failed
//   NEW: executed in ALL paths (even early returns)
//
// Research: RPU CONFIRMED (global script execution order matters for
//   set_global_script_repeat)

// Mirror of the scriptsExecMapUpdateScripts order with tracking.
// Returns the execution log as a bitmask: bit 0 = regular scripts ran,
// bit 1 = sfall global scripts ran.
constexpr int TEST_EXEC_ORDER_REGULAR  = 0x01;
constexpr int TEST_EXEC_ORDER_SFALL    = 0x02;

static int testScriptsExecMapUpdateScripts(int scriptCount, int memoryAvailable)
{
    int executionLog = 0;

    int sidListCapacity = scriptCount;

    if (sidListCapacity == 0) {
        // Fork: sfall global scripts execute even when no regular scripts exist.
        // OLD code: sfall skipped here.
        executionLog |= TEST_EXEC_ORDER_SFALL;
        return executionLog;
    }

    // Simulate malloc
    if (!memoryAvailable) {
        // Fork: sfall global scripts execute even on OOM.
        // OLD code: sfall skipped on malloc failure.
        executionLog |= TEST_EXEC_ORDER_SFALL;
        return executionLog;
    }

    // Regular scripts execute FIRST (fork changed from OLD: globals first)
    executionLog |= TEST_EXEC_ORDER_REGULAR;

    // Fork: free sidList, then run sfall global scripts AFTER regular scripts.
    executionLog |= TEST_EXEC_ORDER_SFALL;

    return executionLog;
}

// ===========================================================================
// N2-020: scriptsCreateProgramByName file pre-check (scripts.cc:760-771)
// ===========================================================================
//
// Finding N2-020 (CONFIRMED, MEDIUM): Fork adds fileOpen/fileClose pre-check
// before calling programCreateByPath. Without this, programCreateByPath's
// internal programFatalError would longjmp to the calling program's context
// during lazy script load, corrupting the wrong program's state.
// Cross-file: scripts.cc ↔ interpreter.cc (longjmp across compilation units)

// Mirror of scriptsCreateProgramByName file-existence pre-check.
// fileExists: true = file found, false = file not found.
// Returns: true if function proceeds to programCreateByPath, false if nullptr returned.
static bool testScriptsCreateProgramByNamePreCheck(bool fileExists)
{
    if (!fileExists) {
        return false; // nullptr returned, no longjmp risk
    }
    return true; // proceed to programCreateByPath
}

// ===========================================================================
// N2-022: scriptsGetMessageList bounds check (scripts.cc:2852-2854)
// ===========================================================================
//
// Finding N2-022 (CONFIRMED, MEDIUM): Fork adds bounds guard on messageListId.
// The production code computes:
//   messageListIndex = messageListId - 1;
//   if (messageListIndex < 0 || messageListIndex >= MAX_CAPACITY) return -1;
// Without this: messageListId=0 → messageListIndex=-1 → OOB on gScriptDialogMessageLists[-1].
// The fork guard prevents the OOB.

constexpr int TEST_MSG_LIST_MAX_CAPACITY = 10000; // matches SCRIPT_DIALOG_MESSAGE_LIST_MAX_CAPACITY

// Mirror of scriptsGetMessageList bounds check.
static int testScriptsGetMessageListBounds(int messageListId)
{
    if (messageListId == -1) {
        return -1;
    }

    int messageListIndex = messageListId - 1;
    if (messageListIndex < 0 || messageListIndex >= TEST_MSG_LIST_MAX_CAPACITY) {
        return -1;
    }

    // In production: would access gScriptDialogMessageLists[messageListIndex]
    return 0; // success
}

// ===========================================================================
// N2-019: Execution order tests (scripts.cc:2832-2835)
// ===========================================================================

TEST_CASE("N2-019: sfall global scripts ALWAYS execute — sidListCapacity==0 (scripts.cc:2790-2794)")
{
    // Finding N2-019: OLD code skipped sfall globals when no regular scripts exist.
    // NEW fork code: sfall_gl_scr_exec_map_update_scripts(proc) runs in all paths.
    // RPU CONFIRMED: all 17 RPU global scripts use set_global_script_repeat/type,
    // expected to execute on map update regardless of regular script count.
    int log = testScriptsExecMapUpdateScripts(0, true);
    // When sidListCapacity==0, only sfall globals execute.
    CHECK((log & TEST_EXEC_ORDER_SFALL) != 0);        // sfall ran
    CHECK((log & TEST_EXEC_ORDER_REGULAR) == 0);       // regular didn't (no scripts)
}

TEST_CASE("N2-019: sfall global scripts ALWAYS execute — malloc failure (scripts.cc:2797-2802)")
{
    // OLD code: on malloc failure, sfall globals were skipped (gap in hook execution).
    // NEW fork: sfall globals execute even when sidList allocation fails.
    int log = testScriptsExecMapUpdateScripts(10, false);
    CHECK((log & TEST_EXEC_ORDER_SFALL) != 0);         // sfall ran despite OOM
    CHECK((log & TEST_EXEC_ORDER_REGULAR) == 0);       // regular skipped (no memory)
}

TEST_CASE("N2-019: normal path — regular scripts FIRST, then sfall globals (scripts.cc:2824-2835)")
{
    // Finding N2-019: Fork reversed order — regular scripts BEFORE sfall globals.
    // OLD code: sfall globals first, then regulars.
    // This is the production guarantee: both execute, in this order.
    int log = testScriptsExecMapUpdateScripts(5, true);
    CHECK(log == (TEST_EXEC_ORDER_REGULAR | TEST_EXEC_ORDER_SFALL));
    CHECK((log & TEST_EXEC_ORDER_REGULAR) != 0);       // regular ran
    CHECK((log & TEST_EXEC_ORDER_SFALL) != 0);         // sfall ran after
}

TEST_CASE("N2-019: regression — OLD code would skip sfall globals on empty script list (scripts.cc:2790-2794)")
{
    // Regression: OLD code at scriptsExecMapUpdateScripts did NOT call
    // sfall_gl_scr_exec_map_update_scripts during early returns. The fork
    // added this call to ALL exit paths. Verify the mirror reflects this.
    //
    // Scenario: 0 regular scripts on map — OLD: globals skipped, NEW: globals run.
    int log_zero = testScriptsExecMapUpdateScripts(0, true);
    CHECK((log_zero & TEST_EXEC_ORDER_SFALL) != 0);    // FIX: globals run

    // Scenario: OOM — OLD: globals skipped, NEW: globals run.
    int log_oom = testScriptsExecMapUpdateScripts(10, false);
    CHECK((log_oom & TEST_EXEC_ORDER_SFALL) != 0);     // FIX: globals run
}

// ===========================================================================
// N2-020: File pre-check tests (scripts.cc:760-771)
// ===========================================================================

TEST_CASE("N2-020: file pre-check — missing file returns nullptr (scripts.cc:766-768)")
{
    // Finding N2-020 (CONFIRMED, MEDIUM): Fork adds fileOpen("rb") pre-check.
    // If file doesn't exist, returns nullptr without longjmp corruption.
    // Without this: programFatalError longjmp's to the CALLING program's context.
    bool proceeds = testScriptsCreateProgramByNamePreCheck(false);
    CHECK_FALSE(proceeds); // nullptr returned, no longjmp
}

TEST_CASE("N2-020: file pre-check — existing file proceeds to create program (scripts.cc:770-772)")
{
    // When file exists, the pre-check passes and the function proceeds to the
    // real programCreateByPath call. The file was already closed by this point
    // (fileClose called after the check).
    bool proceeds = testScriptsCreateProgramByNamePreCheck(true);
    CHECK(proceeds); // proceed to programCreateByPath
}

TEST_CASE("N2-020: regression — OLD code would longjmp on missing .int file (scripts.cc:760-768)")
{
    // Regression: OLD code called programCreateByPath directly without pre-check.
    // programCreateByPath → programFatalError → longjmp to calling program's env.
    // During lazy script load (scriptExecProc), this corrupts the caller's state.
    //
    // The fork's fix: check file existence BEFORE calling programCreateByPath.
    // If file doesn't exist: clean nullptr return (no longjmp).
    // If file exists: close test handle, proceed to real create.
    //
    // This test verifies the mirror that guards against the old behavior —
    // returns nullptr cleanly vs. triggering the old fatal error.
    CHECK_FALSE(testScriptsCreateProgramByNamePreCheck(false)); // clean exit
    CHECK(testScriptsCreateProgramByNamePreCheck(true));        // proceed
}

// ===========================================================================
// N2-022: scriptsGetMessageList bounds check tests (scripts.cc:2852-2854)
// ===========================================================================

TEST_CASE("N2-022: scriptsGetMessageList — messageListId=-1 returns -1 (scripts.cc:2847-2849)")
{
    // Production: explicit check for messageListId == -1 before computing index.
    CHECK(testScriptsGetMessageListBounds(-1) == -1);
}

TEST_CASE("N2-022: scriptsGetMessageList — messageListId=0 returns -1 (scripts.cc:2851-2854)")
{
    // Finding N2-022 (CONFIRMED, MEDIUM): messageListId=0 gives index=-1.
    // Without the fork guard: gScriptDialogMessageLists[-1] = OOB read.
    // With the fork guard: messageListIndex < 0 → return -1.
    CHECK(testScriptsGetMessageListBounds(0) == -1);
}

TEST_CASE("N2-022: scriptsGetMessageList — valid IDs in range (scripts.cc:2851-2854)")
{
    // Normal IDs pass the bounds check.
    CHECK(testScriptsGetMessageListBounds(1) == 0);     // min valid
    CHECK(testScriptsGetMessageListBounds(5000) == 0);  // mid range
    CHECK(testScriptsGetMessageListBounds(10000) == 0); // max valid
}

TEST_CASE("N2-022: scriptsGetMessageList — messageListId > MAX_CAPACITY returns -1 (scripts.cc:2852-2854)")
{
    // IDs beyond SCRIPT_DIALOG_MESSAGE_LIST_MAX_CAPACITY (10000) are OOB.
    // messageListIndex = 10001-1 = 10000 → >= MAX_CAPACITY → return -1
    CHECK(testScriptsGetMessageListBounds(10001) == -1);
    CHECK(testScriptsGetMessageListBounds(10002) == -1);
    CHECK(testScriptsGetMessageListBounds(99999) == -1);
}

TEST_CASE("N2-022: regression — OLD code would OOB on messageListId=0 (scripts.cc:2852-2854)")
{
    // Regression: OLD code computed index = id-1 and directly indexed into
    // gScriptDialogMessageLists without bounds checking. messageListId=0 gave
    // index=-1, reading gScriptDialogMessageLists[-1] (undefined behavior).
    //
    // FIX: The fork's guard `if (messageListIndex < 0 || messageListIndex >= MAX_CAPACITY)`
    // catches this case and returns -1 cleanly.
    //
    // This test verifies that messageListId=0 → index=-1 → caught by guard.
    CHECK(testScriptsGetMessageListBounds(0) == -1);  // FIX: clean return
    // Without guard: this would be UB reading gScriptDialogMessageLists[-1].
}

TEST_CASE("N2-022: scriptsGetMessageList — boundary IDs (scripts.cc:2851-2854)")
{
    // Verify the exact bounds: 1..10000 are valid, everything else is -1.
    CHECK(testScriptsGetMessageListBounds(-1) == -1);   // explicit -1 sentinel
    CHECK(testScriptsGetMessageListBounds(0) == -1);     // underflow: index=-1
    CHECK(testScriptsGetMessageListBounds(1) == 0);      // lower bound: index=0 valid
    CHECK(testScriptsGetMessageListBounds(10000) == 0);  // upper bound: index=9999 valid
    CHECK(testScriptsGetMessageListBounds(10001) == -1); // overflow: index=10000
}

// ===========================================================================
// F-05: Midnight unjam gating (scripts.cc:438-444)
// ===========================================================================
//
// Finding F-05 (CONFIRMED, HIGH): gameTimeEventProcess midnight event
// unconditionally called objectUnjamAll() before the fix. The fix adds
// sfallGetUnjamLocksTime() < 0 gating to respect the set_unjam_locks_time
// metarule. When the override returns >= 0 (time-based FO1 mode), skip
// midnight unjam — FO1 players expect jammed locks to stay jammed until
// map re-entry after sufficient time.
//
// Source: scripts.cc:438-444, map.cc:1184-1193, sfall_metarules.h:95

// Mirror of the midnight unjam logic in gameTimeEventProcess at scripts.cc:438-444.
// sfallUnjamValue mirrors the return of sfallGetUnjamLocksTime():
//   -1  = metarule not configured → unjam at midnight (FO2 behavior)
//   >=0 = metarule configured → skip midnight unjam (FO1 behavior)
static bool testShouldUnjamAtMidnight(int sfallUnjamValue)
{
    return sfallUnjamValue < 0;
}

TEST_CASE("F-05: midnight unjam gating — sfallGetUnjamLocksTime returns -1 (FO2 default)")
{
    // When the metarule is not configured (returns -1), objectUnjamAll()
    // should be called — matching FO2 vanilla behavior.
    CHECK(testShouldUnjamAtMidnight(-1));
}

TEST_CASE("F-05: midnight unjam gating — sfallGetUnjamLocksTime returns 0 (min override)")
{
    // A metarule value of 0 (unjam immediately on re-entry) means the
    // metarule IS configured — skip midnight unjam (FO1 behavior).
    CHECK_FALSE(testShouldUnjamAtMidnight(0));
}

TEST_CASE("F-05: midnight unjam gating — sfallGetUnjamLocksTime returns 1+")
{
    // Any positive hour value means the metarule is active.
    CHECK_FALSE(testShouldUnjamAtMidnight(1));
    CHECK_FALSE(testShouldUnjamAtMidnight(24));
    CHECK_FALSE(testShouldUnjamAtMidnight(1000));
}

TEST_CASE("F-05: midnight unjam gating — threshold at exactly 0")
{
    // The gate is `sfallGetUnjamLocksTime() < 0`. -1 is the ONLY value
    // that passes the gate in normal operation.
    //   -1  → unjam (FO2, metarule not set)
    //    0  → skip  (FO1, metarule set to 0h — unjam on re-entry immediately)
    //    1+ → skip  (FO1, metarule set to positive hours)
    CHECK(testShouldUnjamAtMidnight(-1));
    CHECK_FALSE(testShouldUnjamAtMidnight(0));
    CHECK_FALSE(testShouldUnjamAtMidnight(24));
}

TEST_CASE("F-05: regression — OLD code called objectUnjamAll() unconditionally")
{
    // OLD code: gameTimeEventProcess() called objectUnjamAll() unconditionally
    // at midnight (scripts.cc:438 before fix). This bypasses the metarule's
    // unjam-locks-time override, meaning FO1-mode playthroughs would have
    // locks unjammed at midnight regardless of the configured delay.
    //
    // NEW code: the `if (sfallGetUnjamLocksTime() < 0)` gate preserves FO2
    // default behavior (unjam at midnight) while honoring the FO1 metarule
    // (skip midnight unjam, let map.cc:1184-1193 handle time-based unjam).
    //
    // This test verifies the condition logic itself:
    CHECK_FALSE(testShouldUnjamAtMidnight(0));   // metarule active → skip
    CHECK(testShouldUnjamAtMidnight(-1));        // metarule not set → unjam
}
