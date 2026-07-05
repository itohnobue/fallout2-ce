#ifndef FALLOUT_SFALL_OPCODES_H_
#define FALLOUT_SFALL_OPCODES_H_

namespace fallout {

class ScriptHookCall;

ScriptHookCall* hookOpcodeGetCurrentCall(const char* opcodeName);

void sfallOpcodesInit();
void sfallOpcodesExit();

// Expose animation callback reset for hooks subsystem cleanup.
// Resets sfallAnimCallbackProgram to nullptr and
// sfallAnimCallbackProcedureIndex to -1, preventing stale pointer
// use-after-free after game reset.
void sfallAnimCallbackReset();

// Perk frequency override set by set_perk_freq (0x8247).
// 0 = use engine default (3 levels, or 4 with Skilled trait).
// Positive value = use this number of levels between perk selections.
// Integration point: characterEditorUpdateLevel() in character_editor.cc.
extern int gPerkFrequencyOverride;

// Skill maximum cap set by set_skill_max (0x81A2).
// Default 300 (vanilla match). Clamped to this value in skill increment
// paths. Integration point: skillAddForce() in skill.cc.
extern int gSkillMaxCap;

} // namespace fallout

#endif /* FALLOUT_SFALL_OPCODES_H_ */
