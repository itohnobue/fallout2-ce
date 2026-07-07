#ifndef FALLOUT_SFALL_GLOBAL_SCRIPTS_H_
#define FALLOUT_SFALL_GLOBAL_SCRIPTS_H_

#include "interpreter.h"

namespace fallout {

bool sfall_gl_scr_init();
void sfall_gl_scr_reset();
void sfall_gl_scr_exit();
void sfall_gl_scr_exec_start_proc();
void sfall_gl_scr_remove_all();

// Load and auto-register hs_*.int hook scripts from the HookScriptsPath
// directory (parsed from ddraw.ini [Misc]).  Call sfallParseHookScriptsPath()
// before this function.  Each procedure whose name starts with "hs_" is
// mapped to the corresponding HookType and registered via scriptHooksRegister.
void sfall_gl_scr_load_hook_scripts();
void sfall_gl_scr_exec_map_update_scripts(int action);
void sfall_gl_scr_process_main();
void sfall_gl_scr_process_input();
void sfall_gl_scr_process_worldmap();
void sfall_gl_scr_set_repeat(Program* program, int frames);
void sfall_gl_scr_set_type(Program* program, int type);
// F-058: Returns tri-state for game_loaded() opcode:
//   2=first load, 1=reload, 0=otherwise
int sfall_gl_scr_is_loaded(Program* program);
void sfall_gl_scr_increment_load_count();
// Reset the game load count to 0 for new games. Called from
// sfallOnAfterNewGame() to ensure game_loaded() correctly
// returns 2 (first load) after a new game start — even when
// a save had been loaded earlier in the same session and the
// counter was > 0.
void sfall_gl_scr_reset_load_count();
void sfall_gl_scr_update(int burstSize);

} // namespace fallout

#endif /* FALLOUT_SFALL_GLOBAL_SCRIPTS_H_ */
