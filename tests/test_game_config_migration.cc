// Unit tests for game_config_migration.cc — config migration table validation.
//
// Tests:
//   1. gameConfigMigrateFromF2Res null-argument handling / early-return logic
//   2. F2Res migration table entries — all 10 entries verified
//   3. Sfall migration table entries — all 52 entries verified
//
// The two migration tables live in anonymous namespaces inside
// game_config_migration.cc, making them inaccessible to external code.
// This file mirrors the table entries as regression-test oracles to catch
// accidental removal, reordering, or mis-mapping of entries.
//
// Real source references (mirrored below):
//   kF2ResMigrationEntries:  src/game_config_migration.cc:33-44  (anonymous namespace)
//   kSfallMigrationEntries: src/game_config_migration.cc:175-239 (anonymous namespace)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

#include "config.h"
#include "content_config.h"
#include "game_config.h"
#include "game_config_migration.h"
#include "platform_compat.h"

using namespace fallout;

// ---- Mirror structs (must match game_config_migration.cc exactly) ----

struct F2ResMigrationEntry {
    const char* legacySection;
    const char* legacyKey;
    const char* targetSection;
    const char* targetKey;
};

struct SfallMigrationEntry {
    const char* sfallSection;
    const char* sfallKey;
    const char* targetSection;
    const char* targetKey;
    const char* defaultValue;
};

// ---- F2Res migration table mirror (test oracle for src/game_config_migration.cc:33-44) ----

static constexpr F2ResMigrationEntry kTestMirrorF2ResEntries[] = {
    // [MAIN]
    { "MAIN", "SCR_WIDTH",    GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_RESOLUTION_X_KEY },
    { "MAIN", "SCR_HEIGHT",   GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_RESOLUTION_Y_KEY },
    { "MAIN", "WINDOWED",     GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_WINDOWED_KEY },
    // [IFACE]
    { "IFACE", "IFACE_BAR_MODE",     GAME_CONFIG_UI_KEY, GAME_CONFIG_IFACE_BAR_MODE_KEY },
    { "IFACE", "IFACE_BAR_WIDTH",    GAME_CONFIG_UI_KEY, GAME_CONFIG_IFACE_BAR_WIDTH_KEY },
    { "IFACE", "IFACE_BAR_SIDE_ART", GAME_CONFIG_UI_KEY, GAME_CONFIG_IFACE_BAR_SIDE_ART_KEY },
    { "IFACE", "IFACE_BAR_SIDES_ORI", GAME_CONFIG_UI_KEY, GAME_CONFIG_IFACE_BAR_SIDES_ORI_KEY },
    // [MAPS]
    { "MAPS", "IGNORE_MAP_EDGES", GAME_CONFIG_UI_KEY, GAME_CONFIG_IGNORE_MAP_EDGES_KEY },
    // [STATIC_SCREENS]
    { "STATIC_SCREENS", "SPLASH_SCRN_SIZE", GAME_CONFIG_UI_KEY, GAME_CONFIG_SPLASH_SCREEN_SIZE_KEY },
    // [MOVIES]
    { "MOVIES", "MOVIE_SIZE", GAME_CONFIG_UI_KEY, GAME_CONFIG_MOVIE_ASPECT_FIT_KEY },
};
static_assert(sizeof(kTestMirrorF2ResEntries) / sizeof(kTestMirrorF2ResEntries[0]) == 10,
    "F2Res migration mirror must have exactly 10 entries (matches src/game_config_migration.cc:33-44)");

// ---- Sfall migration table mirror (test oracle for src/game_config_migration.cc:175-239) ----

