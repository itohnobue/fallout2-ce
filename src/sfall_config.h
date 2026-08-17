#ifndef SFALL_CONFIG_H
#define SFALL_CONFIG_H

#include "config.h"

namespace fallout {

#define SFALL_CONFIG_FILE_NAME "ddraw.ini"

#define SFALL_CONFIG_MAIN_KEY "Main"
#define SFALL_CONFIG_MISC_KEY "Misc"
#define SFALL_CONFIG_SCRIPTS_KEY "Scripts"
#define SFALL_CONFIG_SPEED_KEY "Speed"
#define SFALL_CONFIG_DEBUGGING_KEY "Debugging"

#define SFALL_CONFIG_SPEED_MULTI_KEY "SpeedMulti"
#define SFALL_CONFIG_SPEED_MULTI_INITIAL_KEY "SpeedMultiInitial"

#define SFALL_CONFIG_OVERRIDE_CRITICALS_MODE_KEY "OverrideCriticalTable"
#define SFALL_CONFIG_OVERRIDE_CRITICALS_FILE_KEY "OverrideCriticalFile"
#define SFALL_CONFIG_BOOKS_FILE_KEY "BooksFile"
#define SFALL_CONFIG_ELEVATORS_FILE_KEY "ElevatorsFile"
#define SFALL_CONFIG_SKILLS_FILE_KEY "SkillsFile"
#define SFALL_CONFIG_UNARMED_FILE_KEY "UnarmedFile"
#define SFALL_CONFIG_PERKS_FILE_KEY "PerksFile"
#define SFALL_CONFIG_INI_CONFIG_FOLDER "IniConfigFolder"
#define SFALL_CONFIG_CONFIG_FILE "ConfigFile"
#define SFALL_CONFIG_PATCH_FILE "PatchFile"

#define SFALL_CONFIG_FALLOUT1_BEHAVIOR_KEY "Fallout1Behavior"
#define SFALL_CONFIG_ALLOW_UNSAFE_SCRIPTING_KEY "AllowUnsafeScripting"
#define SFALL_CONFIG_ENABLE_HERO_APPEARANCE_MOD_KEY "EnableHeroAppearanceMod"
#define SFALL_CONFIG_USE_FILESYSTEM_OVERRIDE_KEY "UseFileSystemOverride"
#define SFALL_CONFIG_OVERRIDE_ART_CACHE_SIZE_KEY "OverrideArtCacheSize"
#define SFALL_CONFIG_EXTRA_SAVE_SLOTS_KEY "ExtraSaveSlots"
#define SFALL_CONFIG_PROCESSOR_IDLE_KEY "ProcessorIdle"
#define SFALL_CONFIG_BOX_BAR_COLOURS_KEY "BoxBarColours"
#define SFALL_CONFIG_ART_CACHE_SIZE_KEY "ArtCacheSize"
#define SFALL_CONFIG_FEMALE_DIALOG_MSGS_KEY "FemaleDialogMsgs"
// H-06: WorldMapSlots — RPU gl_k_modini.ssl requires this to be 21; CE's
// default is 21 so the ddraw.ini fallback tier satisfies RPU out of the box.
#define SFALL_CONFIG_WORLDMAP_SLOTS_KEY "WorldMapSlots"
// World map travel-time percentage modifier (100 = normal, 0 = time stops).
// Multiplies the game-time increment per world-map step (sfall Main.cpp:
// mapMultiMod = WorldMapTimeMod / 100.0, applied in addition to the
// Pathfinder perk and set_map_time_multi).
#define SFALL_CONFIG_WORLDMAP_TIME_MOD_KEY "WorldMapTimeMod"
// WorldMapEncounterFix=1 gates the per-step random-encounter roll to at
// most once per WorldMapEncounterRate walking frames (rate-based cadence;
// higher rate = slower encounters; sfall default 5). Fix=0 (sfall and et tu
// default) keeps the vanilla per-step roll. WorldMapEncounterRate is only
// consulted when Fix=1, mirroring sfall's code.
#define SFALL_CONFIG_WORLDMAP_ENCOUNTER_FIX_KEY "WorldMapEncounterFix"
#define SFALL_CONFIG_WORLDMAP_ENCOUNTER_RATE_KEY "WorldMapEncounterRate"

// Art cache size (MB) used when [Misc] OverrideArtCacheSize=1 and no explicit
// [Misc] ArtCacheSize key is present. Mirrors sfall's fixed behavior: sfall
// does NOT have an ArtCacheSize key — OverrideArtCacheSize=1 sets the cache
// to a fixed 261 MB ("Changed OverrideArtCacheSize to set the art cache size
// to 261 instead of 256", sfall-readme.txt:361; the option exists "to fix
// F2RP EPA crashes", sfall-readme.txt:1727). RPU ships OverrideArtCacheSize=1
// with no ArtCacheSize key, so 261 is what RPU's config requests. 261 also
// stays above CE's own 32 MB default (settings.h:21) — the previous default
// of 20 MB was below it, dropping the cache on RPU installs.
#define SFALL_CONFIG_ART_CACHE_SIZE_DEFAULT 261
// Clamp bounds (MB) for the art cache size, mirroring the CE
// settings.system.art_cache_size clamp (src/settings.cc:155: clamp(8, 512)).
#define SFALL_CONFIG_ART_CACHE_SIZE_MIN 8
#define SFALL_CONFIG_ART_CACHE_SIZE_MAX 512

extern bool gSfallConfigInitialized;
extern Config gSfallConfig;

extern bool gFallout1Behavior;

// Config booleans parsed from ddraw.ini. Wired status noted per-global.
extern bool gAllowUnsafeScripting; // INTENTIONALLY UNWIRED: opcodes registered unconditionally; flag parsed but never gates registration
extern bool gEnableHeroAppearanceMod; // DEAD: feature always active; flag parsed in sfall_ini.cc but unwired in CE — no code gate exists
extern bool gUseFileSystemOverride; // INTENTIONALLY UNWIRED: VFS priority handles this
extern bool gOverrideArtCacheSize; // WIRED: when set, art.cc uses the sfall [Misc] ArtCacheSize value (gSfallArtCacheSize) instead of settings.system.art_cache_size
extern bool gExtraSaveSlots; // WIRED: consumed at loadsave.cc for save slot page count
extern bool gProcessorIdle; // PARSED, NO ENGINE CHANGE NEEDED: CE's FPS limiter already yields the CPU every frame via SDL_Delay (src/fps_limiter.cc); parsed for ddraw.ini acceptance and documented
extern int gBoxBarColours; // PARSED, INERT: accepted for ddraw.ini compatibility; CE has no sfall box-bar colour rendering equivalent (cosmetic)
extern int gSfallArtCacheSize; // WIRED: consumed by art.cc via sfallArtCacheSizeMb() when gOverrideArtCacheSize is set
extern int gFemaleDialogMsgs; // WIRED: 0=normal, 1=dialog_female for female PC, 2=+cuts_female; consumed by messageListGetLocalizedDir (message.cc) for dialog/cutscene message dir selection
extern int gWorldMapTimeMod; // WIRED: percent multiplier for world-map game-time increment (worldmap.cc wmGameTimeIncrement); 100 = normal
extern bool gWorldMapEncounterFix; // WIRED: 1 = rate-gated world-map encounter rolls (worldmap.cc walk loop); 0 = vanilla per-step rolls
extern int gWorldMapEncounterRate; // WIRED: walking frames between encounter rolls when gWorldMapEncounterFix is set; higher = slower

bool sfallConfigInit(int argc, char** argv);
void sfallConfigExit();

// Art cache size selection (MB): returns gSfallArtCacheSize when
// gOverrideArtCacheSize is set, otherwise fallbackMb (the CE setting).
// The result is clamped to [SFALL_CONFIG_ART_CACHE_SIZE_MIN,
// SFALL_CONFIG_ART_CACHE_SIZE_MAX] to mirror the CE setting clamp.
int sfallArtCacheSizeMb(int fallbackMb);

} // namespace fallout

#endif /* SFALL_CONFIG_H */
