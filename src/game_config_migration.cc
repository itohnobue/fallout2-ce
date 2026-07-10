#include "game_config_migration.h"

#include <assert.h>
#include <iterator>
#include <stdio.h>
#include <string.h>
#include <string_view>

#include "content_config.h"
#include "debug.h"
#include "game_config.h"
#include "platform_compat.h"
#include "settings.h"
#include "sfall_config.h"

namespace fallout {

namespace {

#define F2_RES_CONFIG_FILE_NAME "f2_res.ini"

    struct F2ResMigrationEntry {
        const char* legacySection;
        const char* legacyKey;
        const char* targetSection;
        const char* targetKey;
    };

    static bool gameConfigHasKey(Config* config, const char* section, const char* key);
    static bool gameConfigNeedsF2ResMigration(Config* gameConfig);
    static bool gameConfigMigrateMainMenuScaleModeKey(Config* legacyConfig, Config* gameConfig);
    static bool gameConfigMigrateStringKey(Config* legacyConfig, Config* gameConfig, const F2ResMigrationEntry& entry);
    static bool gameConfigMigrateScaleKey(Config* legacyConfig, Config* gameConfig);

    static constexpr F2ResMigrationEntry kF2ResMigrationEntries[] = {
        { "MAIN", "SCR_WIDTH", GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_RESOLUTION_X_KEY },
        { "MAIN", "SCR_HEIGHT", GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_RESOLUTION_Y_KEY },
        { "MAIN", "WINDOWED", GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_WINDOWED_KEY },
        { "IFACE", "IFACE_BAR_MODE", GAME_CONFIG_UI_KEY, GAME_CONFIG_IFACE_BAR_MODE_KEY },
        { "IFACE", "IFACE_BAR_WIDTH", GAME_CONFIG_UI_KEY, GAME_CONFIG_IFACE_BAR_WIDTH_KEY },
        { "IFACE", "IFACE_BAR_SIDE_ART", GAME_CONFIG_UI_KEY, GAME_CONFIG_IFACE_BAR_SIDE_ART_KEY },
        { "IFACE", "IFACE_BAR_SIDES_ORI", GAME_CONFIG_UI_KEY, GAME_CONFIG_IFACE_BAR_SIDES_ORI_KEY },
        { "MAPS", "IGNORE_MAP_EDGES", GAME_CONFIG_UI_KEY, GAME_CONFIG_IGNORE_MAP_EDGES_KEY },
        { "STATIC_SCREENS", "SPLASH_SCRN_SIZE", GAME_CONFIG_UI_KEY, GAME_CONFIG_SPLASH_SCREEN_SIZE_KEY },
        { "MOVIES", "MOVIE_SIZE", GAME_CONFIG_UI_KEY, GAME_CONFIG_MOVIE_ASPECT_FIT_KEY },
    };

    static bool gameConfigHasKey(Config* config, const char* section, const char* key)
    {
        char* value;
        return configGetString(config, section, key, &value);
    }

    static bool gameConfigNeedsF2ResMigration(Config* gameConfig)
    {
        assert(gameConfig != nullptr);

        return !gameConfigHasKey(gameConfig, GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_RESOLUTION_X_KEY);
    }

    static bool gameConfigMigrateStringKey(Config* legacyConfig, Config* gameConfig, const F2ResMigrationEntry& entry)
    {
        assert(legacyConfig != nullptr && gameConfig != nullptr);

        if (gameConfigHasKey(gameConfig, entry.targetSection, entry.targetKey)) {
            return false;
        }

        char* value;
        if (!configGetString(legacyConfig, entry.legacySection, entry.legacyKey, &value)) {
            return false;
        }

        return configSetString(gameConfig, entry.targetSection, entry.targetKey, value);
    }

