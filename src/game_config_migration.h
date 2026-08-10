#ifndef FALLOUT_GAME_CONFIG_MIGRATION_H_
#define FALLOUT_GAME_CONFIG_MIGRATION_H_

#include <stddef.h>

#include "config.h"

namespace fallout {

bool gameConfigMigrateFromF2Res(const char* gameConfigFilePath, Config* gameConfig);
void contentConfigTryMigrateFromSfall(const char* contentConfigPath);

// Number of entries in kSfallContentMappings (content_config.cc) and
// kSfallMigrationEntries (game_config_migration.cc). Both tables MUST
// cover the same set of ddraw.ini keys mapped to content_config sections.
// When adding new migration entries, update this count and add the
// corresponding entry to BOTH tables to keep them synchronized.
// P-19: reduced 59 → 56 — ViewXPos/ViewYPos/WorldMapSlots rows removed
// (dead targets, zero consumers; WorldMapSlots now served by the
// gSfallConfig default of 21 per H-06).
// 5dc9135/68ff38e/f7841ee: 56 → 63 — StartGDialogFix (dialog) + 6 [sound]
// music override rows added.
inline constexpr size_t kSfallMigrationEntryCount = 63;

} // namespace fallout

#endif /* FALLOUT_GAME_CONFIG_MIGRATION_H_ */
