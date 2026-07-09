#include "sfall_global_vars.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "debug.h"

namespace fallout {

struct SfallGlobalVarsState {
    std::unordered_map<uint64_t, int> vars;
    std::unordered_map<uint64_t, float> floatVars;
    // F-003/F-004: String-valued global vars for persisting dynamically-
    // allocated string state (perk names/descs, fake perk/trait names/descs,
    // perkbox title, movie path overrides). Serialized after the float section
    // in save format version 2+.
    std::unordered_map<uint64_t, std::string> stringVars;
};

#pragma pack(push)
#pragma pack(8)

// Matches Sfall's `GlobalVar` to maintain binary compatibility.
struct GlobalVarEntry {
    uint64_t key;
    int32_t value;
    int32_t unused;
};

// Extension for float-valued global variables (not present in original sfall).
struct FloatVarEntry {
    uint64_t key;
    float value;
};

#pragma pack(pop)

// Magic number for save format identification: "SFGV" (Sfall Global Vars).
static const uint32_t kSfallGlobalVarsMagic = 0x53464756;
// F-003/F-004: Version 2 adds a string section after the float section.
// String entries are length-prefixed UTF-8 records. Old code (version 1)
// ignores the trailing string data — backward compatible.
// New code loading old saves (version 1) skips the string section.
static const int32_t kSfallGlobalVarsVersion = 2;
// F-16 (FIX): Maximum number of entries for both integer and float global vars.
// Mirrors the load-time cap at line 181 (which rejects counts > 10000).
// The integer store has this check at line 308; the float store was missing it.
static constexpr size_t kMaxGlobalVars = 10000;

static bool sfall_gl_vars_store(uint64_t key, int value);
static bool sfall_gl_vars_fetch(uint64_t key, int& value);
static bool sfall_gl_vars_remove(uint64_t key);

static SfallGlobalVarsState* sfall_gl_vars_state = nullptr;

bool sfall_gl_vars_init()
{
    sfall_gl_vars_state = new (std::nothrow) SfallGlobalVarsState();
    if (sfall_gl_vars_state == nullptr) {
        return false;
    }

    return true;
}

void sfall_gl_vars_reset()
{
    sfall_gl_vars_state->vars.clear();
    sfall_gl_vars_state->floatVars.clear();
    sfall_gl_vars_state->stringVars.clear();
}

void sfall_gl_vars_exit()
{
    if (sfall_gl_vars_state != nullptr) {
        delete sfall_gl_vars_state;
        sfall_gl_vars_state = nullptr;
    }
}

bool sfall_gl_vars_save(File* stream)
{
    // ============================================================
    // IMPORTANT: This save format supports int, float, AND string values
    // (string support added in version 2 for F-003/F-004 fix).
    // String state is stored as length-prefixed UTF-8 records after
    // the float section.
    // ============================================================

    // Write format header: magic number + version for forward compatibility.
    uint32_t magic = kSfallGlobalVarsMagic;
    if (fileWrite(&magic, sizeof(magic), 1, stream) != 1) {
        return false;
    }
    int32_t version = kSfallGlobalVarsVersion;
    if (fileWrite(&version, sizeof(version), 1, stream) != 1) {
        return false;
    }

    // Save integer global vars.
    int count = static_cast<int>(sfall_gl_vars_state->vars.size());
    if (fileWrite(&count, sizeof(count), 1, stream) != 1) {
        return false;
    }

    GlobalVarEntry entry = { 0 };
    for (auto& pair : sfall_gl_vars_state->vars) {
        entry.key = pair.first;
        entry.value = pair.second;

        if (fileWrite(&entry, sizeof(entry), 1, stream) != 1) {
            return false;
        }
    }

    // Save float global vars.
    int floatCount = static_cast<int>(sfall_gl_vars_state->floatVars.size());
    if (fileWrite(&floatCount, sizeof(floatCount), 1, stream) != 1) {
        return false;
    }

    FloatVarEntry floatEntry = { 0 };
    for (auto& pair : sfall_gl_vars_state->floatVars) {
        floatEntry.key = pair.first;
        floatEntry.value = pair.second;

        if (fileWrite(&floatEntry, sizeof(floatEntry), 1, stream) != 1) {
            return false;
        }
    }

    // F-003/F-004: Save string global vars (version 2+).
    // String entries are length-prefixed: uint64_t key, int32_t length,
    // followed by length bytes of UTF-8 data (no null terminator in the
    // length count). Null/empty strings are NOT stored — the caller
    // should not call store_string for them.
    int stringCount = static_cast<int>(sfall_gl_vars_state->stringVars.size());
    if (fileWrite(&stringCount, sizeof(stringCount), 1, stream) != 1) {
        return false;
    }

    for (auto& pair : sfall_gl_vars_state->stringVars) {
        uint64_t key = pair.first;
        const std::string& value = pair.second;
        int32_t len = static_cast<int32_t>(value.size());

        if (fileWrite(&key, sizeof(key), 1, stream) != 1) {
            return false;
        }
        if (fileWrite(&len, sizeof(len), 1, stream) != 1) {
            return false;
        }
        if (len > 0 && fileWrite(value.data(), static_cast<size_t>(len), 1, stream) != 1) {
            return false;
        }
    }

    return true;
}

bool sfall_gl_vars_load(File* stream)
{
    // ============================================================
    // IMPORTANT: Int, float, AND string values are restored from savegames
    // (string support added in version 2 for F-003/F-004 fix). Old saves
    // (version 1) without string data are handled gracefully.
    // ============================================================

    // Clear existing state so that on any load failure the state
    // is empty rather than partially populated (see E-09).
    sfall_gl_vars_state->vars.clear();
    sfall_gl_vars_state->floatVars.clear();
    sfall_gl_vars_state->stringVars.clear();

    // Detect save format: new format starts with magic number "SFGV".
    // Old format starts with an int32 count (valid range 0-10000, never equals magic).
    uint32_t magicOrCount;
    if (fileRead(&magicOrCount, sizeof(magicOrCount), 1, stream) != 1) {
        return false;
    }

    int count;
    bool hasFloatVars = false;
    int32_t version = 0;  // F-003/F-004: visible for string section check

    if (magicOrCount == kSfallGlobalVarsMagic) {
        // New format with version header.
        if (fileRead(&version, sizeof(version), 1, stream) != 1) {
            return false;
        }
        // Support version 1 and any future versions (forward-compatible).
        // The count-delimited entry format means unknown future versions
        // can be parsed as long as the core entry layout is unchanged.
        if (version < 1) {
            return false;
        }

        if (fileRead(&count, sizeof(count), 1, stream) != 1) {
            return false;
        }

        hasFloatVars = true;
    } else {
        // Old format: first word is the count (no magic, no version).
        // magicOrCount was read as uint32_t; reinterpret as int32_t count.
        static_assert(sizeof(count) == sizeof(magicOrCount), "int and uint32_t must be same size");
        memcpy(&count, &magicOrCount, sizeof(count));
    }

    if (count < 0 || count > static_cast<int>(kMaxGlobalVars)) {
        return false;
    }

    sfall_gl_vars_state->vars.reserve(count);

    GlobalVarEntry entry;
    while (count > 0) {
        if (fileRead(&entry, sizeof(entry), 1, stream) != 1) {
            return false;
        }

        sfall_gl_vars_state->vars[entry.key] = static_cast<int>(entry.value);

        count--;
    }

    // Load float global vars (new format only).
    if (hasFloatVars) {
        int floatCount;
        if (fileRead(&floatCount, sizeof(floatCount), 1, stream) != 1) {
            return false;
        }

        if (floatCount < 0 || floatCount > static_cast<int>(kMaxGlobalVars)) {
            return false;
        }

        sfall_gl_vars_state->floatVars.reserve(floatCount);

        FloatVarEntry floatEntry;
        while (floatCount > 0) {
            if (fileRead(&floatEntry, sizeof(floatEntry), 1, stream) != 1) {
                return false;
            }

            sfall_gl_vars_state->floatVars[floatEntry.key] = floatEntry.value;

            floatCount--;
        }
    }

    // F-003/F-004: Load string global vars (version 2+ only).
    // Old saves (version 1) have no string section — skip.
    if (hasFloatVars && version >= 2) {
        int stringCount;
        if (fileRead(&stringCount, sizeof(stringCount), 1, stream) != 1) {
            return false;
        }

        if (stringCount < 0 || stringCount > static_cast<int>(kMaxGlobalVars)) {
            return false;
        }

        sfall_gl_vars_state->stringVars.reserve(static_cast<size_t>(stringCount));

        for (int i = 0; i < stringCount; i++) {
            uint64_t key = 0;
            int32_t len = 0;

            if (fileRead(&key, sizeof(key), 1, stream) != 1) {
                return false;
            }
            if (fileRead(&len, sizeof(len), 1, stream) != 1) {
                return false;
            }

            // Reject negative or unreasonably large lengths.
            if (len < 0 || len > 65536) {
                return false;
            }

            if (len > 0) {
                // Read string data into a temporary buffer, then construct std::string.
                // Use vector<char> for automatic cleanup and exception safety.
                std::vector<char> buf(static_cast<size_t>(len));
                if (fileRead(buf.data(), static_cast<size_t>(len), 1, stream) != 1) {
                    return false;
                }
                sfall_gl_vars_state->stringVars[key] = std::string(buf.data(), static_cast<size_t>(len));
            } else {
                // Empty string — store as empty std::string.
                sfall_gl_vars_state->stringVars[key] = std::string();
            }
        }
    }

    return true;
}

// Convert a C-string key to a uint64_t for use as a map key.
//
// Keys of exactly 8 characters are packed directly into the uint64_t
// (matching the sfall convention for backward compatibility).
//
// Keys shorter than 8 characters are zero-padded in the high bytes.
//
// Keys longer than 8 characters are hashed with FNV-1a to produce a
// 64-bit value. This namespace is disjoint from packed keys because
// FNV-1a output differs from any short ASCII string's direct packing.
//
// An empty key always fails (ok=false).
static uint64_t sfall_gl_vars_key_to_uint64(const char* key, bool& ok)
{
    size_t len = strlen(key);
    if (len == 0) {
        ok = false;
        return 0;
    }

    if (len <= 8) {
        // Pad short keys with zeros in the high bytes.
        // For exactly-8-char keys this is byte-identical to the old behavior.
        ok = true;
        uint64_t result = 0;
        memcpy(&result, key, len);
        return result;
    }

    // Keys longer than 8 chars: FNV-1a 64-bit hash.
    // This produces a well-distributed 64-bit value that is effectively
    // disjoint from the packed-key namespace (packed ASCII keys have
    // the high bit of every byte clear).
    ok = true;
    constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    constexpr uint64_t FNV_PRIME = 1099511628211ULL;

    uint64_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < len; i++) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(key[i]));
        hash *= FNV_PRIME;
    }
    return hash;
}