    static bool gameConfigMigrateMainMenuScaleModeKey(Config* legacyConfig, Config* gameConfig)
    {
        assert(legacyConfig != nullptr && gameConfig != nullptr);

        if (gameConfigHasKey(gameConfig, GAME_CONFIG_UI_KEY, GAME_CONFIG_MAIN_MENU_SCALE_MODE_KEY)) {
            return false;
        }

        int scaleMode;
        if (!configGetInt(legacyConfig, "MAINMENU", "MAIN_MENU_SIZE", &scaleMode)) {
            return false;
        }

        bool legacyScaleButtonsAndText = false;
        if (configGetBool(legacyConfig, "MAINMENU", "SCALE_BUTTONS_AND_TEXT_MENU", &legacyScaleButtonsAndText)
            && legacyScaleButtonsAndText
            && scaleMode != 0) {
            scaleMode = 2;
        }

        return configSetInt(gameConfig, GAME_CONFIG_UI_KEY, GAME_CONFIG_MAIN_MENU_SCALE_MODE_KEY, scaleMode);
    }

    static bool gameConfigMigrateScaleKey(Config* legacyConfig, Config* gameConfig)
    {
        assert(legacyConfig != nullptr && gameConfig != nullptr);

        if (gameConfigHasKey(gameConfig, GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_SCALE_KEY)) {
            return false;
        }

        int value;
        if (!configGetInt(legacyConfig, "MAIN", "SCALE_2X", &value)) {
            return false;
        }

        return configSetInt(gameConfig, GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_SCALE_KEY, value + 1);
    }
} // namespace

// Migrate settings F2_RES.INI to fallout2.cfg
//
// Only happens a single time, after which fallout2.cfg is the source of truth
bool gameConfigMigrateFromF2Res(const char* gameConfigFilePath, Config* gameConfig)
{
    if (gameConfigFilePath == nullptr || gameConfig == nullptr) {
        return false;
    }

    if (!gameConfigNeedsF2ResMigration(gameConfig)) {
        return false;
    }

    char f2ResFilePath[COMPAT_MAX_PATH];
    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    compat_splitpath(gameConfigFilePath, drive, dir, nullptr, nullptr);
    compat_makepath(f2ResFilePath, drive, dir, F2_RES_CONFIG_FILE_NAME, nullptr);

    Config legacyConfig;
    if (!configInit(&legacyConfig)) {
        return false;
    }

    bool migrated = false;
    if (configRead(&legacyConfig, f2ResFilePath, false)) {
        for (const auto& entry : kF2ResMigrationEntries) {
            if (gameConfigMigrateStringKey(&legacyConfig, gameConfig, entry)) {
                migrated = true;
            }
        }

        if (gameConfigMigrateMainMenuScaleModeKey(&legacyConfig, gameConfig)) {
            migrated = true;
        }

        if (gameConfigMigrateScaleKey(&legacyConfig, gameConfig)) {
            migrated = true;
        }
    }

    configFree(&legacyConfig);
    return migrated;
}

namespace {

    constexpr char kSfallMisc[] = "Misc";
    constexpr char kSfallInterface[] = "Interface";

    struct SfallMigrationEntry {
        const char* sfallSection;
        const char* sfallKey;
        const char* targetSection;
        const char* targetKey;
        // If the sfall value matches this string, skip migration (value is already the default).
        // nullptr means always migrate when the key is present.
        const char* defaultValue;
    };

