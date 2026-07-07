#ifndef SFALL_EXT_H
#define SFALL_EXT_H

#include <string>
#include <vector>

namespace fallout {

struct XFile;
typedef XFile File;

void sfallLoadMods();
bool sfallSaveGameData(File* stream);
bool sfallLoadGameData(File* stream);

// Parses GlobalScriptPaths from ddraw.ini [Misc] into a comma-separated list of
// path globs.  Returns true on success (empty list is not an error — it means
// the key was absent or empty, and the caller should use its default).
bool sfallParseGlobalScriptPaths();

// Returns the parsed global script path globs (e.g. "scripts\\gl*.int",
// "scripts\\sfall\\gl*.int").  Call sfallParseGlobalScriptPaths() first.
const std::vector<std::string>& sfallGetGlobalScriptPaths();

// Parses HookScriptsPath from ddraw.ini [Misc].  Returns the path string
// (defaults to "scripts" if the key is absent or empty).  This directory
// is where hs_*.int hook script files are auto-discovered and loaded.
// Call sfallParseHookScriptsPath() before sfall_gl_scr_load_hook_scripts().
bool sfallParseHookScriptsPath();

// Returns the parsed hook scripts path.  Call sfallParseHookScriptsPath() first.
const std::string& sfallGetHookScriptsPath();

} // namespace fallout

#endif /* SFALL_EXT_H */