bool sfall_gl_vars_store(const char* key, int value)
{
    bool ok;
    uint64_t numericKey = sfall_gl_vars_key_to_uint64(key, ok);
    if (!ok) {
        return false;
    }
    return sfall_gl_vars_store(numericKey, value);
}

bool sfall_gl_vars_store(int key, int value)
{
    return sfall_gl_vars_store(static_cast<uint64_t>(key), value);
}

bool sfall_gl_vars_fetch(const char* key, int& value)
{
    bool ok;
    uint64_t numericKey = sfall_gl_vars_key_to_uint64(key, ok);
    if (!ok) {
        return false;
    }
    return sfall_gl_vars_fetch(numericKey, value);
}

bool sfall_gl_vars_fetch(int key, int& value)
{
    return sfall_gl_vars_fetch(static_cast<uint64_t>(key), value);
}

bool sfall_gl_vars_remove(const char* key)
{
    bool ok;
    uint64_t numericKey = sfall_gl_vars_key_to_uint64(key, ok);
    if (!ok) {
        return false;
    }
    return sfall_gl_vars_remove(numericKey);
}

bool sfall_gl_vars_remove(int key)
{
    return sfall_gl_vars_remove(static_cast<uint64_t>(key));
}

static bool sfall_gl_vars_store(uint64_t key, int value)
{
    // F-001: Always store the value, even when 0. Previously the erase-on-0
    // convention caused explicitly-set zero values (e.g., gPipboyAvailableOverride=0
    // meaning "pipboy unavailable") to be lost on save/load round-trip.
    // Use sfall_gl_vars_remove() for explicit deletion.

    // Cap at 10000 entries to prevent unbounded runtime growth from
    // script-driven insertions. Load-time already rejects counts > 10000
    // (line 155); mirror that limit at runtime. Updating an existing key
    // is always allowed regardless of the cap.
    //
    // NOTE: save path (sfall_gl_vars_save, line 86) writes ALL entries
    // unconditionally; beyond-cap entries added before this fix was
    // applied will persist through save/load. The cap only restricts
    // new keys added at runtime starting now.
    if (sfall_gl_vars_state->vars.size() >= kMaxGlobalVars
        && sfall_gl_vars_state->vars.find(key) == sfall_gl_vars_state->vars.end()) {
        debugPrint("sfall_gl_vars_store: global vars cap (%zu) exceeded, rejecting key %llu\n",
            kMaxGlobalVars, static_cast<unsigned long long>(key));
        return false;
    }

    sfall_gl_vars_state->vars[key] = value;
    return true;
}