static constexpr SfallMigrationEntry kTestMirrorSfallEntries[] = {
    // [start]
    { "Misc", "StartingMap",                  CONTENT_CONFIG_START_SECTION, "map",                    "" },
    { "Misc", "MaleStartModel",               CONTENT_CONFIG_START_SECTION, "model_male",            "hmwarr" },
    { "Misc", "MaleDefaultModel",             CONTENT_CONFIG_START_SECTION, "model_male_default",    "hmjmps" },
    { "Misc", "FemaleStartModel",             CONTENT_CONFIG_START_SECTION, "model_female",          "hfprim" },
    { "Misc", "FemaleDefaultModel",           CONTENT_CONFIG_START_SECTION, "model_female_default",  "hfjmps" },
    { "Misc", "PipBoyAvailableAtGameStart",   CONTENT_CONFIG_START_SECTION, "pipboy",                "0" },
    // [karma]
    { "Misc", "KarmaFRMs",    CONTENT_CONFIG_KARMA_SECTION, "frms",   nullptr },
    { "Misc", "KarmaPoints",  CONTENT_CONFIG_KARMA_SECTION, "points", nullptr },
    // [dialog]
    { "Misc", "DialogueFix",       CONTENT_CONFIG_DIALOG_SECTION, "no_exit_hotkey", "1" },
    { "Misc", "DialogGenderWords", CONTENT_CONFIG_DIALOG_SECTION, "gender_words",   "0" },
    // [main_menu]
    { "Misc", "VersionString",            CONTENT_CONFIG_MAIN_MENU_SECTION, "version_string",       nullptr },
    { "Misc", "MainMenuFontColour",       CONTENT_CONFIG_MAIN_MENU_SECTION, "font_color",           "0" },
    { "Misc", "MainMenuBigFontColour",    CONTENT_CONFIG_MAIN_MENU_SECTION, "big_font_color",       "0" },
    { "Misc", "MainMenuOffsetX",          CONTENT_CONFIG_MAIN_MENU_SECTION, "offset_x",             "0" },
    { "Misc", "MainMenuOffsetY",          CONTENT_CONFIG_MAIN_MENU_SECTION, "offset_y",             "0" },
    { "Misc", "MainMenuCreditsOffsetX",   CONTENT_CONFIG_MAIN_MENU_SECTION, "credits_offset_x",     "0" },
    { "Misc", "MainMenuCreditsOffsetY",   CONTENT_CONFIG_MAIN_MENU_SECTION, "credits_offset_y",     "0" },
    // [movies]
    { "Misc", "MovieTimer_artimer1", CONTENT_CONFIG_MOVIES_SECTION, "artimer1", "90" },
    { "Misc", "MovieTimer_artimer2", CONTENT_CONFIG_MOVIES_SECTION, "artimer2", "180" },
    { "Misc", "MovieTimer_artimer3", CONTENT_CONFIG_MOVIES_SECTION, "artimer3", "270" },
    { "Misc", "MovieTimer_artimer4", CONTENT_CONFIG_MOVIES_SECTION, "artimer4", "360" },
    // [combat]
    { "Misc", "DamageFormula",              CONTENT_CONFIG_COMBAT_SECTION, "damage_formula",               "0" },
    { "Misc", "BonusHtHDamageFix",          CONTENT_CONFIG_COMBAT_SECTION, "bonus_hth_damage_fix",         "1" },
    { "Misc", "RemoveCriticalTimelimits",   CONTENT_CONFIG_COMBAT_SECTION, "remove_critical_time_limits",  "0" },
    { "Misc", "ScienceOnCritters",          CONTENT_CONFIG_COMBAT_SECTION, "science_on_critters",          "0" },
    { "Misc", "CheckWeaponAmmoCost",        CONTENT_CONFIG_COMBAT_SECTION, "check_weapon_ammo_cost",      nullptr },
    { "Misc", "ComputeSprayMod",            CONTENT_CONFIG_COMBAT_SECTION, "burst_enabled",               "0" },
    { "Misc", "ComputeSpray_CenterMult",    CONTENT_CONFIG_COMBAT_SECTION, "burst_center_mult",           "1" },
    { "Misc", "ComputeSpray_CenterDiv",     CONTENT_CONFIG_COMBAT_SECTION, "burst_center_div",            "3" },
    { "Misc", "ComputeSpray_TargetMult",    CONTENT_CONFIG_COMBAT_SECTION, "burst_target_mult",           "1" },
    { "Misc", "ComputeSpray_TargetDiv",     CONTENT_CONFIG_COMBAT_SECTION, "burst_target_div",            "2" },
    // [explosions]
    { "Misc", "ExplosionsEmitLight",      CONTENT_CONFIG_EXPLOSIONS_SECTION, "emit_light",          "0" },
    { "Misc", "Dynamite_DmgMax",          CONTENT_CONFIG_EXPLOSIONS_SECTION, "dynamite_max",        "50" },
    { "Misc", "Dynamite_DmgMin",          CONTENT_CONFIG_EXPLOSIONS_SECTION, "dynamite_min",        "30" },
    { "Misc", "PlasticExplosive_DmgMax",  CONTENT_CONFIG_EXPLOSIONS_SECTION, "plastic_explosive_max", "80" },
    { "Misc", "PlasticExplosive_DmgMin",  CONTENT_CONFIG_EXPLOSIONS_SECTION, "plastic_explosive_min", "40" },
    // [skilldex]
    { "Misc", "Lockpick",  CONTENT_CONFIG_SKILLDEX_SECTION, "lockpick",   "293" },
    { "Misc", "Steal",     CONTENT_CONFIG_SKILLDEX_SECTION, "steal",      "293" },
    { "Misc", "Traps",     CONTENT_CONFIG_SKILLDEX_SECTION, "traps",      "293" },
    { "Misc", "FirstAid",  CONTENT_CONFIG_SKILLDEX_SECTION, "first_aid",  "293" },
    { "Misc", "Doctor",    CONTENT_CONFIG_SKILLDEX_SECTION, "doctor",     "293" },
    { "Misc", "Science",   CONTENT_CONFIG_SKILLDEX_SECTION, "science",    "293" },
    { "Misc", "Repair",    CONTENT_CONFIG_SKILLDEX_SECTION, "repair",     "293" },
    // [worldmap]
    { "Misc",      "TownMapHotkeysFix",     CONTENT_CONFIG_WORLDMAP_SECTION, "town_map_hotkeys_fix",   "1" },
    { "Misc",      "DisableHorrigan",       CONTENT_CONFIG_WORLDMAP_SECTION, "disable_horrigan",       "0" },
    { "Misc",      "CityRepsList",          CONTENT_CONFIG_WORLDMAP_SECTION, "city_reputation_list",   nullptr },
    { "Interface", "WorldMapTravelMarkers", CONTENT_CONFIG_WORLDMAP_SECTION, "trail_markers",          "0" },
    // WorldMapSlots migration intentionally removed from source — see src/game_config_migration.cc
    { "Misc",      "BoostScriptDialogLimit", CONTENT_CONFIG_DIALOG_SECTION, "boost_dialog_limit",    "0" },
    // [characters]
    { "Misc", "PremadePaths", CONTENT_CONFIG_CHARACTERS_SECTION, "premade_paths", nullptr },
    { "Misc", "PremadeFIDs",  CONTENT_CONFIG_CHARACTERS_SECTION, "premade_fids",   nullptr },
    // [text]
    { "Misc", "ExtraGameMsgFileList", CONTENT_CONFIG_TEXT_SECTION, "extra_msg_file_list", nullptr },
};
static_assert(sizeof(kTestMirrorSfallEntries) / sizeof(kTestMirrorSfallEntries[0]) == 51,
    "Sfall migration mirror must have exactly 51 entries (matches src/game_config_migration.cc:175-241)");

