// Unit tests for config.cc fork validation changes and sfall_config.cc lifecycle.
//
// Tests configGetInt validation (non-numeric / overflow / hex base),
// configGetDouble validation (non-numeric / overflow), configGetIntList
// (exact count, overcount protection), and sfallConfigInit/Exit lifecycle.
//
// Fork changes tested (from 24199e9):
//   config.cc:234-241   configGetInt  — non-numeric (end==stringValue) + overflow rejection
//   config.cc:896-909   configGetDouble — same validation added
//   config.cc:275-298   configGetIntList — overcount protection + last-item fix
//   sfall_config.cc    sfallConfigInit — 6 new config key defaults + global read-back

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cerrno>
#include <climits>
#include <cstring>

#include "config.h"
#include "dictionary.h"
#include "sfall_config.h"

using namespace fallout;

// =============================================================
// PART 1 — configGetInt validation (fork changes)
// =============================================================

TEST_CASE("configGetInt — fork validation changes")
{
    Config cfg;
    configInit(&cfg);

    SUBCASE("rejects completely non-numeric input (end == stringValue)")
    {
        configSetString(&cfg, "Misc", "NonNumeric", "abc");
        int val = 42;
        CHECK_FALSE(configGetInt(&cfg, "Misc", "NonNumeric", &val));
        CHECK(val == 42); // unchanged on failure
    }

    SUBCASE("rejects input with only whitespace and no digits")
    {
        configSetString(&cfg, "Misc", "Whitespace", "   ");
        int val = 42;
        CHECK_FALSE(configGetInt(&cfg, "Misc", "Whitespace", &val));
        CHECK(val == 42);
    }

    SUBCASE("rejects empty string")
    {
        configSetString(&cfg, "Misc", "Empty", "");
        int val = 42;
        // configGetString returns true for empty string, but strtol("")
        // sets end == stringValue (no conversion) -> configGetInt rejects
        CHECK_FALSE(configGetInt(&cfg, "Misc", "Empty", &val));
        CHECK(val == 42);
    }

    SUBCASE("rejects overflow: value > INT_MAX")
    {
        configSetString(&cfg, "Misc", "Overflow", "99999999999999999999");
        int val = 0;
        CHECK_FALSE(configGetInt(&cfg, "Misc", "Overflow", &val));
    }

    SUBCASE("rejects underflow: value < INT_MIN")
    {
        configSetString(&cfg, "Misc", "Underflow", "-99999999999999999999");
        int val = 0;
        CHECK_FALSE(configGetInt(&cfg, "Misc", "Underflow", &val));
    }

    SUBCASE("accepts valid hex base (base=16)")
    {
        configSetString(&cfg, "Misc", "HexVal", "FF");
        int val = 0;
        // Use 6-arg overload to disambiguate: defaultValue=0, base=16.
        CHECK(configGetInt(&cfg, "Misc", "HexVal", &val, 0, 16));
        CHECK(val == 255);
    }

    SUBCASE("accepts hex with 0x prefix via base=16")
    {
        // strtol with base=16 handles 0x prefix
        configSetString(&cfg, "Misc", "HexPrefix", "0xFF");
        int val = 0;
        CHECK(configGetInt(&cfg, "Misc", "HexPrefix", &val, 0, 16));
        CHECK(val == 255);
    }

    SUBCASE("rejects hex value when base=10 (non-numeric digits)")
    {
        configSetString(&cfg, "Misc", "HexInDec", "FF");
        int val = 0;
        // strtol("FF", &end, 10) -> end == "FF" (no decimal digits)
        CHECK_FALSE(configGetInt(&cfg, "Misc", "HexInDec", &val));
    }

    SUBCASE("accepts negative values within INT_MIN..INT_MAX")
    {
        configSetString(&cfg, "Misc", "Neg", "-42");
        int val = 0;
        CHECK(configGetInt(&cfg, "Misc", "Neg", &val));
        CHECK(val == -42);
    }

    SUBCASE("accepts zero")
    {
        configSetString(&cfg, "Misc", "Zero", "0");
        int val = 1;
        CHECK(configGetInt(&cfg, "Misc", "Zero", &val));
        CHECK(val == 0);
    }

    SUBCASE("accepts INT_MAX boundary")
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", INT_MAX);
        configSetString(&cfg, "Misc", "MaxInt", buf);
        int val = 0;
        CHECK(configGetInt(&cfg, "Misc", "MaxInt", &val));
        CHECK(val == INT_MAX);
    }

    SUBCASE("accepts INT_MIN boundary")
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", INT_MIN);
        configSetString(&cfg, "Misc", "MinInt", buf);
        int val = 0;
        CHECK(configGetInt(&cfg, "Misc", "MinInt", &val));
        CHECK(val == INT_MIN);
    }

    SUBCASE("handles trailing non-numeric chars (partial conversion)")
    {
        // strtol("123abc", &end, 10) converts 123, end points to "abc".
        // The fork's validation only rejects when end == stringValue (no
        // conversion at all) and on overflow. Partial conversions succeed.
        configSetString(&cfg, "Misc", "Trailing", "123abc");
        int val = 0;
        CHECK(configGetInt(&cfg, "Misc", "Trailing", &val));
        CHECK(val == 123);
    }

    SUBCASE("handles leading whitespace")
    {
        configSetString(&cfg, "Misc", "LeadWS", "  42");
        int val = 0;
        CHECK(configGetInt(&cfg, "Misc", "LeadWS", &val));
        CHECK(val == 42);
    }

    SUBCASE("handles trailing whitespace")
    {
        configSetString(&cfg, "Misc", "TrailWS", "42  ");
        int val = 0;
        CHECK(configGetInt(&cfg, "Misc", "TrailWS", &val));
        CHECK(val == 42);
    }

    SUBCASE("null valuePtr returns false")
    {
        configSetString(&cfg, "Misc", "Valid", "42");
        CHECK_FALSE(configGetInt(&cfg, "Misc", "Valid", nullptr));
    }

    SUBCASE("non-existent key returns false")
    {
        int val = 0;
        CHECK_FALSE(configGetInt(&cfg, "Misc", "NoSuchKey", &val));
        CHECK(val == 0);
    }

    SUBCASE("nullptr config handled gracefully (configGetString rejects)")
    {
        int val = 0;
        CHECK_FALSE(configGetInt(nullptr, "Misc", "Key", &val));
    }

    configFree(&cfg);
}

