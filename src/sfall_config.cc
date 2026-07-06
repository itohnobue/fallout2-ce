#include "sfall_config.h"

#include "platform_compat.h"
#include <stdio.h>
#include <string.h>

namespace fallout {

bool gSfallConfigInitialized = false;
Config gSfallConfig;

bool gFallout1Behavior = false;

// TODO: The following 5 config booleans are parsed from ddraw.ini but have
// zero consumers in the engine. They need to be wired to their respective
// feature implementations or removed if the features are abandoned.
// See F-012 in the audit synthesis report for full details.
bool gAllowUnsafeScripting = false;        // TODO: wire to scripting safety gates
bool gEnableHeroAppearanceMod = false;     // TODO: wire to hero appearance FID pipeline
bool gUseFileSystemOverride = false;       // TODO: wire to asset loading path selection
bool gOverrideArtCacheSize = false;        // TODO: wire to art cache size configuration
bool gExtraSaveSlots = false;              // TODO: wire to save slot count

bool sfallConfigInit(int argc, char** argv)
{
    if (gSfallConfigInitialized) {
        return false;
    }

    if (!configInit(&gSfallConfig)) {
        return false;
    }

    // Initialize defaults.
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_OVERRIDE_CRITICALS_MODE_KEY, 2);
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_OVERRIDE_CRITICALS_FILE_KEY, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BOOKS_FILE_KEY, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_ELEVATORS_FILE_KEY, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_CONFIG_FILE, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PATCH_FILE, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_SCRIPTS_KEY, SFALL_CONFIG_INI_CONFIG_FOLDER, "");

    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_FALLOUT1_BEHAVIOR_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_ENABLE_HERO_APPEARANCE_MOD_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_USE_FILESYSTEM_OVERRIDE_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_OVERRIDE_ART_CACHE_SIZE_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_EXTRA_SAVE_SLOTS_KEY, 0);

    configSetInt(&gSfallConfig, SFALL_CONFIG_DEBUGGING_KEY, SFALL_CONFIG_ALLOW_UNSAFE_SCRIPTING_KEY, 0);

    char path[COMPAT_MAX_PATH];
    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    compat_splitpath(argv[0], drive, dir, nullptr, nullptr);
    compat_makepath(path, drive, dir, SFALL_CONFIG_FILE_NAME, nullptr);

    configRead(&gSfallConfig, path, false);

    // Read config values into globals.
    int tempVal = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_FALLOUT1_BEHAVIOR_KEY, &tempVal, 0);
    gFallout1Behavior = tempVal != 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_DEBUGGING_KEY, SFALL_CONFIG_ALLOW_UNSAFE_SCRIPTING_KEY, &tempVal, 0);
    gAllowUnsafeScripting = tempVal != 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_ENABLE_HERO_APPEARANCE_MOD_KEY, &tempVal, 0);
    gEnableHeroAppearanceMod = tempVal != 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_USE_FILESYSTEM_OVERRIDE_KEY, &tempVal, 0);
    gUseFileSystemOverride = tempVal != 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_OVERRIDE_ART_CACHE_SIZE_KEY, &tempVal, 0);
    gOverrideArtCacheSize = tempVal != 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_EXTRA_SAVE_SLOTS_KEY, &tempVal, 0);
    gExtraSaveSlots = tempVal != 0;

    gSfallConfigInitialized = true;

    return true;
}

void sfallConfigExit()
{
    if (gSfallConfigInitialized) {
        configFree(&gSfallConfig);
        gSfallConfigInitialized = false;
    }
}

} // namespace fallout