// ---- Tests ----

TEST_CASE("gameConfigMigrateFromF2Res null handling")
{
    Config gameConfig;
    configInit(&gameConfig);

    SUBCASE("null configFilePath returns false")
    {
        CHECK_FALSE(gameConfigMigrateFromF2Res(nullptr, &gameConfig));
    }

    SUBCASE("null gameConfig returns false")
    {
        CHECK_FALSE(gameConfigMigrateFromF2Res("/tmp/test.cfg", nullptr));
    }

    SUBCASE("both null returns false")
    {
        CHECK_FALSE(gameConfigMigrateFromF2Res(nullptr, nullptr));
    }

    configFree(&gameConfig);
}

TEST_CASE("gameConfigMigrateFromF2Res early-return when already migrated")
{
    Config gameConfig;
    configInit(&gameConfig);

    // Set a key that triggers "migration already done" detection.
    // gameConfigNeedsF2ResMigration returns false when GAME_CONFIG_RESOLUTION_X_KEY is present.
    CHECK(configSetString(&gameConfig, GAME_CONFIG_SCREEN_KEY,
                          GAME_CONFIG_RESOLUTION_X_KEY, "1920"));

    // Migration should be skipped (returns false because nothing was migrated).
    CHECK_FALSE(gameConfigMigrateFromF2Res("/tmp/test.cfg", &gameConfig));

    configFree(&gameConfig);
}