TEST_CASE("configGetInt with default value — fork validation interaction")
{
    Config cfg;
    configInit(&cfg);

    SUBCASE("non-numeric input falls back to default")
    {
        configSetString(&cfg, "Misc", "Bad", "xyz");
        int val = 0;
        // overload returns true (used default), value set to default
        CHECK(configGetInt(&cfg, "Misc", "Bad", &val, 99));
        CHECK(val == 99);
    }

    SUBCASE("overflow input falls back to default")
    {
        configSetString(&cfg, "Misc", "Bad", "99999999999999999999");
        int val = 0;
        CHECK(configGetInt(&cfg, "Misc", "Bad", &val, 42));
        CHECK(val == 42);
    }

    SUBCASE("missing key falls back to default")
    {
        int val = 0;
        CHECK(configGetInt(&cfg, "Misc", "Missing", &val, 77));
        CHECK(val == 77);
    }

    SUBCASE("null args still return false even with default overload")
    {
        int val = 0;
        CHECK_FALSE(configGetInt(nullptr, "S", "K", &val, 99));
        CHECK_FALSE(configGetInt(&cfg, nullptr, "K", &val, 99));
        CHECK_FALSE(configGetInt(&cfg, "S", nullptr, &val, 99));
        CHECK_FALSE(configGetInt(&cfg, "S", "K", nullptr, 99));
    }

    configFree(&cfg);
}

// =============================================================
// PART 2 — configGetDouble validation (fork changes)
// =============================================================

