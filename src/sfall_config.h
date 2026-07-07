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
#define SFALL_CONFIG_UNARMED_FILE_KEY "UnarmedFile"
#define SFALL_CONFIG_INI_CONFIG_FOLDER "IniConfigFolder"
#define SFALL_CONFIG_CONFIG_FILE "ConfigFile"
#define SFALL_CONFIG_PATCH_FILE "PatchFile"

#define SFALL_CONFIG_FALLOUT1_BEHAVIOR_KEY "Fallout1Behavior"
#define SFALL_CONFIG_ALLOW_UNSAFE_SCRIPTING_KEY "AllowUnsafeScripting"
#define SFALL_CONFIG_ENABLE_HERO_APPEARANCE_MOD_KEY "EnableHeroAppearanceMod"
#define SFALL_CONFIG_USE_FILESYSTEM_OVERRIDE_KEY "UseFileSystemOverride"
#define SFALL_CONFIG_OVERRIDE_ART_CACHE_SIZE_KEY "OverrideArtCacheSize"
#define SFALL_CONFIG_EXTRA_SAVE_SLOTS_KEY "ExtraSaveSlots"

extern bool gSfallConfigInitialized;
extern Config gSfallConfig;

extern bool gFallout1Behavior;

// Config booleans parsed from ddraw.ini. Wired status noted per-global.
extern bool gAllowUnsafeScripting; // INTENTIONALLY UNWIRED: opcodes registered unconditionally; flag parsed but never gates registration
extern bool gEnableHeroAppearanceMod; // WIRED: consumed via sfallConfigGetHeroAppearanceMod()
extern bool gUseFileSystemOverride; // INTENTIONALLY UNWIRED: VFS priority handles this
extern bool gOverrideArtCacheSize; // INTENTIONALLY UNWIRED: art.cc uses settings.system.art_cache_size instead
extern bool gExtraSaveSlots; // WIRED: consumed at loadsave.cc for save slot page count

bool sfallConfigInit(int argc, char** argv);
void sfallConfigExit();

// Provides read-only access to gEnableHeroAppearanceMod for consumption
// in the hero appearance opcode registration pipeline (sfall_opcodes.cc).
bool sfallConfigGetHeroAppearanceMod();

} // namespace fallout

#endif /* SFALL_CONFIG_H */