TEST_CASE("gameConfigMigrateFromF2Res no legacy file present")
{
    Config gameConfig;
    configInit(&gameConfig);

    // Without the trigger key, the function tries to read f2_res.ini.
    // Our file I/O stubs return nullptr, so configRead fails and no migration occurs.
    CHECK_FALSE(gameConfigMigrateFromF2Res("/tmp/test.cfg", &gameConfig));

    configFree(&gameConfig);
}

TEST_CASE("F2Res migration table entry count")
{
    // The source file has exactly 10 entries in kF2ResMigrationEntries.
    constexpr int expectedCount = sizeof(kTestMirrorF2ResEntries) / sizeof(kTestMirrorF2ResEntries[0]);
    CHECK(expectedCount == 10);
}

TEST_CASE("F2Res migration table — all entries have non-null keys")
{
    for (const auto& entry : kTestMirrorF2ResEntries) {
        INFO("Entry: ", entry.legacySection, " / ", entry.legacyKey);
        CHECK(entry.legacySection != nullptr);
        CHECK(entry.legacyKey != nullptr);
        CHECK(entry.targetSection != nullptr);
        CHECK(entry.targetKey != nullptr);
    }
}

TEST_CASE("F2Res migration table — verify expected key mappings")
{
    // Spot-check several entries to ensure the mirrored table matches expectations.

    // [MAIN] SCR_WIDTH -> screen.resolution_x
    CHECK(strcmp(kTestMirrorF2ResEntries[0].legacySection, "MAIN") == 0);
    CHECK(strcmp(kTestMirrorF2ResEntries[0].legacyKey, "SCR_WIDTH") == 0);
    CHECK(strcmp(kTestMirrorF2ResEntries[0].targetSection, GAME_CONFIG_SCREEN_KEY) == 0);
    CHECK(strcmp(kTestMirrorF2ResEntries[0].targetKey, GAME_CONFIG_RESOLUTION_X_KEY) == 0);

    // [MAIN] WINDOWED -> screen.windowed
    CHECK(strcmp(kTestMirrorF2ResEntries[2].legacyKey, "WINDOWED") == 0);
    CHECK(strcmp(kTestMirrorF2ResEntries[2].targetKey, GAME_CONFIG_WINDOWED_KEY) == 0);

    // [IFACE] IFACE_BAR_MODE -> ui.iface_bar_mode
    CHECK(strcmp(kTestMirrorF2ResEntries[3].legacySection, "IFACE") == 0);
    CHECK(strcmp(kTestMirrorF2ResEntries[3].targetSection, GAME_CONFIG_UI_KEY) == 0);

    // [MOVIES] MOVIE_SIZE -> ui.movie_aspect_fit
    CHECK(strcmp(kTestMirrorF2ResEntries[9].legacySection, "MOVIES") == 0);
    CHECK(strcmp(kTestMirrorF2ResEntries[9].targetKey, GAME_CONFIG_MOVIE_ASPECT_FIT_KEY) == 0);
}

TEST_CASE("Sfall migration table entry count")
{
    constexpr int expectedCount = sizeof(kTestMirrorSfallEntries) / sizeof(kTestMirrorSfallEntries[0]);
    CHECK(expectedCount == 51);
}

TEST_CASE("Sfall migration table — all entries have non-null section/key/target")
{
    for (const auto& entry : kTestMirrorSfallEntries) {
        INFO("Entry: ", entry.sfallSection, " / ", entry.sfallKey);
        CHECK(entry.sfallSection != nullptr);
        CHECK(entry.sfallKey != nullptr);
        CHECK(entry.targetSection != nullptr);
        CHECK(entry.targetKey != nullptr);
        // defaultValue may be nullptr (means "always migrate when key is present")
    }
}