TEST_CASE("configGetDouble — fork validation changes")
{
    Config cfg;
    configInit(&cfg);

    SUBCASE("rejects completely non-numeric input")
    {
        configSetString(&cfg, "Physics", "Speed", "fast");
        double val = 1.0;
        CHECK_FALSE(configGetDouble(&cfg, "Physics", "Speed", &val));
        CHECK(val == 1.0); // unchanged on failure
    }

    SUBCASE("rejects empty string")
    {
        configSetString(&cfg, "Physics", "Empty", "");
        double val = 1.0;
        CHECK_FALSE(configGetDouble(&cfg, "Physics", "Empty", &val));
        CHECK(val == 1.0);
    }

    SUBCASE("rejects overflow (1e9999 exceeds double range)")
    {
        configSetString(&cfg, "Physics", "Huge", "1e9999");
        double val = 0.0;
        CHECK_FALSE(configGetDouble(&cfg, "Physics", "Huge", &val));
    }

    SUBCASE("accepts valid double")
    {
        configSetString(&cfg, "Physics", "Gravity", "9.81");
        double val = 0.0;
        CHECK(configGetDouble(&cfg, "Physics", "Gravity", &val));
        CHECK(val == doctest::Approx(9.81));
    }

    SUBCASE("accepts scientific notation")
    {
        configSetString(&cfg, "Physics", "SciNot", "1.5e3");
        double val = 0.0;
        CHECK(configGetDouble(&cfg, "Physics", "SciNot", &val));
        CHECK(val == doctest::Approx(1500.0));
    }

    SUBCASE("accepts negative double")
    {
        configSetString(&cfg, "Physics", "Neg", "-3.14");
        double val = 0.0;
        CHECK(configGetDouble(&cfg, "Physics", "Neg", &val));
        CHECK(val == doctest::Approx(-3.14));
    }

    SUBCASE("accepts zero")
    {
        configSetString(&cfg, "Physics", "Zero", "0.0");
        double val = 1.0;
        CHECK(configGetDouble(&cfg, "Physics", "Zero", &val));
        CHECK(val == doctest::Approx(0.0));
    }

    SUBCASE("handles trailing non-numeric chars (partial conversion)")
    {
        // strtod("123.45abc", &end) converts 123.45, end points to "abc".
        // Fork only rejects when end == stringValue or errno == ERANGE.
        configSetString(&cfg, "Physics", "Trailing", "123.45abc");
        double val = 0.0;
        CHECK(configGetDouble(&cfg, "Physics", "Trailing", &val));
        CHECK(val == doctest::Approx(123.45));
    }

    SUBCASE("null valuePtr returns false")
    {
        configSetString(&cfg, "Physics", "G", "9.81");
        CHECK_FALSE(configGetDouble(&cfg, "Physics", "G", nullptr));
    }

    SUBCASE("non-existent key returns false")
    {
        double val = 0.0;
        CHECK_FALSE(configGetDouble(&cfg, "Physics", "NoKey", &val));
    }

    configFree(&cfg);
}

// =============================================================
// PART 3 — configGetIntList (fork changes)
// =============================================================

TEST_CASE("configGetIntList — normal operation")
{
    Config cfg;
    configInit(&cfg);

    SUBCASE("extracts exact count from comma-separated list")
    {
        configSetString(&cfg, "Misc", "List", "10,20,30,40,50");
        int arr[5] = {0, 0, 0, 0, 0};
        CHECK(configGetIntList(&cfg, "Misc", "List", arr, 5));
        CHECK(arr[0] == 10);
        CHECK(arr[1] == 20);
        CHECK(arr[2] == 30);
        CHECK(arr[3] == 40);
        CHECK(arr[4] == 50);
    }

    SUBCASE("returns true when count matches exactly")
    {
        configSetString(&cfg, "Misc", "List", "1,2,3");
        int arr[3] = {0, 0, 0};
        CHECK(configGetIntList(&cfg, "Misc", "List", arr, 3));
        CHECK(arr[0] == 1);
        CHECK(arr[1] == 2);
        CHECK(arr[2] == 3);
    }

    SUBCASE("handles negative values in list")
    {
        configSetString(&cfg, "Misc", "List", "-5,0,5");
        int arr[3] = {0, 0, 0};
        CHECK(configGetIntList(&cfg, "Misc", "List", arr, 3));
        CHECK(arr[0] == -5);
        CHECK(arr[1] == 0);
        CHECK(arr[2] == 5);
    }

    configFree(&cfg);
}

