#include "reaction.h"

#include "debug.h"
#include "scripts.h"

namespace fallout {

extern bool gFallout1Behavior;

// d6d58ab: configurable reaction thresholds (set_reaction_thresholds
// metarule). The fork defaults keep the FO1/FO2-specific thresholds used
// before the metarule existed (see reactionTranslateValue comment).
// Declared with the FO2 defaults so any use before the first gameReset
// (which re-derives them from gFallout1Behavior) still sees sane values.
static int neutralReactionThreshold = -51;
static int goodReactionThreshold = 49;

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
    if (value > goodReactionThreshold) {
        return NPC_REACTION_GOOD;
    } else if (value > neutralReactionThreshold) {
        return NPC_REACTION_NEUTRAL;
    } else {
        return NPC_REACTION_BAD;
    }
}

void reactionSetThresholds(int neutralThreshold, int goodThreshold)
{
    neutralReactionThreshold = neutralThreshold;
    goodReactionThreshold = goodThreshold;

    debugPrint("Reaction: set thresholds neutral=%d good=%d\n", neutralThreshold, goodThreshold);
}

void reactionResetThresholds()
{
    // FO1 uses 25/-25 reaction thresholds. FO2 original binary uses 49/-51,
    // confirmed via binary address evidence and Et Tu VOODOO patches.
    int goodThreshold = gFallout1Behavior ? 25 : 49;
    int neutralThreshold = gFallout1Behavior ? -25 : -51;
    reactionSetThresholds(neutralThreshold, goodThreshold);
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
