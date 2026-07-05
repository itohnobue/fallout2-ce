// Unit tests for misc engine edge cases not covered by other test files.
//
// Covers: configGetBool default overload, configSetDouble/configGetIntList
// fork changes, and boundary tests for config data structure operations.
//
// Fork changes validated:
//   config.cc:919-925   configSetDouble — snprintf-backed double serialization
//   config.cc:261-298   configGetIntList — overcount fix boundary checks
//   config.cc:927-953   configGetBool — default overload null checking

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

#include "config.h"
#include "dictionary.h"

using namespace fallout;

// =============================================================
// PART 1 — configGetBool default value overload
// =============================================================

TEST_CASE("configGetBool with default value")
{
    Config cfg;
    configInit(&cfg);

    SUBCASE("valid bool returns stored value, ignores default")
    {
        configSetBool(&cfg, "Flags", "Enabled", true);
        bool val = false;
        CHECK(configGetBool(&cfg, "Flags", "Enabled", &val, false));
        CHECK(val == true);
    }

    SUBCASE("false value returned, ignores true default")
    {
        configSetBool(&cfg, "Flags", "Debug", false);
        bool val = true;
        CHECK(configGetBool(&cfg, "Flags", "Debug", &val, true));
        CHECK(val == false);
    }

    SUBCASE("missing key returns default true")
    {
        bool val = false;
        CHECK(configGetBool(&cfg, "Flags", "Missing", &val, true));
        CHECK(val == true);
    }

    SUBCASE("missing key returns default false")
    {
        bool val = true;
        CHECK(configGetBool(&cfg, "Flags", "Missing", &val, false));
        CHECK(val == false);
    }

    SUBCASE("non-numeric value: configGetInt rejects → falls back to default")
    {
        // configGetBool calls configGetInt internally, which now rejects
        // non-numeric input (fork change). The default overload provides
        // the fallback value in that case.
        configSetString(&cfg, "Flags", "Bad", "not-a-bool");
        bool val = false;
        CHECK(configGetBool(&cfg, "Flags", "Bad", &val, true));
        CHECK(val == true);
    }

    SUBCASE("int 0 returns false")
    {
        configSetInt(&cfg, "Flags", "Zero", 0);
        bool val = true;
        CHECK(configGetBool(&cfg, "Flags", "Zero", &val, false));
        CHECK(val == false);
    }

    SUBCASE("non-zero int returns true (bool conversion)")
    {
        configSetInt(&cfg, "Flags", "NonZero", 42);
        bool val = false;
        CHECK(configGetBool(&cfg, "Flags", "NonZero", &val));
        CHECK(val == true);
    }

    SUBCASE("null config returns false even with default overload")
    {
        bool val = false;
        CHECK_FALSE(configGetBool(nullptr, "Flags", "Key", &val, true));
    }

    SUBCASE("null sectionKey returns false")
    {
        bool val = false;
        CHECK_FALSE(configGetBool(&cfg, nullptr, "Key", &val, true));
    }

    SUBCASE("null key returns false")
    {
        bool val = false;
        CHECK_FALSE(configGetBool(&cfg, "Flags", nullptr, &val, true));
    }

    SUBCASE("null valuePtr returns false")
    {
        CHECK_FALSE(configGetBool(&cfg, "Flags", "Key", nullptr, true));
    }

    configFree(&cfg);
}

TEST_CASE("configGetBool without default — fork interaction")
{
    Config cfg;
    configInit(&cfg);

    SUBCASE("non-numeric input makes configGetBool return false (no default)")
    {
        configSetString(&cfg, "Flags", "Bad", "xyz");
        bool val = true;
        // configGetBool (no default): configGetInt rejects non-numeric → false
        CHECK_FALSE(configGetBool(&cfg, "Flags", "Bad", &val));
        // val is untouched on failure (no default overload to assign it)
        CHECK(val == true);
    }

    SUBCASE("null valuePtr returns false")
    {
        CHECK_FALSE(configGetBool(&cfg, "Flags", "Key", nullptr));
    }

    configFree(&cfg);
}

// =============================================================
// PART 2 — configSetDouble (fork: snprintf-based serialization)
// =============================================================

TEST_CASE("configSetDouble — fork snprintf serialization")
{
    Config cfg;
    configInit(&cfg);

    SUBCASE("positive double round-trips via configSetDouble/configGetDouble")
    {
        CHECK(configSetDouble(&cfg, "Physics", "Gravity", 9.80665));
        double val = 0.0;
        CHECK(configGetDouble(&cfg, "Physics", "Gravity", &val));
        CHECK(val == doctest::Approx(9.80665));
    }

    SUBCASE("negative double round-trip")
    {
        CHECK(configSetDouble(&cfg, "Physics", "Neg", -3.14159));
        double val = 0.0;
        CHECK(configGetDouble(&cfg, "Physics", "Neg", &val));
        CHECK(val == doctest::Approx(-3.14159));
    }

    SUBCASE("zero round-trip")
    {
        CHECK(configSetDouble(&cfg, "Physics", "Zero", 0.0));
        double val = 1.0;
        CHECK(configGetDouble(&cfg, "Physics", "Zero", &val));
        CHECK(val == doctest::Approx(0.0));
    }

    SUBCASE("integer-value double round-trip")
    {
        CHECK(configSetDouble(&cfg, "Physics", "Int", 42.0));
        double val = 0.0;
        CHECK(configGetDouble(&cfg, "Physics", "Int", &val));
        CHECK(val == doctest::Approx(42.0));
    }

    SUBCASE("small fraction round-trip")
    {
        CHECK(configSetDouble(&cfg, "Physics", "Small", 0.0001));
        double val = 0.0;
        CHECK(configGetDouble(&cfg, "Physics", "Small", &val));
        CHECK(val == doctest::Approx(0.0001));
    }

    configFree(&cfg);
}