TEST_CASE("configGetIntList — fork overcount fix (fewer items than requested)")
{
    Config cfg;
    configInit(&cfg);

    SUBCASE("returns true when list has exactly as many items as requested")
    {
        // 3 items requested, 3 items provided → count goes 3→2→1→0 → returns true
        configSetString(&cfg, "Misc", "List", "10,20,30");
        int arr[3] = {0, 0, 0};
        CHECK(configGetIntList(&cfg, "Misc", "List", arr, 3));
        CHECK(arr[0] == 10);
        CHECK(arr[1] == 20);
        CHECK(arr[2] == 30);
    }

    SUBCASE("sfall fix: overcount still returns false but last item is consumed")
    {
        // Fork fix (config.cc:291-296): before the fix, the last item in the
        // list was silently dropped when count exceeded actual items. The fix
        // adds a post-loop block that consumes the last remaining item:
        //   if (count > 0) { *arr = atoi(string); count--; }
        // This preserves data correctness — the last item IS written — even
        // though the return value is still false when count != 0.
        //
        // "10,20,30" with count=5: loop consumes "10","20", then break.
        // Post-loop: arr[2]=30, count=2. Return 2!=0 → false.
        configSetString(&cfg, "Misc", "List", "10,20,30");
        int arr[5] = {-1, -1, -1, -1, -1};
        CHECK_FALSE(configGetIntList(&cfg, "Misc", "List", arr, 5));
        CHECK(arr[0] == 10);
        CHECK(arr[1] == 20);
        CHECK(arr[2] == 30); // fork fix: last item consumed into arr[2]
        CHECK(arr[3] == -1); // untouched
        CHECK(arr[4] == -1); // untouched
    }

    SUBCASE("single item with count > 1: last item consumed but returns false")
    {
        configSetString(&cfg, "Misc", "List", "42");
        int arr[3] = {-1, -1, -1};

        // count=3 > 1 item → returns false
        // fork fix: arr[0] = 42 (last item consumed)
        CHECK_FALSE(configGetIntList(&cfg, "Misc", "List", arr, 3));
        CHECK(arr[0] == 42);
        CHECK(arr[1] == -1);
        CHECK(arr[2] == -1);
    }

    configFree(&cfg);
}

TEST_CASE("configGetIntList — edge cases and error paths")
{
    Config cfg;
    configInit(&cfg);

    SUBCASE("rejects count < 2")
    {
        configSetString(&cfg, "Misc", "List", "10,20");
        int arr[2] = {0, 0};
        CHECK_FALSE(configGetIntList(&cfg, "Misc", "List", arr, 1));
    }

    SUBCASE("rejects count == 0")
    {
        configSetString(&cfg, "Misc", "List", "10,20");
        int arr[1] = {0};
        CHECK_FALSE(configGetIntList(&cfg, "Misc", "List", arr, 0));
    }

    SUBCASE("rejects null arr")
    {
        configSetString(&cfg, "Misc", "List", "10,20");
        CHECK_FALSE(configGetIntList(&cfg, "Misc", "List", nullptr, 3));
    }

    SUBCASE("rejects non-existent key")
    {
        int arr[3] = {0, 0, 0};
        CHECK_FALSE(configGetIntList(&cfg, "Misc", "NoKey", arr, 3));
    }

    SUBCASE("handles list with trailing comma (empty last element = 0)")
    {
        // "10,20," → atoi("") = 0 for the empty trailing element
        configSetString(&cfg, "Misc", "List", "10,20,");
        int arr[3] = {-1, -1, -1};
        CHECK(configGetIntList(&cfg, "Misc", "List", arr, 3));
        CHECK(arr[0] == 10);
        CHECK(arr[1] == 20);
        CHECK(arr[2] == 0); // atoi("") → 0
    }

    SUBCASE("count == 2 is the minimum valid count")
    {
        configSetString(&cfg, "Misc", "List", "50,100");
        int arr[2] = {0, 0};
        CHECK(configGetIntList(&cfg, "Misc", "List", arr, 2));
        CHECK(arr[0] == 50);
        CHECK(arr[1] == 100);
    }

    SUBCASE("handles whitespace around values (atoi discards leading whitespace)")
    {
        // atoi skips leading whitespace, stops at first non-digit
        // "10, 20" → atoi(" 20") = 20 (atoi skips space)
        configSetString(&cfg, "Misc", "List", "10, 20, 30");
        int arr[3] = {0, 0, 0};
        CHECK(configGetIntList(&cfg, "Misc", "List", arr, 3));
        CHECK(arr[0] == 10);
        CHECK(arr[1] == 20);
        CHECK(arr[2] == 30);
    }

    configFree(&cfg);
}

