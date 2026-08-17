#include "trait_tweak.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "dictionary.h"
#include "memory.h"
#include "perk_tweak.h"
#include "platform_compat.h"
#include "sfall_config.h"

namespace fallout {

TraitTweak gTraitTweak[TRAIT_COUNT];

bool traitTweakHasNoHardcode(Trait trait)
{
    if (trait < TRAIT_FIRST || trait >= TRAIT_COUNT) {
        return false;
    }
    return gTraitTweak[trait].noHardcode;
}

// Parses a pipe-separated "id|mod|id|mod|..." list into the output pair
// arrays. Ids are validated against [validMin, validMax); out-of-range or
// malformed pairs are skipped (the rest still apply). Pairs beyond the
// output capacity are ignored.
static void traitTweakParsePairList(const char* value, int validMin, int validMax, int* ids, int* mods, int maxCount, int* count)
{
    if (value == nullptr) {
        return;
    }

    char buffer[512];
    strncpy(buffer, value, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    int upto = 0;
    char* token = strtok(buffer, "|");
    while (token != nullptr && upto < maxCount) {
        char* end = nullptr;
        errno = 0;
        long id = strtol(token, &end, 10);
        if (end == token || errno == ERANGE) {
            break; // non-numeric id — malformed list, stop
        }
        token = strtok(nullptr, "|");
        if (token == nullptr) {
            break; // trailing id without a mod — malformed pair
        }
        end = nullptr;
        errno = 0;
        long mod = strtol(token, &end, 10);
        if (end == token || errno == ERANGE) {
            break; // non-numeric mod — malformed pair, stop
        }

        if (id >= validMin && id < validMax && mod >= INT_MIN && mod <= INT_MAX) {
            ids[upto] = static_cast<int>(id);
            mods[upto] = static_cast<int>(mod);
            upto++;
        }

        token = strtok(nullptr, "|");
    }

    *count = upto;
}

// Parses the [Traits] section of the PerksFile into gTraitTweak.
static void traitTweakLoadFromConfig(Config* config)
{
    int enable = 0;
    if (!configGetInt(config, "Traits", "Enable", &enable, 0) || enable == 0) {
        return;
    }

    Config* traitsConfig = config;
    for (int i = 0; i < traitsConfig->entriesLength; i++) {
        DictionaryEntry* entry = &(traitsConfig->entries[i]);
        const char* sectionName = entry->key;

        // Trait sections are "t" followed by the numeric trait ID
        // (e.g. "t13" = Night Person in FO1 numbering). Skip everything
        // else (Perks/PerksTweak/Traits/...).
        if (sectionName[0] != 't' && sectionName[0] != 'T') {
            continue;
        }
        if (sectionName[1] < '0' || sectionName[1] > '9') {
            continue;
        }

        // Strict parse: the whole section name must be consumed by strtol
        // (a section named "t13x" would otherwise silently retarget trait 13).
        // Leading whitespace is skipped by strtol; trailing whitespace is
        // tolerated here to match the config reader's section-name trimming.
        char* end = nullptr;
        errno = 0;
        long traitIdLong = strtol(sectionName + 1, &end, 10);
        while (end != sectionName + 1 && isspace(static_cast<unsigned char>(*end))) {
            end++;
        }
        if (end == sectionName + 1 || *end != '\0' || errno == ERANGE || traitIdLong < 0 || traitIdLong > INT_MAX) {
            debugPrint("Perks config: malformed trait section '[%s]' ignored\n", sectionName);
            continue;
        }

        int traitId = static_cast<int>(traitIdLong);
        if (!traitIsValid(traitId)) {
            continue;
        }

        TraitTweak* tweak = &(gTraitTweak[traitId]);

        int value = 0;
        if (configGetInt(traitsConfig, sectionName, "NoHardcode", &value, 0)) {
            tweak->noHardcode = value != 0;
        }

        if (configGetInt(traitsConfig, sectionName, "Image", &value)) {
            tweak->frmId = value;
        }

        char* stringValue = nullptr;
        if (configGetString(traitsConfig, sectionName, "Name", &stringValue)) {
            if (tweak->name != nullptr) {
                internal_free(tweak->name);
            }
            // Truncate to 255 chars: the character selector copies trait
            // names into a 260-byte stack buffer (character_selector.cc:890)
            // with an unbounded strcpy; sfall documents Name as <= 63 chars,
            // so 255 is a generous safety ceiling.
            size_t len = strlen(stringValue);
            if (len > 255) {
                len = 255;
            }
            tweak->name = (char*)internal_malloc(len + 1);
            memcpy(tweak->name, stringValue, len);
            tweak->name[len] = '\0';
        }

        if (configGetString(traitsConfig, sectionName, "Desc", &stringValue)) {
            if (tweak->description != nullptr) {
                internal_free(tweak->description);
            }
            size_t len = strlen(stringValue);
            if (len > 255) {
                len = 255;
            }
            tweak->description = (char*)internal_malloc(len + 1);
            memcpy(tweak->description, stringValue, len);
            tweak->description[len] = '\0';
        }

        if (configGetString(traitsConfig, sectionName, "StatMod", &stringValue)) {
            int ids[TRAIT_TWEAK_MAX_STAT_MODS] = { 0 };
            int mods[TRAIT_TWEAK_MAX_STAT_MODS] = { 0 };
            int count = 0;
            traitTweakParsePairList(stringValue, STAT_FIRST, PRIMARY_STAT_COUNT, ids, mods, TRAIT_TWEAK_MAX_STAT_MODS, &count);
            for (int j = 0; j < count; j++) {
                tweak->statMods[j].stat = static_cast<Stat>(ids[j]);
                tweak->statMods[j].mod = mods[j];
            }
            tweak->statModCount = count;
        }

        if (configGetString(traitsConfig, sectionName, "SkillMod", &stringValue)) {
            int ids[TRAIT_TWEAK_MAX_SKILL_MODS] = { 0 };
            int mods[TRAIT_TWEAK_MAX_SKILL_MODS] = { 0 };
            int count = 0;
            traitTweakParsePairList(stringValue, SKILL_FIRST, SKILL_COUNT, ids, mods, TRAIT_TWEAK_MAX_SKILL_MODS, &count);
            for (int j = 0; j < count; j++) {
                tweak->skillMods[j].skill = static_cast<Skill>(ids[j]);
                tweak->skillMods[j].mod = mods[j];
            }
            tweak->skillModCount = count;
        }
    }
}

void traitTweakLoad()
{
    char* perksFile = perkTweakGetPerksFilePath();
    if (perksFile == nullptr) {
        return;
    }

    ScopedConfig config { perksFile, false };
    if (!config) {
        return;
    }

    traitTweakLoadFromConfig(config.get());
}

void traitTweakFree()
{
    for (int trait = TRAIT_FIRST; trait < TRAIT_COUNT; trait++) {
        TraitTweak* tweak = &(gTraitTweak[trait]);
        if (tweak->name != nullptr) {
            internal_free(tweak->name);
            tweak->name = nullptr;
        }
        if (tweak->description != nullptr) {
            internal_free(tweak->description);
            tweak->description = nullptr;
        }
    }
}

// TEST-ONLY: injects a populated Config so tests can exercise the
// [Traits] parse logic without real file I/O (compat_fopen is stubbed
// to nullptr in the test harness). Guarded behind TEST_ACCESSORS_ENABLED.
#if defined(TEST_ACCESSORS_ENABLED)
void traitTweakLoadFromConfigForTest(Config* config)
{
    traitTweakLoadFromConfig(config);
}
#endif

} // namespace fallout