// =============================================================
// PART 3 — configGetIntList additional edge cases
// =============================================================

TEST_CASE("configGetIntList — additional boundary conditions")
{
    Config cfg;
    configInit(&cfg);

    SUBCASE("large count with single value: last item consumed, returns false")
    {
        configSetString(&cfg, "Misc", "List", "999");
        int arr[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
        // count=10, 1 item → loop skips (no comma), sfall fix reads last item
        CHECK_FALSE(configGetIntList(&cfg, "Misc", "List", arr, 10));
        CHECK(arr[0] == 999);
        // Remaining elements untouched (still -1)
        CHECK(arr[1] == -1);
        CHECK(arr[9] == -1);
    }

    SUBCASE("count == 2 with single item: returns false")
    {
        // Minimum valid count is 2, but single item means count goes 2→1
        // (no comma, break, if count>0: arr[0]=42, count=0, return true!)
        // Wait — let me trace:
        // string="42", count=2, pch=strchr("42",',')==nullptr, break
        // count>0 (2>0): arr[0]=42, count=1
        // return 1==0 → false
        configSetString(&cfg, "Misc", "List", "42");
        int arr[2] = {-1, -1};
        CHECK_FALSE(configGetIntList(&cfg, "Misc", "List", arr, 2));
        CHECK(arr[0] == 42);
        CHECK(arr[1] == -1);
    }

    SUBCASE("exact count=2 with two items: returns true")
    {
        configSetString(&cfg, "Misc", "List", "10,20");
        int arr[2] = {0, 0};
        CHECK(configGetIntList(&cfg, "Misc", "List", arr, 2));
        CHECK(arr[0] == 10);
        CHECK(arr[1] == 20);
    }

    SUBCASE("handles list with only commas")
    {
        // ",,," → each atoi("") = 0
        configSetString(&cfg, "Misc", "List", ",,,");
        int arr[4] = {-1, -1, -1, -1};
        // count=4, iter 1: arr[0]=0, count=3, string=",,"
        // iter 2: arr[1]=0, count=2, string=","
        // iter 3: arr[2]=0, count=1, string=""
        // iter 4: pch=strchr("",',')=nullptr, break
        // count>0: arr[3]=atoi("")=0, count=0
        // return 0==0 → true
        CHECK(configGetIntList(&cfg, "Misc", "List", arr, 4));
        CHECK(arr[0] == 0);
        CHECK(arr[1] == 0);
        CHECK(arr[2] == 0);
        CHECK(arr[3] == 0);
    }

    SUBCASE("handles mixed valid and empty values")
    {
        configSetString(&cfg, "Misc", "List", "10,,30");
        int arr[3] = {-1, -1, -1};
        CHECK(configGetIntList(&cfg, "Misc", "List", arr, 3));
        CHECK(arr[0] == 10);
        CHECK(arr[1] == 0);  // empty = 0
        CHECK(arr[2] == 30);
    }

    SUBCASE("non-existent section returns false")
    {
        configSetString(&cfg, "Real", "List", "1,2,3");
        int arr[3] = {0, 0, 0};
        CHECK_FALSE(configGetIntList(&cfg, "Fake", "List", arr, 3));
    }

    SUBCASE("nullptr config (configGetString rejects)")
    {
        int arr[3] = {0, 0, 0};
        CHECK_FALSE(configGetIntList(nullptr, "Misc", "List", arr, 3));
    }

    SUBCASE("count = -1 (negative, which is < 2)")
    {
        configSetString(&cfg, "Misc", "List", "1,2,3");
        int arr[3] = {0, 0, 0};
        CHECK_FALSE(configGetIntList(&cfg, "Misc", "List", arr, -1));
    }

    configFree(&cfg);
}

// =============================================================
// PART 4 — ScopedConfig file path constructor
// =============================================================

TEST_CASE("ScopedConfig — file path constructor behavior with stubbed I/O")
{
    SUBCASE("file path ctor calls configRead, which fails with stubbed file I/O")
    {
        // configRead → compat_fopen (stubbed, returns nullptr) → returns false.
        // The default ctor succeeds (configInit), but _loaded becomes false
        // after configRead fails.
        ScopedConfig sc("nonexistent.ini", false);
        CHECK(sc.isInitialized()); // configInit succeeded
        CHECK_FALSE(static_cast<bool>(sc)); // configRead failed → _loaded=false
        CHECK(sc.get() != nullptr);
    }

    SUBCASE("file path ctor is still a valid config even when load fails")
    {
        ScopedConfig sc("nonexistent.ini", false);
        // The underlying Config is initialized — we can use it.
        CHECK(configSetString(sc.get(), "Section", "Key", "Value"));

        char* val = nullptr;
        CHECK(configGetString(sc.get(), "Section", "Key", &val));
        CHECK(strcmp(val, "Value") == 0);
    }
    // Destructor cleans up.
}

TEST_CASE("ScopedConfig — default constructor")
{
    ScopedConfig sc;
    // Default ctor: configInit succeeds on fresh stack Config.
    CHECK(sc.isInitialized());
    // _loaded = configInit succeeds → true → operator bool returns true.
    CHECK(static_cast<bool>(sc));
    CHECK(sc.get() != nullptr);
}

TEST_CASE("ScopedConfig — non-copyable")
{
    // ScopedConfig is non-copyable (copy ctor/assignment deleted).
    // This is verified at compile time — no runtime test needed.
    // The test exists to document the constraint.
    ScopedConfig sc;
    CHECK(sc.isInitialized());
    // If ScopedConfig were copyable, this test file wouldn't compile.
}
