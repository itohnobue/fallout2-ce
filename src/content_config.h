#ifndef CONTENT_CONFIG_H
#define CONTENT_CONFIG_H

#include "config.h"

namespace fallout {

#define CONTENT_CONFIG_START_SECTION "start"
#define CONTENT_CONFIG_KARMA_SECTION "karma"
#define CONTENT_CONFIG_ITEMS_SECTION "items"
#define CONTENT_CONFIG_MAPS_SECTION "maps"
#define CONTENT_CONFIG_DIALOG_SECTION "dialog"
#define CONTENT_CONFIG_MISC_SECTION "misc"
#define CONTENT_CONFIG_MAIN_MENU_SECTION "main_menu"
#define CONTENT_CONFIG_SOUND_SECTION "sound"
#define CONTENT_CONFIG_MOVIES_SECTION "movies"
#define CONTENT_CONFIG_STATS_SECTION "stats"
#define CONTENT_CONFIG_COMBAT_SECTION "combat"
#define CONTENT_CONFIG_EXPLOSIONS_SECTION "explosions"
#define CONTENT_CONFIG_SKILLDEX_SECTION "skilldex"
#define CONTENT_CONFIG_WORLDMAP_SECTION "worldmap"
#define CONTENT_CONFIG_CHARACTERS_SECTION "characters"
#define CONTENT_CONFIG_TEXT_SECTION "text"

extern Config gContentConfig;

// Initialize content config and try to load it from file.
void contentConfigInit();
void contentConfigExit();

// Look up a ddraw.ini [section] key in gContentConfig, using the
// migration mapping. Returns the int value or -1 if not found.
// This bridges scripts that still read ddraw.ini keys via get_ini_setting
// to the migrated game.cfg values, ensuring compatibility with RPU/Et Tu
// mods that check migrated config keys (e.g., BoostScriptDialogLimit,
// WorldMapSlots, ElevatorsFile).
int contentConfigLookupSfallInt(const char* section, const char* key);
const char* contentConfigLookupSfallString(const char* section, const char* key);

// Seeds the et tu startup-gate keys ([start] use_filesystem_override,
// allow_unsafe_scripting, fallout1_behavior) into gContentConfig when et tu's
// signature overlay is detected (game#patch.cfg with the FO1 start map
// V13ent.map). Called from contentConfigInit after the patch overlay loads;
// exposed for tests. Idempotent and no-op when the signature is absent or the
// keys are already explicitly set.
void contentConfigSeedEtTuGateKeys();

} // namespace fallout

#endif // CONTENT_CONFIG_H
