#include "sfall_global_scripts.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "animation.h"
#include "db.h"
#include "input.h"
#include "platform_compat.h"
#include "scripts.h"
#include "sfall_config.h"
#include "sfall_ext.h"
#include "sfall_script_hooks.h"

namespace fallout {

static constexpr int kGlobalScriptBusyFlags = PROGRAM_FLAG_FATAL_ERROR
    | PROGRAM_FLAG_CHILD_CALL
    | PROGRAM_FLAG_CHILD_SPAWN;

struct GlobalScript {
    Program* program = nullptr;
    int procs[SCRIPT_PROC_COUNT] = { 0 };
    int repeat = 0;
    int count = 0;
    int mode = 0;
    bool once = true; // Set to false after first game_loaded call for this script instance
};

struct GlobalScriptsState {
    std::vector<std::string> paths;
    std::vector<GlobalScript> globalScripts;
};

static GlobalScriptsState* state = nullptr;

// F-058: Game load counter for game_loaded() tri-state return.
// Incremented each time a game is loaded (loadsave.cc). Used by
// sfall_gl_scr_is_loaded() to distinguish:
//   2 = first load (new game, gGameLoadCount == 0)
//   1 = reload (subsequent load, gGameLoadCount > 0)
//   0 = normal gameplay (once already consumed)
static int gGameLoadCount = 0;

void sfall_gl_scr_increment_load_count()
{
    gGameLoadCount++;
}

void sfall_gl_scr_reset_load_count()
{
    gGameLoadCount = 0;
}

bool sfall_gl_scr_init()
{
    state = new (std::nothrow) GlobalScriptsState();
    if (state == nullptr) {
        return false;
    }

    // F-030: Merge user-configured global script paths from ddraw.ini
    // [Misc] GlobalScriptPaths with hardcoded defaults. User paths take
    // priority (added first). F-141: Use std::set for deduplication.
    std::set<std::string> uniquePaths;
    const std::vector<std::string>& userPaths = sfallGetGlobalScriptPaths();
    char** files;

    for (const auto& userGlob : userPaths) {
        std::string globStr = std::string(userGlob);
        // Extract the directory component from the glob pattern.
        size_t lastSep = globStr.find_last_of("\\/");
        std::string userDir = (lastSep != std::string::npos) ? globStr.substr(0, lastSep) : ".";

        int userFilesLength = fileNameListInit(globStr.c_str(), &files);
        if (userFilesLength != 0) {
            for (int index = 0; index < userFilesLength; index++) {
                char path[COMPAT_MAX_PATH];
                snprintf(path, sizeof(path), "%s\\%s", userDir.c_str(), files[index]);
                uniquePaths.insert(std::string { path });
            }
            fileNameListFree(&files, 0);
        }
    }

    // Load global scripts from both the vanilla "scripts\gl*.int" and
    // the RPU/Et Tu "scripts\sfall\gl*.int" paths. RPU places extended
    // global scripts under the sfall subdirectory — without this second
    // pass those scripts are silently never loaded.
    const char* scriptPath = "scripts\\gl*.int";
    const char* dir = "scripts";
    int filesLength = fileNameListInit(scriptPath, &files);
    if (filesLength != 0) {
        for (int index = 0; index < filesLength; index++) {
            char path[COMPAT_MAX_PATH];
            snprintf(path, sizeof(path), "%s\\%s", dir, files[index]);
            uniquePaths.insert(std::string { path });
        }
        fileNameListFree(&files, 0);
    }

    // Load RPU global scripts from "scripts\sfall\gl*.int".
    const char* sfallScriptPath = "scripts\\sfall\\gl*.int";
    const char* sfallDir = "scripts\\sfall";
    int sfallFilesLength = fileNameListInit(sfallScriptPath, &files);
    if (sfallFilesLength != 0) {
        for (int index = 0; index < sfallFilesLength; index++) {
            char path[COMPAT_MAX_PATH];
            snprintf(path, sizeof(path), "%s\\%s", sfallDir, files[index]);
            uniquePaths.insert(std::string { path });
        }
        fileNameListFree(&files, 0);
    }

    for (const auto& path : uniquePaths) {
        state->paths.push_back(path);
    }

    std::sort(state->paths.begin(), state->paths.end());

    return true;
}

void sfall_gl_scr_reset()
{
    if (state != nullptr) {
        sfall_gl_scr_remove_all();
    }
}

void sfall_gl_scr_exit()
{
    if (state != nullptr) {
        sfall_gl_scr_remove_all();

        delete state;
        state = nullptr;
    }
}

// F-39: Hook name → HookType mapping for hs_*.int auto-registration.
// Procedure names like "hs_tohit", "hs_combatdamage" map to the
// corresponding HookType enum.  Strip the 3-char "hs_" prefix and
// match the remainder against this table.
static constexpr struct {
    const char* name;
    HookType hookType;
} kHookScriptProcNames[] = {
    { "tohit", HOOK_TOHIT },
    { "afterhitroll", HOOK_AFTERHITROLL },
    { "calcapcost", HOOK_CALCAPCOST },
    { "deathanim2", HOOK_DEATHANIM2 },
    { "combatdamage", HOOK_COMBATDAMAGE },
    { "ondeath", HOOK_ONDEATH },
    { "findtarget", HOOK_FINDTARGET },
    { "useobjon", HOOK_USEOBJON },
    { "barterprice", HOOK_BARTERPRICE },
    { "movecost", HOOK_MOVECOST },
    { "itemdamage", HOOK_ITEMDAMAGE },
    { "ammocost", HOOK_AMMOCOST },
    { "useobj", HOOK_USEOBJ },
    { "keypress", HOOK_KEYPRESS },
    { "mouseclick", HOOK_MOUSECLICK },
    { "useskill", HOOK_USESKILL },
    { "steal", HOOK_STEAL },
    { "withinperception", HOOK_WITHINPERCEPTION },
    { "inventorymove", HOOK_INVENTORYMOVE },
    { "invenwield", HOOK_INVENWIELD },
    { "adjustfid", HOOK_ADJUSTFID },
    { "combatturn", HOOK_COMBATTURN },
    { "cartravel", HOOK_CARTRAVEL },
    { "setglobalvar", HOOK_SETGLOBALVAR },
    { "resttimer", HOOK_RESTTIMER },
    { "gamemodechange", HOOK_GAMEMODECHANGE },
    { "useanimobj", HOOK_USEANIMOBJ },
    { "explosivetimer", HOOK_EXPLOSIVETIMER },
    { "descriptionobj", HOOK_DESCRIPTIONOBJ },
    { "useskillon", HOOK_USESKILLON },
    { "onexplosion", HOOK_ONEXPLOSION },
    { "setlighting", HOOK_SETLIGHTING },
    { "sneak", HOOK_SNEAK },
    { "stdprocedure", HOOK_STDPROCEDURE },
    { "stdprocedure_end", HOOK_STDPROCEDURE_END },
    { "targetobject", HOOK_TARGETOBJECT },
    { "encounter", HOOK_ENCOUNTER },
    { "canuseweapon", HOOK_CANUSEWEAPON },
    { "dialog", HOOK_DIALOG },
    { "dialogreaction", HOOK_DIALOGREACTION },
    { "statlevelup", HOOK_STATLEVELUP },
    { "barter", HOOK_BARTER },
    { "message", HOOK_MESSAGE },
};

// Look up a hook procedure name suffix (without "hs_" prefix) in the mapping
// table and return the corresponding HookType, or -1 if not found.
static int sfall_gl_scr_find_hook_type(const char* name)
{
    for (const auto& entry : kHookScriptProcNames) {
        if (compat_stricmp(name, entry.name) == 0) {
            return static_cast<int>(entry.hookType);
        }
    }
    return -1;
}

// Load and auto-register hs_*.int hook scripts from the HookScriptsPath
// directory.  Each procedure whose name starts with "hs_" is mapped to
// the corresponding HookType and registered via scriptHooksRegister().
// This mirrors sfall's automatic hook script loading behaviour.
void sfall_gl_scr_load_hook_scripts()
{
    const std::string& hookPath = sfallGetHookScriptsPath();
    std::string globPattern = hookPath + "\\hs_*.int";

    char** files = nullptr;
    int filesLength = fileNameListInit(globPattern.c_str(), &files);
    if (filesLength == 0) {
        return;
    }

    for (int index = 0; index < filesLength; index++) {
        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "%s\\%s", hookPath.c_str(), files[index]);

        // Pre-check file existence to prevent programCreateByPath's internal
        // programFatalError from longjmp-ing to the calling program's context.
        File* test = fileOpen(path, "rb");
        if (test == nullptr) {
            continue;
        }
        fileClose(test);

        Program* program = programCreateByPath(path);
        if (program == nullptr) {
            continue;
        }

        // Walk all procedures and auto-register those matching "hs_*".
        int procCount = program->procedureCount();
        unsigned char* procPtr = program->procedures + 4;
        for (int pi = 0; pi < procCount; pi++) {
            int nameOffset = stackReadInt32(procPtr, offsetof(Procedure, nameOffset));
            const char* procName = programGetIdentifier(program, nameOffset);

            // Hook procedures use the "hs_" prefix.
            if (procName != nullptr && strlen(procName) > 3
                && procName[0] == 'h' && procName[1] == 's' && procName[2] == '_') {
                int hookType = sfall_gl_scr_find_hook_type(procName + 3);
                if (hookType >= 0 && hookType < HOOK_COUNT) {
                    scriptHooksRegister(program,
                        static_cast<HookType>(hookType), pi, /*atEnd=*/false);
                }
            }

            procPtr += sizeof(Procedure);
        }

        // Run the script once so its procedures are initialized.
        programInterpret(program, -1);

        // Store the program so it participates in cleanup
        // (scriptHooksUnregisterProgram + programFree in
        // sfall_gl_scr_remove_all).  Mark all procs as -1
        // so the ticker/event loops skip hook-only scripts.
        GlobalScript scr;
        scr.program = program;
        for (int a = 0; a < SCRIPT_PROC_COUNT; a++) {
            scr.procs[a] = -1;
        }
        state->globalScripts.push_back(std::move(scr));
    }

