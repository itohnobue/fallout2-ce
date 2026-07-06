#ifndef TRAIT_H
#define TRAIT_H

#include "db.h"
#include "trait_defs.h"

namespace fallout {

int traitsInit();
void traitsReset();
void traitsExit();
int traitsLoad(File* stream);
int traitsSave(File* stream);
void traitsSetSelected(int trait1, int trait2, int trait3 = -1);
void traitsGetSelected(int* trait1, int* trait2, int* trait3 = nullptr);
char* traitGetName(int trait);
char* traitGetDescription(int trait);
int traitGetFrmId(int trait);
bool traitIsSelected(int trait);
int traitGetStatModifier(int stat);
int traitGetSkillModifier(int skill);

// Returns the max number of traits the player can select.
// FO1 (gFallout1Behavior=true) allows 3; FO2 defaults to 2.
int traitGetMaxSelectedCount();

} // namespace fallout

#endif /* TRAIT_H */
