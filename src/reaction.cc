#include "reaction.h"

#include "scripts.h"

namespace fallout {

extern bool gFallout1Behavior;

// 0x4A29D0 reaction_set
int reactionSetValue(Object* critter, int value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_INT;
    programValue.integerValue = value;
    scriptSetLocalVar(critter->sid, 0, programValue);
    return 0;
}

// 0x4A29E8 reaction_to_level
int reactionTranslateValue(int value)
{
    // FO1 uses 25/-25 reaction thresholds. FO2 original binary uses 49/-51,
    // confirmed via binary address evidence and Et Tu VOODOO patches.
    int goodThreshold = gFallout1Behavior ? 25 : 49;
    int neutralThreshold = gFallout1Behavior ? -25 : -51;

    if (value > goodThreshold) {
        return NPC_REACTION_GOOD;
    } else if (value > neutralThreshold) {
        return NPC_REACTION_NEUTRAL;
    } else if (value > -25) {
        return NPC_REACTION_BAD;
    } else if (value > -50) {
        return NPC_REACTION_BAD;
    } else if (value > -75) {
        return NPC_REACTION_BAD;
    } else {
        return NPC_REACTION_BAD;
    }
}

// 0x4A29F0
int _reaction_influence_()
{
    return 0;
}

// 0x4A2B28 reaction_get
int reactionGetValue(Object* critter)
{
    ProgramValue programValue;

    if (scriptGetLocalVar(critter->sid, 0, programValue) == -1) {
        return -1;
    }

    return programValue.integerValue;
}

} // namespace fallout
