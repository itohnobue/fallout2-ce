#ifndef AUTOMAP_H
#define AUTOMAP_H

#include "db.h"
#include "map_defs.h"

namespace fallout {

#define AUTOMAP_DB ("AUTOMAP.DB")
#define AUTOMAP_TMP ("AUTOMAP.TMP")

// The number of map entries that is stored in automap.db.
//
// 160 is the vanilla Fallout 2 map count. The RPU restoration project ships
// 173 maps (maps.txt has [Map 000]..[Map 172]); the vanilla constant silently
// excluded maps 160-172 (EPA sublevels, SF Sheng's, Slaver Camp, safehouses,
// etc.) from the pipboy automap. This matches RPU's exact map count so every
// RPU map gets an automap entry.
//
// AUTOMAP.DB is a per-session generated cache: it is created at
// automapInit/automapReset (automapCreate) and deleted at automapExit.
// HOWEVER, savegames DO carry it: _GameMap2Slot copies the whole DB into the
// save slot as AUTOMAP.DB.SAV (src/loadsave.cc) and _SlotMap2Game restores it
// over the freshly generated DB on load. Because the on-disk layout (header
// version byte, dataSize, offsets table) depends on AUTOMAP_MAP_COUNT, the
// format version was bumped to AUTOMAP_DB_VERSION (2) when the count was
// raised: a pre-change save restores a version-1 DB, the version check in
// automapLoadHeader fails, and the automap load paths (automapGetHeader /
// automapLoadEntry / automapSaveCurrent) regenerate the DB via automapCreate
// so the session — and the next save, which re-copies the fresh DB into the
// slot — use the current 173-entry layout.
//
// NOTE: Do NOT make this dynamic — the on-disk AutomapHeader.offsets array is
// fixed-size by design; automapSaveHeader/automapLoadHeader serialize
// AUTOMAP_OFFSET_COUNT (= AUTOMAP_MAP_COUNT * ELEVATION_COUNT) entries.
#define AUTOMAP_MAP_COUNT (173)

// On-disk AUTOMAP.DB format version. Version 1 was the vanilla 160-map
// layout (header 1925 bytes); version 2 marks the raised 173-entry layout
// (header 2081 bytes). automapLoadHeader rejects any other version, and the
// automap load paths regenerate the database when they hit a mismatched
// version (e.g. a version-1 AUTOMAP.DB.SAV restored from a pre-change save).
#define AUTOMAP_DB_VERSION (2)

// View options for rendering automap for map window. These are stored in
// [gAutomapFlags] and is saved in save game file.
enum AutomapFlags : int {
    AUTOMAP_NONE = 0x00,

    // NOTE: This is a special flag to denote the map is activated in the game (as
    // opposed to the mapper). It's always on. Turning it off produces nice color
    // coded map with all objects and their types visible, however there is no way
    // you can do it within the game UI.
    AUTOMAP_IN_GAME = 0x01,

    // High details is on.
    AUTOMAP_WTH_HIGH_DETAILS = 0x02,

    // Scanner is active.
    AUTOMAP_WITH_SCANNER = 0x04,
};

constexpr inline AutomapFlags operator~(AutomapFlags rhs)
{
    return static_cast<AutomapFlags>(~static_cast<int>(rhs));
}

inline AutomapFlags& operator&=(AutomapFlags& lhs, AutomapFlags rhs)
{
    lhs = static_cast<AutomapFlags>(static_cast<int>(lhs) & static_cast<int>(rhs));
    return lhs;
}

inline AutomapFlags& operator|=(AutomapFlags& lhs, AutomapFlags rhs)
{
    lhs = static_cast<AutomapFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
    return lhs;
}

typedef struct AutomapHeader {
    unsigned char version;

    // The size of entire automap database (including header itself).
    int dataSize;

    // Offsets from the beginning of the automap database file into
    // entries data.
    //
    // These offsets are specified for every map/elevation combination. A value
    // of 0 specifies that there is no data for appropriate map/elevation
    // combination.
    int offsets[AUTOMAP_MAP_COUNT][ELEVATION_COUNT];
} AutomapHeader;

int automapInit();
int automapReset();
void automapExit();
int automapLoad(File* stream);
int automapSave(File* stream);
int _automapDisplayMap(int map);
void automapShow(bool isInGame, bool isUsingScanner);
int automapRenderInPipboyWindow(int win, int map, int elevation);
int automapSaveCurrent();
int automapGetHeader(AutomapHeader** automapHeaderPtr);
int automapGetWindow();

// M-145: Returns true if the map index is within the AUTOMAP.DB entry range
// ([0, AUTOMAP_MAP_COUNT)). AUTOMAP_MAP_COUNT is 173, matching RPU's map
// count, but mods with more than 173 maps still exceed it; every automap site
// that indexes offsets[]/_displayMapList[] by map number must validate via
// this helper before accessing.
bool automapMapIndexIsValid(int map);

void automapSetDisplayMap(int map, bool available);

} // namespace fallout

#endif /* AUTOMAP_H */