static bool sfall_gl_vars_fetch(uint64_t key, int& value)
{
    auto it = sfall_gl_vars_state->vars.find(key);
    if (it == sfall_gl_vars_state->vars.end()) {
        return false;
    }

    value = it->second;

    return true;
}

// Explicit removal of a global var (replaces the erased-on-0 convention).
// Returns true if the entry existed and was removed, false if not found.
static bool sfall_gl_vars_remove(uint64_t key)
{
    return sfall_gl_vars_state->vars.erase(key) > 0;
}

// --- Float global vars (parallel storage) ---

bool sfall_gl_vars_store_float(const char* key, float value)
{
    bool ok;
    uint64_t numericKey = sfall_gl_vars_key_to_uint64(key, ok);
    if (!ok) {
        return false;
    }
    // F-16 (FIX): Same size cap as integer vars to prevent unbounded runtime growth.
    if (sfall_gl_vars_state->floatVars.size() >= kMaxGlobalVars
        && sfall_gl_vars_state->floatVars.find(numericKey) == sfall_gl_vars_state->floatVars.end()) {
        debugPrint("sfall_gl_vars_store_float: float vars cap (%zu) exceeded, rejecting key\n",
            kMaxGlobalVars);
        return false;
    }
    sfall_gl_vars_state->floatVars[numericKey] = value;
    return true;
}

