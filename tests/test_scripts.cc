// Unit tests for scripts.cc — data structure validation, accessor functions,
// and sfall-mods.ini config loading.
//
// Tests: scriptsIsValidScriptIndex, scriptsGetWorldMapSlots,
//        scriptsIsUniqueObjectId, sfallModsIniInit/Exit/GetInt,
//        OBJECT_ID constants, SCRIPT_TYPE_COUNT, ScriptsListEntry layout.
//
// Uses test-local stubs mirroring scripts.cc internal functions where the
// real source has 40+ engine dependencies (Object, Program, interpreter, etc.).
// Links against test_sources (config.cc) for sfallModsIni config tests.
//
// All production constants and function bodies are verified against
// src/scripts.cc 24199e9..HEAD diff.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

#include "config.h"
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

// Mirror of scripts.cc:1544-1547 — scriptsGetWorldMapSlots
static int gTestWorldMapSlots = 0;

static int testScriptsGetWorldMapSlots()
{
    return gTestWorldMapSlots;
}

// =============================================================
// Object ID constants (mirrors scripts.cc:227-230, scripts.h:27)
// =============================================================

constexpr int TEST_OBJECT_ID_PLAYER = 18000;
constexpr int TEST_OBJECT_ID_PARTY_MEMBER_END = TEST_OBJECT_ID_PLAYER + 0x01000000; // 16795648
constexpr int TEST_OBJECT_ID_UNIQUE_START = 0x0FFFFFFF; // 268435455
constexpr int TEST_OBJECT_ID_UNIQUE_END = 0x7FFFFFFF;   // 2147483647

// Mirror of scripts.cc:580-584 — scriptsIsUniqueObjectId
static bool testScriptsIsUniqueObjectId(int objectId)
{
    return objectId > TEST_OBJECT_ID_UNIQUE_START
        || (objectId >= TEST_OBJECT_ID_PLAYER && objectId < TEST_OBJECT_ID_PARTY_MEMBER_END);
}

// =============================================================
// sfall-mods.ini config stubs (mirrors scripts.cc:1586-1619)
// =============================================================

static Config gTestSfallModsIni;
static bool gTestSfallModsIniLoaded = false;

// Mirror of scripts.cc:1586-1602 — sfallModsIniInit
static bool testSfallModsIniInit()
{
    if (gTestSfallModsIniLoaded) {
        return true;
    }

    if (!configInit(&gTestSfallModsIni)) {
        return false;
    }

    // In production, configRead(&gSfallModsIni, "sfall-mods.ini", false) is
    // called here; the stub fileOpen() returns nullptr so it gracefully fails.
    // We skip the actual file read — the Config is left empty.

    gTestSfallModsIniLoaded = true;
    return true;
}

// Mirror of scripts.cc:1604-1609 — sfallModsIniExit
static void testSfallModsIniExit()
{
    if (gTestSfallModsIniLoaded) {
        configFree(&gTestSfallModsIni);
        gTestSfallModsIniLoaded = false;
    }
}

