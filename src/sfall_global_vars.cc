#include "sfall_global_vars.h"

#include <cstdint>
#include <cstring>
#include <unordered_map>

namespace fallout {

struct SfallGlobalVarsState {
    std::unordered_map<uint64_t, int> vars;
    std::unordered_map<uint64_t, float> floatVars;
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
static const int32_t kSfallGlobalVarsVersion = 1;

static bool sfall_gl_vars_store(uint64_t key, int value);
static bool sfall_gl_vars_fetch(uint64_t key, int& value);

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

    return true;
}

bool sfall_gl_vars_load(File* stream)
{
    // Clear existing state so that on any load failure the state
    // is empty rather than partially populated (see E-09).
    sfall_gl_vars_state->vars.clear();
    sfall_gl_vars_state->floatVars.clear();

    // Detect save format: new format starts with magic number "SFGV".
    // Old format starts with an int32 count (valid range 0-10000, never equals magic).
    uint32_t magicOrCount;
    if (fileRead(&magicOrCount, sizeof(magicOrCount), 1, stream) != 1) {
        return false;
    }

    int count;
    bool hasFloatVars = false;

    if (magicOrCount == kSfallGlobalVarsMagic) {
        // New format with version header.
        int32_t version;
        if (fileRead(&version, sizeof(version), 1, stream) != 1) {
            return false;
        }
        // Only version 1 is supported currently.
        if (version < 1 || version > 1) {
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

    if (count < 0 || count > 10000) {
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

        if (floatCount < 0 || floatCount > 10000) {
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

    return true;
}

bool sfall_gl_vars_store(const char* key, int value)
{
    if (strlen(key) != 8) {
        return false;
    }

    uint64_t numericKey;
    memcpy(&numericKey, key, sizeof(numericKey));
    return sfall_gl_vars_store(numericKey, value);
}

bool sfall_gl_vars_store(int key, int value)
{
    return sfall_gl_vars_store(static_cast<uint64_t>(key), value);
}

bool sfall_gl_vars_fetch(const char* key, int& value)
{
    if (strlen(key) != 8) {
        return false;
    }

    uint64_t numericKey;
    memcpy(&numericKey, key, sizeof(numericKey));
    return sfall_gl_vars_fetch(numericKey, value);
}

bool sfall_gl_vars_fetch(int key, int& value)
{
    return sfall_gl_vars_fetch(static_cast<uint64_t>(key), value);
}

static bool sfall_gl_vars_store(uint64_t key, int value)
{
    auto it = sfall_gl_vars_state->vars.find(key);
    if (it == sfall_gl_vars_state->vars.end()) {
        sfall_gl_vars_state->vars.emplace(key, value);
    } else {
        if (value == 0) {
            sfall_gl_vars_state->vars.erase(it);
        } else {
            it->second = value;
        }
    }

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

// --- Float global vars (parallel storage) ---

bool sfall_gl_vars_store_float(const char* key, float value)
{
    if (strlen(key) != 8) {
        return false;
    }

    uint64_t numericKey;
    memcpy(&numericKey, key, sizeof(numericKey));
    sfall_gl_vars_state->floatVars[numericKey] = value;
    return true;
}

bool sfall_gl_vars_store_float(int key, float value)
{
    sfall_gl_vars_state->floatVars[static_cast<uint64_t>(key)] = value;
    return true;
}

bool sfall_gl_vars_fetch_float(const char* key, float& value)
{
    if (strlen(key) != 8) {
        return false;
    }

    uint64_t numericKey;
    memcpy(&numericKey, key, sizeof(numericKey));
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

} // namespace fallout
