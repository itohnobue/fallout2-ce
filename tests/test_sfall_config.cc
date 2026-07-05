// Unit tests for sfall_config.cc — sfall-specific config wrapper lifecycle,
// default value initialization, and global boolean flag population.
//
// Tests: sfallConfigInit, sfallConfigExit, default config values,
//        global boolean flags, double-init guard, exit-without-init guard,
//        full init→exit→reinit cycle.
//
// This test LINKS sfall_config.cc. That file depends on:
//   config.cc (Config init/set/get/free)  — already in test_sources
//   dictionary.cc                          — already in test_sources
//   memory.cc                              — already in test_sources
//   platform_compat (compat_splitpath, compat_makepath) — in test_stubs
//
// With the existing stubs (compat_splitpath→empty, compat_fopen→nullptr),
// sfallConfigInit will:
//   1. Initialize gSfallConfig (configInit succeeds)
//   2. Set 11 default config keys
//   3. Build an empty path via stubbed compat_splitpath/makepath
//   4. configRead fails silently (compat_fopen returns nullptr)
//   5. Populate 6 global booleans from defaults (all false)
//   6. Return true
//
// This validates the lifecycle, defaults, and guard logic without
// needing a real ddraw.ini file on disk.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "config.h"
#include "sfall_config.h"

using namespace fallout;

// =============================================================
// Reset helpers — ensure clean state between tests.
// =============================================================

static void resetConfigState() {
    // If config was initialized, free it and reset the global flag
    // so we can test fresh init.
    if (gSfallConfigInitialized) {
        configFree(&gSfallConfig);
        gSfallConfigInitialized = false;
    }
    // Reset global booleans to their default (false).
    gFallout1Behavior = false;
    gAllowUnsafeScripting = false;
    gEnableHeroAppearanceMod = false;
    gUseFileSystemOverride = false;
    gOverrideArtCacheSize = false;
    gExtraSaveSlots = false;
}

// =============================================================
// Lifecycle Tests
// =============================================================

TEST_CASE("sfallConfigInit / sfallConfigExit lifecycle") {
    resetConfigState();

    SUBCASE("init succeeds and sets initialized flag") {
        char dummyArg0[] = "fallout2-ce";
        char* argv[] = { dummyArg0 };
        CHECK(sfallConfigInit(1, argv));
        CHECK(gSfallConfigInitialized);

        // Cleanup
        sfallConfigExit();
        CHECK_FALSE(gSfallConfigInitialized);
    }

    SUBCASE("double init returns false (guard)") {
        char dummyArg0[] = "fallout2-ce";
        char* argv[] = { dummyArg0 };
        CHECK(sfallConfigInit(1, argv));
        CHECK(gSfallConfigInitialized);

        // Second init should be rejected
        CHECK_FALSE(sfallConfigInit(1, argv));

        sfallConfigExit();
    }

    SUBCASE("exit without init is safe") {
        resetConfigState();
        // Ensure clean state — config should not be initialized
        CHECK_FALSE(gSfallConfigInitialized);

        // Exit should be a no-op when not initialized
        sfallConfigExit();
        CHECK_FALSE(gSfallConfigInitialized);
    }

    SUBCASE("full cycle: init → exit → re-init") {
        char dummyArg0[] = "fallout2-ce";
        char* argv[] = { dummyArg0 };

        // First init
        CHECK(sfallConfigInit(1, argv));
        CHECK(gSfallConfigInitialized);

        // Set a non-default value to verify cleanup
        configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, "TestKey", 42);
        int val = 0;
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, "TestKey", &val, -1));
        CHECK(val == 42);

        // Exit
        sfallConfigExit();
        CHECK_FALSE(gSfallConfigInitialized);

        // Re-init should succeed with fresh state
        CHECK(sfallConfigInit(1, argv));
        CHECK(gSfallConfigInitialized);

        // Previous value should be gone
        val = -1;
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, "TestKey", &val, -1));
        CHECK(val == -1); // default returned, value not persisted

        sfallConfigExit();
    }

    // Ensure cleanup even if a test fails
    if (gSfallConfigInitialized) {
        sfallConfigExit();
    }
}

TEST_CASE("sfallConfigInit — default config values") {
    resetConfigState();

    char dummyArg0[] = "fallout2-ce";
    char* argv[] = { dummyArg0 };
    REQUIRE(sfallConfigInit(1, argv));

    SUBCASE("OverrideCriticalTable defaults to 2") {
        int val = 0;
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_OVERRIDE_CRITICALS_MODE_KEY, &val, -1));
        CHECK(val == 2);
    }

    SUBCASE("string defaults are empty") {
        char* val = nullptr;
        CHECK(configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_OVERRIDE_CRITICALS_FILE_KEY, &val, "notfound"));
        CHECK(strcmp(val, "notfound") == 0); // empty string → returns default

        CHECK(configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_BOOKS_FILE_KEY, &val, "notfound"));
        CHECK(strcmp(val, "notfound") == 0);

        CHECK(configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_ELEVATORS_FILE_KEY, &val, "notfound"));
        CHECK(strcmp(val, "notfound") == 0);

        CHECK(configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_CONFIG_FILE, &val, "notfound"));
        CHECK(strcmp(val, "notfound") == 0);

        CHECK(configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_PATCH_FILE, &val, "notfound"));
        CHECK(strcmp(val, "notfound") == 0);

        CHECK(configGetString(&gSfallConfig, SFALL_CONFIG_SCRIPTS_KEY,
            SFALL_CONFIG_INI_CONFIG_FOLDER, &val, "notfound"));
        CHECK(strcmp(val, "notfound") == 0);
    }

    SUBCASE("new post-fork flags default to 0") {
        int val = -1;
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_FALLOUT1_BEHAVIOR_KEY, &val, -1));
        CHECK(val == 0);

        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_ENABLE_HERO_APPEARANCE_MOD_KEY, &val, -1));
        CHECK(val == 0);

        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_USE_FILESYSTEM_OVERRIDE_KEY, &val, -1));
        CHECK(val == 0);

        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_OVERRIDE_ART_CACHE_SIZE_KEY, &val, -1));
        CHECK(val == 0);

        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_EXTRA_SAVE_SLOTS_KEY, &val, -1));
        CHECK(val == 0);

        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_DEBUGGING_KEY,
            SFALL_CONFIG_ALLOW_UNSAFE_SCRIPTING_KEY, &val, -1));
        CHECK(val == 0);
    }

    SUBCASE("Fallout1Behavior defaults to 0 (int)") {
        int val = -1;
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_FALLOUT1_BEHAVIOR_KEY, &val, -1));
        CHECK(val == 0);
    }

    sfallConfigExit();
}