    constexpr SfallMigrationEntry kSfallMigrationEntries[] = {
        // [start]
        { kSfallMisc, "StartingMap", CONTENT_CONFIG_START_SECTION, "map", "" },
        { kSfallMisc, "MaleStartModel", CONTENT_CONFIG_START_SECTION, "model_male", "hmwarr" },
        { kSfallMisc, "MaleDefaultModel", CONTENT_CONFIG_START_SECTION, "model_male_default", "hmjmps" },
        { kSfallMisc, "FemaleStartModel", CONTENT_CONFIG_START_SECTION, "model_female", "hfprim" },
        { kSfallMisc, "FemaleDefaultModel", CONTENT_CONFIG_START_SECTION, "model_female_default", "hfjmps" },
        { kSfallMisc, "PipBoyAvailableAtGameStart", CONTENT_CONFIG_START_SECTION, "pipboy", "0" },
        { kSfallMisc, "Fallout1Behavior", CONTENT_CONFIG_START_SECTION, "fallout1_behavior", "0" },
        // F-04: UseFileSystemOverride migration — eliminates first-launch RPU friction.
        // The feature itself is intentionally unwired in CE (VFS handles it),
        // but the migration entry prevents RPU scripts from incorrectly detecting
        // a missing config (they check != 1, and absence returns 0/false).
        { kSfallMisc, "UseFileSystemOverride", CONTENT_CONFIG_START_SECTION, "use_filesystem_override", "0" },
        // [karma]
        { kSfallMisc, "KarmaFRMs", CONTENT_CONFIG_KARMA_SECTION, "frms" },
        { kSfallMisc, "KarmaPoints", CONTENT_CONFIG_KARMA_SECTION, "points" },
        // [dialog]
        { kSfallMisc, "DialogueFix", CONTENT_CONFIG_DIALOG_SECTION, "no_exit_hotkey", "1" },
        { kSfallMisc, "DialogGenderWords", CONTENT_CONFIG_DIALOG_SECTION, "gender_words", "0" },
        // [main_menu]
        { kSfallMisc, "VersionString", CONTENT_CONFIG_MAIN_MENU_SECTION, "version_string" },
        { kSfallMisc, "MainMenuFontColour", CONTENT_CONFIG_MAIN_MENU_SECTION, "font_color", "0" },
        { kSfallMisc, "MainMenuBigFontColour", CONTENT_CONFIG_MAIN_MENU_SECTION, "big_font_color", "0" },
        { kSfallMisc, "MainMenuOffsetX", CONTENT_CONFIG_MAIN_MENU_SECTION, "offset_x", "0" },
        { kSfallMisc, "MainMenuOffsetY", CONTENT_CONFIG_MAIN_MENU_SECTION, "offset_y", "0" },
        { kSfallMisc, "MainMenuCreditsOffsetX", CONTENT_CONFIG_MAIN_MENU_SECTION, "credits_offset_x", "0" },
        { kSfallMisc, "MainMenuCreditsOffsetY", CONTENT_CONFIG_MAIN_MENU_SECTION, "credits_offset_y", "0" },
        // [movies]
        { kSfallMisc, "MovieTimer_artimer1", CONTENT_CONFIG_MOVIES_SECTION, "artimer1", "90" },
        { kSfallMisc, "MovieTimer_artimer2", CONTENT_CONFIG_MOVIES_SECTION, "artimer2", "180" },
        { kSfallMisc, "MovieTimer_artimer3", CONTENT_CONFIG_MOVIES_SECTION, "artimer3", "270" },
        { kSfallMisc, "MovieTimer_artimer4", CONTENT_CONFIG_MOVIES_SECTION, "artimer4", "360" },
        // [combat]
        { kSfallMisc, "DamageFormula", CONTENT_CONFIG_COMBAT_SECTION, "damage_formula", "0" },
        { kSfallMisc, "BonusHtHDamageFix", CONTENT_CONFIG_COMBAT_SECTION, "bonus_hth_damage_fix", "1" },
        { kSfallMisc, "RemoveCriticalTimelimits", CONTENT_CONFIG_COMBAT_SECTION, "remove_critical_time_limits", "0" },
        { kSfallMisc, "ScienceOnCritters", CONTENT_CONFIG_COMBAT_SECTION, "science_on_critters", "0" },
        { kSfallMisc, "CheckWeaponAmmoCost", CONTENT_CONFIG_COMBAT_SECTION, "check_weapon_ammo_cost", nullptr },
        { kSfallMisc, "ComputeSprayMod", CONTENT_CONFIG_COMBAT_SECTION, "burst_enabled", "0" },
        { kSfallMisc, "ComputeSpray_CenterMult", CONTENT_CONFIG_COMBAT_SECTION, "burst_center_mult", "1" },
        { kSfallMisc, "ComputeSpray_CenterDiv", CONTENT_CONFIG_COMBAT_SECTION, "burst_center_div", "3" },
        { kSfallMisc, "ComputeSpray_TargetMult", CONTENT_CONFIG_COMBAT_SECTION, "burst_target_mult", "1" },
        { kSfallMisc, "ComputeSpray_TargetDiv", CONTENT_CONFIG_COMBAT_SECTION, "burst_target_div", "2" },
        // [explosions]
        { kSfallMisc, "ExplosionsEmitLight", CONTENT_CONFIG_EXPLOSIONS_SECTION, "emit_light", "0" },
        { kSfallMisc, "Dynamite_DmgMax", CONTENT_CONFIG_EXPLOSIONS_SECTION, "dynamite_max", "50" },
        { kSfallMisc, "Dynamite_DmgMin", CONTENT_CONFIG_EXPLOSIONS_SECTION, "dynamite_min", "30" },
        { kSfallMisc, "PlasticExplosive_DmgMax", CONTENT_CONFIG_EXPLOSIONS_SECTION, "plastic_explosive_max", "80" },
        { kSfallMisc, "PlasticExplosive_DmgMin", CONTENT_CONFIG_EXPLOSIONS_SECTION, "plastic_explosive_min", "40" },
        // [skilldex]
        { kSfallMisc, "Lockpick", CONTENT_CONFIG_SKILLDEX_SECTION, "lockpick", "293" },
        { kSfallMisc, "Steal", CONTENT_CONFIG_SKILLDEX_SECTION, "steal", "293" },
        { kSfallMisc, "Traps", CONTENT_CONFIG_SKILLDEX_SECTION, "traps", "293" },
        { kSfallMisc, "FirstAid", CONTENT_CONFIG_SKILLDEX_SECTION, "first_aid", "293" },
        { kSfallMisc, "Doctor", CONTENT_CONFIG_SKILLDEX_SECTION, "doctor", "293" },
        { kSfallMisc, "Science", CONTENT_CONFIG_SKILLDEX_SECTION, "science", "293" },
        { kSfallMisc, "Repair", CONTENT_CONFIG_SKILLDEX_SECTION, "repair", "293" },
        // [worldmap]
        { kSfallMisc, "TownMapHotkeysFix", CONTENT_CONFIG_WORLDMAP_SECTION, "town_map_hotkeys_fix", "1" },
        { kSfallMisc, "DisableHorrigan", CONTENT_CONFIG_WORLDMAP_SECTION, "disable_horrigan", "0" },
        { kSfallMisc, "CityRepsList", CONTENT_CONFIG_WORLDMAP_SECTION, "city_reputation_list" },
        { kSfallInterface, "WorldMapTravelMarkers", CONTENT_CONFIG_WORLDMAP_SECTION, "trail_markers", "0" },
        // FO1 worldmap position keys — migrated from ddraw.ini [Misc] to
        // content_config worldmap section for et tu compatibility.
        { kSfallMisc, "StartXPos", CONTENT_CONFIG_WORLDMAP_SECTION, "start_x_pos" },
        { kSfallMisc, "StartYPos", CONTENT_CONFIG_WORLDMAP_SECTION, "start_y_pos" },
        { kSfallMisc, "ViewXPos", CONTENT_CONFIG_WORLDMAP_SECTION, "view_x_pos" },
        { kSfallMisc, "ViewYPos", CONTENT_CONFIG_WORLDMAP_SECTION, "view_y_pos" },
        // WorldMapSlots — migrated to worldmap encounter_slots. Scripts
        // can access this value via get_ini_setting and the config bridge
        // (contentConfigLookupSfallInt) when ddraw.ini is not present.
        { kSfallMisc, "WorldMapSlots", CONTENT_CONFIG_WORLDMAP_SECTION, "encounter_slots" },
        // ElevatorsFile — migrated to worldmap elevators_file. The
        // elevator system reads this from gSfallConfig; the migration
        // preserves the value for use via the config bridge and future
        // consumers that read from gContentConfig.
        { kSfallMisc, "ElevatorsFile", CONTENT_CONFIG_WORLDMAP_SECTION, "elevators_file" },

        // BoxBarCount migration intentionally removed — `add_iface_tag` metarule provides
        // equivalent functionality and 5 pre-allocated tag slots match the sfall baseline.
        // There is no engine-level consumer for BoxBarCount in CE.

        { kSfallMisc, "BoostScriptDialogLimit", CONTENT_CONFIG_DIALOG_SECTION, "boost_dialog_limit", "0" },
        // [characters]
        { kSfallMisc, "PremadePaths", CONTENT_CONFIG_CHARACTERS_SECTION, "premade_paths" },
        { kSfallMisc, "PremadeFIDs", CONTENT_CONFIG_CHARACTERS_SECTION, "premade_fids" },
        // [text]
    { kSfallMisc, "ExtraGameMsgFileList", CONTENT_CONFIG_TEXT_SECTION, "extra_msg_file_list" },
};

// SYNC WARNING: kSfallMigrationEntries MUST be kept synchronized with
// kSfallContentMappings in content_config.cc (same ddraw.ini keys
// covering the same target sections). When adding or removing entries
// here, update kSfallContentMappings and kSfallMigrationEntryCount
// in game_config_migration.h to match.
static_assert(std::size(kSfallMigrationEntries) == kSfallMigrationEntryCount,
    "kSfallMigrationEntries entry count does not match kSfallMigrationEntryCount; "
    "update BOTH tables in game_config_migration.cc and content_config.cc");

// Compile-time verification: kSfallMigrationEntries must contain no duplicate
// (sfallSection, sfallKey) pairs. A duplicate entry means one of the tables
// (migration or content mapping) has a stale copy-paste error that the
// count-only static_assert above cannot detect.
constexpr bool sfallMigrationEntriesNoDuplicates()
{
    for (size_t i = 0; i < std::size(kSfallMigrationEntries); ++i) {
        std::string_view iSection(kSfallMigrationEntries[i].sfallSection);
        std::string_view iKey(kSfallMigrationEntries[i].sfallKey);
        for (size_t j = i + 1; j < std::size(kSfallMigrationEntries); ++j) {
            std::string_view jSection(kSfallMigrationEntries[j].sfallSection);
            std::string_view jKey(kSfallMigrationEntries[j].sfallKey);
            if (iSection == jSection && iKey == jKey) {
                return false;
            }
        }
    }
    return true;
}

static_assert(sfallMigrationEntriesNoDuplicates(),
    "kSfallMigrationEntries contains duplicate (sfallSection, sfallKey) pairs. "
    "Remove the duplicate entry and verify kSfallContentMappings in content_config.cc.");

} // anonymous namespace

// Migrate sfall settings from ddraw.ini to game.cfg.
//
// Re-migrates on each run: loads the existing patch file (if any) and updates
// only the migration entries whose values have changed in ddraw.ini. This
// prevents stale migrated values from overriding user ddraw.ini changes.
// Non-migration keys in the patch file are preserved.
static bool contentConfigMigrateFromSfall(Config* sfallConfig, const char* contentConfigFilePath)
{
    assert(sfallConfig != nullptr && contentConfigFilePath != nullptr);

    Config migratedConfig;
    if (!configInit(&migratedConfig)) {
        return false;
    }

    // Load the existing patch file to preserve non-migration keys and detect
    // which migration entries need updating. Returns false if the file does
    // not exist (first run) — that's fine; we proceed with an empty config.
    configRead(&migratedConfig, contentConfigFilePath, false);

    bool migrated = false;
    // Migrate start year/month/day only when explicitly set (not the sfall -1 sentinel)
    // AND when the value differs from what's already in the patch file.
    auto migrateStartInt = [&](const char* sfallKey, const char* targetKey, int defaultValue) {
        int value;
        if (configGetInt(sfallConfig, "Misc", sfallKey, &value) && value >= 0 && value != defaultValue) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", value);
            char* existingValue;
            if (!configGetString(&migratedConfig, CONTENT_CONFIG_START_SECTION, targetKey, &existingValue)
                || strcmp(existingValue, buf) != 0) {
                configSetString(&migratedConfig, CONTENT_CONFIG_START_SECTION, targetKey, buf);
                migrated = true;
            }
        }
    };
    migrateStartInt("StartYear", "year", 2241);
    migrateStartInt("StartMonth", "month", 6);
    migrateStartInt("StartDay", "day", 24);

