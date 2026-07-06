#ifndef FALLOUT_SFALL_METARULES_H_
#define FALLOUT_SFALL_METARULES_H_

#include "db.h"
#include "opcode_context.h"

#include <map>

namespace fallout {

// Spray settings for burst fire pattern customization.
// Set via set_spray_settings metarule. Consumer: combat.cc _compute_spray.
struct SpraySettings {
    int flags = 0;
    int pid = -1;
    int radius = 0;
    int count = 0;
    bool active = false;
};

// Drug data override for set_drugs_data metarule.
// Set via set_drugs_data metarule. Consumer: item.cc drugItemTakeDrug.
struct DrugData {
    int addictionRate = 0;
    int effectDuration = 0;
};

typedef void(MetaruleHandler)(OpcodeContext& ctx);

// The type of argument, not the same as actual data type. Useful for validation.
enum OpcodeArgumentType {
    ARG_ANY = 0, // no validation (default)
    ARG_INT, // integer only
    ARG_OBJECT, // non-null pointer/object
    ARG_STRING, // string only
    ARG_INTSTR, // integer OR string
    ARG_NUMBER, // float OR integer
};

struct MetaruleInfo {
    const char* name;
    MetaruleHandler* handler;
    int minArgs;
    int maxArgs;
    int errorReturn;
    OpcodeArgumentType argumentTypes[METARULE_MAX_ARGS];
};

extern const MetaruleInfo kMetarules[];
extern const std::size_t kMetarulesCount;

class Program;

void sfall_metarule(Program* program, int args);
void sfall_metarules_reset();
// Save/load metarule state to/from File* stream.
// Called by sfallgv.sav persistence in sfall_ext.cc.
void sfall_metarules_save(File* stream);
void sfall_metarules_load(File* stream);
void mf_string_format(OpcodeContext& ctx);

// Returns override town title for given area index, or nullptr if no override set.
const char* sfallGetTownTitleOverride(int areaIndex);
// Returns override car interface art FID, or -1 if no override set.
int sfallGetCarIntfaceArtFid();

// Returns pointer to spray settings struct (nullptr-safe: always valid, check .active).
const SpraySettings* sfallGetSpraySettings();

// Looks up drug data override for the given drug index.
// Returns true if an override exists, populating out params.
bool sfallGetDrugDataOverride(int drugIndex, int* outAddictionRate, int* outEffectDuration);

// Returns true if the specified trait has been added to the player via
// the add_trait metarule. Added traits participate in stat/skill modifier
// calculations alongside the two player-selected traits.
// Implementation is in sfall_metarules.cc.
bool sfallIsTraitAdded(int traitId);

} // namespace fallout

#endif /* FALLOUT_SFALL_METARULES_H_ */