    if (files != nullptr) {
        fileNameListFree(&files, 0);
    }
}

void sfall_gl_scr_exec_start_proc()
{
    // Load hs_*.int hook scripts before loading global scripts.
    // Called on every new game / game load so hook scripts are
    // auto-registered after gameReset clears them.  Scripts are
    // idempotent — re-loading the same file simply re-registers
    // the same procedures.
    sfall_gl_scr_load_hook_scripts();

    for (auto& path : state->paths) {
        // Pre-check file existence to prevent programCreateByPath's internal
        // programFatalError from longjmp-ing to the calling program's context.
        // programFatalError longjmps to gInterpreterCurrentProgram->env, which
        // during lazy script load points to the currently executing script, not
        // the script being loaded. This would corrupt the wrong program's state.
        File* test = fileOpen(path.c_str(), "rb");
        if (test == nullptr) {
            continue;
        }
        fileClose(test);

        Program* program = programCreateByPath(path.c_str());
        if (program != nullptr) {
            GlobalScript scr;
            scr.program = program;

            for (int action = 0; action < SCRIPT_PROC_COUNT; action++) {
                scr.procs[action] = programFindProcedure(program, gScriptProcNames[action]);
            }

            state->globalScripts.push_back(std::move(scr));

            programInterpret(program, -1);
        }
    }

    tickersAdd(sfall_gl_scr_process_input);
}

