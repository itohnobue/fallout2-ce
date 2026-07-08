#include "sfall_ext.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "db.h"
#include "debug.h"
#include "platform_compat.h"
#include "scripts.h"
#include "sfall_arrays.h"
#include "sfall_config.h"
#include "sfall_global_vars.h"
#include "sfall_metarules.h"
#include "sfall_opcodes.h"

namespace fallout {

// Config key for global script path globs (RPU places scripts under
// scripts/sfall/ in addition to the default scripts/).  Comma-separated.
static constexpr const char* kGlobalScriptPathsKey = "GlobalScriptPaths";

// Section and key prefix for ExtraPatches support — individual named patch
// files beyond the template-based PatchFile mechanism in game.cc.
static constexpr const char* kExtraPatchesSection = "ExtraPatches";
static constexpr const char* kExtraPatchesKeyPrefix = "PatchFile";

// Parsed global script path globs, populated by sfallParseGlobalScriptPaths().
static std::vector<std::string> g_globalScriptPaths;

/**
 * Parse GlobalScriptPaths from ddraw.ini [Misc] section.
 *
 * Sfall supports a comma-separated list of path globs (e.g.
 * "scripts\\gl*.int,scripts\\sfall\\gl*.int").  CE previously
 * hardcoded only "scripts\\gl*.int", silently ignoring RPU's
 * scripts\\sfall\\gl*.int globals.
 */
bool sfallParseGlobalScriptPaths()
{
    g_globalScriptPaths.clear();

    char* rawPaths = nullptr;
    if (!configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, kGlobalScriptPathsKey, &rawPaths)
        || rawPaths == nullptr
        || rawPaths[0] == '\0') {
        return true; // absent or empty is not an error — caller uses default
    }

    // Split on commas.
    std::string pathsStr { rawPaths };
    size_t pos = 0;
    while (pos < pathsStr.size()) {
        // Skip leading whitespace.
        while (pos < pathsStr.size() && isspace(static_cast<unsigned char>(pathsStr[pos]))) {
            pos++;
        }
        if (pos >= pathsStr.size()) {
            break;
        }

        size_t end = pathsStr.find(',', pos);
        if (end == std::string::npos) {
            end = pathsStr.size();
        }

        // Trim trailing whitespace.
        size_t trail = end;
        while (trail > pos && isspace(static_cast<unsigned char>(pathsStr[trail - 1]))) {
            trail--;
        }

        if (trail > pos) {
            std::string glob = pathsStr.substr(pos, trail - pos);
            g_globalScriptPaths.push_back(std::move(glob));
        }

        pos = end + 1;
    }

    return true;
}

const std::vector<std::string>& sfallGetGlobalScriptPaths()
{
    return g_globalScriptPaths;
}

// Config key for hook script directory.  hs_*.int files are auto-discovered
// in this directory and loaded as hook handler scripts.  Defaults to "scripts".
static constexpr const char* kHookScriptsPathKey = "HookScriptsPath";

// Parsed hook scripts path, populated by sfallParseHookScriptsPath().
static std::string g_hookScriptsPath;

/**
 * Parse HookScriptsPath from ddraw.ini [Misc] section.
 *
 * Sfall hs_*.int hook scripts are auto-discovered in the directory
 * specified by this key.  If absent or empty, defaults to "scripts".
 */
bool sfallParseHookScriptsPath()
{
    g_hookScriptsPath.clear();

    char* rawPath = nullptr;
    if (!configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, kHookScriptsPathKey, &rawPath)
        || rawPath == nullptr
        || rawPath[0] == '\0') {
        g_hookScriptsPath = "scripts"; // default
        return true;
    }

    g_hookScriptsPath = rawPath;
    return true;
}

const std::string& sfallGetHookScriptsPath()
{
    return g_hookScriptsPath;
}

/**
 * Load mods from the mod directory
 */
