#include "sfall_config.h"

#include "debug.h"
#include "platform_compat.h"

#include <algorithm>
#include <stdio.h>
#include <string.h>

namespace fallout {

bool gSfallConfigInitialized = false;
Config gSfallConfig;

bool gFallout1Behavior = false;

// Config globals parsed from ddraw.ini.
//
// Wiring status summary:
//   gAllowUnsafeScripting   — INTENTIONALLY UNWIRED: VOODOO write/call_offset opcodes
//                             are registered unconditionally at sfall_opcodes.cc to
//                             prevent script crashes; flag is parsed but never gates
//                             any behavior.
//   gEnableHeroAppearanceMod — DEAD / UNWIRED (F-17): config flag parsed
//                               by sfall_ini.cc but no CE code gate exists;
//                               hero appearance feature is always-on by design.
//                               The extern global is retained for sfall_ini.cc
//                               backward compatibility but is never consumed.
//   gUseFileSystemOverride   — INTENTIONALLY UNWIRED: VFS priority ordering
//                               provides equivalent override behavior without
//                               this flag (master_patches/ dir > .dat files).
//   gOverrideArtCacheSize     — WIRED: consumed by sfallArtCacheSizeMb()
//                               (art.cc artInit) — when set, the art cache
//                               uses the sfall [Misc] ArtCacheSize value
//                               (gSfallArtCacheSize) instead of
//                               settings.system.art_cache_size.
//   gExtraSaveSlots           — WIRED: consumed at loadsave.cc for save slot
//                               page count (true → 10 pages / false → 1 page).
//   gProcessorIdle            — PARSED, NO ENGINE CHANGE NEEDED: sfall's
//                               "reduce CPU usage by allowing the system to
//                               go idle" is already satisfied by CE's FPS
//                               limiter, which yields the CPU every frame
//                               (SDL_Delay in fps_limiter.cc throttle()).
//   gBoxBarColours            — PARSED, INERT: accepted for ddraw.ini
//                               compatibility; CE has no sfall box-bar colour
//                               rendering equivalent (cosmetic).
//   gSfallArtCacheSize        — WIRED: the [Misc] ArtCacheSize override value
//                               (MB) used when gOverrideArtCacheSize is set.
//   gFemaleDialogMsgs         — WIRED: 0/1/2 mirror of sfall's FemaleDialogMsgs.
//                               Consumed by messageListGetLocalizedDir
//                               (message.cc): a female player loads dialog
//                               messages from dialog_female (>=1) and cutscene
//                               subtitles from cuts_female (>=2), with fallback
//                               to the normal dirs when the female dirs are
//                               absent (English installs ship none — inert).
bool gAllowUnsafeScripting = false;
bool gEnableHeroAppearanceMod = false;
bool gUseFileSystemOverride = false;
bool gOverrideArtCacheSize = false;
bool gExtraSaveSlots = false;
bool gProcessorIdle = false;
int gBoxBarColours = 0;
int gSfallArtCacheSize = SFALL_CONFIG_ART_CACHE_SIZE_DEFAULT;
int gFemaleDialogMsgs = 0;
int gWorldMapTimeMod = 100;
bool gWorldMapEncounterFix = false;
int gWorldMapEncounterRate = 5;

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
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_SKILLS_FILE_KEY, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PERKS_FILE_KEY, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_CONFIG_FILE, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PATCH_FILE, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_SCRIPTS_KEY, SFALL_CONFIG_INI_CONFIG_FOLDER, "");

    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_FALLOUT1_BEHAVIOR_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_USE_FILESYSTEM_OVERRIDE_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_OVERRIDE_ART_CACHE_SIZE_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_EXTRA_SAVE_SLOTS_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PROCESSOR_IDLE_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BOX_BAR_COLOURS_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_ART_CACHE_SIZE_KEY, SFALL_CONFIG_ART_CACHE_SIZE_DEFAULT);
    // P3 RPU parity: FemaleDialogMsgs — sfall default is 0 (normal dirs).
    // The value only matters for non-English RPU translations that ship
    // dialog_female/cuts_female dirs; English installs are unaffected.
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_FEMALE_DIALOG_MSGS_KEY, 0);
    // World map travel-time modifier (percent) and encounter-rate gating.
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_WORLDMAP_TIME_MOD_KEY, 100);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_WORLDMAP_ENCOUNTER_FIX_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_WORLDMAP_ENCOUNTER_RATE_KEY, 5);
    // H-06: WorldMapSlots defaults to 21. RPU's gl_k_modini.ssl checks
    // `get_ini_setting("ddraw.ini|Misc|WorldMapSlots") != 21` and calls
    // signal_end_game (ending the session) on any other value. sfall's own
    // default is 0, which also fails RPU's check — the only value that
    // satisfies RPU is 21. This default flows to scripts through the
    // gSfallConfig fallback tier in op_get_ini_setting (sfall_ini.cc) when
    // ddraw.ini has no WorldMapSlots key.
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_WORLDMAP_SLOTS_KEY, 21);

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
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_USE_FILESYSTEM_OVERRIDE_KEY, &tempVal, 0);
    gUseFileSystemOverride = tempVal != 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_OVERRIDE_ART_CACHE_SIZE_KEY, &tempVal, 0);
    gOverrideArtCacheSize = tempVal != 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_EXTRA_SAVE_SLOTS_KEY, &tempVal, 0);
    gExtraSaveSlots = tempVal != 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PROCESSOR_IDLE_KEY, &tempVal, 0);
    gProcessorIdle = tempVal != 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BOX_BAR_COLOURS_KEY, &tempVal, 0);
    gBoxBarColours = tempVal;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_ART_CACHE_SIZE_KEY, &tempVal, SFALL_CONFIG_ART_CACHE_SIZE_DEFAULT);
    gSfallArtCacheSize = tempVal;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_FEMALE_DIALOG_MSGS_KEY, &tempVal, 0);
    gFemaleDialogMsgs = tempVal;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_WORLDMAP_TIME_MOD_KEY, &tempVal, 100);
    // P2: clamp WorldMapTimeMod to [0, 1000] (1000 = 10x world-map travel
    // time). Without the upper bound, a crafted/typo value > ~47.8M pushes
    // the double->int cast in wmGameTimeIncrement (worldmap.cc:5310-5312)
    // out of range — UB. With the clamp the max product is
    // 18000 * 1.0 * 100 (script multi clamp) * 10 << INT_MAX.
    gWorldMapTimeMod = tempVal;
    if (gWorldMapTimeMod < 0) {
        gWorldMapTimeMod = 0;
    } else if (gWorldMapTimeMod > 1000) {
        gWorldMapTimeMod = 1000;
    }
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_WORLDMAP_ENCOUNTER_FIX_KEY, &tempVal, 0);
    gWorldMapEncounterFix = tempVal != 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_WORLDMAP_ENCOUNTER_RATE_KEY, &tempVal, 5);
    gWorldMapEncounterRate = tempVal;
    if (gWorldMapEncounterRate < 1) {
        gWorldMapEncounterRate = 1;
    }

    // P3 RPU parity: ProcessorIdle and BoxBarColours need no engine behavior —
    // CE already yields the CPU every frame via the FPS limiter (SDL_Delay in
    // fps_limiter.cc throttle()) and has no sfall box-bar colour rendering
    // equivalent. Log one-line notes so the accepted-but-inert state is
    // observable at config load time.
    if (gProcessorIdle) {
        debugPrint("ProcessorIdle=1: accepted; CE's FPS limiter already yields the CPU every frame (src/fps_limiter.cc). No additional idle handling needed.\n");
    }
    if (gBoxBarColours != 0) {
        debugPrint("BoxBarColours=%d: accepted but inert — CE has no sfall box-bar colour rendering equivalent (cosmetic setting).\n", gBoxBarColours);
    }

    // gEnableHeroAppearanceMod is unconditionally false since F-17
    // removed its config key and parsing. The feature is always active
    // in CE. Reset here so that repeated sfallConfigInit/Exit cycles
    // do not leave stale state from a previous init.
    gEnableHeroAppearanceMod = false;

    gSfallConfigInitialized = true;

    return true;
}

int sfallArtCacheSizeMb(int fallbackMb)
{
    int size = gOverrideArtCacheSize ? gSfallArtCacheSize : fallbackMb;
    // Mirror the CE settings.system.art_cache_size clamp (src/settings.cc:155:
    // clamp(8, 512)) so the cacheInit input is always in range regardless of
    // which source supplied it.
    return std::clamp(size, SFALL_CONFIG_ART_CACHE_SIZE_MIN, SFALL_CONFIG_ART_CACHE_SIZE_MAX);
}

void sfallConfigExit()
{
    if (gSfallConfigInitialized) {
        configFree(&gSfallConfig);
        gSfallConfigInitialized = false;
    }
}

} // namespace fallout