    for (const auto& entry : kSfallMigrationEntries) {
        char* value;
        if (configGetString(sfallConfig, entry.sfallSection, entry.sfallKey, &value)) {
            // UH-05: Skip only truly empty sfall values. Do NOT skip values
            // that match entry.defaultValue — a prior migration may have left
            // a non-default value in the content config that needs to be
            // reverted when the user changes ddraw.ini back to the default.
            // The comparison against existingValue below handles that correctly.
            if (value[0] == '\0') {
                continue;
            }
            // Only update if the value in the patch file differs from the
            // current ddraw.ini value. This avoids unnecessary writes and
            // preserves the patch file when nothing has changed.
            char* existingValue;
            if (!configGetString(&migratedConfig, entry.targetSection, entry.targetKey, &existingValue)
                || strcmp(existingValue, value) != 0) {
                configSetString(&migratedConfig, entry.targetSection, entry.targetKey, value);
                migrated = true;
            }
        }
    }

    if (migrated) {
        // Ensure all directory components exist before writing.
        char drive[COMPAT_MAX_DRIVE];
        char dirPart[COMPAT_MAX_DIR];
        char pathWithoutFile[COMPAT_MAX_PATH];
        compat_splitpath(contentConfigFilePath, drive, dirPart, nullptr, nullptr);
        compat_makepath(pathWithoutFile, drive, dirPart, nullptr, nullptr);
        compat_mkdir_recursive(pathWithoutFile);

        if (!configWrite(&migratedConfig, contentConfigFilePath, false)) {
            debugPrint("Failed to write migrated settings to %s!\n", contentConfigFilePath);
            migrated = false;
        }
    }