TEST_CASE("sfallConfigInit — global boolean flags") {
    resetConfigState();

    char dummyArg0[] = "fallout2-ce";
    char* argv[] = { dummyArg0 };
    REQUIRE(sfallConfigInit(1, argv));

    SUBCASE("all globals default to false") {
        CHECK_FALSE(gFallout1Behavior);
        CHECK_FALSE(gAllowUnsafeScripting);
        CHECK_FALSE(gEnableHeroAppearanceMod);
        CHECK_FALSE(gUseFileSystemOverride);
        CHECK_FALSE(gOverrideArtCacheSize);
        CHECK_FALSE(gExtraSaveSlots);
    }

    SUBCASE("config defaults are initialized as integers") {
        // After init, the config values exist as integer 0.
        // Setting non-zero values via configSetInt should be reflected.
        configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_FALLOUT1_BEHAVIOR_KEY, 1);
        int val = 0;
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_FALLOUT1_BEHAVIOR_KEY, &val, -1));
        CHECK(val == 1);
        // NOTE: The global bool gFallout1Behavior will NOT update
        // on-the-fly; it's only set during sfallConfigInit.
    }

    sfallConfigExit();
}

TEST_CASE("sfallConfigInit with no argv (0 args)") {
    resetConfigState();

    // Edge case: argc = 0, argv may be nullptr or empty
    char* argv[] = {};
    // This should fail because argv[0] access is needed for path building.
    // Or it may work with a safe guard. Document the behavior.
    bool result = sfallConfigInit(0, argv);
    // If it succeeds or fails, the important thing is that it doesn't crash.
    // Force cleanup regardless.
    if (gSfallConfigInitialized) {
        sfallConfigExit();
    }
    // NOTE: Actual behavior depends on compat_splitpath/makepath stubs;
    // the test documents that init with 0 args must not crash.
    CHECK(true); // reached without crash
}

TEST_CASE("sfallConfigExit resets flags") {
    resetConfigState();

    char dummyArg0[] = "fallout2-ce";
    char* argv[] = { dummyArg0 };
    REQUIRE(sfallConfigInit(1, argv));

    // Manually manipulate globals to verify sfallConfigExit doesn't reset them
    // (the exit function only frees the config, it doesn't reset globals)
    gFallout1Behavior = true;
    sfallConfigExit();
    // sfallConfigExit does NOT reset globals — only frees the Config.
    // This documents the current behavior.
    CHECK(gFallout1Behavior); // unchanged by exit
    CHECK_FALSE(gSfallConfigInitialized);

    // Reset for subsequent tests
    gFallout1Behavior = false;
}

// Verify SFALL_CONFIG_FILE_NAME constant
TEST_CASE("sfall_config constants") {
    CHECK(strcmp(SFALL_CONFIG_FILE_NAME, "ddraw.ini") == 0);
    CHECK(strcmp(SFALL_CONFIG_MAIN_KEY, "Main") == 0);
    CHECK(strcmp(SFALL_CONFIG_MISC_KEY, "Misc") == 0);
    CHECK(strcmp(SFALL_CONFIG_SCRIPTS_KEY, "Scripts") == 0);
    CHECK(strcmp(SFALL_CONFIG_SPEED_KEY, "Speed") == 0);
    CHECK(strcmp(SFALL_CONFIG_DEBUGGING_KEY, "Debugging") == 0);

    // Key name constants
    CHECK(strcmp(SFALL_CONFIG_SPEED_MULTI_KEY, "SpeedMulti") == 0);
    CHECK(strcmp(SFALL_CONFIG_SPEED_MULTI_INITIAL_KEY, "SpeedMultiInitial") == 0);

    // Post-fork key name constants
    CHECK(strcmp(SFALL_CONFIG_FALLOUT1_BEHAVIOR_KEY, "Fallout1Behavior") == 0);
    CHECK(strcmp(SFALL_CONFIG_ALLOW_UNSAFE_SCRIPTING_KEY, "AllowUnsafeScripting") == 0);
    CHECK(strcmp(SFALL_CONFIG_ENABLE_HERO_APPEARANCE_MOD_KEY, "EnableHeroAppearanceMod") == 0);
    CHECK(strcmp(SFALL_CONFIG_USE_FILESYSTEM_OVERRIDE_KEY, "UseFileSystemOverride") == 0);
    CHECK(strcmp(SFALL_CONFIG_OVERRIDE_ART_CACHE_SIZE_KEY, "OverrideArtCacheSize") == 0);
    CHECK(strcmp(SFALL_CONFIG_EXTRA_SAVE_SLOTS_KEY, "ExtraSaveSlots") == 0);
}
