#ifndef TRAIT_TWEAK_H
#define TRAIT_TWEAK_H

#include "config.h"
#include "skill_defs.h"
#include "stat_defs.h"
#include "trait_defs.h"

namespace fallout {

// Maximum StatMod pairs per trait (one per primary stat).
#define TRAIT_TWEAK_MAX_STAT_MODS PRIMARY_STAT_COUNT
// Maximum SkillMod pairs per trait (one per skill).
#define TRAIT_TWEAK_MAX_SKILL_MODS SKILL_COUNT

// sfall [Traits] section (config/Perks.ini) — per-trait overrides.
//
// Mirrors sfall's PerksFile [Traits] section semantics (et tu ships
// [Traits] Enable=1 with Night Person/Skilled NoHardcode rows):
//   - gated by Enable=1 in the [Traits] section
//   - each [tN] block (N = trait ID) carries:
//       NoHardcode=1  — disable the trait's hardcoded engine effects
//       Name/Desc     — display text overrides (strdup'd, freed at exit)
//       Image         — art override (intrface.lst line)
//       StatMod       — pipe-separated statID|mod pairs (e.g. 1|-1|4|-1)
//       SkillMod      — pipe-separated skillID|mod pairs
//   - the engine applies StatMod/SkillMod contributions for selected
//     traits on top of (or instead of, with NoHardcode) the hardcoded
//     trait_adjust_stat/trait_adjust_skill effects
//
// Trait IDs are positional (et tu uses FO1's numbering, where index 13
// is Night Person instead of FO2's Sex Appeal; CE's enum keeps the FO2
// layout — the index maps directly, and the display name comes from the
// mod's trait.msg).
struct TraitTweak {
    bool noHardcode = false;

    // Art override; -1 = no override (trait.msg/engine default applies).
    int frmId = -1;

    // Display overrides (strdup'd, owned by this module; freed at exit).
    char* name = nullptr;
    char* description = nullptr;

    // StatMod pairs: stat to modify + modifier per pair.
    struct StatMod {
        Stat stat;
        int mod;
    };
    StatMod statMods[TRAIT_TWEAK_MAX_STAT_MODS] = {};
    int statModCount = 0;

    // SkillMod pairs: skill to modify + modifier per pair.
    struct SkillMod {
        Skill skill;
        int mod;
    };
    SkillMod skillMods[TRAIT_TWEAK_MAX_SKILL_MODS] = {};
    int skillModCount = 0;
};

// Per-trait overrides loaded from the [Traits] section of the file
// configured in ddraw.ini [Misc] PerksFile (see trait.cc consumers).
extern TraitTweak gTraitTweak[TRAIT_COUNT];

// Loads gTraitTweak from the [Traits] section of the PerksFile.
// Missing/empty PerksFile -> no-op. On reload, Name/Desc overrides are
// replaced; non-string fields (NoHardcode/Image/StatMod/SkillMod) keep
// their current values when a re-read file drops the key (absent-key =
// keep current, consistent with the [Perks] section precedent). Safe to
// call multiple times.
void traitTweakLoad();

// Frees strdup'd Name/Desc overrides (called from traitsExit).
void traitTweakFree();

// Returns true when the trait has NoHardcode set (hardcoded engine
// effects disabled for it).
bool traitTweakHasNoHardcode(Trait trait);

// ============================================================
// TEST-ONLY: config injection for the [Traits] parser. Runs the
// same parse logic traitTweakLoad() applies to the file-backed
// Config, against the given Config (populated via configSetInt/
// configSetString). Guarded behind TEST_ACCESSORS_ENABLED.
// ============================================================
#if defined(TEST_ACCESSORS_ENABLED)
void traitTweakLoadFromConfigForTest(Config* config);
#endif

} // namespace fallout

#endif /* TRAIT_TWEAK_H */
