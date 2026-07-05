#ifndef FALLOUT_SFALL_OPCODES_H_
#define FALLOUT_SFALL_OPCODES_H_

namespace fallout {

struct Object;
class ScriptHookCall;

ScriptHookCall* hookOpcodeGetCurrentCall(const char* opcodeName);

void sfallOpcodesInit();
void sfallOpcodesExit();

// Resets all opcode-related global state (fake perks/traits, XP modifier,
// hit chance globals, knockback settings, perkbox title, etc.) for clean
// game reset between save/load cycles. Called from gameReset().
void sfallOpcodesReset();

// Expose animation callback reset for hooks subsystem cleanup.
// Resets sfallAnimCallbackProgram to nullptr and
// sfallAnimCallbackProcedureIndex to -1, preventing stale pointer
// use-after-free after game reset.
void sfallAnimCallbackReset();

// Invoke the registered sfall animation callback procedure on the given object.
// Called from animation completion paths. The callback procedure receives the
// animated object as its argument.
void sfallAnimCallbackInvoke(Object* object);

// Close all VFS file handles. Called on game reset and program exit to
// prevent handle exhaustion across save/load cycles.
void sfallVfsCloseAll();

// Perk frequency override set by set_perk_freq (0x8247).
// 0 = use engine default (3 levels, or 4 with Skilled trait).
// Positive value = use this number of levels between perk selections.
// Integration point: characterEditorUpdateLevel() in character_editor.cc.
extern int gPerkFrequencyOverride;

// Skill points per level modifier set by mod_skill_points_per_level (0x8246).
// Added to the base skill point calculation in characterEditorUpdateLevel().
extern int gSkillPointsPerLevelMod;

// Last target/attacker tracking for get_last_target (0x8248) and
// get_last_attacker (0x8249) sfall opcodes.
extern int gLastAttacker;
extern int gLastTarget;

// Skill maximum cap set by set_skill_max (0x81A2).
// Default 300 (vanilla match). Clamped to this value in skill increment
// paths. Integration point: skillAddForce() in skill.cc.
extern int gSkillMaxCap;

// XP modifier percentage set by set_xp_mod (0x81AA).
// Default 100 (no modification). Applied in pcAddExperience() as a
// multiplier: adjusted_xp = xp * gXpModPercentage / 100.
extern int gXpModPercentage;

} // namespace fallout

#endif /* FALLOUT_SFALL_OPCODES_H_ */