bool sfall_gl_vars_store_float(int key, float value)
{
    uint64_t numericKey = static_cast<uint64_t>(key);
    // F-16 (FIX): Same size cap as integer vars to prevent unbounded runtime growth.
    if (sfall_gl_vars_state->floatVars.size() >= kMaxGlobalVars
        && sfall_gl_vars_state->floatVars.find(numericKey) == sfall_gl_vars_state->floatVars.end()) {
        debugPrint("sfall_gl_vars_store_float: float vars cap (%zu) exceeded, rejecting key %llu\n",
            kMaxGlobalVars, static_cast<unsigned long long>(key));
        return false;
    }
    sfall_gl_vars_state->floatVars[numericKey] = value;
    return true;
}

bool sfall_gl_vars_fetch_float(const char* key, float& value)
{
    bool ok;
    uint64_t numericKey = sfall_gl_vars_key_to_uint64(key, ok);
    if (!ok) {
        return false;
    }
    auto it = sfall_gl_vars_state->floatVars.find(numericKey);
    if (it == sfall_gl_vars_state->floatVars.end()) {
        return false;
    }

    value = it->second;
    return true;
}

bool sfall_gl_vars_fetch_float(int key, float& value)
{
    auto it = sfall_gl_vars_state->floatVars.find(static_cast<uint64_t>(key));
    if (it == sfall_gl_vars_state->floatVars.end()) {
        return false;
    }

    value = it->second;
    return true;
}

// --- String global vars (F-003/F-004 fix) ---

bool sfall_gl_vars_store_string(const char* key, const char* value)
{
    if (sfall_gl_vars_state == nullptr) {
        return false;
    }

    bool ok;
    uint64_t numericKey = sfall_gl_vars_key_to_uint64(key, ok);
    if (!ok) {
        return false;
    }

    // Null or empty value — remove any existing entry (same semantics
    // as the setter opcodes: setting to null/empty clears the override).
    if (value == nullptr || value[0] == '\0') {
        sfall_gl_vars_state->stringVars.erase(numericKey);
        return true;
    }

    // Same size cap as int/float vars to prevent unbounded runtime growth.
    if (sfall_gl_vars_state->stringVars.size() >= kMaxGlobalVars
        && sfall_gl_vars_state->stringVars.find(numericKey) == sfall_gl_vars_state->stringVars.end()) {
        debugPrint("sfall_gl_vars_store_string: string vars cap (%zu) exceeded, rejecting key\n",
            kMaxGlobalVars);
        return false;
    }

    sfall_gl_vars_state->stringVars[numericKey] = std::string(value);
    return true;
}

char* sfall_gl_vars_fetch_string(const char* key)
{
    if (sfall_gl_vars_state == nullptr) {
        return nullptr;
    }

    bool ok;
    uint64_t numericKey = sfall_gl_vars_key_to_uint64(key, ok);
    if (!ok) {
        return nullptr;
    }

    auto it = sfall_gl_vars_state->stringVars.find(numericKey);
    if (it == sfall_gl_vars_state->stringVars.end()) {
        return nullptr;
    }

    const std::string& value = it->second;
    if (value.empty()) {
        return nullptr;
    }

    // Allocate and return a copy — caller must delete[].
    size_t len = value.size() + 1;
    char* result = new (std::nothrow) char[len];
    if (result == nullptr) {
        return nullptr;
    }
    memcpy(result, value.c_str(), len);
    return result;
}

bool sfall_gl_vars_remove_string(const char* key)
{
    if (sfall_gl_vars_state == nullptr) {
        return false;
    }

    bool ok;
    uint64_t numericKey = sfall_gl_vars_key_to_uint64(key, ok);
    if (!ok) {
        return false;
    }

    return sfall_gl_vars_state->stringVars.erase(numericKey) > 0;
}

} // namespace fallout
