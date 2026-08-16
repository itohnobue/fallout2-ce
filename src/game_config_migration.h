#ifndef FALLOUT_GAME_CONFIG_MIGRATION_H_
#define FALLOUT_GAME_CONFIG_MIGRATION_H_

#include <stddef.h>

#include "config.h"

namespace fallout {

bool gameConfigMigrateFromF2Res(const char* gameConfigFilePath, Config* gameConfig);
void contentConfigTryMigrateFromF2Res(const char* contentConfigPath);
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
// sync2 (upstream 1cce144): 63 → 70 — upstream additions with live consumers
// (DisableSpecialMapIDs, InventoryApCost, QuickPocketsApCostReduction,
// WorldMapTerrainInfo, XPTable, ViewXPos/ViewYPos to [start] worldmap_view_*).
// StartXPos/StartYPos keep the fork [worldmap] targets (F-072 Et Tu path).
// Et tu startup gate (2026-08-16): 70 → 71 — Debugging|AllowUnsafeScripting
// → [start] allow_unsafe_scripting (seeded when et tu's game#patch.cfg
// overlay is detected so gl_0.ssl's first-run forced restart is avoided).
inline constexpr size_t kSfallMigrationEntryCount = 71;

} // namespace fallout

#endif /* FALLOUT_GAME_CONFIG_MIGRATION_H_ */