void sfallLoadMods()
{
    // SFALL: additional mods from the mods directory / mods_order.txt
    const char* modsPath = "mods";
    const char* loadOrderFilename = "mods_order.txt";

    char loadOrderFilepath[COMPAT_MAX_PATH];
    compat_makepath(loadOrderFilepath, nullptr, modsPath, loadOrderFilename, nullptr);

    // If the mods folder does not exist, create it.
    compat_mkdir(modsPath);

    // If load order file does not exist, initialize it automatically with mods already in the mods folder.
    if (compat_access(loadOrderFilepath, 0) != 0) {
        debugPrint("Generating Mods Order file based on the contents of Mods folder: %s\n", loadOrderFilepath);

        File* stream = fileOpen(loadOrderFilepath, "wt");
        if (stream != nullptr) {
            char** fileList;
            int fileListLength = fileNameListInit("mods\\*.dat", &fileList);

            for (int index = 0; index < fileListLength; index++) {
                fileWriteString(fileList[index], stream);
                fileWriteString("\n", stream);
            }
            fileClose(stream);
            fileNameListFree(&fileList, 0);
        }
    }

    // Add mods from load order file.
    File* stream = fileOpen(loadOrderFilepath, "r");
    if (stream != nullptr) {
        int numMods = 0;
        char mod[COMPAT_MAX_PATH];
        while (fileReadString(mod, COMPAT_MAX_PATH, stream)) {
            std::string modPath { mod };

            if (modPath.find_first_of(";#") != std::string::npos)
                continue; // skip comments

            // ltrim
            modPath.erase(modPath.begin(), std::find_if(modPath.begin(), modPath.end(), [](unsigned char ch) {
                return !isspace(ch);
            }));

            // rtrim
            modPath.erase(std::find_if(modPath.rbegin(), modPath.rend(), [](unsigned char ch) {
                return !isspace(ch);
            }).base(),
                modPath.end());

            if (modPath.empty())
                continue; // skip empty lines

            // Reject entries containing ".." path components to prevent
            // directory traversal out of the mods/ sandbox.
            if (modPath.find("..") != std::string::npos) {
                debugPrint("Rejecting unsafe mod entry (contains '..'): %s\n", modPath.c_str());
                continue;
            }

            char normalizedModPath[COMPAT_MAX_PATH];
            compat_makepath(normalizedModPath, nullptr, modsPath, modPath.c_str(), nullptr);

            if (compat_access(normalizedModPath, 0) == 0) {
                debugPrint("Loading mod %s\n", normalizedModPath);
                if (dbOpen(normalizedModPath, nullptr) != -1) {
                    numMods++;
                } else {
                    debugPrint("Error opening mod %s\n", normalizedModPath);
                }
            } else {
                debugPrint("Skipping invalid mod entry %s in %s\n", normalizedModPath, loadOrderFilepath);
            }
        }
        fileClose(stream);
        debugPrint("Loaded %d mods from %s\n", numMods, loadOrderFilepath);
    } else {
        debugPrint("Error opening %s for read\n", loadOrderFilepath);
    }

    // Load individually-named patch files from [ExtraPatches] section in
    // ddraw.ini.  Sfall uses PatchFile0, PatchFile1, ... keys to specify
    // additional .dat files.  Stop when a key is not found.
    int extraPatchesLoaded = 0;
    for (int patchIdx = 0; patchIdx < 100; patchIdx++) {
        char key[32];
        snprintf(key, sizeof(key), "%s%d", kExtraPatchesKeyPrefix, patchIdx);

        char* patchPath = nullptr;
        if (!configGetString(&gSfallConfig, kExtraPatchesSection, key, &patchPath)
            || patchPath == nullptr
            || patchPath[0] == '\0') {
            break; // no more entries
        }

        // Reject paths containing ".." to prevent directory traversal.
        if (compat_path_contains_traversal(patchPath)) {
            debugPrint("Rejecting unsafe ExtraPatches entry (contains '..'): %s\n", patchPath);
            continue;
        }

        if (compat_access(patchPath, 0) == 0) {
            debugPrint("Loading extra patch %s\n", patchPath);
            if (dbOpen(patchPath, nullptr) != -1) {
                extraPatchesLoaded++;
            } else {
                debugPrint("Error opening extra patch %s\n", patchPath);
            }
        } else {
            debugPrint("Skipping missing extra patch %s\n", patchPath);
        }
    }

    if (extraPatchesLoaded > 0) {
        debugPrint("Loaded %d extra patches from [%s]\n", extraPatchesLoaded, kExtraPatchesSection);
    }
}

// Binary layout of sfallgv.sav (must match sfall's SaveGame2 / LoadGame_Before order):
//   global vars | nextObjectId(4) | addedYears(4) | fakeTraitsCount(4) |
//   fakePerksCount(4) | fakeSelectablePerksCount(4) | arrays | drugPidsCount(4)
//
// Sections that CE doesn't implement are written/read as zero.

