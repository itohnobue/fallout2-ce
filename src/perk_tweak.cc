#include "perk_tweak.h"

#include "config.h"
#include "debug.h"
#include "platform_compat.h"
#include "sfall_config.h"

#include <climits>

namespace fallout {

// FO2 default values come from the in-class initializers in perk_tweak.h;
// an absent file or key leaves them untouched.
PerkTweak gPerkTweak;

char* perkTweakGetPerksFilePath()
{
    char* perksFile = nullptr;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PERKS_FILE_KEY, &perksFile);
    if (perksFile == nullptr || perksFile[0] == '\0') {
        return nullptr;
    }
    // Pointer is owned by gSfallConfig (valid for the process lifetime);
    // callers must NOT free it.
    return perksFile;
}

// Reads a single [PerksTweak] key with sfall's gate+clamp semantics:
//   - key absent from the file -> keep the current (default) value
//   - value below minGate -> NOT applied, keep the current value
//     (sfall: `if (value >= minValue)` before patching)
//   - value above maxClamp -> clamped to maxClamp
static void perkTweakReadInt(Config* config, const char* key, int* out, int minGate, int maxClamp)
{
    int value = 0;
    if (!configGetInt(config, "PerksTweak", key, &value)) {
        return;
    }
    if (value < minGate) {
        return;
    }
    if (value > maxClamp) {
        value = maxClamp;
    }
    *out = value;
}

// Reads a [PerksTweak] key with the VaultCityInoculations semantics:
// any value applies, clamped to [minClamp, maxClamp] on both sides.
static void perkTweakReadIntClampedBoth(Config* config, const char* key, int* out, int minClamp, int maxClamp)
{
    int value = 0;
    if (!configGetInt(config, "PerksTweak", key, &value)) {
        return;
    }
    if (value < minClamp) {
        value = minClamp;
    } else if (value > maxClamp) {
        value = maxClamp;
    }
    *out = value;
}

static void perkTweakLoadFromConfig(Config* config)
{
    perkTweakReadInt(config, "NightVisionBonus", &gPerkTweak.nightVisionBonus, 0, 100);
    perkTweakReadInt(config, "SurvivalistBonus", &gPerkTweak.survivalistBonus, 0, 125);
    perkTweakReadInt(config, "MasterTraderBonus", &gPerkTweak.masterTraderBonus, 0, INT_MAX);
    perkTweakReadInt(config, "MrFixitBonus", &gPerkTweak.mrFixitBonus, 0, 125);
    perkTweakReadInt(config, "MedicFirstAidBonus", &gPerkTweak.medicFirstAidBonus, 0, 125);
    perkTweakReadInt(config, "MedicDoctorBonus", &gPerkTweak.medicDoctorBonus, 0, 125);
    perkTweakReadInt(config, "MasterThiefBonus", &gPerkTweak.masterThiefBonus, 0, 125);
    perkTweakReadInt(config, "SpeakerBonus", &gPerkTweak.speakerBonus, 0, 125);
    perkTweakReadInt(config, "GhostBonus", &gPerkTweak.ghostBonus, 0, 125);
    perkTweakReadInt(config, "RangerOutdoorsmanBonus", &gPerkTweak.rangerOutdoorsmanBonus, 0, 125);
    perkTweakReadInt(config, "WeaponLongRangeBonus", &gPerkTweak.weaponLongRangeBonus, 2, 100);
    perkTweakReadInt(config, "WeaponAccurateBonus", &gPerkTweak.weaponAccurateBonus, 0, 125);
    perkTweakReadInt(config, "WeaponScopeRangePenalty", &gPerkTweak.weaponScopeRangePenalty, 0, 100);
    perkTweakReadInt(config, "WeaponScopeRangeBonus", &gPerkTweak.weaponScopeRangeBonus, 2, 100);
    perkTweakReadIntClampedBoth(config, "VaultCityInoculationsPoisonBonus", &gPerkTweak.vaultCityInoculationsPoisonBonus, -100, 100);
    perkTweakReadIntClampedBoth(config, "VaultCityInoculationsRadBonus", &gPerkTweak.vaultCityInoculationsRadBonus, -100, 100);
    perkTweakReadInt(config, "CautiousNatureBonus", &gPerkTweak.cautiousNatureBonus, -12, 20);
    perkTweakReadInt(config, "DemolitionExpertBonus", &gPerkTweak.demolitionExpertBonus, 0, 999);
    perkTweakReadInt(config, "GamblerBonus", &gPerkTweak.gamblerBonus, 0, 125);
    perkTweakReadInt(config, "HarmlessBonus", &gPerkTweak.harmlessBonus, 0, 125);
    perkTweakReadInt(config, "LivingAnatomyBonus", &gPerkTweak.livingAnatomyBonus, 0, 125);
    perkTweakReadInt(config, "LivingAnatomyDoctorBonus", &gPerkTweak.livingAnatomyDoctorBonus, 0, 125);
    perkTweakReadInt(config, "NegotiatorBonus", &gPerkTweak.negotiatorBonus, 0, 125);
    perkTweakReadInt(config, "PyromaniacBonus", &gPerkTweak.pyromaniacBonus, 0, 125);
    perkTweakReadInt(config, "SalesmanBonus", &gPerkTweak.salesmanBonus, 0, 999);
    perkTweakReadInt(config, "StonewallPercent", &gPerkTweak.stonewallPercent, 0, 100);
    perkTweakReadInt(config, "ThiefBonus", &gPerkTweak.thiefBonus, 0, 125);
    perkTweakReadInt(config, "WeaponHandlingBonus", &gPerkTweak.weaponHandlingBonus, 0, 10);
    perkTweakReadInt(config, "VaultCityTrainingFirstAidBonus", &gPerkTweak.vaultCityTrainingFirstAidBonus, 0, 125);
    perkTweakReadInt(config, "VaultCityTrainingDoctorBonus", &gPerkTweak.vaultCityTrainingDoctorBonus, 0, 125);
    perkTweakReadInt(config, "ExpertExcrementExpeditorBonus", &gPerkTweak.expertExcrementExpeditorBonus, 0, 125);
    perkTweakReadInt(config, "EducatedBonus", &gPerkTweak.educatedBonus, 0, 125);
    perkTweakReadInt(config, "HealerMinBonus", &gPerkTweak.healerMinBonus, 0, 999);
    perkTweakReadInt(config, "HealerMaxBonus", &gPerkTweak.healerMaxBonus, 0, 999);
    perkTweakReadInt(config, "LifegiverBonus", &gPerkTweak.lifegiverBonus, 0, 125);
    perkTweakReadInt(config, "ComprehensionBonus", &gPerkTweak.comprehensionBonus, 0, INT_MAX);
}

void perkTweakLoad()
{
    char* perksFile = perkTweakGetPerksFilePath();
    if (perksFile == nullptr) {
        return;
    }

    ScopedConfig config { perksFile, false };
    if (!config) {
        debugPrint("Perks config %s not found.\n", perksFile);
        return;
    }

    perkTweakLoadFromConfig(config.get());
}

// TEST-ONLY: injects a populated Config so tests can exercise the
// PerksTweak gate+clamp semantics without real file I/O (compat_fopen
// is stubbed to nullptr in the test harness). Guarded behind
// TEST_ACCESSORS_ENABLED — test define before include.
#if defined(TEST_ACCESSORS_ENABLED)
void perkTweakLoadFromConfigForTest(Config* config)
{
    perkTweakLoadFromConfig(config);
}
#endif

} // namespace fallout
