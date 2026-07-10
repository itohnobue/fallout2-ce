#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <string>
#include <vector>

#include "db.h"
#include "debug.h"
#include "memory.h"
#include "platform_compat.h"

namespace fallout {

#define CONFIG_FILE_MAX_LINE_LENGTH (1024)

// The initial number of sections (or key-value) pairs in the config.
#define CONFIG_INITIAL_CAPACITY (10)

struct CaseInsensitiveLess {
    bool operator()(const std::string& a, const std::string& b) const
    {
        return compat_stricmp(a.c_str(), b.c_str()) < 0;
    }
};

typedef std::set<std::string, CaseInsensitiveLess> StringSet;

static bool configParseLine(Config* config, char* string);
static bool configParseKeyValue(char* string, char* key, size_t keySize, char* value, size_t valueSize);
static bool configEnsureSectionExists(Config* config, const char* sectionKey);
static bool configTrimString(char* string);

static bool configWriteDb(Config* config, const char* filePath);
static bool configWriteStandard(Config* config, const char* filePath);
static bool configWriteSideBySide(Config* config, const char* filePath, int flags);
static bool configWriteSection(FILE* stream, const char* sectionName, ConfigSection* section, const StringSet* handledKeys);

// F-14 (FIX): Validate INI value for write safety.
// INI files are parsed line-by-line with configParseLine. Characters that
// introduce new lines (\n, \r) or section headers ([, ]) in a key or value
// can inject arbitrary config entries when the file is re-read. This function
// rejects such values before they reach the write path. The return value
// matches the fprintf convention: true = safe to write, false = reject.
static bool configWriteIsValueSafe(const char* str)
{
    if (str == nullptr) return false;
    for (const char* p = str; *p != '\0'; ++p) {
        unsigned char ch = static_cast<unsigned char>(*p);
        if (ch == '\n' || ch == '\r') {
            return false; // newlines inject new config lines
        }
    }
    return true;
}

// F-14 (FIX): Validate INI section/key name for write safety.
// Rejects names containing '[' or ']' which would inject section headers,
// and '=' or ';' which would corrupt the key=value parse.
static bool configWriteIsNameSafe(const char* str)
{
    if (str == nullptr) return false;
    for (const char* p = str; *p != '\0'; ++p) {
        unsigned char ch = static_cast<unsigned char>(*p);
        if (ch == '\n' || ch == '\r' || ch == '[' || ch == ']') {
            return false;
        }
    }
    return true;
}

// Last section key read from .INI file.
//
// 0x518224
static char gConfigLastSectionKey[CONFIG_FILE_MAX_LINE_LENGTH] = "unknown";

// 0x42BD90
bool configInit(Config* config)
{
    if (config == nullptr) {
        return false;
    }

    if (dictionaryInit(config, CONFIG_INITIAL_CAPACITY, sizeof(ConfigSection), nullptr) != 0) {
        return false;
    }

    return true;
}

// 0x42BDBC
void configFree(Config* config)
{
    if (config == nullptr) {
        return;
    }

    for (int sectionIndex = 0; sectionIndex < config->entriesLength; sectionIndex++) {
        DictionaryEntry* sectionEntry = &(config->entries[sectionIndex]);

        ConfigSection* section = (ConfigSection*)sectionEntry->value;
        for (int keyValueIndex = 0; keyValueIndex < section->entriesLength; keyValueIndex++) {
            DictionaryEntry* keyValueEntry = &(section->entries[keyValueIndex]);

            char** value = (char**)keyValueEntry->value;
            internal_free(*value);
            *value = nullptr;
        }

        dictionaryFree(section);
    }

    dictionaryFree(config);
}

// Parses command line argments and adds them into the config.
//
// The expected format of [argv] elements are "[section]key=value", otherwise
// the element is silently ignored.
//
// NOTE: This function trims whitespace in key-value pair, but not in section.
// I don't know if this is intentional or it's bug.
//
// 0x42BE38
bool configParseCommandLineArguments(Config* config, int argc, char** argv)
{
    if (config == nullptr) {
        return false;
    }

    for (int arg = 0; arg < argc; arg++) {
        char* pch;
        char* string = argv[arg];

        // Find opening bracket.
        pch = strchr(string, '[');
        if (pch == nullptr) {
            continue;
        }

        char* sectionKey = pch + 1;

        // Find closing bracket.
        pch = strchr(sectionKey, ']');
        if (pch == nullptr) {
            continue;
        }

        *pch = '\0';

        char key[CONFIG_FILE_MAX_LINE_LENGTH];
        char value[CONFIG_FILE_MAX_LINE_LENGTH];
        if (configParseKeyValue(pch + 1, key, sizeof(key), value, sizeof(value))) {
            if (!configSetString(config, sectionKey, key, value)) {
                *pch = ']';
                return false;
            }
        }

        *pch = ']';
    }

    return true;
}

// TODO: use const char** for valuePtr to enforce read-only API
// 0x42BF48
bool configGetString(Config* config, const char* sectionKey, const char* key, char** valuePtr)
{
    if (config == nullptr || sectionKey == nullptr || key == nullptr || valuePtr == nullptr) {
        return false;
    }

    int sectionIndex = dictionaryGetIndexByKey(config, sectionKey);
    if (sectionIndex == -1) {
        return false;
    }

    DictionaryEntry* sectionEntry = &(config->entries[sectionIndex]);
    ConfigSection* section = (ConfigSection*)sectionEntry->value;

    int index = dictionaryGetIndexByKey(section, key);
    if (index == -1) {
        return false;
    }

    DictionaryEntry* keyValueEntry = &(section->entries[index]);
    *valuePtr = *(char**)keyValueEntry->value;

    return true;
}

bool configGetString(Config* config, const char* sectionKey, const char* key, char** valuePtr, const char* defaultValue)
{
    if (config == nullptr || sectionKey == nullptr || key == nullptr || valuePtr == nullptr) {
        return false;
    }
    if (!configGetString(config, sectionKey, key, valuePtr) || (*valuePtr)[0] == '\0') {
        *valuePtr = nullptr;
        if (defaultValue != nullptr) {
            // Return defaultValue directly. No copy is performed to avoid the
            // static-buffer sharing issue where consecutive calls corrupt prior
            // results (e.g. art.cc:220-229 holds 4 pointers across 4 sequential
            // calls to load model names). Callers must treat the returned pointer
            // as read-only when it originates from defaultValue (typically a
            // string literal or global constant).
            *valuePtr = const_cast<char*>(defaultValue);
        }
    }
    return true;
}

// 0x42BF90
bool configSetString(Config* config, const char* sectionKey, const char* key, const char* value)
{
    if (config == nullptr || sectionKey == nullptr || key == nullptr || value == nullptr) {
        return false;
    }

    int sectionIndex = dictionaryGetIndexByKey(config, sectionKey);
    if (sectionIndex == -1) {
        if (!configEnsureSectionExists(config, sectionKey)) {
            return false;
        }
        sectionIndex = dictionaryGetIndexByKey(config, sectionKey);
    }

    DictionaryEntry* sectionEntry = &(config->entries[sectionIndex]);
    ConfigSection* section = (ConfigSection*)sectionEntry->value;

    int index = dictionaryGetIndexByKey(section, key);

    char* valueCopy = internal_strdup(value);
    if (valueCopy == nullptr) {
        return false;
    }

    if (index != -1) {
        DictionaryEntry* keyValueEntry = &(section->entries[index]);

        char** existingValue = (char**)keyValueEntry->value;
        internal_free(*existingValue);
        *existingValue = nullptr;

        dictionaryRemoveValue(section, key);
    }

    if (dictionaryAddValue(section, key, &valueCopy) == -1) {
        internal_free(valueCopy);
        return false;
    }

    return true;
}

// 0x42C05C
// UF-H-043: Default base=10 (always decimal) to prevent octal-parse
// failures on leading-zero-padded values like "08"/"09".
bool configGetInt(Config* config, const char* sectionKey, const char* key, int* valuePtr, unsigned char base /* = 10 */)
{
    if (valuePtr == nullptr) {
        return false;
    }

    char* stringValue;
    if (!configGetString(config, sectionKey, key, &stringValue)) {
        return false;
    }

    char* end;
    errno = 0;
    long l = strtol(stringValue, &end, base);

    // Reject if no conversion was performed (completely non-numeric input).
    if (end == stringValue) {
        return false;
    }

    // Reject on overflow/underflow.
    if (errno == ERANGE || l < INT_MIN || l > INT_MAX) {
        return false;
    }

    *valuePtr = static_cast<int>(l);

    return true;
}

bool configGetInt(Config* config, const char* sectionKey, const char* key, int* valuePtr, const int defaultValue, unsigned char base /* = 10 */)
{
    if (config == nullptr || sectionKey == nullptr || key == nullptr || valuePtr == nullptr) {
        return false;
    }
    if (!configGetInt(config, sectionKey, key, valuePtr, base)) {
        *valuePtr = defaultValue;
    }
    return true;
}

// 0x42C090
bool configGetIntList(Config* config, const char* sectionKey, const char* key, int* arr, int count)
{
    if (arr == nullptr || count < 2) {
        return false;
    }

    char* string;
    if (!configGetString(config, sectionKey, key, &string)) {
        return false;
    }

    char temp[CONFIG_FILE_MAX_LINE_LENGTH];
    string = strncpy(temp, string, CONFIG_FILE_MAX_LINE_LENGTH - 1);
    temp[CONFIG_FILE_MAX_LINE_LENGTH - 1] = '\0'; // Ensure null termination (M-78: strncpy may omit null byte)

    char* pch;
    while (1) {
        pch = strchr(string, ',');
        if (pch == nullptr) {
            break;
        }

        *pch = '\0';
        // Handle empty string between commas (e.g., "10,,20") — value 0.
        if (*string == '\0') {
            *arr++ = 0;
        } else {
            // Use strtol with errno+ERANGE to avoid UB on overflow (F-48).
            char* end;
            errno = 0;
            long l = strtol(string, &end, 10);
            if (end == string || errno == ERANGE || l < INT_MIN || l > INT_MAX) {
                return false;
            }
            *arr++ = static_cast<int>(l);
        }
        string = pch + 1;

        count--;
        if (count == 0) {
            break;
        }
    }

    // If the loop exited because count hit 0 but more comma-separated values
    // were still available in the string, the list was silently truncated.
    if (count == 0 && pch != nullptr) {
        return false;
    }

    // SFALL: Fix getting last item in a list if the list has less than the
    // requested number of values (for `chem_primary_desire`).
    if (count > 0) {
        // Handle empty string (e.g., trailing comma "10,20,") — the
        // empty element after the final comma represents value 0.
        if (*string == '\0') {
            *arr = 0;
            count--;
        } else {
            // Use strtol with errno+ERANGE to avoid UB on overflow (F-48).
            {
                char* end;
                errno = 0;
                long l = strtol(string, &end, 10);
                if (end == string || errno == ERANGE || l < INT_MIN || l > INT_MAX) {
                    return false;
                }
                *arr = static_cast<int>(l);
            }
            count--;
        }
    }

    return count == 0;
}

// 0x42C160
bool configSetInt(Config* config, const char* sectionKey, const char* key, int value)
{
    char stringValue[20];
    compat_itoa(value, stringValue, 10);

    return configSetString(config, sectionKey, key, stringValue);
}

// Reads .INI file into config.
//
// 0x42C280
bool configRead(Config* config, const char* filePath, bool isDb)
{
    if (config == nullptr || filePath == nullptr) {
        return false;
    }

    char string[CONFIG_FILE_MAX_LINE_LENGTH];

    if (isDb) {
        File* stream = fileOpen(filePath, "rb");

        // CE: Return `false` if file does not exists in database.
        if (stream == nullptr) {
            return false;
        }

        while (fileReadString(string, sizeof(string), stream) != nullptr) {
            configParseLine(config, string);
        }
        fileClose(stream);
    } else {
        FILE* stream = compat_fopen(filePath, "rt");

        // CE: Return `false` if file does not exists on the file system.
        if (stream == nullptr) {
            return false;
        }

        // Check for UTF-8 BOM (byte order mark) at the start of the file.
        // UTF-8 BOM = 0xEF 0xBB 0xBF. If present, skip past it so the
        // first '[' section header is recognized correctly. Without this
        // check, all config entries in BOM-prefixed files fall under the
        // "unknown" section and are effectively ignored.
        // Windows Notepad adds BOM by default when saving as "UTF-8".
        int firstBytes[3];
        firstBytes[0] = fgetc(stream);
        firstBytes[1] = fgetc(stream);
        firstBytes[2] = fgetc(stream);
        bool hasBom = (firstBytes[0] == 0xEF && firstBytes[1] == 0xBB && firstBytes[2] == 0xBF);
        if (!hasBom) {
            // Not a BOM — seek back to the beginning of the file.
            fseek(stream, 0, SEEK_SET);
        }

        while (compat_fgets(string, sizeof(string), stream) != nullptr) {
            // Detect truncated lines: if the buffer is full and the last
            // character is not a newline, fgets() stopped before reaching
            // end-of-line.  The rest of the line sits in the stream and
            // will be read as the next fgets() call — producing a
            // continuation fragment without '=' that configParseKeyValue()
            // silently drops.  Flush the remainder and warn.
            size_t len = strlen(string);
            if (len == sizeof(string) - 1 && string[sizeof(string) - 2] != '\n') {
                // Line was truncated — flush remainder.
                int c;
                while ((c = fgetc(stream)) != EOF && c != '\n') {
                }
                debugPrint("Warning: Config line longer than %zu chars was truncated in %s\n",
                    sizeof(string) - 1, filePath);
            }
            configParseLine(config, string);
        }
        fclose(stream);
    }

    return true;
}

// Writes config into .INI file.
//
// 0x42C324
bool configWrite(Config* config, const char* filePath, bool isDb)
{
    return configWriteEx(config, filePath, isDb ? CONFIG_IS_DB : CONFIG_DEFAULT);
}

bool configWriteEx(Config* config, const char* filePath, int flags)
{
    if (config == nullptr || filePath == nullptr) {
        return false;
    }

    if (flags & CONFIG_IS_DB) {
        return configWriteDb(config, filePath);
    }

    if (flags & (CONFIG_RETAIN_COMMENTS | CONFIG_RETAIN_ORDER | CONFIG_RETAIN_UNKNOWN)) {
        return configWriteSideBySide(config, filePath, flags);
    }

    return configWriteStandard(config, filePath);
}

static bool configWriteDb(Config* config, const char* filePath)
{
    File* stream = fileOpen(filePath, "wt");
    if (stream == nullptr) {
        return false;
    }

    for (int i = 0; i < config->entriesLength; i++) {
        DictionaryEntry* sectionEntry = &(config->entries[i]);
        filePrintFormatted(stream, "[%s]\n", sectionEntry->key);

        ConfigSection* section = (ConfigSection*)sectionEntry->value;
        for (int j = 0; j < section->entriesLength; j++) {
            DictionaryEntry* keyValueEntry = &(section->entries[j]);
            filePrintFormatted(stream, "%s=%s\n", keyValueEntry->key, *(char**)keyValueEntry->value);
        }
        filePrintFormatted(stream, "\n");
    }

    fileClose(stream);
    return true;
}

static bool configWriteStandard(Config* config, const char* filePath)
{
    char tempPath[COMPAT_MAX_PATH];
    int tempPathLength = snprintf(tempPath, sizeof(tempPath), "%s.tmp", filePath);
    if (tempPathLength < 0 || tempPathLength >= (int)sizeof(tempPath)) {
        return false;
    }

    FILE* stream = compat_fopen(tempPath, "wt");
    if (stream == nullptr) {
        return false;
    }

    bool ok = true;
    for (int i = 0; i < config->entriesLength && ok; i++) {
        // F-14 (FIX): Validate section name before writing.
        if (!configWriteIsNameSafe(config->entries[i].key)) {
            ok = false;
            break;
        }
        if (fprintf(stream, "[%s]\n", config->entries[i].key) < 0) {
            ok = false;
            break;
        }
        ConfigSection* section = (ConfigSection*)config->entries[i].value;
        for (int j = 0; j < section->entriesLength && ok; j++) {
            DictionaryEntry* entry = &(section->entries[j]);
            char* value = *(char**)entry->value;
            // F-14 (FIX): Reject values containing newlines to prevent
            // injection of additional config lines/sections on re-read.
            if (!configWriteIsValueSafe(value)) {
                debugPrint("Config: rejected unsafe value for key '%s' in section '%s' (contains control characters)\n",
                    entry->key, config->entries[i].key);
                ok = false;
                break;
            }
            if (fprintf(stream, "%s=%s\n", entry->key, value) < 0) {
                ok = false;
                break;
            }
        }
        if (ok && fprintf(stream, "\n") < 0) {
            ok = false;
        }
    }

    fclose(stream);

    if (!ok) {
        compat_remove(tempPath);
        return false;
    }

    // Atomically replace original by writing to temp file then renaming.
    std::string backupPath = std::string(filePath) + ".bak";

    compat_remove(backupPath.c_str()); // ignore errors — may not exist

    if (compat_rename(filePath, backupPath.c_str()) != 0 && errno != ENOENT) {
        compat_remove(tempPath);
        return false;
    }

    if (compat_rename(tempPath, filePath) != 0) {
        // Restore backup on failure.
        compat_rename(backupPath.c_str(), filePath);
        return false;
    }

    compat_remove(backupPath.c_str()); // ignore errors — cleanup only
    return true;
}

static bool configWriteSection(FILE* stream, const char* sectionName, ConfigSection* section, const StringSet* handledKeys)
{
    if (sectionName != nullptr) {
        if (fprintf(stream, "[%s]\n", sectionName) < 0) {
            return false;
        }
    }

    for (int i = 0; i < section->entriesLength; i++) {
        DictionaryEntry* entry = &(section->entries[i]);
        if (handledKeys == nullptr || handledKeys->find(entry->key) == handledKeys->end()) {
            char* value = *(char**)entry->value;
            // F-14 (FIX): Validate value before writing.
            if (!configWriteIsValueSafe(value)) {
                debugPrint("Config: rejected unsafe value for key '%s' in section '%s'\n",
                    entry->key, sectionName ? sectionName : "(null)");
                return false;
            }
            if (fprintf(stream, "%s=%s\n", entry->key, value) < 0) {
                return false;
            }
        }
    }

    return true;
}

static bool configWriteSideBySide(Config* config, const char* filePath, int flags)
{
    bool retainUnknown = (flags & CONFIG_RETAIN_UNKNOWN) != 0;

    FILE* original = compat_fopen(filePath, "rt");
    if (original == nullptr) {
        if (errno == ENOENT) {
            // File doesn't exist, nothing to retain, perform standard write.
            return configWriteStandard(config, filePath);
        }
        // Other error (e.g., permissions), fail as requested.
        return false;
    }

    char tempPath[COMPAT_MAX_PATH];
    int tempPathLength = snprintf(tempPath, sizeof(tempPath), "%s.tmp", filePath);
    if (tempPathLength < 0 || tempPathLength >= (int)sizeof(tempPath)) {
        fclose(original);
        return false;
    }
    FILE* output = compat_fopen(tempPath, "wt");
    if (output == nullptr) {
        fclose(original);
        return false;
    }

    StringSet handledSections;
    StringSet handledKeys;
    std::vector<std::string> pendingLines;

    bool ok = true;

    char currentSectionName[CONFIG_FILE_MAX_LINE_LENGTH] = "";
    ConfigSection* currentSection = nullptr;
    // Treat pre-section preamble as "known" so its comments are preserved.
    bool currentSectionKnown = true;

    char line[CONFIG_FILE_MAX_LINE_LENGTH];
    while (ok && compat_fgets(line, sizeof(line), original) != nullptr) {
        // Detect truncated lines: if the buffer is full and the last character
        // is not a newline, the line was longer than our buffer. Flush the
        // remainder to prevent continuation data from corrupting subsequent lines.
        {
            size_t len = strlen(line);
            if (len == sizeof(line) - 1 && line[sizeof(line) - 2] != '\n') {
                debugPrint("Warning: Config line longer than %zu chars was truncated in configWriteSideBySide\n",
                    sizeof(line) - 1);
                int c;
                while ((c = fgetc(original)) != EOF && c != '\n') {
                }
            }
        }

        char lineCopy[CONFIG_FILE_MAX_LINE_LENGTH];
        strcpy(lineCopy, line);

        char* trimmed = lineCopy;
        while (isspace(static_cast<unsigned char>(*trimmed))) {
            trimmed++;
        }

        // Buffer comments and empty lines; drop them if inside a suppressed section.
        if (*trimmed == '\0' || *trimmed == ';' || *trimmed == '#') {
            if (!currentSectionKnown && !retainUnknown) {
                continue;
            }
            if ((flags & CONFIG_RETAIN_COMMENTS) || (*trimmed == '\0')) {
                pendingLines.emplace_back(line);
            }
            continue;
        }

        if (*trimmed == '[') {
            // End current section, write out remaining keys from it.
            if (currentSection != nullptr) {
                if (!configWriteSection(output, nullptr, currentSection, &handledKeys)) {
                    ok = false;
                }
                handledKeys.clear();
                currentSection = nullptr;
            }

            // Parse the section name before deciding whether to write.
            currentSectionKnown = false;
            currentSectionName[0] = '\0';

            char* end = strchr(trimmed, ']');
            if (end != nullptr) {
                *end = '\0';
                strcpy(currentSectionName, trimmed + 1);
                configTrimString(currentSectionName);
                *end = ']';

                int sectionIndex = dictionaryGetIndexByKey(config, currentSectionName);
                if (sectionIndex != -1) {
                    currentSection = (ConfigSection*)config->entries[sectionIndex].value;
                    handledSections.insert(currentSectionName);
                    currentSectionKnown = true;
                }
            }

            if (currentSectionKnown || retainUnknown) {
                // Flush buffered comments/empty lines that preceded this header.
                for (const std::string& l : pendingLines) {
                    if (fprintf(output, "%s", l.c_str()) < 0) {
                        ok = false;
                        break;
                    }
                }
                pendingLines.clear();
                if (fprintf(output, "%s", line) < 0) {
                    ok = false;
                }
            } else {
                // Suppress unknown section entirely, discard buffered lines.
                pendingLines.clear();
            }
            continue;
        }

        char* equals = strchr(trimmed, '=');
        if (equals != nullptr) {
            *equals = '\0';
            char key[CONFIG_FILE_MAX_LINE_LENGTH];
            strcpy(key, trimmed);
            configTrimString(key);
            *equals = '=';

            char* value = nullptr;
            if (currentSection != nullptr) {
                configGetString(config, currentSectionName, key, &value);
            }

            if (value != nullptr) {
                // Flush lines that were between previous key and this one.
                for (const std::string& l : pendingLines) {
                    if (fprintf(output, "%s", l.c_str()) < 0) {
                        ok = false;
                        break;
                    }
                }
                pendingLines.clear();

                const char* k = key;
                char* comment = nullptr;

                if (flags & CONFIG_RETAIN_COMMENTS) {
                    *equals = '\0';
                    k = trimmed;
                    if ((comment = strchr(equals + 1, ';')) == nullptr) {
                        comment = strchr(equals + 1, '#');
                    }
                }

                if (fprintf(output, "%s=%s%s%s", k, value, comment ? " " : "", comment ? comment : "\n") < 0) {
                    ok = false;
                }
                handledKeys.insert(key);
            } else if (retainUnknown) {
                // Key not in config - write verbatim from original file.
                for (const std::string& l : pendingLines) {
                    if (fprintf(output, "%s", l.c_str()) < 0) {
                        ok = false;
                        break;
                    }
                }
                pendingLines.clear();
                if (fprintf(output, "%s", line) < 0) {
                    ok = false;
                }
            }
            // else: drop key; keep pending lines buffered so they attach to the
            // next surviving key rather than becoming orphaned.
            continue;
        }

        // Unknown line structure - buffer if retaining comments/structure.
        if ((currentSectionKnown || retainUnknown) && (flags & CONFIG_RETAIN_COMMENTS)) {
            pendingLines.emplace_back(line);
        }
    }

    // Write remaining keys in the last processed section.
    if (ok && currentSection != nullptr) {
        if (!configWriteSection(output, nullptr, currentSection, &handledKeys)) {
            ok = false;
        }
    }

    // Flush remaining comments/empty lines.
    for (const std::string& l : pendingLines) {
        if (fprintf(output, "%s", l.c_str()) < 0) {
            ok = false;
            break;
        }
    }
    pendingLines.clear();

    // Write completely new sections, separated from existing content.
    bool firstNewSection = true;
    for (int i = 0; ok && i < config->entriesLength; i++) {
        DictionaryEntry* sectionEntry = &(config->entries[i]);
        if (handledSections.find(sectionEntry->key) == handledSections.end()) {
            if (firstNewSection) {
                if (fprintf(output, "\n") < 0) {
                    ok = false;
                    break;
                }
                firstNewSection = false;
            }
            if (!configWriteSection(output, sectionEntry->key, (ConfigSection*)sectionEntry->value, nullptr)) {
                ok = false;
                break;
            }
            if (fprintf(output, "\n") < 0) {
                ok = false;
                break;
            }
        }
    }

    fclose(output);
    fclose(original);

    if (!ok) {
        compat_remove(tempPath);
        return false;
    }

    std::string backupPath = std::string(filePath) + ".bak";

    compat_remove(backupPath.c_str()); // ignore errors — may not exist

    if (compat_rename(filePath, backupPath.c_str()) != 0) {
        compat_remove(tempPath);
        return false;
    }

    if (compat_rename(tempPath, filePath) != 0) {
        compat_rename(backupPath.c_str(), filePath);
        return false;
    }

    compat_remove(backupPath.c_str()); // ignore errors — cleanup only
    return true;
}

// Parses a line from .INI file into config.
//
// A line either contains a "[section]" section key or "key=value" pair. In the
// first case section key is not added to config immediately, instead it is
// stored in [gConfigLastSectionKey] for later usage. This prevents empty
// sections in the config.
//
// In case of key-value pair it pretty straight forward - it adds key-value
// pair into previously read section key stored in [gConfigLastSectionKey].
//
// Returns `true` when a section was parsed or key-value pair was parsed and
// added to the config, or `false` otherwise.
//
// 0x42C4BC
static bool configParseLine(Config* config, char* string)
{
    char* pch;

    // Find comment marker and truncate the string.
    // NOTE: Only ';' is treated as a comment marker, matching sfall's INI
    // parser behavior. '#' is NOT treated as a comment marker because sfall
    // uses '#' in valid INI values (e.g., color values like #FF0000).
    pch = strchr(string, ';');

    if (pch != nullptr) {
        *pch = '\0';
    }

    // CE: Original implementation treats any line with brackets as section key.
    // The problem can be seen when loading Olympus settings (ddraw.ini), which
    // contains the following line:
    //
    //  ```ini
    //  VersionString=Olympus 2207 [Complete].
    //  ```
    //
    // It thinks that [Complete] is a start of new section, and puts remaining
    // keys there.

    // Skip leading whitespace.
    while (isspace(static_cast<unsigned char>(*string))) {
        string++;
    }

    // Check if it's a section key.
    if (*string == '[') {
        char* sectionKey = string + 1;

        // Find closing bracket.
        pch = strchr(sectionKey, ']');
        if (pch != nullptr) {
            *pch = '\0';
            strcpy(gConfigLastSectionKey, sectionKey);
            return configTrimString(gConfigLastSectionKey);
        }

        // Malformed section header — line starts with '[' but has no closing ']'.
        // Falling through to key-value parsing would silently merge content into
        // the previous section. Reject instead.
        return false;
    }

    char key[CONFIG_FILE_MAX_LINE_LENGTH];
    char value[CONFIG_FILE_MAX_LINE_LENGTH];
    if (!configParseKeyValue(string, key, sizeof(key), value, sizeof(value))) {
        return false;
    }

    return configSetString(config, gConfigLastSectionKey, key, value);
}

// Splits "key=value" pair from [string] and copy appropriate parts into [key]
// and [value] respectively.
//
// Both key and value are trimmed.
//
// 0x42C594
static bool configParseKeyValue(char* string, char* key, size_t keySize, char* value, size_t valueSize)
{
    if (string == nullptr || key == nullptr || value == nullptr) {
        return false;
    }

    // Find equals character.
    char* pch = strchr(string, '=');
    if (pch == nullptr) {
        return false;
    }

    *pch = '\0';

    strncpy(key, string, keySize - 1);
    key[keySize - 1] = '\0';
    strncpy(value, pch + 1, valueSize - 1);
    value[valueSize - 1] = '\0';

    *pch = '=';

    configTrimString(key);
    configTrimString(value);

    return true;
}

// Ensures the config has a section with specified key.
//
// Return `true` if section exists or it was successfully added, or `false`
// otherwise.
//
// 0x42C638
static bool configEnsureSectionExists(Config* config, const char* sectionKey)
{
    if (config == nullptr || sectionKey == nullptr) {
        return false;
    }

    if (dictionaryGetIndexByKey(config, sectionKey) != -1) {
        // Section already exists, no need to do anything.
        return true;
    }

    ConfigSection section;
    if (dictionaryInit(&section, CONFIG_INITIAL_CAPACITY, sizeof(char**), nullptr) == -1) {
        return false;
    }

    if (dictionaryAddValue(config, sectionKey, &section) == -1) {
        return false;
    }

    return true;
}

// Removes leading and trailing whitespace from the specified string.
//
// 0x42C698
static bool configTrimString(char* string)
{
    if (string == nullptr) {
        return false;
    }

    size_t length = strlen(string);
    if (length == 0) {
        return true;
    }

    // Starting from the end of the string, loop while it's a whitespace and
    // decrement string length.
    char* pch = string + length - 1;
    while (length != 0 && isspace(static_cast<unsigned char>(*pch))) {
        length--;
        pch--;
    }

    // pch now points to the last non-whitespace character.
    pch[1] = '\0';

    // Starting from the beginning of the string loop while it's a whitespace
    // and decrement string length.
    pch = string;
    while (isspace(static_cast<unsigned char>(*pch))) {
        pch++;
        length--;
    }

    // pch now points for to the first non-whitespace character.
    memmove(string, pch, length + 1);

    return true;
}

// 0x42C718
bool configGetDouble(Config* config, const char* sectionKey, const char* key, double* valuePtr)
{
    if (valuePtr == nullptr) {
        return false;
    }

    char* stringValue;
    if (!configGetString(config, sectionKey, key, &stringValue)) {
        return false;
    }

    char* end;
    errno = 0;
    double d = strtod(stringValue, &end);

    // Reject if no conversion was performed (completely non-numeric input).
    if (end == stringValue) {
        return false;
    }

    // Reject on overflow/underflow.
    if (errno == ERANGE) {
        return false;
    }

    *valuePtr = d;

    return true;
}

// 0x42C74C
bool configSetDouble(Config* config, const char* sectionKey, const char* key, double value)
{
    char stringValue[32];
    snprintf(stringValue, sizeof(stringValue), "%.6f", value);

    return configSetString(config, sectionKey, key, stringValue);
}

// NOTE: Boolean-typed variant of [configGetInt].
bool configGetBool(Config* config, const char* sectionKey, const char* key, bool* valuePtr)
{
    if (valuePtr == nullptr) {
        return false;
    }

    int integerValue;
    if (!configGetInt(config, sectionKey, key, &integerValue)) {
        return false;
    }

    *valuePtr = integerValue != 0;

    return true;
}

bool configGetBool(Config* config, const char* sectionKey, const char* key, bool* valuePtr, const bool defaultValue)
{
    if (config == nullptr || sectionKey == nullptr || key == nullptr || valuePtr == nullptr) {
        return false;
    }
    if (!configGetBool(config, sectionKey, key, valuePtr)) {
        *valuePtr = defaultValue;
    }
    return true;
}

// NOTE: Boolean-typed variant of [configGetInt].
bool configSetBool(Config* config, const char* sectionKey, const char* key, bool value)
{
    return configSetInt(config, sectionKey, key, value ? 1 : 0);
}

ScopedConfig::ScopedConfig()
{
    _loaded = configInit(&_config);
}

ScopedConfig::ScopedConfig(const char* filePath, bool isDb)
    : ScopedConfig()
{
    if (_loaded) {
        _loaded = configRead(&_config, filePath, isDb);
    }
}

ScopedConfig::~ScopedConfig()
{
    if (_config.isInitialized()) {
        configFree(&_config);
    }
}

bool ScopedConfig::isInitialized() const
{
    return _config.isInitialized();
}

Config* ScopedConfig::get()
{
    return &_config;
}

const Config* ScopedConfig::get() const
{
    return &_config;
}

ScopedConfig::operator bool() const
{
    return _loaded;
}

} // namespace fallout
