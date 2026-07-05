// Stubs for symbols needed by source files under test.
// These provide minimal no-op implementations so tests can link
// without pulling in the entire engine dependency graph.
//
// Functions that are NOT tested (e.g. file I/O) return failure values.
// The actual unit tests exercise only the in-memory data structure operations.

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "db.h"
#include "debug.h"
#include "interpreter.h"
#include "object.h"
#include "platform_compat.h"
#include "settings.h"
#include "sfall_script_hooks.h"

namespace fallout {

// =============================================================
// debug.h stubs
// =============================================================

// memory.cc calls debugPrint for error messages; no-op for tests.
int debugPrint(const char* /*format*/, ...)
{
    return 0;
}

// =============================================================
// platform_compat.h stubs
// =============================================================

// dictionary.cc uses compat_stricmp for case-insensitive binary search.
int compat_stricmp(const char* s1, const char* s2)
{
    while (*s1 && *s2) {
        int c1 = static_cast<unsigned char>(*s1);
        int c2 = static_cast<unsigned char>(*s2);
        if (c1 >= 'A' && c1 <= 'Z')
            c1 += 'a' - 'A';
        if (c2 >= 'A' && c2 <= 'Z')
            c2 += 'a' - 'A';
        if (c1 != c2)
            return c1 - c2;
        s1++;
        s2++;
    }
    return static_cast<unsigned char>(*s1) - static_cast<unsigned char>(*s2);
}

// config.cc uses compat_itoa for int-to-string conversion (configSetInt).
char* compat_itoa(int value, char* buffer, int /*radix*/)
{
    snprintf(buffer, 20, "%d", value);
    return buffer;
}

// config.cc uses compat_fopen for configRead/configWrite file I/O.
// Stub: always returns nullptr (file not found).
FILE* compat_fopen(const char* /*path*/, const char* /*mode*/)
{
    return nullptr;
}

// config.cc and dictionary.cc use compat_fgets.
// Stub: always returns nullptr (EOF/error).
char* compat_fgets(char* /*buffer*/, int /*maxCount*/, FILE* /*stream*/)
{
    return nullptr;
}

// config.cc uses compat_remove for configWriteSideBySide.
int compat_remove(const char* /*path*/)
{
    return -1;
}

// config.cc uses compat_rename for configWriteSideBySide.
int compat_rename(const char* /*oldName*/, const char* /*newName*/)
{
    return -1;
}

// game_config_migration.cc uses compat_splitpath to decompose paths.
void compat_splitpath(const char* /*path*/, char* drive, char* dir,
                      char* fname, char* ext)
{
    // Stub: zero-initialize output buffers to produce empty strings
    // for all path components. Matches the real implementation's contract
    // of producing null-terminated empty strings when a component is absent.
    // See platform_compat.cc:71-140 for the real implementation.
    if (drive != nullptr) drive[0] = '\0';
    if (dir != nullptr) dir[0] = '\0';
    if (fname != nullptr) fname[0] = '\0';
    if (ext != nullptr) ext[0] = '\0';
}

// game_config_migration.cc uses compat_makepath to construct paths.
void compat_makepath(char* path, const char* /*drive*/, const char* /*dir*/,
                     const char* /*fname*/, const char* /*ext*/)
{
    // Stub: zero-initialize the output path to produce an empty string.
    // See platform_compat.cc:142-220 for the real implementation.
    if (path != nullptr) path[0] = '\0';
}

// game_config_migration.cc uses compat_file_exists to check for existing configs.
bool compat_file_exists(const char* /*filePath*/)
{
    return false;
}

// game_config_migration.cc uses compat_mkdir_recursive for output directory creation.
int compat_mkdir_recursive(const char* /*path*/)
{
    return -1;
}

// game_config_migration.cc uses compat_is_dir to validate paths.
bool compat_is_dir(const char* /*path*/)
{
    return false;
}

// =============================================================
// db.h / xfile.h stubs (File I/O)
// =============================================================

// config.cc uses fileOpen for DAT-based config reading.
File* fileOpen(const char* /*filename*/, const char* /*mode*/)
{
    return nullptr;
}

// config.cc uses fileClose.
int fileClose(File* /*stream*/)
{
    return -1;
}

// sfall_global_vars.cc uses fileRead for save/load.
size_t fileRead(void* /*buf*/, size_t /*size*/, size_t /*count*/, File* /*stream*/)
{
    return 0;
}

// sfall_global_vars.cc uses fileWrite for save.
size_t fileWrite(const void* /*buf*/, size_t /*size*/, size_t /*count*/, File* /*stream*/)
{
    return 0;
}

// config.cc uses fileReadString for configRead.
char* fileReadString(char* /*str*/, size_t /*size*/, File* /*stream*/)
{
    return nullptr;
}

// config.cc uses filePrintFormatted for configWriteDb.
int filePrintFormatted(File* /*stream*/, const char* /*format*/, ...)
{
    return 0;
}

} // namespace fallout

// =============================================================
// interpreter.h stubs (inside namespace fallout)
// =============================================================

