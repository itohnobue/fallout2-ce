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
//   2. Set the default config keys — 18 configSet calls (11 configSetInt +
//      7 configSetString) in sfallConfigInit, src/sfall_config.cc:77-106;
//      EnableHeroAppearanceMod is deliberately NOT set as a default
//   3. Build an empty path via stubbed compat_splitpath/makepath
//   4. configRead fails silently (compat_fopen returns nullptr)
//   5. Populate 9 globals from defaults (booleans false, ints 0/261)
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
    gProcessorIdle = false;
    // Reset int globals to their defaults.
    gBoxBarColours = 0;
    gSfallArtCacheSize = SFALL_CONFIG_ART_CACHE_SIZE_DEFAULT;
    gFemaleDialogMsgs = 0;
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

        // F-017: EnableHeroAppearanceMod key is intentionally NOT set
        // as a default (the feature is always-on; the config flag was dead).
        // Use the 4-arg configGetInt (no defaultValue) to check absence —
        // the 5-arg overload always returns true because it falls back to
        // the default value regardless of key existence.
        CHECK_FALSE(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_ENABLE_HERO_APPEARANCE_MOD_KEY, &val));

        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_USE_FILESYSTEM_OVERRIDE_KEY, &val, -1));
        CHECK(val == 0);

        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_OVERRIDE_ART_CACHE_SIZE_KEY, &val, -1));
        CHECK(val == 0);

        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_EXTRA_SAVE_SLOTS_KEY, &val, -1));
        CHECK(val == 0);

        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_PROCESSOR_IDLE_KEY, &val, -1));
        CHECK(val == 0);

        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_BOX_BAR_COLOURS_KEY, &val, -1));
        CHECK(val == 0);

        // RPU parity: ArtCacheSize defaults to sfall's fixed override value
        // (261 MB when OverrideArtCacheSize=1 — sfall has no ArtCacheSize
        // key; sfall-readme.txt:361 "set the art cache size to 261").
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_ART_CACHE_SIZE_KEY, &val, -1));
        CHECK(val == SFALL_CONFIG_ART_CACHE_SIZE_DEFAULT);

        // P3 RPU parity: FemaleDialogMsgs defaults to 0 (normal dirs).
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_FEMALE_DIALOG_MSGS_KEY, &val, -1));
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

    SUBCASE("H-06: WorldMapSlots defaults to 21") {
        // RPU's gl_k_modini.ssl requires get_ini_setting("ddraw.ini|Misc|WorldMapSlots")
        // to equal 21 or it calls signal_end_game. sfall's own default is 0,
        // which also fails the check — 21 is the only value that satisfies RPU.
        int val = -1;
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_WORLDMAP_SLOTS_KEY, &val, -1));
        CHECK(val == 21);
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
        CHECK_FALSE(gProcessorIdle);
    }

    SUBCASE("new int globals default to 0 / sfall art cache default") {
        CHECK(gBoxBarColours == 0);
        CHECK(gSfallArtCacheSize == SFALL_CONFIG_ART_CACHE_SIZE_DEFAULT);
        CHECK(gFemaleDialogMsgs == 0);
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

    // P3 RPU parity key name constants
    CHECK(strcmp(SFALL_CONFIG_PROCESSOR_IDLE_KEY, "ProcessorIdle") == 0);
    CHECK(strcmp(SFALL_CONFIG_BOX_BAR_COLOURS_KEY, "BoxBarColours") == 0);
    CHECK(strcmp(SFALL_CONFIG_ART_CACHE_SIZE_KEY, "ArtCacheSize") == 0);
    CHECK(strcmp(SFALL_CONFIG_FEMALE_DIALOG_MSGS_KEY, "FemaleDialogMsgs") == 0);
    CHECK(SFALL_CONFIG_ART_CACHE_SIZE_DEFAULT == 261);
    CHECK(SFALL_CONFIG_ART_CACHE_SIZE_MIN == 8);
    CHECK(SFALL_CONFIG_ART_CACHE_SIZE_MAX == 512);
}