    configFree(&migratedConfig);
    return migrated;
}

void contentConfigTryMigrateFromSfall(const char* contentConfigPath)
{
    if (!gSfallConfig.isInitialized() || gSfallConfig.entriesLength == 0) {
        debugPrint("Skipping ddraw.ini migration: sfall config not initialized or empty.\n");
        // Nothing to migrate.
        return;
    }
    const auto& masterPatches = settings.system.master_patches_path;
    if (masterPatches.empty()) {
        debugPrint("Failed to migrate from ddraw.ini: no master_patches is set.\n");
        return;
    }
    if (!compat_is_dir(masterPatches.c_str())) {
        // master_patches must point to an existing folder. Don't migrate when it's missing or not a directory.
        debugPrint("Skipping ddraw.ini migration: master_patches \"%s\" is not a directory.\n", masterPatches.c_str());
        return;
    }
    char contentCfgPath[COMPAT_MAX_PATH];
    int pathResult = snprintf(contentCfgPath, sizeof(contentCfgPath), "%s\\%s", masterPatches.c_str(), contentConfigPath);
    if (pathResult < 0 || pathResult >= (int)sizeof(contentCfgPath)) {
        debugPrint("Failed to construct content config path: path too long.\n");
        return;
    }
    if (contentConfigMigrateFromSfall(&gSfallConfig, contentCfgPath)) {
        debugPrint("Migrated settings from ddraw.ini to game.cfg.\n");
    }
}

} // namespace fallout