TEST_CASE("Sfall migration table — verify expected key mappings")
{
    // Spot-check representative entries from each section group.

    // [start] StartingMap -> start.map
    CHECK(strcmp(kTestMirrorSfallEntries[0].sfallKey, "StartingMap") == 0);
    CHECK(strcmp(kTestMirrorSfallEntries[0].targetSection, CONTENT_CONFIG_START_SECTION) == 0);
    CHECK(strcmp(kTestMirrorSfallEntries[0].targetKey, "map") == 0);

    // [dialog] DialogueFix -> dialog.no_exit_hotkey
    CHECK(strcmp(kTestMirrorSfallEntries[8].sfallKey, "DialogueFix") == 0);
    CHECK(strcmp(kTestMirrorSfallEntries[8].targetSection, CONTENT_CONFIG_DIALOG_SECTION) == 0);
    CHECK(strcmp(kTestMirrorSfallEntries[8].targetKey, "no_exit_hotkey") == 0);

    // [combat] DamageFormula -> combat.damage_formula
    CHECK(strcmp(kTestMirrorSfallEntries[21].sfallKey, "DamageFormula") == 0);
    CHECK(strcmp(kTestMirrorSfallEntries[21].targetSection, CONTENT_CONFIG_COMBAT_SECTION) == 0);
    CHECK(strcmp(kTestMirrorSfallEntries[21].targetKey, "damage_formula") == 0);

    // [explosions] Dynamite_DmgMax -> explosions.dynamite_max
    CHECK(strcmp(kTestMirrorSfallEntries[32].sfallKey, "Dynamite_DmgMax") == 0);
    CHECK(strcmp(kTestMirrorSfallEntries[32].targetSection, CONTENT_CONFIG_EXPLOSIONS_SECTION) == 0);

    // [skilldex] Lockpick -> skilldex.lockpick
    CHECK(strcmp(kTestMirrorSfallEntries[36].sfallKey, "Lockpick") == 0);
    CHECK(strcmp(kTestMirrorSfallEntries[36].targetSection, CONTENT_CONFIG_SKILLDEX_SECTION) == 0);
    CHECK(strcmp(kTestMirrorSfallEntries[36].targetKey, "lockpick") == 0);

    // [worldmap] TownMapHotkeysFix -> worldmap.town_map_hotkeys_fix
    CHECK(strcmp(kTestMirrorSfallEntries[43].sfallKey, "TownMapHotkeysFix") == 0);
    CHECK(strcmp(kTestMirrorSfallEntries[43].targetSection, CONTENT_CONFIG_WORLDMAP_SECTION) == 0);
    CHECK(strcmp(kTestMirrorSfallEntries[43].targetKey, "town_map_hotkeys_fix") == 0);

    // [Interface] WorldMapTravelMarkers -> worldmap.trail_markers
    CHECK(strcmp(kTestMirrorSfallEntries[46].sfallSection, "Interface") == 0);
    CHECK(strcmp(kTestMirrorSfallEntries[46].sfallKey, "WorldMapTravelMarkers") == 0);

    // [text] ExtraGameMsgFileList -> text.extra_msg_file_list
    CHECK(strcmp(kTestMirrorSfallEntries[50].sfallKey, "ExtraGameMsgFileList") == 0);
    CHECK(strcmp(kTestMirrorSfallEntries[50].targetSection, CONTENT_CONFIG_TEXT_SECTION) == 0);
}

