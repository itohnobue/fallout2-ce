#include "content_config.h"

#include <cstring> // for strcmp
#include <iterator>

#include "debug.h"
#include "game_config_migration.h"
#include "platform_compat.h"

namespace fallout {

Config gContentConfig;

constexpr char kConfigPath[] = R"(config\game.cfg)";
constexpr char kConfigPatchPath[] = R"(config\game#patch.cfg)";

void contentConfigInit()
{
    if (gContentConfig.isInitialized()) {
        return;
    }

    // Try to migrate some settings from sfall.
    contentConfigTryMigrateFromSfall(kConfigPatchPath);

    if (!configInit(&gContentConfig)) {
        debugPrint("\nCONTENT CONFIG: Failed to initialize content config dictionary!\n");
        return;
    }

    configRead(&gContentConfig, kConfigPath, true);
    // Patch config allows to override only certain fields, without replacing the whole file.
    // TODO: remove this after config patching by mods is implemented inside configRead
    configRead(&gContentConfig, kConfigPatchPath, true);
}

void contentConfigExit()
{
    if (!gContentConfig.isInitialized()) return;

    configFree(&gContentConfig);
}

struct SfallContentMapping {
    const char* sfallSection;
    const char* sfallKey;
    const char* targetSection;
    const char* targetKey;
};

// Maps ddraw.ini keys to their migrated gContentConfig equivalents.
// Covers the same migration entries as kSfallMigrationEntries in
// game_config_migration.cc, allowing scripts using get_ini_setting
// to read values that have been migrated to game.cfg.
// SYNC: static_assert verifies count matches kSfallMigrationEntryCount
// in game_config_migration.h.
static const SfallContentMapping kSfallContentMappings[] = {
    { "Misc", "StartingMap", CONTENT_CONFIG_START_SECTION, "map" },
    { "Misc", "MaleStartModel", CONTENT_CONFIG_START_SECTION, "model_male" },
    { "Misc", "MaleDefaultModel", CONTENT_CONFIG_START_SECTION, "model_male_default" },
    { "Misc", "FemaleStartModel", CONTENT_CONFIG_START_SECTION, "model_female" },
    { "Misc", "FemaleDefaultModel", CONTENT_CONFIG_START_SECTION, "model_female_default" },
    { "Misc", "PipBoyAvailableAtGameStart", CONTENT_CONFIG_START_SECTION, "pipboy" },
    { "Misc", "Fallout1Behavior", CONTENT_CONFIG_START_SECTION, "fallout1_behavior" },
    // F-04: UseFileSystemOverride migration — see game_config_migration.cc for rationale.
    { "Misc", "UseFileSystemOverride", CONTENT_CONFIG_START_SECTION, "use_filesystem_override" },
    { "Misc", "KarmaFRMs", CONTENT_CONFIG_KARMA_SECTION, "frms" },
    { "Misc", "KarmaPoints", CONTENT_CONFIG_KARMA_SECTION, "points" },
    { "Misc", "DialogueFix", CONTENT_CONFIG_DIALOG_SECTION, "no_exit_hotkey" },
    { "Misc", "DialogGenderWords", CONTENT_CONFIG_DIALOG_SECTION, "gender_words" },
    { "Misc", "BoostScriptDialogLimit", CONTENT_CONFIG_DIALOG_SECTION, "boost_dialog_limit" },
    { "Misc", "VersionString", CONTENT_CONFIG_MAIN_MENU_SECTION, "version_string" },
    { "Misc", "MainMenuFontColour", CONTENT_CONFIG_MAIN_MENU_SECTION, "font_color" },
    { "Misc", "MainMenuBigFontColour", CONTENT_CONFIG_MAIN_MENU_SECTION, "big_font_color" },
    { "Misc", "MainMenuOffsetX", CONTENT_CONFIG_MAIN_MENU_SECTION, "offset_x" },
    { "Misc", "MainMenuOffsetY", CONTENT_CONFIG_MAIN_MENU_SECTION, "offset_y" },
    { "Misc", "MainMenuCreditsOffsetX", CONTENT_CONFIG_MAIN_MENU_SECTION, "credits_offset_x" },
    { "Misc", "MainMenuCreditsOffsetY", CONTENT_CONFIG_MAIN_MENU_SECTION, "credits_offset_y" },
    { "Misc", "MovieTimer_artimer1", CONTENT_CONFIG_MOVIES_SECTION, "artimer1" },
    { "Misc", "MovieTimer_artimer2", CONTENT_CONFIG_MOVIES_SECTION, "artimer2" },
    { "Misc", "MovieTimer_artimer3", CONTENT_CONFIG_MOVIES_SECTION, "artimer3" },
    { "Misc", "MovieTimer_artimer4", CONTENT_CONFIG_MOVIES_SECTION, "artimer4" },
    { "Misc", "DamageFormula", CONTENT_CONFIG_COMBAT_SECTION, "damage_formula" },
    { "Misc", "BonusHtHDamageFix", CONTENT_CONFIG_COMBAT_SECTION, "bonus_hth_damage_fix" },
    { "Misc", "RemoveCriticalTimelimits", CONTENT_CONFIG_COMBAT_SECTION, "remove_critical_time_limits" },
    { "Misc", "ScienceOnCritters", CONTENT_CONFIG_COMBAT_SECTION, "science_on_critters" },
    { "Misc", "CheckWeaponAmmoCost", CONTENT_CONFIG_COMBAT_SECTION, "check_weapon_ammo_cost" },
    { "Misc", "ComputeSprayMod", CONTENT_CONFIG_COMBAT_SECTION, "burst_enabled" },
    { "Misc", "ComputeSpray_CenterMult", CONTENT_CONFIG_COMBAT_SECTION, "burst_center_mult" },
    { "Misc", "ComputeSpray_CenterDiv", CONTENT_CONFIG_COMBAT_SECTION, "burst_center_div" },
    { "Misc", "ComputeSpray_TargetMult", CONTENT_CONFIG_COMBAT_SECTION, "burst_target_mult" },
    { "Misc", "ComputeSpray_TargetDiv", CONTENT_CONFIG_COMBAT_SECTION, "burst_target_div" },
    { "Misc", "ExplosionsEmitLight", CONTENT_CONFIG_EXPLOSIONS_SECTION, "emit_light" },
    { "Misc", "Dynamite_DmgMax", CONTENT_CONFIG_EXPLOSIONS_SECTION, "dynamite_max" },
    { "Misc", "Dynamite_DmgMin", CONTENT_CONFIG_EXPLOSIONS_SECTION, "dynamite_min" },
    { "Misc", "PlasticExplosive_DmgMax", CONTENT_CONFIG_EXPLOSIONS_SECTION, "plastic_explosive_max" },
    { "Misc", "PlasticExplosive_DmgMin", CONTENT_CONFIG_EXPLOSIONS_SECTION, "plastic_explosive_min" },
    { "Misc", "Lockpick", CONTENT_CONFIG_SKILLDEX_SECTION, "lockpick" },
    { "Misc", "Steal", CONTENT_CONFIG_SKILLDEX_SECTION, "steal" },
    { "Misc", "Traps", CONTENT_CONFIG_SKILLDEX_SECTION, "traps" },
    { "Misc", "FirstAid", CONTENT_CONFIG_SKILLDEX_SECTION, "first_aid" },
    { "Misc", "Doctor", CONTENT_CONFIG_SKILLDEX_SECTION, "doctor" },
    { "Misc", "Science", CONTENT_CONFIG_SKILLDEX_SECTION, "science" },
    { "Misc", "Repair", CONTENT_CONFIG_SKILLDEX_SECTION, "repair" },
    { "Misc", "TownMapHotkeysFix", CONTENT_CONFIG_WORLDMAP_SECTION, "town_map_hotkeys_fix" },
    { "Misc", "DisableHorrigan", CONTENT_CONFIG_WORLDMAP_SECTION, "disable_horrigan" },
    { "Misc", "CityRepsList", CONTENT_CONFIG_WORLDMAP_SECTION, "city_reputation_list" },
    { "Interface", "WorldMapTravelMarkers", CONTENT_CONFIG_WORLDMAP_SECTION, "trail_markers" },
    { "Misc", "StartXPos", CONTENT_CONFIG_WORLDMAP_SECTION, "start_x_pos" },
    { "Misc", "StartYPos", CONTENT_CONFIG_WORLDMAP_SECTION, "start_y_pos" },
    { "Misc", "ViewXPos", CONTENT_CONFIG_WORLDMAP_SECTION, "view_x_pos" },
    { "Misc", "ViewYPos", CONTENT_CONFIG_WORLDMAP_SECTION, "view_y_pos" },
    { "Misc", "WorldMapSlots", CONTENT_CONFIG_WORLDMAP_SECTION, "encounter_slots" },
    { "Misc", "ElevatorsFile", CONTENT_CONFIG_WORLDMAP_SECTION, "elevators_file" },
    { "Misc", "PremadePaths", CONTENT_CONFIG_CHARACTERS_SECTION, "premade_paths" },
    { "Misc", "PremadeFIDs", CONTENT_CONFIG_CHARACTERS_SECTION, "premade_fids" },
    { "Misc", "ExtraGameMsgFileList", CONTENT_CONFIG_TEXT_SECTION, "extra_msg_file_list" },
};

// SYNC WARNING: kSfallContentMappings MUST be kept synchronized with
// kSfallMigrationEntries in game_config_migration.cc (same ddraw.ini
// keys covering the same target sections). When adding or removing
// entries here, update kSfallMigrationEntries and kSfallMigrationEntryCount
// in game_config_migration.h to match.
static_assert(std::size(kSfallContentMappings) == kSfallMigrationEntryCount,
    "kSfallContentMappings entry count does not match kSfallMigrationEntryCount; "
    "update BOTH tables in content_config.cc and game_config_migration.cc");

static const SfallContentMapping* findSfallContentMapping(const char* section, const char* key)
{
    for (const auto& entry : kSfallContentMappings) {
        if (compat_stricmp(entry.sfallSection, section) == 0
            && compat_stricmp(entry.sfallKey, key) == 0) {
            return &entry;
        }
    }
    return nullptr;
}

int contentConfigLookupSfallInt(const char* section, const char* key)
{
    if (!gContentConfig.isInitialized()) {
        return -1;
    }

    const SfallContentMapping* mapping = findSfallContentMapping(section, key);
    if (mapping == nullptr) {
        return -1;
    }

    int value = 0;
    if (configGetInt(&gContentConfig, mapping->targetSection, mapping->targetKey, &value)) {
        return value;
    }
    return -1;
}

const char* contentConfigLookupSfallString(const char* section, const char* key)
{
    // F-042: This function is part of the public API (declared in content_config.h)
    // but is not yet wired to any callers. It mirrors contentConfigLookupSfallInt()
    // and exists to provide string-type config lookups for mod scripts that need
    // to read migrated ddraw.ini string values from game.cfg. No callers exist
    // yet because all current migrated sfall keys are integer-valued. When a
    // string-type key is added to the migration mapping, this function provides
    // the ready lookup bridge without requiring an API change.
    if (!gContentConfig.isInitialized()) {
        return nullptr;
    }

    const SfallContentMapping* mapping = findSfallContentMapping(section, key);
    if (mapping == nullptr) {
        return nullptr;
    }

    char* value = nullptr;
    if (configGetString(&gContentConfig, mapping->targetSection, mapping->targetKey, &value)) {
        return value;
    }
    return nullptr;
}

} // namespace fallout