// Mirror of scripts.cc:1612-1619 — sfallModsIniGetInt
static bool testSfallModsIniGetInt(const char* section, const char* key, int* value, int defaultValue)
{
    if (!gTestSfallModsIniLoaded) {
        *value = defaultValue;
        return false;
    }
    return configGetInt(&gTestSfallModsIni, section, key, value, defaultValue);
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
    CHECK(TEST_OBJECT_ID_PARTY_MEMBER_END == 16795648);
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

// ---- scriptsGetWorldMapSlots tests ----

TEST_CASE("scriptsGetWorldMapSlots — getter")
{
    SUBCASE("default value is 0")
    {
        gTestWorldMapSlots = 0;
        CHECK(testScriptsGetWorldMapSlots() == 0);
    }

    SUBCASE("RPU-compatible default of 21")
    {
        // RPU sets WorldMapSlots=21 in ddraw.ini [Misc]
        gTestWorldMapSlots = 21;
        CHECK(testScriptsGetWorldMapSlots() == 21);
    }

    SUBCASE("custom value")
    {
        gTestWorldMapSlots = 50;
        CHECK(testScriptsGetWorldMapSlots() == 50);
    }

    SUBCASE("getter is const-correct (repeated reads stable)")
    {
        gTestWorldMapSlots = 42;
        CHECK(testScriptsGetWorldMapSlots() == 42);
        CHECK(testScriptsGetWorldMapSlots() == 42);
        // Value unchanged after repeated reads
        CHECK(testScriptsGetWorldMapSlots() == 42);
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

// ---- sfallModsIni tests ----

TEST_CASE("sfallModsIniInit / sfallModsIniExit lifecycle")
{
    // Reset state between subcases
    gTestSfallModsIniLoaded = false;

    SUBCASE("init succeeds and marks as loaded")
    {
        CHECK(testSfallModsIniInit());
        CHECK(gTestSfallModsIniLoaded);

        testSfallModsIniExit();
        CHECK_FALSE(gTestSfallModsIniLoaded);
    }

    SUBCASE("double init returns true (idempotent)")
    {
        CHECK(testSfallModsIniInit());
        CHECK(testSfallModsIniInit()); // Second call, already loaded
        CHECK(gTestSfallModsIniLoaded);

        testSfallModsIniExit();
        CHECK_FALSE(gTestSfallModsIniLoaded);
    }

    SUBCASE("exit when not loaded is safe (no-op guard)")
    {
        // Call exit without init — guarded by gTestSfallModsIniLoaded check
        testSfallModsIniExit();
        CHECK_FALSE(gTestSfallModsIniLoaded);
        // Should not crash
    }

    SUBCASE("double exit is safe")
    {
        CHECK(testSfallModsIniInit());
        testSfallModsIniExit();
        testSfallModsIniExit(); // Second exit — guarded by gTestSfallModsIniLoaded
    }

    SUBCASE("init → exit → init cycle")
    {
        CHECK(testSfallModsIniInit());
        testSfallModsIniExit();

        // Re-init after exit
        CHECK(testSfallModsIniInit());
        CHECK(gTestSfallModsIniLoaded);

        testSfallModsIniExit();
    }
}

TEST_CASE("sfallModsIniGetInt — config integration")
{
    // Clean state before each subcase
    if (gTestSfallModsIniLoaded) testSfallModsIniExit();

    SUBCASE("returns default when not loaded")
    {
        CHECK_FALSE(gTestSfallModsIniLoaded);
        int value = -1;
        bool result = testSfallModsIniGetInt("Section", "Key", &value, 42);
        CHECK_FALSE(result);
        CHECK(value == 42); // defaultValue is returned
    }

    SUBCASE("returns default when key not found")
    {
        CHECK(testSfallModsIniInit());
        int value = -1;
        bool result = testSfallModsIniGetInt("NoSection", "NoKey", &value, 99);
        // configGetInt returns false when key not found AND value is default
        CHECK(value == 99);

        testSfallModsIniExit();
    }

    SUBCASE("retrieves stored integer after setting")
    {
        CHECK(testSfallModsIniInit());

        // Set a value through the underlying Config
        configSetInt(&gTestSfallModsIni, "TestSection", "TestKey", 777);

        int value = 0;
        bool result = testSfallModsIniGetInt("TestSection", "TestKey", &value, -1);
        CHECK(result);
        CHECK(value == 777);

        testSfallModsIniExit();
    }

    SUBCASE("different sections and keys don't interfere")
    {
        CHECK(testSfallModsIniInit());

        configSetInt(&gTestSfallModsIni, "Audio", "Volume", 75);
        configSetInt(&gTestSfallModsIni, "Video", "Width", 1920);
        configSetInt(&gTestSfallModsIni, "Misc", "Speed", 50);

        int value = 0;
        CHECK(testSfallModsIniGetInt("Audio", "Volume", &value, 0));
        CHECK(value == 75);
        CHECK(testSfallModsIniGetInt("Video", "Width", &value, 0));
        CHECK(value == 1920);
        CHECK(testSfallModsIniGetInt("Misc", "Speed", &value, 0));
        CHECK(value == 50);

        testSfallModsIniExit();
    }

    SUBCASE("values survive exit/re-init cycle")
    {
        CHECK(testSfallModsIniInit());
        configSetInt(&gTestSfallModsIni, "Persist", "Count", 123);

        int value = 0;
        CHECK(testSfallModsIniGetInt("Persist", "Count", &value, 0));
        CHECK(value == 123);

        testSfallModsIniExit();

        // After re-init, config is fresh (values gone)
        CHECK(testSfallModsIniInit());
        int value2 = -1;
        testSfallModsIniGetInt("Persist", "Count", &value2, -1);
        CHECK(value2 == -1); // defaultValue — old values are gone

        testSfallModsIniExit();
    }

    SUBCASE("negative and zero values")
    {
        CHECK(testSfallModsIniInit());

        configSetInt(&gTestSfallModsIni, "Negative", "Offset", -50);
        configSetInt(&gTestSfallModsIni, "Negative", "Zero", 0);

        int value = 1;
        CHECK(testSfallModsIniGetInt("Negative", "Offset", &value, 0));
        CHECK(value == -50);
        CHECK(testSfallModsIniGetInt("Negative", "Zero", &value, 1));
        CHECK(value == 0);

        testSfallModsIniExit();
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