void sfall_gl_scr_remove_all()
{
    if (state == nullptr) {
        return;
    }

    tickersRemove(sfall_gl_scr_process_input);

    for (auto& scr : state->globalScripts) {
        // Unregister hook references before freeing the program to prevent
        // hook vectors from retaining dangling Program* references.
        // Without this, hook dispatch iterating the hook vector after
        // programFree() would dereference freed memory.
        scriptHooksUnregisterProgram(scr.program);
        programFree(scr.program);
    }

    state->globalScripts.clear();
}

// Execute proc if it is found and not "busy".  Returns true if proc was executed
static bool sfall_gl_scr_execute_proc_if_ready(Program* program, int proc)
{
    // matches check in scriptExecProc()
    if (proc != -1 && (program->flags & kGlobalScriptBusyFlags) == 0) {
        programExecuteProcedure(program, proc);
        return true;
    }

    return false;
}

void sfall_gl_scr_exec_map_update_scripts(int action)
{
    for (auto& scr : state->globalScripts) {
        if (scr.mode == 0 || scr.mode == 3) {
            sfall_gl_scr_execute_proc_if_ready(scr.program, scr.procs[action]);
        }
    }
}

static void sfall_gl_scr_process_simple(int mode1, int mode2)
{
    for (auto& scr : state->globalScripts) {
        if (scr.repeat != 0 && (scr.mode == mode1 || scr.mode == mode2)) {
            // Reset combat check per-script to prevent state leakage between
            // global scripts. reg_anim_combat_check flips the global
            // gRegAnimCombatCheck flag in animation.cc. Resetting it before
            // each script ensures one script disabling combat check doesn't
            // affect later scripts in the same tick.
            animationResetCombatCheck();

            scr.count++;
            if (scr.count >= scr.repeat) {
                if (sfall_gl_scr_execute_proc_if_ready(scr.program, scr.procs[SCRIPT_PROC_START])) {
                    scr.count = 0;
                } else {
                    scr.count = scr.repeat;
                }
            }
        }
    }
}