// sfall_opcodes test calls hookOpcodeGetCurrentCall, which calls
// programPrintError when no ScriptHookCall is active.
namespace fallout {
    void programPrintError(const char* /*format*/, ...)
    {
        // no-op: tests don't validate error message content
    }
} // namespace fallout

// =============================================================
// sfall_script_hooks.h stubs
// =============================================================

// test_sfall_opcodes calls hookOpcodeGetCurrentCall, which calls
// ScriptHookCall::current(). In unit test context there is no active
// hook call, so return nullptr.
namespace fallout {
    ScriptHookCall* ScriptHookCall::current()
    {
        return nullptr;
    }
} // namespace fallout

// ---- Global variables defined by the engine ----
// game_config_migration.cc references Settings settings and gSfallConfig.
// NOTE: gSfallConfig is defined in sfall_config.cc (in test_sources).
// Do NOT redefine here — causes duplicate symbol when linking test_sources.
namespace fallout {
    Settings settings;
    // Config gSfallConfig; — MOVED to sfall_config.cc (via test_sources)

    // sfall_opcodes.cc extern globals — defined here so test_sfall_opcodes
    // can be self-contained without linking sfall_opcodes.cc (150+ engine deps).
    int gPerkFrequencyOverride = 0;
    int gSkillPointsPerLevelMod = 0;
    int gLastAttacker = -1;
    int gLastTarget = -1;
    int gSkillMaxCap = 300;
    int gXpModPercentage = 100;
}

// =============================================================
// memory_manager.h stubs (safe allocation wrappers)
// =============================================================
// test_window.cc uses the _safe variants that include file/line
// tracking. Minimal forwarding stubs.

namespace fallout {
    void* internal_malloc_safe(size_t size, const char* /*file*/, int /*line*/)
    {
        return malloc(size);
    }

    void* internal_realloc_safe(void* ptr, size_t size, const char* /*file*/, int /*line*/)
    {
        return realloc(ptr, size);
    }

    void internal_free_safe(void* ptr, const char* /*file*/, int /*line*/)
    {
        free(ptr);
    }
} // namespace fallout

// =============================================================
// interpreter.h stubs — ProgramValue (used by sfall_arrays.cc)
// =============================================================
namespace fallout {

    ProgramValue::ProgramValue()
        : opcode(0), integerValue(0) {}

    ProgramValue::ProgramValue(int value)
        : opcode(0), integerValue(value) {}

    ProgramValue::ProgramValue(unsigned int value)
        : opcode(0), integerValue(static_cast<int>(value)) {}

    ProgramValue::ProgramValue(bool value)
        : opcode(0), integerValue(value ? 1 : 0) {}

    ProgramValue::ProgramValue(float value)
        : opcode(0), floatValue(value) {}

    ProgramValue::ProgramValue(Object* /*value*/)
        : opcode(0), pointerValue(nullptr) {}

    ProgramValue::ProgramValue(Attack* /*value*/)
        : opcode(0), pointerValue(nullptr) {}

    ProgramValue::ProgramValue(const char* /*value*/)
        : opcode(0), pointerValue(nullptr) {}

    bool ProgramValue::isEmpty() const { return opcode == 0; }
    bool ProgramValue::isInt() const { return false; }
    bool ProgramValue::isFloat() const { return false; }
    bool ProgramValue::isString() const { return false; }
    float ProgramValue::asFloat() const { return 0.0f; }
    bool ProgramValue::isPointer() const { return false; }
    int ProgramValue::asInt() const { return integerValue; }
    Object* ProgramValue::asObject() const { return nullptr; }
    const char* ProgramValue::asString(Program*) const { return nullptr; }
    const char* ProgramValue::typeDebugString() const { return "UNKNOWN"; }

} // namespace fallout

// =============================================================
// sfall_opcodes.h stubs — lifecycle functions
// test_sfall_opcodes.cc calls these but we no longer link
// sfall_opcodes.cc (it has 150+ engine deps).
// =============================================================
namespace fallout {

    void sfallVfsCloseAll()
    {
        // No-op: VFS handle slots are not available in test context.
    }

    void sfallAnimCallbackReset()
    {
        // No-op: animation callback state is not available in test context.
    }

    void sfallOpcodesReset()
    {
        // Reset extern globals to their documented defaults.
        // Matches sfall_opcodes.cc lines ~3460-3475.
        gPerkFrequencyOverride = 0;
        gSkillPointsPerLevelMod = 0;
        gLastAttacker = -1;
        gLastTarget = -1;
        gSkillMaxCap = 300;
        gXpModPercentage = 100;
    }

    void sfallOpcodesExit()
    {
        // Matches sfall_opcodes.cc lines ~4163-4167.
        sfallAnimCallbackReset();
        sfallVfsCloseAll();
    }

    ScriptHookCall* hookOpcodeGetCurrentCall(const char* /*opcodeName*/)
    {
        // In test context there is no active hook call — return nullptr.
        return nullptr;
    }

} // namespace fallout