bool sfallSaveGameData(File* stream)
{
    // Store current runtime opcode state into the sfall global vars map
    // before serializing, so it's included in sfallgv.sav.
    sfallOpcodeStateSave();

    if (!sfall_gl_vars_save(stream)) {
        debugPrint("LOADSAVE (SFALL): ** Error saving global vars **\n");
        return false;
    }

    if (fileWriteInt32(stream, scriptsGetUniqueObjectIdCounter()) == -1) {
        debugPrint("LOADSAVE (SFALL): ** Error saving next object id **\n");
        return false;
    }

    // Write zeros for CE-unimplemented fields: addedYears, fakeTraitsCount,
    // fakePerksCount, fakeSelectablePerksCount
    int32_t zero = 0;
    for (int32_t i = 0; i < 4; i++) {
        if (fileWrite(&zero, sizeof(zero), 1, stream) != 1) {
            debugPrint("LOADSAVE (SFALL): ** Error saving stub fields **\n");
            return false;
        }
    }

    if (!sfallArraysSave(stream)) {
        debugPrint("LOADSAVE (SFALL): ** Error saving arrays **\n");
        return false;
    }

    if (fileWrite(&zero, sizeof(zero), 1, stream) != 1) { // drugPidsCount
        debugPrint("LOADSAVE (SFALL): ** Error saving drug pids **\n");
        return false;
    }

    // Append metarule state after the legacy binary sections.
    if (!sfall_metarules_save(stream)) {
        debugPrint("LOADSAVE (SFALL): ** Error saving metarule state **\n");
        return false;
    }

    return true;
}

// F-038: sfallLoadGameData is called from loadsave.cc:2098, which runs
// AFTER the 27 engine SAVE.DAT load handlers (loadsave.cc:2073-2087) but
// BEFORE global script start procedures (loadsave.cc:2128).
//
// Load order within this function:
//   1. sfall_gl_vars_load        — globals (immediately available)
//   2. sfallOpcodeStateLoad      — opcode state from globals
//   3. nextObjectId, skipped sections, drugPidsCount — binary fields
//   4. sfallArraysLoad           — sfall arrays (available from here onward)
//   5. sfall_metarules_load      — metarule state
//
// Consequence: sfall arrays are NOT available during the 27-handler window
// (step 1 above has not run yet when handlers fire). This is practically
// benign because vanilla engine SAVE.DAT handlers do not access sfall arrays —
// they only load engine-level game objects, critter data, and map state.
// sfall start-procedure scripts (which DO use arrays) run AFTER this function
// completes, so arrays are fully available to global/start scripts.
bool sfallLoadGameData(File* stream)
{
    if (!sfall_gl_vars_load(stream)) {
        debugPrint("LOADSAVE (SFALL): ** Error loading global vars **\n");
        return false;
    }

    // Restore opcode globals from the loaded sfall global vars map.
    sfallOpcodeStateLoad();

    int32_t nextObjectId;
    if (fileReadInt32(stream, &nextObjectId) == -1) {
        scriptsRestoreUniqueObjectIdCounter(OBJECT_ID_UNIQUE_START);
        return true; // old save, stop gracefully
    }

    // Skip sections CE doesn't implement: addedYears, fakeTraitsCount,
    // fakePerksCount, fakeSelectablePerksCount
    int32_t ignored;
    for (int32_t i = 0; i < 4; i++) {
        if (fileRead(&ignored, sizeof(ignored), 1, stream) != 1) {
            scriptsRestoreUniqueObjectIdCounter(nextObjectId);
            return true; // old save, stop gracefully
        }
    }

    if (!sfallArraysLoad(stream)) {
        // Corrupted save.
        debugPrint("LOADSAVE (SFALL): ** Error loading arrays **\n");
        return false;
    }

    // Consume drugPidsCount (4 bytes, written by sfallSaveGameData).
    // This field is unused by CE but its bytes must be consumed so that
    // sfall_metarules_load() reads its own version marker, not garbage.
    int32_t ignoredDrugPidsCount;
    if (fileReadInt32(stream, &ignoredDrugPidsCount) == -1) {
        debugPrint("LOADSAVE (SFALL): ** Error reading drug pids **\n");
        scriptsRestoreUniqueObjectIdCounter(nextObjectId);
        return false;
    }

    scriptsRestoreUniqueObjectIdCounter(nextObjectId);

    // Read the trailing metarule state section (may not exist in older saves).
    // sfall_metarules_load() gracefully checks its own version marker.
    if (!sfall_metarules_load(stream)) {
        debugPrint("LOADSAVE (SFALL): ** Error loading metarules **\n");
        return false;
    }

    return true;
}

} // namespace fallout