void sfall_gl_scr_process_main()
{
    // Fire mode 0 (timed) and mode 3 (always) from the main loop ticker.
    // Mode 0 scripts with repeat != 0 receive timer-based execution here
    // in addition to event-driven map_update execution, matching sfall
    // semantics where mode 0 = "timed".
    sfall_gl_scr_process_simple(0, 3);
}

void sfall_gl_scr_process_input()
{
    sfall_gl_scr_process_simple(1, 1);
}

void sfall_gl_scr_process_worldmap()
{
    sfall_gl_scr_process_simple(2, 3);
}

static GlobalScript* sfall_gl_scr_map_program_to_scr(Program* program)
{
    auto it = std::find_if(state->globalScripts.begin(),
        state->globalScripts.end(),
        [&program](const GlobalScript& scr) {
            return scr.program == program;
        });
    return it != state->globalScripts.end() ? &(*it) : nullptr;
}

void sfall_gl_scr_set_repeat(Program* program, int frames)
{
    // Reject negative timer values.  A negative repeat would cause
    // per-frame execution (scr.count >= scr.repeat immediately true),
    // matching the validation pattern in sfall_gl_scr_set_type.
    if (frames < 0) {
        return;
    }

    GlobalScript* scr = sfall_gl_scr_map_program_to_scr(program);
    if (scr != nullptr) {
        scr->repeat = frames;
    }
}

void sfall_gl_scr_set_type(Program* program, int type)
{
    if (type < 0 || type > 3) {
        return;
    }

    GlobalScript* scr = sfall_gl_scr_map_program_to_scr(program);
    if (scr != nullptr) {
        scr->mode = type;
    }
}

// F-058: Returns tri-state for game_loaded() opcode:
//   2 = first load (once=true AND gGameLoadCount==0 — fresh game start)
//   1 = reload (once=true AND gGameLoadCount>0 — save game loaded)
//   0 = otherwise (once already consumed, or not a global script)
int sfall_gl_scr_is_loaded(Program* program)
{
    GlobalScript* scr = sfall_gl_scr_map_program_to_scr(program);
    if (scr != nullptr) {
        if (scr->once) {
            scr->once = false;
            return (gGameLoadCount == 0) ? 2 : 1;
        }

        return 0;
    }

    // Not a global script.
    // Per sfall 4.4.5 fix: game_loaded() should return 0 for non-global scripts.
    return 0;
}

void sfall_gl_scr_update(int burstSize)
{
    for (auto& scr : state->globalScripts) {
        programInterpret(scr.program, burstSize);
    }
}

} // namespace fallout
