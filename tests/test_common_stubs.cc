// Stubs for symbols needed by source files under test.
// These provide minimal no-op implementations so tests can link
// without pulling in the entire engine dependency graph.
//
// Functions that are NOT tested (e.g. file I/O) return failure values.
// The actual unit tests exercise only the in-memory data structure operations.

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "db.h"
#include "debug.h"
#include "platform_compat.h"
#include "settings.h"

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
void compat_splitpath(const char* /*path*/, char* /*drive*/, char* /*dir*/,
                      char* /*fname*/, char* /*ext*/)
{
    // Stub: do nothing — migration tests don't depend on real path decomposition.
}

// game_config_migration.cc uses compat_makepath to construct paths.
void compat_makepath(char* /*path*/, const char* /*drive*/, const char* /*dir*/,
                     const char* /*fname*/, const char* /*ext*/)
{
    // Stub: do nothing — migration tests don't depend on real path construction.
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

// ---- Global variables defined by the engine ----
// game_config_migration.cc references Settings settings and gSfallConfig.
namespace fallout {
    Settings settings;
    Config gSfallConfig;
}