// =============================================================
// PART 4 — sfallConfigInit / sfallConfigExit lifecycle
// =============================================================

// sfall_config.cc tests require linking against sfall_config.cc.
// See CMakeLists.txt REQUIREMENTS section in the report.
//
// Existing stubs that make this workable:
//   compat_splitpath   → zeroes output buffers (already in test_common_stubs.cc)
//   compat_makepath    → zeroes output path (already in test_common_stubs.cc)
//   configRead         → fileOpen/compat_fopen returns nullptr → configRead returns false
//   gSfallConfig       → extern Config declared in test_common_stubs.cc:171

TEST_CASE("sfallConfigInit — default initialization")
{
    // Clean up from any prior test.
    sfallConfigExit();

    SUBCASE("first init succeeds")
    {
        // sfallConfigInit requires argc >= 1 (dereferences argv[0]).
        // The stub compat_splitpath doesn't actually read argv[0] —
        // it zeroes all output buffers regardless of input.
        const char* fakeArgv0 = "fallout2-ce.exe";
        char* argv[] = { const_cast<char*>(fakeArgv0), nullptr };
        CHECK(sfallConfigInit(1, argv));
        CHECK(gSfallConfigInitialized);
    }

    SUBCASE("double init returns false")
    {
        const char* fakeArgv0 = "fallout2-ce.exe";
        char* argv[] = { const_cast<char*>(fakeArgv0), nullptr };
        CHECK(sfallConfigInit(1, argv));
        // Second init should reject.
        CHECK_FALSE(sfallConfigInit(1, argv));
    }

    sfallConfigExit();
}

TEST_CASE("sfallConfigInit — global variables set to false/0 as defaults")
{
    sfallConfigExit();

    const char* fakeArgv0 = "fallout2-ce.exe";
    char* argv[] = { const_cast<char*>(fakeArgv0), nullptr };

    // Force globals to true before init to confirm init sets them to false.
    gFallout1Behavior = true;
    gAllowUnsafeScripting = true;
    gEnableHeroAppearanceMod = true;
    gUseFileSystemOverride = true;
    gOverrideArtCacheSize = true;
    gExtraSaveSlots = true;

    CHECK(sfallConfigInit(1, argv));

    // configRead fails (stubbed fileOpen returns nullptr), so all globals
    // stay at configGetInt defaults (0 → false).
    CHECK_FALSE(gFallout1Behavior);
    CHECK_FALSE(gAllowUnsafeScripting);
    CHECK_FALSE(gEnableHeroAppearanceMod);
    CHECK_FALSE(gUseFileSystemOverride);
    CHECK_FALSE(gOverrideArtCacheSize);
    CHECK_FALSE(gExtraSaveSlots);

    sfallConfigExit();
}

