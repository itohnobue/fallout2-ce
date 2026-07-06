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
    bool once = true;
};

struct GlobalScriptsState {
    std::vector<std::string> paths;
    std::vector<GlobalScript> globalScripts;
};

static GlobalScriptsState* state = nullptr;

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

void sfall_gl_scr_exec_start_proc()
{
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
    // Only fire mode 3 (always) from the main loop ticker.
    // Mode 0 scripts fire exclusively from map_update triggers
    // (sfall_gl_scr_exec_map_update_scripts); including them here
    // causes double-execution for mode 0 scripts with repeat != 0.
    sfall_gl_scr_process_simple(3, 3);
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

bool sfall_gl_scr_is_loaded(Program* program)
{
    GlobalScript* scr = sfall_gl_scr_map_program_to_scr(program);
    if (scr != nullptr) {
        if (scr->once) {
            scr->once = false;
            return true;
        }

        return false;
    }

    // Not a global script.
    // Per sfall 4.4.5 fix: game_loaded() should return false for non-global scripts.
    // (The 4.2.9-4.4.4 bug was that it always returned 1 from normal scripts.)
    return false;
}

void sfall_gl_scr_update(int burstSize)
{
    for (auto& scr : state->globalScripts) {
        programInterpret(scr.program, burstSize);
    }
}

} // namespace fallout
