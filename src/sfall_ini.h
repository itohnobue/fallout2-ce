#ifndef FALLOUT_SFALL_INI_H_
#define FALLOUT_SFALL_INI_H_

#include "config.h"
#include "dictionary.h"
#include "interpreter.h"
#include "opcode_context.h"
#include <cstddef>

namespace fallout {

// Sets base directory to lookup .ini files.
void sfall_ini_set_base_path(const char* path);

// Clears all cached .ini configs and config arrays. Called on game reset.
void sfall_ini_cache_clear();

// Reads integer key identified by "fileName|section|key" triplet into `value`.
bool sfall_ini_get_int(const char* triplet, int* value);

// Reads string key identified by "fileName|section|key" triplet into `value`.
// Returns true if the triplet was parsed successfully; sets *found (if non-null)
// to indicate whether the key was actually present in the INI/file.
bool sfall_ini_get_string(const char* triplet, char* value, size_t size, bool* found = nullptr);

// Writes integer key identified by "fileName|section|key" triplet.
bool sfall_ini_set_int(const char* triplet, int value);

// Writes string key identified by "fileName|section|key" triplet.
bool sfall_ini_set_string(const char* triplet, const char* value);

// metarule and opcode implementations
void mf_set_ini_setting(OpcodeContext& ctx);
void mf_get_ini_section(OpcodeContext& ctx);
void mf_get_ini_sections(OpcodeContext& ctx);
void mf_get_ini_config(OpcodeContext& ctx);
void op_get_ini_setting(Program* program);
void op_get_ini_string(Program* program);

// ============================================================
// TEST-ONLY: Config injection for sfall_ini cache (I2-M68, I2-M75).
// Injects a Config under the given fileName into the internal
// ini cache so tests can exercise strtol overflow, LP64 branches,
// and end-to-end config lookup without needing real file I/O.
// Returns the Config* for further population via configSetString etc.
// The cache takes ownership; the pointer stays valid until cache_clear.
// Guarded behind TEST_ACCESSORS_ENABLED — test define before include.
// ============================================================
#if defined(TEST_ACCESSORS_ENABLED)
Config* sfall_ini_inject_config_for_test(const char* fileName);
#endif

} // namespace fallout

#endif /* FALLOUT_SFALL_INI_H_ */
