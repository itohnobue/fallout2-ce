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

// Persist all opcode-related global state into the sfall global vars map
// (which is serialized to sfallgv.sav). Must be called BEFORE sfall_gl_vars_save()
// during save so the current runtime values are included in the save file.
void sfallOpcodeStateSave();

// Restore all opcode-related global state from the sfall global vars map.
// Must be called AFTER sfall_gl_vars_load() during load to rebuild the
// runtime state from the save file. If no saved state exists (first run,
// old save), the sfallOpcodesReset() defaults remain in effect.
void sfallOpcodeStateLoad();

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

// ============================================================
// Knockback globals (F-004) — exposed for combat integration.
// Set via opcodes 0x8195-0x819A. Integration point: the knockback
// calculation in combat.cc (attackComputeDamage) should consult these
// values to override vanilla knockback behavior.
// ============================================================
extern int sfallWeaponKnockbackType;
extern float sfallWeaponKnockbackValue;
extern int sfallTargetKnockbackType;
extern float sfallTargetKnockbackValue;
extern int sfallAttackerKnockbackType;
extern float sfallAttackerKnockbackValue;

// ============================================================
// Hit chance globals (F-005, F-006) — exposed for combat integration.
// Set via opcodes 0x81C5 (set_critter_hit_chance_mod) and 0x81C6
// (set_base_hit_chance_mod), 0x81A1 (set_hit_chance_max).
// Integration point: attackDetermineToHit() in combat.cc should clamp
// to-hit using sfallHitChanceMax instead of hardcoded 95, and apply
// sfallHitChanceMod as a bonus/malus.
// ============================================================
extern int sfallHitChanceMod;
extern int sfallHitChanceMax;

// ============================================================
// Pipboy availability override (F-019).
// Set via opcode 0x818B (set_pipboy_available).
// -1 = not set (use engine default/static config).
//  0 = pipboy forcefully unavailable.
//  1 = pipboy forcefully available.
// Integration point: pipboyOpen() in pipboy.cc should check this value
// alongside the engine's pipboy_available_at_game_start.
// ============================================================
extern int gPipboyAvailableOverride;

// ============================================================
// HP per level modifier (F-007). Set via opcode 0x81CE.
// Integration point: stat.cc HP-per-level calculation should add
// this modifier to endurance/2 + 2 + lifegiver bonus.
// ============================================================
int sfallGetHpPerLevelMod();

// ============================================================
// Pyromaniac damage modifier (F-030). Set via opcode 0x81CB.
// Integration point: combat.cc attackComputeDamage pyromaniac
// calculation should add this modifier.
// ============================================================
int sfallGetPyromaniacMod();

// ============================================================
// Swift Learner XP modifier (F-033). Set via opcode 0x81CD.
// Integration point: stat.cc XP calculation (Swift Learner perk)
// should add this modifier to perkGetRank(PERK_SWIFT_LEARNER) * 5.
// ============================================================
int sfallGetSwiftLearnerMod();

// ============================================================
// Perk level modifier (F-035). Set via opcode 0x81AB.
// Integration point: character_editor.cc characterEditorUpdateLevel()
// should subtract this from the perk frequency progression.
// ============================================================
int sfallGetPerkLevelMod();

// ============================================================
// Skill modifier globals (F-034). Set via opcodes 0x81C7/0x81C8.
// Integration point: skill.cc skillGetValue() should add these
// modifiers to the skill value calculation.
// ============================================================
int sfallGetCritterSkillMod();
int sfallGetBaseSkillMod();

// ============================================================
// Perk add mode and clear-selectable-perks flags (F-036).
// Set via opcodes 0x81C3/0x81C4.
// Integration point: character_editor.cc perk selection dialog
// should consult these when building the perk list.
// ============================================================
int sfallGetPerkAddMode();
bool sfallGetClearSelectablePerks();
int sfallGetHideRealPerks();

// ============================================================
// Fake perk/trait arrays (F-037). Populated via opcodes
// 0x81BB (set_fake_perk), 0x81BC (set_fake_trait),
// 0x81BD (set_selectable_perk).
// Integration point: character_editor.cc perk dialog should iterate
// these entries alongside built-in engine perks.
// ============================================================
struct FakePerkEntry {
    char* name;
    int level;
    int image;
    char* desc;
    bool active;
};
struct FakeTraitEntry {
    char* name;
    int active;
    int image;
    char* desc;
};
const FakePerkEntry* sfallGetFakePerks(int* outCount);
int sfallGetFakePerkCount();
const FakeTraitEntry* sfallGetFakeTraits(int* outCount);

// ============================================================
// Per-critter aimed-shot override flags (F-016).
// Set via opcodes 0x823E (force_aimed_shots) and 0x823F
// (disable_aimed_shots). Keyed by proto ID (PID).
// Returns true if the critter PID has a forced aimed-shot override.
// ============================================================
bool sfallGetForceAimedShots(int pid);
bool sfallGetDisableAimedShots(int pid);

// ============================================================
// Per-critter hit chance modifier entry (F-019).
// Separate from the global sfallHitChanceMod/sfallHitChanceMax
// which are used by set_base_hit_chance_mod (0x81C6).
// Set via opcode 0x81C5 (set_critter_hit_chance_mod).
// ============================================================
bool sfallGetCritterHitChanceMod(Object* critter, int& outMod, int& outMax);

// ============================================================
// Perk property override arrays (F-015). Each is indexed by
// perk ID (PERK_COUNT entries). -1 sentinel = no override set.
// Set via opcodes 0x8178-0x8188 (set_perk_*),
// 0x817A (set_perk_level routed through perkSetMinLevel).
// Integration point: perk.cc should check these arrays before
// returning gPerkDescriptions[] values.
// ============================================================
int sfallGetPerkImageOverride(int perkID);
int sfallGetPerkRanksOverride(int perkID);
int sfallGetPerkStatOverride(int perkID);
int sfallGetPerkStatMagOverride(int perkID);
int sfallGetPerkSkill1Override(int perkID);
int sfallGetPerkSkill1MagOverride(int perkID);
int sfallGetPerkSkill2Override(int perkID);
int sfallGetPerkSkill2MagOverride(int perkID);
int sfallGetPerkTypeOverride(int perkID);
int sfallGetPerkSpecialOverride(int perkID, int statIdx);

// ============================================================
// Movie path override (F-021). Set via opcode 0x8177.
// Returns nullptr if no override set for the given movie ID.
// Integration point: movie.cc should check this override when
// resolving movie file paths.
// ============================================================
const char* sfallGetMoviePathOverride(int movieId);

} // namespace fallout

#endif /* FALLOUT_SFALL_OPCODES_H_ */
