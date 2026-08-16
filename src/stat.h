#ifndef STAT_H
#define STAT_H

#include "db.h"
#include "obj_types.h"
#include "proto_types.h"
#include "stat_defs.h"

namespace fallout {

#define STAT_ERR_INVALID_STAT (-5)

int statsInit();
int statsReset();
int statsExit();
int statsLoad(File* stream);
int statsSave(File* stream);
void statResetUnspentApBonuses();
void statSetUnspentApBonus(int multiplier);
int statGetUnspentApBonus();
void statSetUnspentApPerkBonus(int multiplier);
int statGetUnspentApPerkBonus();
int statGetConfiguredMaximum(Stat stat, bool npc);
int statGetConfiguredMinimum(Stat stat, bool npc);
void statSetPcMaximum(Stat stat, int maximum);
void statSetPcMinimum(Stat stat, int minimum);
void statSetNpcMaximum(Stat stat, int maximum);
void statSetNpcMinimum(Stat stat, int minimum);
int critterGetStat(Object* critter, Stat stat);
int critterGetBaseStatWithTraitModifier(Object* critter, Stat stat);
int critterGetBaseStat(Object* critter, Stat stat);
int critterGetBonusStat(Object* critter, Stat stat);
int critterSetBaseStat(Object* critter, Stat stat, int value);
int critterIncBaseStat(Object* critter, Stat stat);
int critterDecBaseStat(Object* critter, Stat stat);
int critterSetBonusStat(Object* critter, Stat stat, int value);
void protoCritterDataResetStats(CritterProtoData* data);
void critterUpdateDerivedStats(Object* critter);
char* statGetName(Stat stat);
char* statGetDescription(Stat stat);
char* statGetValueDescription(int value);
int pcGetStat(PcStat pcStat);
int pcSetStat(PcStat pcStat, int value);
void pcStatsReset();
int pcGetExperienceForNextLevel();
int pcGetExperienceForLevel(int level);
char* pcStatGetName(PcStat pcStat);
char* pcStatGetDescription(PcStat pcStat);
int statGetFrmId(Stat stat);
int statRoll(Object* critter, Stat stat, int modifier, int* howMuch);
int pcAddExperience(int xp, int* xpGained = nullptr);
int pcAddExperienceWithOptions(int xp, bool doParty, int* xpGained = nullptr);
int pcSetExperience(int xp);

// Note: statIsValid() and pcStatIsValid() are defined inline in stat_defs.h
// (included above) — upstream moved them there in the enum-hardening
// refactor. Do not redeclare them here as static inline: that produces a
// "static declaration follows non-static declaration" error in any TU that
// includes both headers.

// Sets the maximum value for a stat (used by set_stat_max et al. sfall opcodes).
// Validates stat index; silently ignored on invalid stat.
void statSetMaxValue(int stat, int value);

// Sets the minimum value for a stat (used by set_stat_min et al. sfall opcodes).
// Validates stat index; silently ignored on invalid stat.
void statSetMinValue(int stat, int value);

// Returns the current maximum value for a stat, or -1 if stat is invalid.
// Reads the live (possibly overridden) value from gStatDescriptions[],
// unlike sfall_metarules.cc which uses a static const kDefaultStatLimits table.
int statGetMaxValue(int stat);

// Returns the current minimum value for a stat, or -1 if stat is invalid.
// Reads the live (possibly overridden) value from gStatDescriptions[].
int statGetMinValue(int stat);

// Returns the effective PC level cap, gated by gFallout1Behavior.
// FO1 mode (gFallout1Behavior=true): cap is 21.
// FO2 mode (gFallout1Behavior=false): cap is PC_LEVEL_MAX (99).
int statGetLevelCap();

} // namespace fallout

#endif /* STAT_H */