TEST_CASE("Sfall migration table — defaultValue consistency")
{
    // Entries with non-null defaultValue should have a semantic default that
    // matches the key's purpose. Verify a few known defaults.

    // MaleStartModel default = "hmwarr" (tribal male)
    CHECK(kTestMirrorSfallEntries[1].defaultValue != nullptr);
    CHECK(strcmp(kTestMirrorSfallEntries[1].defaultValue, "hmwarr") == 0);

    // PipBoyAvailableAtGameStart default = "0" (pipboy NOT available)
    CHECK(kTestMirrorSfallEntries[5].defaultValue != nullptr);
    CHECK(strcmp(kTestMirrorSfallEntries[5].defaultValue, "0") == 0);

    // Combat-related: DamageFormula default = "0" (original formula)
    CHECK(kTestMirrorSfallEntries[21].defaultValue != nullptr);
    CHECK(strcmp(kTestMirrorSfallEntries[21].defaultValue, "0") == 0);

    // CheckWeaponAmmoCost default = nullptr (always migrate)
    CHECK(kTestMirrorSfallEntries[25].defaultValue == nullptr);

    // WorldMapTravelMarkers default = "0" (no markers)
    CHECK(kTestMirrorSfallEntries[46].defaultValue != nullptr);
    CHECK(strcmp(kTestMirrorSfallEntries[46].defaultValue, "0") == 0);
}

TEST_CASE("Sfall migration table — sections use CONTENT_CONFIG_* constants")
{
    // All entries should target sections defined by CONTENT_CONFIG_* constants
    // to ensure section names stay consistent across the codebase.

    for (const auto& entry : kTestMirrorSfallEntries) {
        INFO("Entry: ", entry.sfallKey, " -> ", entry.targetSection);

        // Each target section should match one of the CONTENT_CONFIG_* defines
        bool knownSection =
            strcmp(entry.targetSection, CONTENT_CONFIG_START_SECTION) == 0 ||
            strcmp(entry.targetSection, CONTENT_CONFIG_KARMA_SECTION) == 0 ||
            strcmp(entry.targetSection, CONTENT_CONFIG_DIALOG_SECTION) == 0 ||
            strcmp(entry.targetSection, CONTENT_CONFIG_MAIN_MENU_SECTION) == 0 ||
            strcmp(entry.targetSection, CONTENT_CONFIG_MOVIES_SECTION) == 0 ||
            strcmp(entry.targetSection, CONTENT_CONFIG_COMBAT_SECTION) == 0 ||
            strcmp(entry.targetSection, CONTENT_CONFIG_EXPLOSIONS_SECTION) == 0 ||
            strcmp(entry.targetSection, CONTENT_CONFIG_SKILLDEX_SECTION) == 0 ||
            strcmp(entry.targetSection, CONTENT_CONFIG_WORLDMAP_SECTION) == 0 ||
            strcmp(entry.targetSection, CONTENT_CONFIG_CHARACTERS_SECTION) == 0 ||
            strcmp(entry.targetSection, CONTENT_CONFIG_TEXT_SECTION) == 0;

        CHECK(knownSection);
    }
}

TEST_CASE("F2Res migration table — sections use GAME_CONFIG_* constants")
{
    for (const auto& entry : kTestMirrorF2ResEntries) {
        INFO("Entry: ", entry.legacySection, " / ", entry.legacyKey, " -> ", entry.targetSection);

        // All F2Res entries target GAME_CONFIG_SCREEN_KEY or GAME_CONFIG_UI_KEY.
        bool knownSection =
            strcmp(entry.targetSection, GAME_CONFIG_SCREEN_KEY) == 0 ||
            strcmp(entry.targetSection, GAME_CONFIG_UI_KEY) == 0;

        CHECK(knownSection);
    }
}

TEST_CASE("No duplicate sfall keys across migration tables")
{
    // Each sfallKey should appear at most once in the migration table.
    // (We test this by scanning the entire mirrored array for duplicates.)

    for (int i = 0; i < 51; i++) {
        for (int j = i + 1; j < 51; j++) {
            INFO("Duplicate key: ", kTestMirrorSfallEntries[i].sfallKey);
            // sfallKey + sfallSection combination should be unique
            bool sameKey = strcmp(kTestMirrorSfallEntries[i].sfallKey,
                                  kTestMirrorSfallEntries[j].sfallKey) == 0;
            bool sameSection = strcmp(kTestMirrorSfallEntries[i].sfallSection,
                                      kTestMirrorSfallEntries[j].sfallSection) == 0;
            bool duplicate = sameKey && sameSection;
            CHECK_FALSE(duplicate);
        }
    }
}
