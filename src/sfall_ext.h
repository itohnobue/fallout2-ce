#ifndef SFALL_EXT_H
#define SFALL_EXT_H

#include <string>
#include <vector>

struct XFile;
typedef XFile File;

namespace fallout {

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

} // namespace fallout

#endif /* SFALL_EXT_H */