TEST_CASE("sfallConfigInit — config keys set to defaults in gSfallConfig")
{
    sfallConfigExit();

    const char* fakeArgv0 = "fallout2-ce.exe";
    char* argv[] = { const_cast<char*>(fakeArgv0), nullptr };
    CHECK(sfallConfigInit(1, argv));

    SUBCASE("OverrideCriticalTable defaults to 2")
    {
        int val = 0;
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_OVERRIDE_CRITICALS_MODE_KEY, &val));
        CHECK(val == 2);
    }

    SUBCASE("OverrideCriticalFile defaults to empty string")
    {
        char* val = nullptr;
        CHECK(configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_OVERRIDE_CRITICALS_FILE_KEY, &val));
        CHECK(val != nullptr);
        CHECK(strcmp(val, "") == 0);
    }

    SUBCASE("Fallout1Behavior config key defaults to 0")
    {
        int val = -1;
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_FALLOUT1_BEHAVIOR_KEY, &val));
        CHECK(val == 0);
    }

    SUBCASE("AllowUnsafeScripting config key defaults to 0")
    {
        int val = -1;
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_DEBUGGING_KEY,
            SFALL_CONFIG_ALLOW_UNSAFE_SCRIPTING_KEY, &val));
        CHECK(val == 0);
    }

    SUBCASE("EnableHeroAppearanceMod config key absent (F-17: always-on)")
    {
        int val = -1;
        // Use 4-arg configGetInt (no defaultValue) to check absence —
        // the 5-arg overload always returns true regardless of key existence.
        CHECK_FALSE(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_ENABLE_HERO_APPEARANCE_MOD_KEY, &val));
        // val stays -1 (unchanged) because configGetInt returned false
    }

    SUBCASE("UseFileSystemOverride config key defaults to 0")
    {
        int val = -1;
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_USE_FILESYSTEM_OVERRIDE_KEY, &val));
        CHECK(val == 0);
    }

    SUBCASE("OverrideArtCacheSize config key defaults to 0")
    {
        int val = -1;
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_OVERRIDE_ART_CACHE_SIZE_KEY, &val));
        CHECK(val == 0);
    }

    SUBCASE("ExtraSaveSlots config key defaults to 0")
    {
        int val = -1;
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_EXTRA_SAVE_SLOTS_KEY, &val));
        CHECK(val == 0);
    }

    SUBCASE("IniConfigFolder defaults to empty string")
    {
        char* val = nullptr;
        CHECK(configGetString(&gSfallConfig, SFALL_CONFIG_SCRIPTS_KEY,
            SFALL_CONFIG_INI_CONFIG_FOLDER, &val));
        CHECK(val != nullptr);
        CHECK(strcmp(val, "") == 0);
    }

    sfallConfigExit();
}

TEST_CASE("sfallConfigExit lifecycle")
{
    SUBCASE("exit without init is safe (no-op)")
    {
        gSfallConfigInitialized = false;
        sfallConfigExit();
        CHECK_FALSE(gSfallConfigInitialized);
    }

    SUBCASE("exit after init cleans up")
    {
        sfallConfigExit(); // ensure clean state
        const char* fakeArgv0 = "fallout2-ce.exe";
        char* argv[] = { const_cast<char*>(fakeArgv0), nullptr };
        CHECK(sfallConfigInit(1, argv));
        CHECK(gSfallConfigInitialized);

        sfallConfigExit();
        CHECK_FALSE(gSfallConfigInitialized);
    }

    SUBCASE("re-init after exit succeeds")
    {
        sfallConfigExit(); // ensure clean state
        const char* fakeArgv0 = "fallout2-ce.exe";
        char* argv[] = { const_cast<char*>(fakeArgv0), nullptr };

        CHECK(sfallConfigInit(1, argv));
        sfallConfigExit();

        // Re-init should succeed since exit clears gSfallConfigInitialized.
        CHECK(sfallConfigInit(1, argv));
        CHECK(gSfallConfigInitialized);

        sfallConfigExit();
    }

    SUBCASE("double exit is safe")
    {
        sfallConfigExit(); // ensure clean state
        const char* fakeArgv0 = "fallout2-ce.exe";
        char* argv[] = { const_cast<char*>(fakeArgv0), nullptr };

        CHECK(sfallConfigInit(1, argv));
        sfallConfigExit();
        sfallConfigExit(); // second exit — no-op
        CHECK_FALSE(gSfallConfigInitialized);
    }
}
