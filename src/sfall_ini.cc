#include "sfall_ini.h"

#include <algorithm>
#include <cerrno> // for errno, ERANGE
#include <climits> // for INT_MAX, INT_MIN
#include <cstdio> // for snprintf
#include <cstdlib> // for strtol
#include <cstring> // for strncpy, strlen
#include <memory>
#include <string>
#include <unordered_map>

#include "config.h"
#include "content_config.h"
#include "debug.h"
#include "interpreter.h"
#include "opcode_context.h"
#include "platform_compat.h"
#include "sfall_arrays.h"
#include "sfall_config.h"

namespace fallout {

// The max length of `fileName` chunk in the triplet.
static constexpr size_t kFileNameMaxSize = 63;

// The max length of `section` chunk in the triplet.
static constexpr size_t kSectionMaxSize = 32;

// Special .ini file names which are accessed without adding base path.
static constexpr const char* kSystemConfigFileNames[] = {
    "ddraw.ini",
    "f2_res.ini",
};

static char basePath[COMPAT_MAX_PATH];

// Parses "fileName|section|key" triplet into parts. `fileName` and `section`
// chunks are copied into appropriate variables. Returns the pointer to `key`,
// or `nullptr` on any error.
static const char* parse_ini_triplet(const char* triplet, char* fileName, char* section)
{
    const char* fileNameSectionSep = strchr(triplet, '|');
    if (fileNameSectionSep == nullptr) {
        return nullptr;
    }

    size_t fileNameLength = fileNameSectionSep - triplet;
    if (fileNameLength >= kFileNameMaxSize) {
        return nullptr;
    }

    const char* sectionKeySep = strchr(fileNameSectionSep + 1, '|');
    if (sectionKeySep == nullptr) {
        return nullptr;
    }

    size_t sectionLength = sectionKeySep - fileNameSectionSep - 1;
    if (sectionLength >= kSectionMaxSize) {
        return nullptr;
    }

    strncpy(fileName, triplet, fileNameLength);
    fileName[fileNameLength] = '\0';

    // Reject filenames containing ".." to prevent directory traversal
    // out of the mods/config directory. After stripping path separators
    // and parent-directory references, a traversal attempt like
    // "..\\system.ini" or "mods\\..\\ddraw.ini" would bypass access
    // controls, so we reject any filename component that contains "..".
    // This matches the existing guard in VFS and mod loading paths.
    if (strstr(fileName, "..") != nullptr) {
        return nullptr;
    }

    // Reject Windows reserved device names (CON, NUL, PRN, AUX,
    // COM1-COM9, LPT1-LPT9) to prevent potential file access issues
    // on Windows. The check matches the base name (without extension)
    // case-insensitively against the reserved list.
    {
        const char* dot = strrchr(fileName, '.');
        size_t baseLen = dot ? static_cast<size_t>(dot - fileName) : fileNameLength;

        static const char* kReservedNames[] = {
            "CON", "NUL", "PRN", "AUX",
            "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
            "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
        };

        for (const auto& reserved : kReservedNames) {
            size_t rlen = strlen(reserved);
            if (baseLen == rlen && compat_strnicmp(fileName, reserved, rlen) == 0) {
                return nullptr;
            }
        }
    }

    strncpy(section, fileNameSectionSep + 1, sectionLength);
    section[sectionLength] = '\0';

    return sectionKeySep + 1;
}

// Returns `true` if given `fileName` is a special system .ini file name.
static bool is_system_file_name(const char* fileName)
{
    for (auto& systemFileName : kSystemConfigFileNames) {
        if (compat_stricmp(systemFileName, fileName) == 0) {
            return true;
        }
    }

    return false;
}

// Reads the INI file into `config`, trying the base directory first for
// non-system files and falling back to the working directory on failure.
static bool sfall_read_named_ini(const char* iniFileName, Config* config)
{
    if (basePath[0] != '\0' && !is_system_file_name(iniFileName)) {
        char path[COMPAT_MAX_PATH];
        int pathResult = snprintf(path, sizeof(path), "%s\\%s", basePath, iniFileName);
        if (pathResult >= 0 && pathResult < (int)sizeof(path) && configRead(config, path, false)) {
            return true;
        }
    }

    return configRead(config, iniFileName, false);
}

void sfall_ini_set_base_path(const char* path)
{
    if (path != nullptr) {
        strncpy(basePath, path, COMPAT_MAX_PATH - 1);
        basePath[COMPAT_MAX_PATH - 1] = '\0';

        size_t length = strlen(basePath);
        if (length > 0) {
            if (basePath[length - 1] == '\\' || basePath[length - 1] == '/') {
                basePath[length - 1] = '\0';
            }
        }
    } else {
        basePath[0] = '\0';
    }
}

// Frees a cached `Config` via `configFree` before releasing it.
struct ConfigDeleter {
    void operator()(Config* config) const
    {
        if (config != nullptr) {
            if (config->isInitialized()) {
                configFree(config);
            }
            delete config;
        }
    }
};

using CachedConfigPtr = std::unique_ptr<Config, ConfigDeleter>;

// Maps a requested .ini file name to its parsed `Config`. A `nullptr` value
// marks a file that could not be read, so it isn't retried.
static std::unordered_map<std::string, CachedConfigPtr> iniConfigCache;

// Maps a requested .ini file name to a persistent array of its contents, so
// repeated calls return the same array. Separate caches for file and DAT reads.
static std::unordered_map<std::string, ArrayId> iniConfigArrayCache;
static std::unordered_map<std::string, ArrayId> iniConfigArrayCacheDat;

// Returns the parsed `Config` for `iniFileName`, reading and caching it on first
// access. Returns `nullptr` if the file cannot be read (the miss is cached).
static Config* sfall_get_ini_config(const char* iniFileName)
{
    if (iniFileName == nullptr) {
        return nullptr;
    }

    auto cacheHit = iniConfigCache.find(iniFileName);
    if (cacheHit != iniConfigCache.end()) {
        return cacheHit->second.get();
    }

    CachedConfigPtr config(new Config());
    if (!configInit(config.get()) || !sfall_read_named_ini(iniFileName, config.get())) {
        // Negative cache: remember that this file could not be read.
        iniConfigCache.emplace(iniFileName, nullptr);
        return nullptr;
    }

    return iniConfigCache.emplace(iniFileName, std::move(config)).first->second.get();
}

// Returns `false` on triplet parse or config initialization error.
// Returns `true` otherwise, copies the setting value into `value` (or empty
// string if the setting is missing), and optionally reports whether the key was
// found via `found`.
static bool sfall_ini_get_string_internal(const char* triplet, char* value, size_t size, bool* found)
{
    char fileName[kFileNameMaxSize];
    char section[kSectionMaxSize];
    const char* key = parse_ini_triplet(triplet, fileName, section);
    if (key == nullptr) {
        return false;
    }

    if (found != nullptr) {
        *found = false;
    }

    if (size == 0) {
        return false;
    }

    Config* config = sfall_get_ini_config(fileName);

    // NOTE: Sfall's `GetIniSetting` returns error code (-1) only when it cannot
    // parse triplet. Otherwise the default for string settings is empty string.
    value[0] = '\0';

    if (config != nullptr) {
        char* stringValue;
        if (configGetString(config, section, key, &stringValue)) {
            strncpy(value, stringValue, size - 1);
            value[size - 1] = '\0';
            if (found != nullptr) {
                *found = true;
            }
        }
    }

    return true;
}

// Return codes for sfall_ini_get_int_detailed to distinguish failure modes
// at the script level. Matches sfall convention for RPU compatibility:
//   -1 = key not found (file and section exist, key doesn't)
//   -2 = file not found / cannot be read
//   -3 = invalid triplet format ("file|section|key" parse failure)
enum {
    SFALL_INI_OK = 0,
    SFALL_INI_KEY_NOT_FOUND = -1,
    SFALL_INI_FILE_NOT_FOUND = -2,
    SFALL_INI_PARSE_ERROR = -3
};

// Reads integer key identified by "fileName|section|key" triplet into `value`,
// returning a detailed status code that distinguishes missing-key from
// missing-file from parse-error. See SFALL_INI_* enum above.
static int sfall_ini_get_int_detailed(const char* triplet, int* value)
{
    char fileName[kFileNameMaxSize];
    char section[kSectionMaxSize];
    const char* key = parse_ini_triplet(triplet, fileName, section);
    if (key == nullptr) {
        return SFALL_INI_PARSE_ERROR;
    }

    Config* config = sfall_get_ini_config(fileName);
    if (config == nullptr) {
        return SFALL_INI_FILE_NOT_FOUND;
    }

    char* stringValue;
    if (!configGetString(config, section, key, &stringValue)) {
        return SFALL_INI_KEY_NOT_FOUND;
    }

    // Use strtol with errno+ERANGE to avoid undefined behavior on overflow.
    // Saturated to INT_MAX/INT_MIN on overflow — scripts receive the closest
    // representable value rather than UB or rejection (matching the robustness
    // goal of the existing configGetInt pattern at config.cc:232-244).
    char* end;
    errno = 0;
    long l = strtol(stringValue, &end, 10);
    if (end == stringValue) {
        *value = 0;
    } else if (errno == ERANGE || l < INT_MIN || l > INT_MAX) {
        *value = (l == LONG_MAX || l > INT_MAX) ? INT_MAX : INT_MIN;
    } else {
        *value = static_cast<int>(l);
    }
    return SFALL_INI_OK;
}

// false: on error
// true: on key found
// If the key exists but is the empty string, returns true and value to 0
bool sfall_ini_get_int(const char* triplet, int* value)
{
    return sfall_ini_get_int_detailed(triplet, value) == SFALL_INI_OK;
}

bool sfall_ini_get_string(const char* triplet, char* value, size_t size)
{
    return sfall_ini_get_string_internal(triplet, value, size, nullptr);
}

bool sfall_ini_set_int(const char* triplet, int value)
{
    char stringValue[20];
    compat_itoa(value, stringValue, 10);

    return sfall_ini_set_string(triplet, stringValue);
}

bool sfall_ini_set_string(const char* triplet, const char* value)
{
    char fileName[kFileNameMaxSize];
    char section[kSectionMaxSize];

    const char* key = parse_ini_triplet(triplet, fileName, section);
    if (key == nullptr) {
        return false;
    }

    // Invalidate cached data so subsequent reads pick up the new value. The
    // persistent array is left for game reset to free (it holds nested sub-array
    // IDs and may still be referenced by a script).
    iniConfigCache.erase(fileName);
    iniConfigArrayCache.erase(fileName);

    ScopedConfig config;
    if (!config) {
        return false;
    }

    char path[COMPAT_MAX_PATH];
    bool loaded = false;

    if (basePath[0] != '\0' && !is_system_file_name(fileName)) {
        // Attempt to load requested file in base directory.
        int result = snprintf(path, sizeof(path), "%s\\%s", basePath, fileName);
        if (result >= 0 && result < (int)sizeof(path)) {
            loaded = configRead(config.get(), path, false);
        }
    }

    if (!loaded) {
        // There was no base path set, requested file is a system config, or
        // non-system config file was not found the base path - attempt to load
        // from current working directory.
        strcpy(path, fileName);
        loaded = configRead(config.get(), path, false);
    }

    configSetString(config.get(), section, key, value);

    bool saved = configWrite(config.get(), path, false);

    return saved;
}

static const ConfigSection* sfall_find_section_in_config(Config* config, const char* section_name)
{
    if (config == nullptr || section_name == nullptr) {
        return nullptr;
    }

    int sectionIndex = dictionaryGetIndexByKey(config, section_name);
    if (sectionIndex == -1) {
        return nullptr;
    }

    DictionaryEntry* sectionEntry = &(config->entries[sectionIndex]);
    return static_cast<const ConfigSection*>(sectionEntry->value);
}

// Copies a config section's key/value pairs into an associative array.
static void copyConfigSectionToArray(ArrayId arrayId, const ConfigSection* section, Program* program)
{
    for (int i = 0; i < section->entriesLength; ++i) {
        const DictionaryEntry* entry = &(section->entries[i]);
        const char* key = entry->key;
        const char* value = *(static_cast<char**>(entry->value));

        if (key != nullptr && value != nullptr) {
            SetArray(arrayId, programMakeString(program, key), programMakeString(program, value), false, program);
        }
    }
}

// set_ini_setting
void mf_set_ini_setting(OpcodeContext& ctx)
{
    const char* triplet = ctx.stringArg(0);
    ProgramValue value = ctx.arg(1);

    bool wrote = false;
    if (value.isString()) {
        const char* stringValue = value.asString(ctx.program());
        wrote = sfall_ini_set_string(triplet, stringValue);
        if (!wrote) {
            debugPrint("set_ini_setting: unable to write '%s' to '%s'",
                stringValue,
                triplet);
        }
    } else {
        int integerValue = value.asInt();
        wrote = sfall_ini_set_int(triplet, integerValue);
        if (!wrote) {
            debugPrint("set_ini_setting: unable to write '%d' to '%s'",
                integerValue,
                triplet);
        }
    }

    // After writing to disk, update in-memory globals so they reflect
    // the current config value. Note: opcode registration gating is a
    // one-time init operation that runs before any script executes, so
    // runtime toggles for those globals only take effect after a restart.
    // See sfall_config.cc for key definitions.
    if (wrote) {
        int intVal = value.isString() ? atoi(value.asString(ctx.program())) : value.asInt();

        if (compat_stricmp(triplet, "ddraw.ini|Debugging|AllowUnsafeScripting") == 0) {
            gAllowUnsafeScripting = (intVal != 0);
        } else if (compat_stricmp(triplet, "ddraw.ini|Misc|Fallout1Behavior") == 0) {
            gFallout1Behavior = (intVal != 0);
            configSetInt(&gContentConfig, CONTENT_CONFIG_START_SECTION, "fallout1_behavior", intVal != 0 ? 1 : 0);
        } else if (compat_stricmp(triplet, "ddraw.ini|Misc|ExtraSaveSlots") == 0) {
            gExtraSaveSlots = (intVal != 0);
        } else if (compat_stricmp(triplet, "ddraw.ini|Misc|EnableHeroAppearanceMod") == 0) {
            gEnableHeroAppearanceMod = (intVal != 0);
        }
    }
}

// get_ini_section
void mf_get_ini_section(OpcodeContext& ctx)
{
    // Arguments: file_path (string), section_name (string)

    const char* filePath = ctx.stringArg(0);
    const char* sectionName = ctx.stringArg(1);

    ArrayId arrayId = CreateTempArray(-1, 0);

    if (filePath == nullptr || sectionName == nullptr) {
        ctx.setReturn(arrayId);
        return;
    }

    Config* config = sfall_get_ini_config(filePath);
    if (config != nullptr) {
        const ConfigSection* section = sfall_find_section_in_config(config, sectionName);
        if (section != nullptr) {
            copyConfigSectionToArray(arrayId, section, ctx.program());
        }
    }

    ctx.setReturn(arrayId);
}

// get_ini_sections
void mf_get_ini_sections(OpcodeContext& ctx)
{
    // Arguments: file_path (string)
    const char* filePath = ctx.stringArg(0);
    ArrayId arrayId = -1;

    if (filePath == nullptr) {
        ctx.setReturn(arrayId);
        return;
    }

    Config* config = sfall_get_ini_config(filePath);
    if (config != nullptr && config->entriesLength > 0) {
        arrayId = CreateTempArray(config->entriesLength, 0);
        for (int i = 0; i < config->entriesLength; ++i) {
            const DictionaryEntry* entry = &(config->entries[i]);
            const char* sectionName = entry->key;

            if (sectionName != nullptr) {
                SetArray(arrayId, programMakeInt(ctx.program(), i), programMakeString(ctx.program(), sectionName), false, ctx.program());
            }
        }
    }

    if (arrayId == -1) {
        arrayId = CreateTempArray(0, 0);
    }

    ctx.setReturn(arrayId);
}

// Builds a persistent associative array mapping each section name to a nested
// associative array of that section's key/value pairs.
static ArrayId sfall_build_config_array(const Config* config, Program* program)
{
    ArrayId arrayId = CreateArray(-1, 0);

    for (int i = 0; i < config->entriesLength; ++i) {
        const DictionaryEntry* sectionEntry = &(config->entries[i]);
        const auto* section = static_cast<const ConfigSection*>(sectionEntry->value);

        ArrayId sectionArrayId = CreateArray(-1, 0);
        copyConfigSectionToArray(sectionArrayId, section, program);

        SetArray(arrayId, programMakeString(program, sectionEntry->key), ProgramValue(sectionArrayId), false, program);
    }

    return arrayId;
}

// get_ini_config
void mf_get_ini_config(OpcodeContext& ctx)
{
    // Arguments: file_name (string), from_dat (bool)
    const char* fileName = ctx.stringArg(0);
    bool isDb = ctx.arg(1).asInt() != 0;

    if (fileName == nullptr || fileName[0] == '\0') {
        ctx.printError("%s() - invalid config file path.", ctx.name());
        ctx.setReturn(0);
        return;
    }

    // Return the cached array if it still exists; otherwise drop the stale entry and rebuild.
    auto& arrayCache = isDb ? iniConfigArrayCacheDat : iniConfigArrayCache;
    auto cacheHit = arrayCache.find(fileName);
    if (cacheHit != arrayCache.end()) {
        if (ArrayExists(cacheHit->second)) {
            ctx.setReturn(cacheHit->second);
            return;
        }
        arrayCache.erase(cacheHit);
    }

    ArrayId arrayId;
    if (isDb) {
        // DAT reads bypass the shared config cache, so read into a local config here.
        ScopedConfig dbConfig(fileName, true);
        if (!dbConfig) {
            ctx.printError("%s() - cannot read config file from DAT: %s", ctx.name(), fileName);
            ctx.setReturn(0);
            return;
        }
        arrayId = sfall_build_config_array(dbConfig.get(), ctx.program());
    } else {
        Config* config = sfall_get_ini_config(fileName);
        if (config == nullptr) {
            ctx.printError("%s() - cannot read config file: %s", ctx.name(), fileName);
            ctx.setReturn(0);
            return;
        }
        arrayId = sfall_build_config_array(config, ctx.program());
    }

    arrayCache.emplace(fileName, arrayId);
    ctx.setReturn(arrayId);
}

// get_ini_setting
void op_get_ini_setting(Program* program)
{
    const char* string = programStackPopString(program);

    // Parse the triplet to identify section and key for fallback lookup.
    char fileName[kFileNameMaxSize];
    char section[kSectionMaxSize];
    const char* keyPtr = parse_ini_triplet(string, fileName, section);

    int value;
    int status = SFALL_INI_PARSE_ERROR;
    if (keyPtr != nullptr) {
        status = sfall_ini_get_int_detailed(string, &value);
    }

    if (status == SFALL_INI_OK) {
        programStackPushInteger(program, value);
    } else if (keyPtr != nullptr) {
        // Config bridge: when a ddraw.ini key is not found, fall back to
        // gContentConfig (game.cfg). This enables RPU/Et Tu scripts that
        // call get_ini_setting("ddraw.ini|Misc|BoostScriptDialogLimit") to
        // receive the correct value from the migrated config even when
        // ddraw.ini does not exist on disk.
        int fallbackValue = contentConfigLookupSfallInt(section, keyPtr);
        if (fallbackValue >= 0) {
            programStackPushInteger(program, fallbackValue);
        } else {
            programStackPushInteger(program, -1);
        }
    } else {
        // Return -1 for ALL error conditions (key not found, file not found,
        // parse error). Returning distinct codes (-2, -3) collides with valid
        // INI values and breaks scripts that check result == -1 for failure.
        programStackPushInteger(program, -1);
    }
}

// get_ini_string
void op_get_ini_string(Program* program)
{
    const char* string = programStackPopString(program);

    char value[256];
    if (sfall_ini_get_string(string, value, sizeof(value))) {
        programStackPushString(program, value);
    } else {
        // Return an empty string for not-found (consistent with the name
        // "get_ini_string"). Returning -1 as integer would cause a type
        // mismatch on the script stack — callers expecting a string would
        // get an integer, which trashes the string pointer slot.
        programStackPushString(program, "");
    }
}

void sfall_ini_cache_clear()
{
    iniConfigCache.clear();

    // Cached array IDs are already invalidated by sfallArraysReset(); just drop the now-stale cache entries.
    iniConfigArrayCache.clear();
    iniConfigArrayCacheDat.clear();
}

// ============================================================
// TEST-ONLY: Config injection for the sfall_ini internal cache.
// Enables tests to exercise strtol ERANGE/LP64 overflow paths,
// end-to-end config lookup, and negative cache behavior without
// needing real file I/O (which requires compat_fopen, game data).
//
// Finding references: F-M69, I2-M68, I2-M75
// ============================================================
#if defined(TEST_ACCESSORS_ENABLED)
Config* sfall_ini_inject_config_for_test(const char* fileName)
{
    CachedConfigPtr config(new Config());
    if (!configInit(config.get())) {
        return nullptr;
    }
    Config* raw = config.get();
    iniConfigCache.emplace(fileName, std::move(config));
    return raw;
}
#endif

} // namespace fallout
