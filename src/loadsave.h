#ifndef LOAD_SAVE_GAME_H
#define LOAD_SAVE_GAME_H

namespace fallout {

typedef enum LoadSaveMode {
    // Special case - loading game from main menu.
    LOAD_SAVE_MODE_FROM_MAIN_MENU,

    // Normal (full-screen) save/load screen.
    LOAD_SAVE_MODE_NORMAL,

    // Quick load/save.
    LOAD_SAVE_MODE_QUICK,
} LoadSaveMode;

void _InitLoadSave();
void _ResetLoadSave();
int lsgSaveGame(int mode);
int lsgLoadGame(int mode);
void lsgDevSetLoadGameSlot(int slot);
int lsgGetTotalSlotCount();
bool _isLoadingGame();
void lsgInit();
int MapDirErase(const char* path, const char* extension);
int _MapDirEraseFile_(const char* relativePath, const char* fileName);

// Metarule3(210-214) save slot accessors — exposed for script-level save slot
// query and control. These access the internal slot cursor state that is
// normally only manipulated through the save/load UI.
int loadsaveGetCurrentSlot();
void loadsaveSetCurrentSlot(int page, int slot);
int loadsaveGetCurrentPage();
int loadsaveGetCurrentSlotInPage();

} // namespace fallout

#endif /* LOAD_SAVE_GAME_H */