TEST_CASE("P3 RPU parity — ProcessorIdle / BoxBarColours parse mapping") {
    resetConfigState();

    char dummyArg0[] = "fallout2-ce";
    char* argv[] = { dummyArg0 };
    REQUIRE(sfallConfigInit(1, argv));

    SUBCASE("globals are populated from the config defaults") {
        // configRead fails in the test environment (compat_fopen → nullptr),
        // so the globals reflect the configSetInt defaults — the same path the
        // production parse loop reads from (sfallConfigInit global-read block).
        CHECK_FALSE(gProcessorIdle);
        CHECK(gBoxBarColours == 0);
        CHECK(gSfallArtCacheSize == SFALL_CONFIG_ART_CACHE_SIZE_DEFAULT);
    }

    SUBCASE("config keys store the values the production parse loop reads") {
        // Same pattern as "config defaults are initialized as integers":
        // the global-read block uses configGetInt on these exact keys.
        configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PROCESSOR_IDLE_KEY, 1);
        configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BOX_BAR_COLOURS_KEY, 11111);
        configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_ART_CACHE_SIZE_KEY, 64);

        int val = -1;
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_PROCESSOR_IDLE_KEY, &val, -1));
        CHECK(val == 1);
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_BOX_BAR_COLOURS_KEY, &val, -1));
        CHECK(val == 11111);
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_ART_CACHE_SIZE_KEY, &val, -1));
        CHECK(val == 64);
    }

    sfallConfigExit();
}

TEST_CASE("P3 RPU parity — FemaleDialogMsgs parse mapping") {
    resetConfigState();

    char dummyArg0[] = "fallout2-ce";
    char* argv[] = { dummyArg0 };
    REQUIRE(sfallConfigInit(1, argv));

    SUBCASE("global defaults to 0 (normal dialog dirs)") {
        // configRead fails in the test environment (compat_fopen → nullptr),
        // so the global reflects the configSetInt default — the same path the
        // production parse loop reads from (sfallConfigInit global-read block).
        CHECK(gFemaleDialogMsgs == 0);
    }

    SUBCASE("config key stores the value the production parse loop reads") {
        // Same pattern as the ProcessorIdle/BoxBarColours mapping test: the
        // global-read block uses configGetInt on this exact key, so a value
        // written here would be picked up on the next sfallConfigInit.
        configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_FEMALE_DIALOG_MSGS_KEY, 2);

        int val = -1;
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_FEMALE_DIALOG_MSGS_KEY, &val, -1));
        CHECK(val == 2);
    }

    sfallConfigExit();
}

TEST_CASE("P2 — worldmap trio (WorldMapTimeMod/EncounterFix/EncounterRate) parse mapping") {
    resetConfigState();

    char dummyArg0[] = "fallout2-ce";
    char* argv[] = { dummyArg0 };
    REQUIRE(sfallConfigInit(1, argv));

    SUBCASE("globals are populated from the config defaults") {
        // configRead fails in the test environment (compat_fopen → nullptr),
        // so the globals reflect the configSetInt defaults — the same path
        // the production parse loop reads from.
        CHECK(gWorldMapTimeMod == 100);
        CHECK_FALSE(gWorldMapEncounterFix);
        CHECK(gWorldMapEncounterRate == 5);
    }

    SUBCASE("config keys store the values the production parse loop reads") {
        configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_WORLDMAP_TIME_MOD_KEY, 50);
        configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_WORLDMAP_ENCOUNTER_FIX_KEY, 1);
        configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_WORLDMAP_ENCOUNTER_RATE_KEY, 30);

        int val = -1;
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_WORLDMAP_TIME_MOD_KEY, &val, -1));
        CHECK(val == 50);
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_WORLDMAP_ENCOUNTER_FIX_KEY, &val, -1));
        CHECK(val == 1);
        CHECK(configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY,
            SFALL_CONFIG_WORLDMAP_ENCOUNTER_RATE_KEY, &val, -1));
        CHECK(val == 30);
    }

    sfallConfigExit();
}

TEST_CASE("sfallArtCacheSizeMb — art cache override selection") {
    resetConfigState();

    char dummyArg0[] = "fallout2-ce";
    char* argv[] = { dummyArg0 };
    REQUIRE(sfallConfigInit(1, argv));

    SUBCASE("without override returns the engine setting, clamped") {
        gOverrideArtCacheSize = false;
        gSfallArtCacheSize = 200; // ignored when override is off
        CHECK(sfallArtCacheSizeMb(32) == 32);
        CHECK(sfallArtCacheSizeMb(1) == SFALL_CONFIG_ART_CACHE_SIZE_MIN);
        CHECK(sfallArtCacheSizeMb(1000) == SFALL_CONFIG_ART_CACHE_SIZE_MAX);
    }

    SUBCASE("with override returns the sfall size, clamped") {
        gOverrideArtCacheSize = true;
        gSfallArtCacheSize = 64;
        CHECK(sfallArtCacheSizeMb(32) == 64);

        gSfallArtCacheSize = 1;
        CHECK(sfallArtCacheSizeMb(32) == SFALL_CONFIG_ART_CACHE_SIZE_MIN);

        gSfallArtCacheSize = 1000;
        CHECK(sfallArtCacheSizeMb(32) == SFALL_CONFIG_ART_CACHE_SIZE_MAX);
    }

    sfallConfigExit();
}
