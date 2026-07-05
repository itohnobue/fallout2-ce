#ifndef CHARACTER_EDITOR_H
#define CHARACTER_EDITOR_H

#include "db.h"

namespace fallout {

extern int gCharacterEditorRemainingCharacterPoints;

int characterEditorShow(bool isCreationMode);
void characterEditorInit();
bool _isdoschar(int ch);
char* _strmfe(char* dest, const char* name, const char* ext);
int characterEditorSave(File* stream);
int characterEditorLoad(File* stream);
void characterEditorReset();
int characterEditorGetWindow();
void characterEditorDisplayStats();

// Navigate the in-game character editor to a specific sub-page.
// folder:  0 = character sheet (main stats/skills view),
//          1 = perks (EDITOR_FOLDER_PERKS),
//          2 = karma (EDITOR_FOLDER_KARMA),
//          3 = kills (EDITOR_FOLDER_KILLS).
// Silently no-ops if the character editor window is not open.
// Integration point: op_hero_select_win (0x8213) in sfall_opcodes.cc.
void characterEditorSelectFolder(int folder);

} // namespace fallout

#endif /* CHARACTER_EDITOR_H */
