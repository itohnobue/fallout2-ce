#include "sfall_opcodes.h"

#include <algorithm>
#include <cstdint>
#include <limits.h>
#include <unordered_map>
#include <math.h>
#include <string.h>

#include "animation.h"
#include "art.h"
#include "character_editor.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "critter.h"
#include "dbox.h"
#include "debug.h"
#include "game.h"
#include "game_dialog.h"
#include "input.h"
#include "interface.h"
#include "interpreter.h"
#include "inventory.h"
#include "item.h"
#include "light.h"
#include "map.h"
#include "memory.h"
#include "message.h"
#include "mouse.h"
#include "object.h"
#include "party_member.h"
#include "perk.h"
#include "proto.h"
#include "proto_instance.h"
#include "random.h"
#include "script_sound.h"
#include "scripts.h"
#include "sfall_animation.h"
#include "sfall_arrays.h"
#include "sfall_global_scripts.h"
#include "sfall_global_vars.h"
#include "sfall_ini.h"
#include "sfall_kb_helpers.h"
#include "sfall_lists.h"
#include "sfall_metarules.h"
#include "sfall_script_hooks.h"
#include "skill.h"
#include "stat.h"
#include "svga.h"
#include "tile.h"
#include "trait.h"
#include "window_manager.h"
#include "worldmap.h"

namespace fallout {

typedef enum ExplosionMetarule {
    EXPL_FORCE_EXPLOSION_PATTERN = 1,
    EXPL_FORCE_EXPLOSION_ART = 2,
    EXPL_FORCE_EXPLOSION_RADIUS = 3,
    EXPL_FORCE_EXPLOSION_DMGTYPE = 4,
    EXPL_STATIC_EXPLOSION_RADIUS = 5,
    EXPL_GET_EXPLOSION_DAMAGE = 6,
    EXPL_SET_DYNAMITE_EXPLOSION_DAMAGE = 7,
    EXPL_SET_PLASTIC_EXPLOSION_DAMAGE = 8,
    EXPL_SET_EXPLOSION_MAX_TARGET = 9,
} ExplosionMetarule;

// Version constants: reported to scripts via sfall_ver_major/minor/build.
// Bumped from 4.4.9 to 4.5.1 for RPU/ET Tu compatibility.
// These remain constexpr for now; CMake-configured version would allow
// compile-time customization for different sfall compatibility targets.
static constexpr int kVersionMajor = 4;
static constexpr int kVersionMinor = 5;
static constexpr int kVersionPatch = 1;
static constexpr int kSfallPathBufferSize = 3200; // matches rotation path size in animation.cc

static void op_art_exists(Program* program)
{
    int fid = programStackPopInteger(program);
    programStackPushInteger(program, artExists(fid));
}

static void op_obj_is_carrying_obj(Program* program)
{
    Object* itemObj = static_cast<Object*>(programStackPopPointer(program));
    Object* invenObj = static_cast<Object*>(programStackPopPointer(program));

    int count = 0;
    if (invenObj != nullptr && itemObj != nullptr) {
        Inventory* inventory = &(invenObj->data.inventory);
        for (int index = 0; index < inventory->length; index++) {
            InventoryItem* inventoryItem = &(inventory->items[index]);
            if (inventoryItem->item == itemObj) {
                if (inventoryItem->quantity <= 0) {
                    debugPrint("%s: obj_is_carrying_obj found non-positive inventory quantity for item %p in owner %p",
                        program->name,
                        itemObj,
                        invenObj);
                    count = 1;
                } else {
                    count = inventoryItem->quantity;
                }
                break;
            }
        }
    }

    programStackPushInteger(program, count);
}

// read_byte — reads a single byte from a specified address.
// In CE we cannot dereference arbitrary engine addresses, so we emulate
// known sfall memory locations with CE-native equivalents.
static void op_read_byte(Program* program)
{
    int addr = programStackPopInteger(program);

    int value = -1;
    switch (addr) {
    case 0x56D38C:
        value = combatGetTargetHighlight();
        break;
    // 0x410003 — Rotators fork detection signature.
    // ETu's gl_rotators.ssl checks this byte to detect the Rotators
    // sfall fork. Returning 0xF4 signals "Rotators engine present"
    // so scripts enable Rotators-specific features.
    case 0x410003:
        value = 0xF4;
        break;
    default:
        programPrintError("%s: attempt to 'read_byte' at 0x%x (not supported)", program->name, addr);
        break;
    }

    programStackPushInteger(program, value);
}

// read_short — reads a 16-bit value from a specified address.
// CE cannot dereference arbitrary engine addresses. Registered as a
// stub returning -1 so scripts that check for this opcode do not crash.
// RPU/ETu scripts do not currently call read_short directly.
static void op_read_short(Program* program)
{
    int addr = programStackPopInteger(program);
    programPrintError("%s: read_short at 0x%x — not supported in CE engine, returning -1", program->name, addr);
    programStackPushInteger(program, -1);
}

// read_int — reads a 32-bit value from a specified address.
// CE cannot dereference arbitrary engine addresses. Registered as a
// stub returning -1 so scripts that check for this opcode do not crash.
// RPU/ETu scripts do not currently call read_int directly.
static void op_read_int(Program* program)
{
    int addr = programStackPopInteger(program);
    programPrintError("%s: read_int at 0x%x — not supported in CE engine, returning -1", program->name, addr);
    programStackPushInteger(program, -1);
}

// set_pc_base_stat
static void op_set_pc_base_stat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBaseStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    critterSetBaseStat(gDude, stat, value);
}

static void op_set_critter_base_stat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBaseStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    if (obj == nullptr || FID_TYPE(obj->fid) != OBJ_TYPE_CRITTER) {
        return;
    }
    critterSetBaseStat(obj, stat, value);
}

// set_pc_extra_stat
static void op_set_pc_bonus_stat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBonusStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    critterSetBonusStat(gDude, stat, value);
}

static void op_set_critter_extra_stat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBonusStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    if (obj == nullptr || FID_TYPE(obj->fid) != OBJ_TYPE_CRITTER) {
        return;
    }
    critterSetBonusStat(obj, stat, value);
}

// get_pc_base_stat
static void op_get_pc_base_stat(Program* program)
{
    // CE: Implementation is different. Sfall obtains value directly from
    // dude's proto. This can have unforeseen consequences when dealing with
    // current stats.
    int stat = programStackPopInteger(program);
    programStackPushInteger(program, critterGetBaseStat(gDude, stat));
}

static void op_get_critter_base_stat(Program* program)
{
    // CE: Implementation is different. Sfall obtains value directly from
    // dude's proto. This can have unforeseen consequences when dealing with
    // current stats.
    int stat = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    if (obj == nullptr || FID_TYPE(obj->fid) != OBJ_TYPE_CRITTER) {
        programStackPushInteger(program, 0);
        return;
    }
    programStackPushInteger(program, critterGetBaseStat(obj, stat));
}

// get_pc_extra_stat
static void op_get_pc_bonus_stat(Program* program)
{
    int stat = programStackPopInteger(program);
    int value = critterGetBonusStat(gDude, stat);
    programStackPushInteger(program, value);
}

static void op_get_critter_extra_stat(Program* program)
{
    int stat = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    if (obj == nullptr || FID_TYPE(obj->fid) != OBJ_TYPE_CRITTER) {
        programStackPushInteger(program, 0);
        return;
    }
    int value = critterGetBonusStat(obj, stat);
    programStackPushInteger(program, value);
}

// tap_key
static void op_tap_key(Program* program)
{
    int key = programStackPopInteger(program);
    sfall_kb_press_key(key);
}

// get_year
static void op_get_year(Program* program)
{
    int year;
    gameTimeGetDate(nullptr, nullptr, &year);
    programStackPushInteger(program, year);
}

// game_loaded
static void op_game_loaded(Program* program)
{
    bool loaded = sfall_gl_scr_is_loaded(program);
    programStackPushInteger(program, loaded ? 1 : 0);
}

// set_global_script_repeat
static void op_set_global_script_repeat(Program* program)
{
    int frames = programStackPopInteger(program);
    sfall_gl_scr_set_repeat(program, frames);
}

// key_pressed
static void op_key_pressed(Program* program)
{
    int key = programStackPopInteger(program);
    bool pressed = sfall_kb_is_key_pressed(key);
    programStackPushInteger(program, pressed ? 1 : 0);
}

// in_world_map
static void op_in_world_map(Program* program)
{
    programStackPushInteger(program, GameMode::isInGameMode(GameMode::kWorldmap) ? 1 : 0);
}

// force_encounter
static void op_force_encounter(Program* program)
{
    int map = programStackPopInteger(program);
    wmForceEncounter(map, 0);
}

// set_world_map_pos
static void op_set_world_map_pos(Program* program)
{
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    wmSetPartyWorldPos(x, y);
}

// get_world_map_x_pos
static void op_get_world_map_x_pos(Program* program)
{
    int x;
    wmGetPartyWorldPos(&x, nullptr);
    programStackPushInteger(program, x);
}

// get_world_map_y_pos
static void op_get_world_map_y_pos(Program* program)
{
    int y;
    wmGetPartyWorldPos(nullptr, &y);
    programStackPushInteger(program, y);
}

// set_map_time_multi
void op_set_map_time_multi(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    wmSetScriptWorldMapMulti(value.asFloat());
}

// active_hand
static void op_active_hand(Program* program)
{
    programStackPushInteger(program, interfaceGetCurrentHand());
}

static void op_get_critter_current_ap(Program* program)
{
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    int actionPoints = 0;
    if (critter != nullptr && FID_TYPE(critter->fid) == OBJ_TYPE_CRITTER) {
        actionPoints = critter->data.critter.combat.ap;
    }

    programStackPushInteger(program, actionPoints);
}

static void op_set_critter_current_ap(Program* program)
{
    int actionPoints = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == nullptr || FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER) {
        programPrintError("set_critter_current_ap: expected critter object");
        return;
    }

    if (actionPoints < 0) {
        actionPoints = 0;
    }

    critter->data.critter.combat.ap = actionPoints;
    if (critter == gDude && isInCombat()) {
        interfaceRenderActionPoints(actionPoints, _combat_free_move);
    }
}

static void op_set_critter_burst_disable(Program* program)
{
    int disable = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == nullptr || FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER) {
        programPrintError("set_critter_burst_disable: expected critter object");
        return;
    }

    aiSetBurstDisabled(critter, disable != 0);
}

static void refreshUnspentApArmorClass()
{
    if (isInCombat() && _combat_whose_turn() != gDude) {
        interfaceRenderArmorClass(false);
    }
}

static void op_set_unspent_ap_bonus(Program* program)
{
    int multiplier = programStackPopInteger(program);
    statSetUnspentApBonus(multiplier);
    refreshUnspentApArmorClass();
}

static void op_get_unspent_ap_bonus(Program* program)
{
    programStackPushInteger(program, statGetUnspentApBonus());
}

static void op_set_unspent_ap_perk_bonus(Program* program)
{
    int multiplier = programStackPopInteger(program);
    statSetUnspentApPerkBonus(multiplier);
    refreshUnspentApArmorClass();
}

static void op_get_unspent_ap_perk_bonus(Program* program)
{
    programStackPushInteger(program, statGetUnspentApPerkBonus());
}

static void op_set_inven_ap_cost(Program* program)
{
    int cost = programStackPopInteger(program);
    cost = std::clamp(cost, 0, 100);
    inventorySetInvenApCost(cost);
}

// toggle_active_hand
static void op_toggle_active_hand(Program* program)
{
    interfaceBarSwapHands(true);
}

// set_global_script_type
static void op_set_global_script_type(Program* program)
{
    int type = programStackPopInteger(program);
    sfall_gl_scr_set_type(program, type);
}

// available_global_script_types
// Returns a bitmask of the global script types supported by the engine.
// Type 0 = timed, Type 1 = background/always, Type 2 = world map only, Type 3 = gameplay.
static void op_available_global_script_types(Program* program)
{
    // All 4 types are supported: bits 0-3 = 0xF = 15
    constexpr int kSupportedTypes = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
    programStackPushInteger(program, kSupportedTypes);
}

// set_sfall_global
static void op_set_sfall_global(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    ProgramValue variable = programStackPopValue(program);

    if (value.isFloat()) {
        // Float values use parallel float storage to preserve precision.
        if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            const char* key = programGetString(program, variable.opcode, variable.integerValue);
            sfall_gl_vars_store_float(key, value.asFloat());
        } else if (variable.opcode == VALUE_TYPE_INT) {
            sfall_gl_vars_store_float(variable.integerValue, value.asFloat());
        }
    } else {
        if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            const char* key = programGetString(program, variable.opcode, variable.integerValue);
            sfall_gl_vars_store(key, value.integerValue);
        } else if (variable.opcode == VALUE_TYPE_INT) {
            sfall_gl_vars_store(variable.integerValue, value.integerValue);
        }
    }
}

// get_sfall_global_int
static void op_get_sfall_global_int(Program* program)
{
    ProgramValue variable = programStackPopValue(program);

    int value = 0;
    if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        const char* key = programGetString(program, variable.opcode, variable.integerValue);
        sfall_gl_vars_fetch(key, value);
    } else if (variable.opcode == VALUE_TYPE_INT) {
        sfall_gl_vars_fetch(variable.integerValue, value);
    }

    programStackPushInteger(program, value);
}

// get_sfall_global_float
static void op_get_sfall_global_float(Program* program)
{
    ProgramValue variable = programStackPopValue(program);

    float value = 0.0f;
    bool found = false;
    if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        const char* key = programGetString(program, variable.opcode, variable.integerValue);
        found = sfall_gl_vars_fetch_float(key, value);
    } else if (variable.opcode == VALUE_TYPE_INT) {
        found = sfall_gl_vars_fetch_float(variable.integerValue, value);
    }

    // Fall back to int storage if no float entry exists (backward compat).
    if (!found) {
        int intValue = 0;
        if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            const char* key = programGetString(program, variable.opcode, variable.integerValue);
            if (sfall_gl_vars_fetch(key, intValue)) {
                value = static_cast<float>(intValue);
            }
        } else if (variable.opcode == VALUE_TYPE_INT) {
            if (sfall_gl_vars_fetch(variable.integerValue, intValue)) {
                value = static_cast<float>(intValue);
            }
        }
    }

    programStackPushFloat(program, value);
}

// get_game_mode
static void op_get_game_mode(Program* program)
{
    programStackPushInteger(program, GameMode::getCurrentGameMode());
}

// get_uptime
static void op_get_uptime(Program* program)
{
    programStackPushInteger(program, getTicks());
}

// set_car_current_town
static void op_set_car_current_town(Program* program)
{
    int area = programStackPopInteger(program);
    wmCarSetCurrentArea(area);
}

// get_bodypart_hit_modifier
static void op_get_bodypart_hit_modifier(Program* program)
{
    int hit_location = programStackPopInteger(program);
    programStackPushInteger(program, combat_get_hit_location_penalty(hit_location));
}

// set_bodypart_hit_modifier
static void op_set_bodypart_hit_modifier(Program* program)
{
    int penalty = programStackPopInteger(program);
    int hit_location = programStackPopInteger(program);
    combat_set_hit_location_penalty(hit_location, penalty);
}

static bool criticalTableArgsAreValid(Program* program, const char* opcodeName, int killType, int hitLocation, int effect, int dataMember)
{
    if (killType < 0 || killType > SFALL_KILL_TYPE_COUNT
        || hitLocation < 0 || hitLocation >= HIT_LOCATION_COUNT
        || effect < 0 || effect >= CRTICIAL_EFFECT_COUNT
        || dataMember < 0 || dataMember >= CRIT_DATA_MEMBER_COUNT) {
        programPrintError("%s: argument values out of range", opcodeName);
        return false;
    }

    return true;
}

static void op_set_critical_table(Program* program)
{
    int value = programStackPopInteger(program);
    int dataMember = programStackPopInteger(program);
    int effect = programStackPopInteger(program);
    int hitLocation = programStackPopInteger(program);
    int killType = programStackPopInteger(program);

    if (!criticalTableArgsAreValid(program, "set_critical_table", killType, hitLocation, effect, dataMember)) {
        return;
    }

    criticalsSetValue(killType, hitLocation, effect, dataMember, value);
}

static void op_get_critical_table(Program* program)
{
    int dataMember = programStackPopInteger(program);
    int effect = programStackPopInteger(program);
    int hitLocation = programStackPopInteger(program);
    int killType = programStackPopInteger(program);

    if (!criticalTableArgsAreValid(program, "get_critical_table", killType, hitLocation, effect, dataMember)) {
        programStackPushInteger(program, 0);
        return;
    }

    programStackPushInteger(program, criticalsGetValue(killType, hitLocation, effect, dataMember));
}

static void op_reset_critical_table(Program* program)
{
    int dataMember = programStackPopInteger(program);
    int effect = programStackPopInteger(program);
    int hitLocation = programStackPopInteger(program);
    int killType = programStackPopInteger(program);

    if (!criticalTableArgsAreValid(program, "reset_critical_table", killType, hitLocation, effect, dataMember)) {
        return;
    }

    criticalsResetValue(killType, hitLocation, effect, dataMember);
}

// sqrt
static void op_sqrt(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, sqrtf(programValue.asFloat()));
}

// abs
static void op_abs(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);

    if (programValue.isInt()) {
        programStackPushInteger(program, abs(programValue.integerValue));
    } else {
        programStackPushFloat(program, abs(programValue.asFloat()));
    }
}

// sin
static void op_sin(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, sinf(programValue.asFloat()));
}

// cos
static void op_cos(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, cosf(programValue.asFloat()));
}

// tan
static void op_tan(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, tanf(programValue.asFloat()));
}

// arctan
static void op_arctan(Program* program)
{
    ProgramValue xValue = programStackPopValue(program);
    ProgramValue yValue = programStackPopValue(program);
    programStackPushFloat(program, atan2f(yValue.asFloat(), xValue.asFloat()));
}

// pow (^)
static void op_power(Program* program)
{
    ProgramValue expValue = programStackPopValue(program);
    ProgramValue baseValue = programStackPopValue(program);

    // CE: Implementation is slightly different, check.
    float result = powf(baseValue.asFloat(), expValue.asFloat());

    if (baseValue.isInt() && expValue.isInt()) {
        // Guard against float-to-int overflow UB (I2-F-005).
        // powf(2.0f, 31.0f) = 2147483648.0f exceeds INT_MAX (2147483647).
        // Fall back to float path on overflow, matching the pattern used by
        // vanilla opAdd/opSubtract/opMultiply in interpreter.cc.
        if (result > INT_MAX || result < INT_MIN) {
            programStackPushFloat(program, result);
        } else {
            // Note: this will truncate the result if power is negative.
            // Keeping it to match sfall.
            programStackPushInteger(program, static_cast<int>(result));
        }
    } else {
        programStackPushFloat(program, result);
    }
}

// log
static void op_log(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, logf(programValue.asFloat()));
}

// ceil
static void op_ceil(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushInteger(program, static_cast<int>(ceilf(programValue.asFloat())));
}

// exp
static void op_exponent(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, expf(programValue.asFloat()));
}

// get_script
static void op_get_script(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    if (obj == nullptr) {
        programStackPushInteger(program, -1);
        return;
    }

    if (obj->sid == -1) {
        programStackPushInteger(program, 0);
        return;
    }

    Script* script;
    if (scriptGetScript(obj->sid, &script) == -1 || script->index < 0) {
        programStackPushInteger(program, 0);
        return;
    }

    programStackPushInteger(program, script->index + 1);
}

// remove_script
static void op_remove_script(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    if (obj == nullptr || obj->sid == -1) {
        return;
    }

    scriptRemove(obj->sid);
    obj->sid = -1;
    obj->scriptIndex = -1;
}

// set_script
static void op_set_script(Program* program)
{
    int scriptId = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (obj == nullptr) {
        return;
    }

    unsigned int rawScriptId = static_cast<unsigned int>(scriptId);
    // sfall encodes set_script() ids as a 1-based script index in the low
    // 28 bits, with the upper bits reserved for flags. The top bit
    // (0x80000000) suppresses map_enter_p_proc after start().
    int scriptIndex = static_cast<int>(rawScriptId & ~0xF0000000u);
    if (scriptIndex == 0) {
        programPrintError("set_script: invalid script index number %d.", scriptIndex);
        return;
    }

    scriptIndex--;
    if (!scriptsIsValidScriptIndex(scriptIndex)) {
        programPrintError("set_script: invalid script index (engine) number %d.", scriptIndex);
        return;
    }

    if (obj->sid != -1) {
        scriptRemove(obj->sid);
        obj->sid = -1;
        obj->scriptIndex = -1;
    }

    int scriptType = (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) ? SCRIPT_TYPE_CRITTER : SCRIPT_TYPE_ITEM;
    if (objectSetScript(obj, scriptType, scriptIndex) == -1) {
        obj->sid = -1;
        obj->scriptIndex = -1;
        return;
    }

    Script* script;
    if (scriptGetScript(obj->sid, &script) == -1) {
        scriptRemove(obj->sid);
        obj->sid = -1;
        obj->scriptIndex = -1;
        return;
    }

    int sid = obj->sid;
    script->owner = obj;
    obj->scriptIndex = scriptIndex;

    scriptExecProc(sid, SCRIPT_PROC_START);
    if ((rawScriptId & 0x80000000u) == 0) {
        // note: if map_enter_p_proc is missing, START gets executed again
        scriptExecProc(sid, SCRIPT_PROC_MAP_ENTER);
    }
}

// get_proto_data
static void op_get_proto_data(Program* program)
{
    int rawOffset = programStackPopInteger(program);
    int pid = programStackPopInteger(program);

    if (rawOffset < 0) {
        programPrintError("get_proto_data: negative offset %d not allowed", rawOffset);
        programStackPushInteger(program, -1);
        return;
    }
    size_t offset = static_cast<size_t>(rawOffset);

    Proto* proto;
    if (protoGetProto(pid, &proto) != 0) {
        programPrintError("get_proto_data: bad proto %d", pid);
        programStackPushInteger(program, -1);
        return;
    }

    // CE: Make sure the requested offset is within memory bounds and is
    // properly aligned.
    if (offset + sizeof(int) > proto_size(PID_TYPE(pid)) || offset % sizeof(int) != 0) {
        programPrintError("get_proto_data: bad offset %d", offset);
        programStackPushInteger(program, -1);
        return;
    }

    int value = *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(proto) + offset);
    programStackPushInteger(program, value);
}

// set_proto_data
static void op_set_proto_data(Program* program)
{
    int value = programStackPopInteger(program);
    int rawOffset = programStackPopInteger(program);
    int pid = programStackPopInteger(program);

    if (rawOffset < 0) {
        programPrintError("set_proto_data: negative offset %d not allowed", rawOffset);
        return;
    }
    size_t offset = static_cast<size_t>(rawOffset);

    Proto* proto;
    if (protoGetProto(pid, &proto) != 0) {
        programPrintError("set_proto_data: bad proto %d", pid);
        return;
    }

    // CE: Make sure the requested offset is within memory bounds and is
    // properly aligned.
    if (offset + sizeof(int) > proto_size(PID_TYPE(pid)) || offset % sizeof(int) != 0) {
        programPrintError("set_proto_data: bad offset %d", offset);
        return;
    }

    *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(proto) + offset) = value;
}

// set_self
static void op_set_self(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int sid = scriptGetSid(program);

    Script* scr;
    if (scriptGetScript(sid, &scr) == 0) {
        scr->overriddenSelf = obj;
    }
}

// list_begin
static void op_list_begin(Program* program)
{
    int listType = programStackPopInteger(program);
    int listId = sfallListsCreate(listType);
    programStackPushInteger(program, listId);
}

// list_next
static void op_list_next(Program* program)
{
    int listId = programStackPopInteger(program);
    Object* obj = sfallListsGetNext(listId);
    programStackPushPointer(program, obj);
}

// list_end
static void op_list_end(Program* program)
{
    int listId = programStackPopInteger(program);
    sfallListsDestroy(listId);
}

// sfall_ver_major
static void op_get_version_major(Program* program)
{
    programStackPushInteger(program, kVersionMajor);
}

// sfall_ver_minor
static void op_get_version_minor(Program* program)
{
    programStackPushInteger(program, kVersionMinor);
}

// sfall_ver_build
static void op_get_version_patch(Program* program)
{
    programStackPushInteger(program, kVersionPatch);
}

// get_weapon_ammo_pid
static void op_get_weapon_ammo_pid(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int pid = -1;
    if (obj != nullptr) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_ITEM) {
            switch (itemGetType(obj)) {
            case ITEM_TYPE_WEAPON:
                pid = weaponGetAmmoTypePid(obj);
                break;
            case ITEM_TYPE_MISC:
                pid = miscItemGetPowerTypePid(obj);
                break;
            }
        }
    }

    programStackPushInteger(program, pid);
}

// There are two problems with this function.
//
// 1. Sfall's implementation changes ammo PID of misc items, which is impossible
// since it's stored in proto, not in the object.
// 2. Changing weapon's ammo PID is done without checking for ammo
// quantity/capacity which can probably lead to bad things.
//
// set_weapon_ammo_pid
static void op_set_weapon_ammo_pid(Program* program)
{
    int ammoTypePid = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (obj != nullptr) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_ITEM) {
            switch (itemGetType(obj)) {
            case ITEM_TYPE_WEAPON: {
                // Validate that the new ammo PID references a valid ammo proto.
                Proto* ammoProto;
                if (protoGetProto(ammoTypePid, &ammoProto) != 0) {
                    programPrintError("set_weapon_ammo_pid: invalid ammo PID %d (proto not found)", ammoTypePid);
                    return;
                }
                if (PID_TYPE(ammoTypePid) != OBJ_TYPE_ITEM || ammoProto->item.type != ITEM_TYPE_AMMO) {
                    programPrintError("set_weapon_ammo_pid: PID %d is not ITEM_TYPE_AMMO", ammoTypePid);
                    return;
                }
                // Clamp current ammo quantity to the new ammo type's max capacity.
                int maxCapacity = ammoProto->item.data.ammo.quantity;
                if (obj->data.item.weapon.ammoQuantity > maxCapacity) {
                    obj->data.item.weapon.ammoQuantity = maxCapacity;
                }
                obj->data.item.weapon.ammoTypePid = ammoTypePid;
                break;
            }
            }
        }
    }
}

// get_weapon_ammo_count
static void op_get_weapon_ammo_count(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    // CE: Implementation is different.
    int ammoQuantityOrCharges = 0;
    if (obj != nullptr) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_ITEM) {
            switch (itemGetType(obj)) {
            case ITEM_TYPE_AMMO:
            case ITEM_TYPE_WEAPON:
                ammoQuantityOrCharges = ammoGetQuantity(obj);
                break;
            case ITEM_TYPE_MISC:
                ammoQuantityOrCharges = miscItemGetCharges(obj);
                break;
            }
        }
    }

    programStackPushInteger(program, ammoQuantityOrCharges);
}

// set_weapon_ammo_count
static void op_set_weapon_ammo_count(Program* program)
{
    int ammoQuantityOrCharges = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    // CE: Implementation is different.
    if (obj != nullptr) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_ITEM) {
            switch (itemGetType(obj)) {
            case ITEM_TYPE_AMMO:
            case ITEM_TYPE_WEAPON:
                ammoSetQuantity(obj, ammoQuantityOrCharges);
                break;
            case ITEM_TYPE_MISC:
                miscItemSetCharges(obj, ammoQuantityOrCharges);
                break;
            }
        }
    }
}

// get_mouse_x
static void op_get_mouse_x(Program* program)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);
    programStackPushInteger(program, x);
}

// get_mouse_y
static void op_get_mouse_y(Program* program)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);
    programStackPushInteger(program, y);
}

// get_mouse_buttons
static void op_get_mouse_buttons(Program* program)
{
    // CE: Implementation is slightly different - it does not handle middle
    // mouse button.
    programStackPushInteger(program, mouse_get_last_buttons());
}

static void op_get_window_under_mouse(Program* program)
{
    programStackPushInteger(program, _win_last_button_winID());
}

// get_screen_width
static void op_get_screen_width(Program* program)
{
    programStackPushInteger(program, screenGetWidth());
}

// get_screen_height
static void op_get_screen_height(Program* program)
{
    programStackPushInteger(program, screenGetHeight());
}

// get_light_level
static void op_get_light_level(Program* program)
{
    programStackPushInteger(program, lightGetAmbientIntensity());
}

// Custom death model FID numbers set via set_dm_model / set_df_model.
// Zero means "use engine default". These are applied in op_refresh_pc_art
// by overriding the proto FID before armor/weapon layering.
static int gCustomMaleHeroModelNum = 0;
static int gCustomFemaleHeroModelNum = 0;

// note: Might need to be updated when Hero Appearance is implemented.
// F-003: Now reads HAp_Race / HApStyle from sfall globals and applies
// them as base FID offsets during model refresh. Race offsets the base
// model number (tens digit) and style is forwarded to the FID pipeline.
// The exact FID mapping table is mod-specific; this provides the engine-level
// infrastructure to consume the stored values.
static void op_refresh_pc_art(Program* program)
{
    if (gDude == nullptr) {
        return;
    }

    Rect rect;
    objectGetRect(gDude, &rect);

    int anim = FID_ANIM_TYPE(gDude->fid);
    int rotation = FID_ROTATION(gDude->fid);

    _proto_dude_update_gender();

    // Apply custom hero model if one was set via set_dm_model / set_df_model.
    int customModelNum = 0;
    if (critterGetStat(gDude, STAT_GENDER) == GENDER_MALE && gCustomMaleHeroModelNum > 0) {
        customModelNum = gCustomMaleHeroModelNum;
    } else if (critterGetStat(gDude, STAT_GENDER) == GENDER_FEMALE && gCustomFemaleHeroModelNum > 0) {
        customModelNum = gCustomFemaleHeroModelNum;
    }

    // Apply Hero Appearance race/style overrides (F-003).
    // HAp_Race and HApStyle are stored by set_hero_race (0x8214) /
    // set_hero_style (0x8215) as sfall global vars.
    int heroRace = 0;
    int heroStyle = 0;
    if (customModelNum <= 0) {
        sfall_gl_vars_fetch("HAp_Race", heroRace);
        sfall_gl_vars_fetch("HApStyle", heroStyle);
    }

    if (customModelNum > 0) {
        Proto* proto;
        if (protoGetProto(gDude->pid, &proto) != -1) {
            proto->fid = buildFid(OBJ_TYPE_CRITTER, customModelNum, 0, 0, 0);
        }
    } else if (heroRace > 0 || heroStyle > 0) {
        // Hero Appearance race/style overrides: read the current base model
        // number from the proto FID and apply race as a tens-digit offset,
        // style as an animation variant. The exact mapping is mod-specific;
        // a HOOK_ADJUSTFID handler can further customize the output.
        Proto* proto;
        if (protoGetProto(gDude->pid, &proto) != -1) {
            int baseFid = proto->fid & 0xFFF; // Extract base model number.
            // Race offsets the tens digit (each race = model variant group).
            // Style shifts the units digit (each style = sub-variant).
            if (heroRace > 0) {
                int raceOffset = (heroRace - 1) * 10;
                baseFid = (baseFid / 10) * 10 + raceOffset + (baseFid % 10);
            }
            if (heroStyle > 0) {
                int styleOffset = heroStyle - 1;
                int baseUnits = baseFid % 10;
                baseFid = baseFid + styleOffset;
                // If style would overflow the units digit, carry over
                // to the tens digit (F-006: fix dead check).
                if (baseUnits + styleOffset >= 10) {
                    baseFid = ((baseFid / 10) + 1) * 10;
                }
            }
            proto->fid = buildFid(OBJ_TYPE_CRITTER, baseFid, 0, 0, 0);
        }
    }

    int fid = inventoryComputeCritterFid(gDude,
        gDude->pid,
        critterGetItem2(gDude),
        critterGetItem1(gDude),
        critterGetArmor(gDude),
        interfaceGetCurrentHand(),
        anim,
        rotation);

    // CE: When changing gender, the refreshed rect can be smaller than the original one,
    // which can leave a momentary ghost.  We union with old rect to avoid that.
    Rect newRect;
    objectSetFid(gDude, fid, nullptr);
    objectGetRect(gDude, &newRect);
    rectUnion(&rect, &newRect, &rect);
    tileWindowRefreshRect(&rect, gDude->elevation);
}

// create_message_window
static void op_create_message_window(Program* program)
{
    static bool showing = false;

    if (showing) {
        return;
    }

    const char* string = programStackPopString(program);
    if (string == nullptr || string[0] == '\0') {
        return;
    }

    char* copy = internal_strdup(string);

    const char* body[4];
    int count = 0;

    char* pch = strchr(copy, '\n');
    while (pch != nullptr && count < 4) {
        *pch = '\0';
        body[count++] = pch + 1;
        pch = strchr(pch + 1, '\n');
    }

    showing = true;
    showDialogBox(copy,
        body,
        count,
        192,
        116,
        _colorTable[32328],
        nullptr,
        _colorTable[32328],
        DIALOG_BOX_LARGE);
    showing = false;

    internal_free(copy);
}

// get_attack_type
static void op_get_attack_type(Program* program)
{
    int hit_mode;
    if (interface_get_current_attack_mode(&hit_mode)) {
        programStackPushInteger(program, hit_mode);
    } else {
        programStackPushInteger(program, -1);
    }
}

static void op_play_sfall_sound(Program* program)
{
    int mode = programStackPopInteger(program);
    const char* path = programStackPopString(program);
    programStackPushInteger(program, scriptSoundPlay(path, mode));
}

static void op_stop_sfall_sound(Program* program)
{
    int soundId = programStackPopInteger(program);
    scriptSoundStop(soundId);
}

// force_encounter_with_flags
static void op_force_encounter_with_flags(Program* program)
{
    unsigned int flags = programStackPopInteger(program);
    int map = programStackPopInteger(program);
    wmForceEncounter(map, flags);
}

// list_as_array
static void op_list_as_array(Program* program)
{
    int type = programStackPopInteger(program);
    int arrayId = ListAsArray(type);
    programStackPushInteger(program, arrayId);
}

// atoi
static void op_parse_int(Program* program)
{
    const char* string = programStackPopString(program);
    programStackPushInteger(program, static_cast<int>(strtol(string, nullptr, 0)));
}

// atof
static void op_atof(Program* program)
{
    const char* string = programStackPopString(program);
    programStackPushFloat(program, static_cast<float>(atof(string)));
}

// tile_under_cursor
static void op_tile_under_cursor(Program* program)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);

    int tile = tileFromScreenXY(x, y);
    programStackPushInteger(program, tile);
}

// gdialog_get_barter_mod
static void op_gdialog_get_barter_mod(Program* program)
{
    programStackPushInteger(program, gameDialogGetBarterModifier());
}

// get_tile_fid
static void op_get_tile_fid(Program* program)
{
    int tileData = programStackPopInteger(program);
    int tile = tileData & 0xFFFFFF;
    int elevation = (tileData >> 24) & 0x0F;
    int mode = tileData >> 28;

    if (!hexGridTileIsValid(tile) || elevation < 0 || elevation >= ELEVATION_COUNT) {
        debugPrint("%s: op_get_tile_fid invalid tile data: tile=%d elevation=%d", program->name, tile, elevation);
        programStackPushInteger(program, 0);
        return;
    }

    int squareTile = squareTileFromTile(tile);
    if (!squareGridTileIsValid(squareTile)) {
        debugPrint("%s: op_get_tile_fid failed to map tile=%d to square index", program->name, tile);
        programStackPushInteger(program, 0);
        return;
    }

    int squareData = _square[elevation]->field_0[squareTile];

    switch (mode) {
    case 1:
        programStackPushInteger(program, (squareData >> 16) & 0x3FFF);
        break;
    case 2:
        programStackPushInteger(program, squareData);
        break;
    default:
        programStackPushInteger(program, squareData & 0x3FFF);
        break;
    }
}

// substr
static void op_substr(Program* program)
{
    auto length = programStackPopInteger(program);
    auto startPos = programStackPopInteger(program);
    const char* str = programStackPopString(program);

    char buf[5120] = { 0 };

    int len = static_cast<int>(strlen(str));

    if (startPos < 0) {
        startPos += len; // start from end
        if (startPos < 0) {
            startPos = 0;
        }
    }

    if (length < 0) {
        length += len - startPos; // cutoff at end
        if (length == 0) {
            programStackPushString(program, buf);
            return;
        }
        length = abs(length); // length can't be negative
    }

    // check position
    if (startPos >= len) {
        // start position is out of string length, return empty string
        programStackPushString(program, buf);
        return;
    }

    if (length == 0 || length + startPos > len) {
        length = len - startPos; // set the correct length, the length of characters goes beyond the end of the string
    }

    if (length > sizeof(buf) - 1) {
        length = sizeof(buf) - 1;
    }

    memcpy(buf, &str[startPos], length);
    buf[length] = '\0';
    programStackPushString(program, buf);
}

// strlen
static void op_get_string_length(Program* program)
{
    const char* string = programStackPopString(program);
    programStackPushInteger(program, static_cast<int>(strlen(string)));
}

// metarule2_explosions
static void op_explosions_metarule(Program* program)
{
    int param2 = programStackPopInteger(program);
    int param1 = programStackPopInteger(program);
    int metarule = programStackPopInteger(program);

    switch (metarule) {
    case EXPL_FORCE_EXPLOSION_PATTERN:
        if (param1 != 0) {
            explosionSetPattern(2, 4);
        } else {
            explosionSetPattern(0, 6);
        }
        programStackPushInteger(program, 0);
        break;
    case EXPL_FORCE_EXPLOSION_ART:
        explosionSetFrm(param1);
        programStackPushInteger(program, 0);
        break;
    case EXPL_FORCE_EXPLOSION_RADIUS:
        explosionSetRadius(param1);
        programStackPushInteger(program, 0);
        break;
    case EXPL_FORCE_EXPLOSION_DMGTYPE:
        explosionSetDamageType(param1);
        programStackPushInteger(program, 0);
        break;
    case EXPL_STATIC_EXPLOSION_RADIUS:
        weaponSetGrenadeExplosionRadius(param1);
        weaponSetRocketExplosionRadius(param2);
        programStackPushInteger(program, 0);
        break;
    case EXPL_GET_EXPLOSION_DAMAGE:
        if (1) {
            int minDamage;
            int maxDamage;
            explosiveGetDamage(param1, &minDamage, &maxDamage);

            ArrayId arrayId = CreateTempArray(2, 0);
            SetArray(arrayId, ProgramValue { 0 }, ProgramValue { minDamage }, false, program);
            SetArray(arrayId, ProgramValue { 1 }, ProgramValue { maxDamage }, false, program);

            programStackPushInteger(program, arrayId);
        }
        break;
    case EXPL_SET_DYNAMITE_EXPLOSION_DAMAGE:
        explosiveSetDamage(PROTO_ID_DYNAMITE_I, param1, param2);
        programStackPushInteger(program, 0);
        break;
    case EXPL_SET_PLASTIC_EXPLOSION_DAMAGE:
        explosiveSetDamage(PROTO_ID_PLASTIC_EXPLOSIVES_I, param1, param2);
        programStackPushInteger(program, 0);
        break;
    case EXPL_SET_EXPLOSION_MAX_TARGET:
        explosionSetMaxTargets(param1);
        programStackPushInteger(program, 0);
        break;
    default:
        programStackPushInteger(program, 0);
        break;
    }
}

// message_str_game
static void op_get_message(Program* program)
{
    int messageId = programStackPopInteger(program);
    int messageListId = programStackPopInteger(program);
    char* text = messageListRepositoryGetMsg(messageListId, messageId);
    programStackPushString(program, text);
}

// save_array
static void op_save_array(Program* program)
{
    auto arrayId = static_cast<ArrayId>(programStackPopInteger(program));
    auto key = programStackPopValue(program);
    auto result = SaveArray(key, arrayId, program);
    switch (result) {
    case SaveArrayResult::InvalidId:
        programPrintError("save_array: array with id %d doesn't exist.", arrayId);
        break;
    case SaveArrayResult::ReservedKey:
        programPrintError("save_array: trying to save array under reserved key.");
        break;
    case SaveArrayResult::InvalidKeyType:
        programPrintError("save_array: invalid key type: %s.", key.typeDebugString());
        break;
    default:;
        // OK
    }
}

// load_array
static void op_load_array(Program* program)
{
    auto key = programStackPopValue(program);
    programStackPushInteger(program, static_cast<int>(LoadArray(key, program)));
}

// array_key
static void op_get_array_key(Program* program)
{
    auto index = programStackPopInteger(program);
    auto arrayId = programStackPopInteger(program);
    auto value = GetArrayKey(arrayId, index, program);
    programStackPushValue(program, value);
}

// create_array
static void op_create_array(Program* program)
{
    auto flags = programStackPopInteger(program);
    auto len = programStackPopInteger(program);
    auto arrayId = CreateArray(len, flags);
    programStackPushInteger(program, arrayId);
}

// temp_array
static void op_temp_array(Program* program)
{
    auto flags = programStackPopInteger(program);
    auto len = programStackPopInteger(program);

    // Special case for array sub-expressions.
    if ((flags & SFALL_ARRAYFLAG_EXPR_POP) != 0) {
        PopExpressionArray();
        programStackPushInteger(program, 0);
        return;
    }

    auto arrayId = CreateTempArray(len, flags);
    programStackPushInteger(program, arrayId);
}

// fix_array
static void op_fix_array(Program* program)
{
    auto arrayId = programStackPopInteger(program);
    FixArray(arrayId);
}

// string_split
static void op_string_split(Program* program)
{
    auto split = programStackPopString(program);
    auto str = programStackPopString(program);
    auto arrayId = StringSplit(str, split);
    programStackPushInteger(program, arrayId);
}

// set_array
static void op_set_array(Program* program)
{
    auto value = programStackPopValue(program);
    auto key = programStackPopValue(program);
    auto arrayId = programStackPopInteger(program);
    SetArray(arrayId, key, value, true, program);
}

// This special opcode is used to implement array expressions.
// It should always push 0 on the stack.
// arrayexpr
static void op_arrayexpr(Program* program)
{
    auto value = programStackPopValue(program);
    auto key = programStackPopValue(program);
    SetArrayFromExpression(key, value, program);
    programStackPushInteger(program, 0);
}

// scan_array
static void op_scan_array(Program* program)
{
    auto value = programStackPopValue(program);
    auto arrayId = programStackPopInteger(program);
    auto returnValue = ScanArray(arrayId, value, program);
    programStackPushValue(program, returnValue);
}

// get_array
static void op_get_array(Program* program)
{
    auto key = programStackPopValue(program);
    auto arrayId = programStackPopValue(program);

    if (arrayId.isInt()) {
        auto value = GetArray(arrayId.integerValue, key, program);
        programStackPushValue(program, value);
    } else if (arrayId.isString() && key.isInt()) {
        auto pos = key.asInt();
        auto str = programGetString(program, arrayId.opcode, arrayId.integerValue);

        char buf[2] = { 0 };
        if (pos < strlen(str)) {
            buf[0] = str[pos];
            programStackPushString(program, buf);
        } else {
            programStackPushString(program, buf);
        }
    } else {
        // Unsupported type combination (e.g. float arrayId or non-int key on
        // string arrayId). Push 0 to maintain stack balance.
        programStackPushInteger(program, 0);
    }
}

// free_array
static void op_free_array(Program* program)
{
    auto arrayId = programStackPopInteger(program);
    FreeArray(arrayId);
}

// len_array
static void op_len_array(Program* program)
{
    auto arrayId = programStackPopInteger(program);
    programStackPushInteger(program, LenArray(arrayId));
}

// resize_array
static void op_resize_array(Program* program)
{
    auto newLen = programStackPopInteger(program);
    auto arrayId = programStackPopInteger(program);
    ResizeArray(arrayId, newLen);
}

// party_member_list
static void op_party_member_list(Program* program)
{
    auto includeHidden = programStackPopInteger(program);
    auto objects = get_all_party_members_objects(includeHidden);
    auto arrayId = CreateTempArray(static_cast<int>(objects.size()), SFALL_ARRAYFLAG_RESERVED);
    for (int i = 0; i < LenArray(arrayId); i++) {
        SetArray(arrayId, ProgramValue { i }, ProgramValue { objects[i] }, false, program);
    }
    programStackPushInteger(program, arrayId);
}

// typeof
static void op_type_of(Program* program)
{
    auto value = programStackPopValue(program);
    if (value.isInt()) {
        programStackPushInteger(program, 1);
    } else if (value.isFloat()) {
        programStackPushInteger(program, 2);
    } else {
        programStackPushInteger(program, 3);
    };
}

// round
static void op_round(Program* program)
{
    float floatValue = programStackPopValue(program).asFloat();
    programStackPushInteger(program, static_cast<int>(lroundf(floatValue)));
}

enum BlockType {
    BLOCKING_TYPE_BLOCK,
    BLOCKING_TYPE_SHOOT,
    BLOCKING_TYPE_AI,
    BLOCKING_TYPE_SIGHT,
    BLOCKING_TYPE_SCROLL,
};

PathBuilderCallback* get_blocking_func(int type)
{
    switch (type) {
    case BLOCKING_TYPE_SHOOT:
        return _obj_shoot_blocking_at;
    case BLOCKING_TYPE_AI:
        return _obj_ai_blocking_at;
    case BLOCKING_TYPE_SIGHT:
        return _obj_sight_blocking_at;
    default:
        return _obj_blocking_at;
    }
}

// obj_blocking_line
static void op_make_straight_path(Program* program)
{
    int type = programStackPopInteger(program);
    int dest = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == nullptr) {
        programStackPushPointer(program, nullptr);
        return;
    }

    int flags = type == BLOCKING_TYPE_SHOOT ? 32 : 0;

    Object* obstacle = nullptr;
    _make_straight_path_func(object, object->tile, dest, nullptr, &obstacle, flags, get_blocking_func(type));
    programStackPushPointer(program, obstacle);
}

// obj_blocking_tile
static void op_obj_blocking_at(Program* program)
{
    int type = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);

    PathBuilderCallback* func = get_blocking_func(type);
    Object* obstacle = func(nullptr, tile, elevation);
    if (obstacle != nullptr) {
        if (type == BLOCKING_TYPE_SHOOT) {
            if ((obstacle->flags & OBJECT_SHOOT_THRU) != 0) {
                obstacle = nullptr;
            }
        }
    }
    programStackPushPointer(program, obstacle);
}

// tile_light
static void op_tile_light(Program* program)
{
    int tile = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    programStackPushInteger(program, lightGetTileIntensity(elevation, tile));
}

// tile_get_objs
static void op_tile_get_objects(Program* program)
{
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);
    ArrayId arrayId = CreateTempArray(0, SFALL_ARRAYFLAG_RESERVED);

    if (!hexGridTileIsValid(tile) || elevation < 0 || elevation >= ELEVATION_COUNT) {
        debugPrint("%s: op_tile_get_objects invalid tile data: tile=%d elevation=%d", program->name, tile, elevation);
        programStackPushInteger(program, arrayId);
        return;
    }

    int index = 0;
    for (Object* object = objectFindFirstAtLocation(elevation, tile); object != nullptr; object = objectFindNextAtLocation()) {
        ResizeArray(arrayId, index + 1);
        SetArray(arrayId, ProgramValue(index++), ProgramValue(object), false, program);
    }

    programStackPushInteger(program, arrayId);
}

// path_find_to
static void op_make_path(Program* program)
{
    int type = programStackPopInteger(program);
    int dest = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    ArrayId arrayId = CreateTempArray(0, 0);

    if (object == nullptr
        || !hexGridTileIsValid(dest)
        || object->elevation < 0
        || object->elevation >= ELEVATION_COUNT
        || !hexGridTileIsValid(object->tile)) {
        debugPrint("%s: op_make_path invalid input: object=%p dest=%d elevation=%d", program->name, object, dest, object != nullptr ? object->elevation : -1);
        programStackPushInteger(program, arrayId);
        return;
    }

    // sfall only requires an empty destination tile when the source object is a critter.
    int requireEmptyDest = PID_TYPE(object->pid) == OBJ_TYPE_CRITTER;

    // XXX: pathfinderFindPath does not accept a destination buffer length. Use the
    // same capacity as the engine's AnimationSad::rotations storage so this
    // wrapper is not the limiting factor.  Sfall uses 800 here
    unsigned char rotations[kSfallPathBufferSize];
    int pathLength = pathfinderFindPath(object, object->tile, dest, rotations, requireEmptyDest, get_blocking_func(type));
    ResizeArray(arrayId, pathLength);
    for (int index = 0; index < pathLength; index++) {
        SetArray(arrayId, ProgramValue(index), ProgramValue(static_cast<int>(rotations[index])), false, program);
    }

    programStackPushInteger(program, arrayId);
}
// sfall_func0
static void op_sfall_func0(Program* program)
{
    sfall_metarule(program, 0);
}

// sfall_func1
static void op_sfall_func1(Program* program)
{
    sfall_metarule(program, 1);
}

// sfall_func2
static void op_sfall_func2(Program* program)
{
    sfall_metarule(program, 2);
}

// sfall_func3
static void op_sfall_func3(Program* program)
{
    sfall_metarule(program, 3);
}

// sfall_func4
static void op_sfall_func4(Program* program)
{
    sfall_metarule(program, 4);
}

// sfall_func5
static void op_sfall_func5(Program* program)
{
    sfall_metarule(program, 5);
}

// sfall_func6
static void op_sfall_func6(Program* program)
{
    sfall_metarule(program, 6);
}

// sfall_func7
static void op_sfall_func7(Program* program)
{
    sfall_metarule(program, 7);
}

// sfall_func8
static void op_sfall_func8(Program* program)
{
    sfall_metarule(program, 8);
}

// ============================================================
// Virtual File System (VFS) opcodes
// sfall VFS allows scripts to read/write files via integer handles.
// Used by RPU (gl_k_goris_derobing, gl_k_walking_speed) for modifying
// FRM animation files at runtime.
// ============================================================

static constexpr int kVfsMaxFiles = 100;
static FILE* sfallVfsFiles[kVfsMaxFiles] = {};
static bool sfallVfsFileOpen[kVfsMaxFiles] = {};
// Track open mode: 0 = unopened, 1 = read-only ("rb"), 2 = read-write ("w+b").
static int sfallVfsFileMode[kVfsMaxFiles] = {};
// Store the original filename for handles so fs_resize can reopen read-only
// handles in read-write mode when needed (F-028).
static const char* sfallVfsFilePath[kVfsMaxFiles] = {};

static int sfallVfsAllocHandle()
{
    for (int i = 0; i < kVfsMaxFiles; i++) {
        if (!sfallVfsFileOpen[i]) {
            sfallVfsFileOpen[i] = true;
            sfallVfsFileMode[i] = 0;
            sfallVfsFilePath[i] = nullptr;
            return i;
        }
    }
    return -1;
}

static void sfallVfsFreeHandle(int id)
{
    if (id >= 0 && id < kVfsMaxFiles) {
        if (sfallVfsFiles[id] != nullptr) {
            fclose(sfallVfsFiles[id]);
            sfallVfsFiles[id] = nullptr;
        }
        sfallVfsFileOpen[id] = false;
        sfallVfsFileMode[id] = 0;
        sfallVfsFilePath[id] = nullptr;
    }
}

void sfallVfsCloseAll()
{
    for (int i = 0; i < kVfsMaxFiles; i++) {
        sfallVfsFreeHandle(i);
    }
}

// Reject paths containing ".." components — prevents path traversal attacks
// (I2-F-014). compat_resolve_path() only case-normalizes; it does not strip or
// reject path separators or directory traversal sequences.
static bool sfallVfsPathContainsTraversal(const char* path)
{
    if (path == nullptr || path[0] == '\0') {
        return true; // empty path is invalid
    }
    // Check for literal ".." as a path component (preceded by start-of-string,
    // "/", or "\").
    if (strncmp(path, "..", 2) == 0
        && (path[2] == '\0' || path[2] == '/' || path[2] == '\\')) {
        return true;
    }
    const char* p = path;
    while (*p != '\0') {
        p = strpbrk(p, "/\\");
        if (p == nullptr) {
            break;
        }
        p++; // skip the separator
        if (strncmp(p, "..", 2) == 0
            && (p[2] == '\0' || p[2] == '/' || p[2] == '\\')) {
            return true;
        }
    }
    return false;
}

// fs_create(path, size) -> fileId (or -1 on error)
static void op_fs_create(Program* program)
{
    int size = programStackPopInteger(program);
    const char* path = programStackPopString(program);

    if (sfallVfsPathContainsTraversal(path)) {
        programPrintError("fs_create: path traversal rejected '%s'", path);
        programStackPushInteger(program, -1);
        return;
    }

    int handle = sfallVfsAllocHandle();
    if (handle < 0) {
        programPrintError("fs_create: no free VFS handles");
        programStackPushInteger(program, -1);
        return;
    }

    FILE* file = compat_fopen(path, "w+b");
    if (file == nullptr) {
        programPrintError("fs_create: cannot create file '%s'", path);
        sfallVfsFileOpen[handle] = false;
        programStackPushInteger(program, -1);
        return;
    }

    // Validate size before fseek to prevent UB with size <= 0
    if (size <= 0) {
        programPrintError("fs_create: invalid size %d", size);
        fclose(file);
        sfallVfsFileOpen[handle] = false;
        programStackPushInteger(program, -1);
        return;
    }

    // Allocate the requested size (simple approach: seek + write)
    fseek(file, static_cast<long>(size) - 1, SEEK_SET);
    fputc(0, file);
    rewind(file);

    sfallVfsFiles[handle] = file;
    sfallVfsFileMode[handle] = 2; // read-write ("w+b")
    sfallVfsFilePath[handle] = path; // stored for fs_resize reopen (F-028)
    programStackPushInteger(program, handle);
}

// fs_copy(path, source_path) -> fileId (or -1 on error)
static void op_fs_copy(Program* program)
{
    const char* sourcePath = programStackPopString(program);
    const char* destPath = programStackPopString(program);

    if (sfallVfsPathContainsTraversal(sourcePath)) {
        programPrintError("fs_copy: path traversal rejected in source '%s'", sourcePath);
        programStackPushInteger(program, -1);
        return;
    }
    if (sfallVfsPathContainsTraversal(destPath)) {
        programPrintError("fs_copy: path traversal rejected in dest '%s'", destPath);
        programStackPushInteger(program, -1);
        return;
    }

    // Open source file for reading
    FILE* srcFile = compat_fopen(sourcePath, "rb");
    if (srcFile == nullptr) {
        programPrintError("fs_copy: cannot open source '%s'", sourcePath);
        programStackPushInteger(program, -1);
        return;
    }

    int handle = sfallVfsAllocHandle();
    if (handle < 0) {
        programPrintError("fs_copy: no free VFS handles");
        fclose(srcFile);
        programStackPushInteger(program, -1);
        return;
    }

    FILE* destFile = compat_fopen(destPath, "w+b");
    if (destFile == nullptr) {
        programPrintError("fs_copy: cannot create dest '%s'", destPath);
        fclose(srcFile);
        sfallVfsFileOpen[handle] = false;
        programStackPushInteger(program, -1);
        return;
    }

    // Copy file contents
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), srcFile)) > 0) {
        fwrite(buf, 1, n, destFile);
    }

    fclose(srcFile);
    rewind(destFile); // position at start for reading
    sfallVfsFiles[handle] = destFile;
    sfallVfsFileMode[handle] = 2; // read-write ("w+b")
    sfallVfsFilePath[handle] = destPath; // stored for fs_resize reopen (F-028)
    programStackPushInteger(program, handle);
}

// fs_find(path) -> fileId (or -1 if not found)
static void op_fs_find(Program* program)
{
    const char* path = programStackPopString(program);

    if (sfallVfsPathContainsTraversal(path)) {
        programPrintError("fs_find: path traversal rejected '%s'", path);
        programStackPushInteger(program, -1);
        return;
    }

    FILE* file = compat_fopen(path, "rb");
    if (file == nullptr) {
        programStackPushInteger(program, -1);
        return;
    }

    int handle = sfallVfsAllocHandle();
    if (handle < 0) {
        programPrintError("fs_find: no free VFS handles");
        fclose(file);
        programStackPushInteger(program, -1);
        return;
    }

    sfallVfsFiles[handle] = file;
    sfallVfsFileMode[handle] = 1; // read-only ("rb")
    sfallVfsFilePath[handle] = path; // stored for fs_resize reopen (F-028)
    programStackPushInteger(program, handle);
}

// fs_write_byte(id, data)
static void op_fs_write_byte(Program* program)
{
    int data = programStackPopInteger(program);
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_write_byte: invalid VFS handle %d", id);
        return;
    }

    fputc(data & 0xFF, sfallVfsFiles[id]);
}

// fs_write_short(id, data)
static void op_fs_write_short(Program* program)
{
    int data = programStackPopInteger(program);
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_write_short: invalid VFS handle %d", id);
        return;
    }

    uint16_t value = static_cast<uint16_t>(data);
    fwrite(&value, sizeof(value), 1, sfallVfsFiles[id]);
}

// fs_write_int(id, data)
static void op_fs_write_int(Program* program)
{
    int data = programStackPopInteger(program);
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_write_int: invalid VFS handle %d", id);
        return;
    }

    int32_t value = static_cast<int32_t>(data);
    fwrite(&value, sizeof(value), 1, sfallVfsFiles[id]);
}

// fs_write_float(id, data)
static void op_fs_write_float(Program* program)
{
    ProgramValue pv = programStackPopValue(program);
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_write_float: invalid VFS handle %d", id);
        return;
    }

    float value = pv.asFloat();
    fwrite(&value, sizeof(value), 1, sfallVfsFiles[id]);
}

// fs_write_string(id, string)
static void op_fs_write_string(Program* program)
{
    const char* string = programStackPopString(program);
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_write_string: invalid VFS handle %d", id);
        return;
    }

    fputs(string, sfallVfsFiles[id]);
}

// fs_write_bstring(id, string) — writes a length-prefixed byte string
static void op_fs_write_bstring(Program* program)
{
    const char* string = programStackPopString(program);
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_write_bstring: invalid VFS handle %d", id);
        return;
    }

    int len = static_cast<int>(strlen(string));
    uint8_t lenByte = static_cast<uint8_t>(len > 255 ? 255 : len);
    fwrite(&lenByte, 1, 1, sfallVfsFiles[id]);
    fwrite(string, 1, lenByte, sfallVfsFiles[id]);
}

// fs_read_byte(id) -> int (-1 on error)
static void op_fs_read_byte(Program* program)
{
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_read_byte: invalid VFS handle %d", id);
        programStackPushInteger(program, -1);
        return;
    }

    int value = fgetc(sfallVfsFiles[id]);
    programStackPushInteger(program, value);
}

// fs_read_short(id) -> int (-1 on error)
static void op_fs_read_short(Program* program)
{
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_read_short: invalid VFS handle %d", id);
        programStackPushInteger(program, -1);
        return;
    }

    uint16_t value;
    if (fread(&value, sizeof(value), 1, sfallVfsFiles[id]) != 1) {
        programStackPushInteger(program, -1);
        return;
    }
    programStackPushInteger(program, static_cast<int>(value));
}

// fs_read_int(id) -> int (-1 on error)
static void op_fs_read_int(Program* program)
{
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_read_int: invalid VFS handle %d", id);
        programStackPushInteger(program, -1);
        return;
    }

    int32_t value;
    if (fread(&value, sizeof(value), 1, sfallVfsFiles[id]) != 1) {
        programStackPushInteger(program, -1);
        return;
    }
    programStackPushInteger(program, value);
}

// fs_read_float(id) -> float (0.0 on error)
static void op_fs_read_float(Program* program)
{
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_read_float: invalid VFS handle %d", id);
        programStackPushFloat(program, 0.0f);
        return;
    }

    float value;
    if (fread(&value, sizeof(value), 1, sfallVfsFiles[id]) != 1) {
        programStackPushFloat(program, 0.0f);
        return;
    }
    programStackPushFloat(program, value);
}

// fs_delete(id)
static void op_fs_delete(Program* program)
{
    int id = programStackPopInteger(program);
    sfallVfsFreeHandle(id);
}

// fs_size(id) -> int (-1 on error)
static void op_fs_size(Program* program)
{
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_size: invalid VFS handle %d", id);
        programStackPushInteger(program, -1);
        return;
    }

    long pos = ftell(sfallVfsFiles[id]);
    fseek(sfallVfsFiles[id], 0, SEEK_END);
    long size = ftell(sfallVfsFiles[id]);
    fseek(sfallVfsFiles[id], pos, SEEK_SET);

    programStackPushInteger(program, static_cast<int>(size));
}

// fs_pos(id) -> int (-1 on error)
static void op_fs_pos(Program* program)
{
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_pos: invalid VFS handle %d", id);
        programStackPushInteger(program, -1);
        return;
    }

    programStackPushInteger(program, static_cast<int>(ftell(sfallVfsFiles[id])));
}

// fs_seek(id, pos)
static void op_fs_seek(Program* program)
{
    int pos = programStackPopInteger(program);
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_seek: invalid VFS handle %d", id);
        return;
    }

    fseek(sfallVfsFiles[id], pos, SEEK_SET);
}

// fs_resize(id, size)
static void op_fs_resize(Program* program)
{
    int size = programStackPopInteger(program);
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_resize: invalid VFS handle %d", id);
        return;
    }

    if (size <= 0) {
        programPrintError("fs_resize: invalid size %d", size);
        return;
    }

    // F-028: handles opened via fs_find are read-only ("rb"). Reopen in
    // read-write mode ("r+b") to safely extend the file. fs_create/fs_copy
    // handles ("w+b") are already writable — mode 2 path is a no-op.
    if (sfallVfsFileMode[id] == 1) {
        const char* path = sfallVfsFilePath[id];
        if (path == nullptr) {
            programPrintError("fs_resize: cannot reopen read-only handle %d (no path)", id);
            return;
        }
        fclose(sfallVfsFiles[id]);
        sfallVfsFiles[id] = compat_fopen(path, "r+b");
        if (sfallVfsFiles[id] == nullptr) {
            // Fallback: reopen as read-only so the handle remains usable
            // for reads even though resize cannot proceed.
            sfallVfsFiles[id] = compat_fopen(path, "rb");
            programPrintError("fs_resize: cannot reopen '%s' for writing", path);
            return;
        }
        sfallVfsFileMode[id] = 2; // now read-write
    }

    fseek(sfallVfsFiles[id], static_cast<long>(size) - 1, SEEK_SET);
    fputc(0, sfallVfsFiles[id]);
}

// ============================================================
// NPC/Hero opcodes
// ============================================================

// inc_npc_level(pid or string name) — 0x81A5
// Increments the level of the party member NPC matching the given PID.
// Calls _partyMemberIncLevels() which handles the full party member
// level-up logic: level_up_every checks, probability rolls, stat
// adjustments, and display messages. The argument is accepted for
// compatibility but _partyMemberIncLevels iterates all members internally.
static void op_inc_npc_level(Program* program)
{
    ProgramValue arg = programStackPopValue(program);
    (void)arg;

    _partyMemberIncLevels();
}

// get_npc_level(pid or string name) -> int — 0x8241
// Returns the current level of the specified party member NPC.
// Party member level data is stored in _partyMemberLevelUpInfoList
// (static in party_member.cc). Until a public accessor is added to
// party_member.h, level retrieval is limited to verifying the NPC
// exists as a party member.
static void op_get_npc_level(Program* program)
{
    ProgramValue arg = programStackPopValue(program);

    int level = 0;

    if (arg.isInt()) {
        int pid = arg.asInt();
        level = partyMemberGetLevel(pid);
    } else if (arg.isString()) {
        // String input (NPC name): resolve to PID by iterating known party
        // member PIDs and matching the proto display name against the input.
        const char* name = arg.asString(program);
        int pid = -1;

        for (int i = 0; i < gPartyMemberDescriptionsLength; i++) {
            int candidatePid = gPartyMemberPids[i];
            const char* protoName = protoGetName(candidatePid);
            if (protoName != nullptr && strcmp(name, protoName) == 0) {
                pid = candidatePid;
                break;
            }
        }

        if (pid > 0) {
            level = partyMemberGetLevel(pid);
        } else {
            debugPrint("get_npc_level: could not find party member proto matching name '%s'", name);
        }
    } else {
        debugPrint("get_npc_level: argument must be an int (PID) or string (name)");
    }

    programStackPushInteger(program, level);
}

// set_dm_model(string model_name)
// Sets the default male hero model (e.g. "hmjmps"). The model name is resolved
// via artListIndex and applied in op_refresh_pc_art by overriding proto->fid.
// Full Hero Appearance integration (armor-aware model switching, death animation
// hooking) requires changes to _proto_dude_update_gender() in proto.cc.
static void op_set_dm_model(Program* program)
{
    const char* modelName = programStackPopString(program);
    if (modelName == nullptr || modelName[0] == '\0') {
        gCustomMaleHeroModelNum = 0;
        return;
    }

    int modelNum = artListIndex(OBJ_TYPE_CRITTER, modelName);
    if (modelNum >= 0) {
        gCustomMaleHeroModelNum = modelNum;
    } else {
        programPrintError("set_dm_model: model '%s' not found in art list", modelName);
    }
}

// set_df_model(string model_name)
// Sets the default female hero model (e.g. "hfjmps").
static void op_set_df_model(Program* program)
{
    const char* modelName = programStackPopString(program);
    if (modelName == nullptr || modelName[0] == '\0') {
        gCustomFemaleHeroModelNum = 0;
        return;
    }

    int modelNum = artListIndex(OBJ_TYPE_CRITTER, modelName);
    if (modelNum >= 0) {
        gCustomFemaleHeroModelNum = modelNum;
    } else {
        programPrintError("set_df_model: model '%s' not found in art list", modelName);
    }
}

// div (/)
static void op_div(Program* program)
{
    ProgramValue divisorValue = programStackPopValue(program);
    ProgramValue dividendValue = programStackPopValue(program);

    // Zero-division check: for float values, compare as float to catch -0.0f
    // (IEEE 754 bit pattern 0x80000000 ≠ 0 integer, bypassing the integer check).
    // For integer values, compare the raw integerValue.
    if ((divisorValue.isFloat() && divisorValue.asFloat() == 0.0f)
        || divisorValue.integerValue == 0) {
        debugPrint("Division by zero");

        // TODO: Looks like execution is not halted in Sfall's div, check.
        programStackPushInteger(program, 0);
        return;
    }

    if (dividendValue.isFloat() || divisorValue.isFloat()) {
        programStackPushFloat(program, dividendValue.asFloat() / divisorValue.asFloat());
    } else {
        // Signed division — matches sfall and universal scripting convention.
        programStackPushInteger(program, dividendValue.integerValue / divisorValue.integerValue);
    }
}

static void op_sprintf(Program* program)
{
    static constexpr MetaruleInfo kSprintfStringFormatScaffold = {
        "string_format",
        mf_string_format,
        2,
        2,
        0,
        { ARG_STRING, ARG_ANY },
    };

    ProgramValue args[2];
    args[0] = programStackPopValue(program);
    args[1] = programStackPopValue(program);

    OpcodeContext ctx(program, &kSprintfStringFormatScaffold, 2, args);
    if (!ctx.validateArguments()) {
        ctx.setReturn("");
        ctx.pushReturnValue();
        return;
    }

    mf_string_format(ctx);
    ctx.pushReturnValue();
}

static void op_charcode(Program* program)
{
    const char* str = programStackPopString(program);
    if (str != nullptr && str[0] != '\0') {
        programStackPushInteger(program, static_cast<int>(str[0]));
    } else {
        programStackPushInteger(program, 0);
    }
}

static void op_show_iface_tag(fallout::Program* program)
{
    int tag = fallout::programStackPopInteger(program);

    switch (tag) {
    case DudeState::DUDE_STATE_SNEAKING:
    case DudeState::DUDE_STATE_LEVEL_UP_AVAILABLE:
    case DudeState::DUDE_STATE_ADDICTED:
        dudeEnableState(tag);
        break;
    default:
        interfaceTagShow(tag);
    }
}

static void op_hide_iface_tag(fallout::Program* program)
{
    int tag = fallout::programStackPopInteger(program);

    switch (tag) {
    case DudeState::DUDE_STATE_SNEAKING:
    case DudeState::DUDE_STATE_LEVEL_UP_AVAILABLE:
    case DudeState::DUDE_STATE_ADDICTED:
        dudeDisableState(tag);
        break;
    default:
        interfaceTagHide(tag);
    }
}

static void op_is_iface_tag_active(fallout::Program* program)
{
    int tag = fallout::programStackPopInteger(program);
    bool isActive = false;

    switch (tag) {
    case DudeState::DUDE_STATE_SNEAKING:
    case DudeState::DUDE_STATE_LEVEL_UP_AVAILABLE:
    case DudeState::DUDE_STATE_ADDICTED:
        isActive = fallout::dudeHasState(tag);
        break;
    case 1: // POISONED
        isActive = critterGetPoison(gDude) > POISON_INDICATOR_THRESHOLD;
        break;
    case 2: // RADIATED
        isActive = critterGetRadiation(gDude) > RADATION_INDICATOR_THRESHOLD;
        break;
    default:
        isActive = interfaceTagIsActive(tag);
    }

    fallout::programStackPushInteger(program, isActive ? 1 : 0);
}

// TODO: move opcodes into several files
// TODO: reduce code duplication by introducing something like OpcodeContext in sfall

static void op_register_hook(Program* program)
{
    constexpr char opcodeName[] = "register_hook";

    int hookId = programStackPopInteger(program);
    if (hookId < 0 || hookId >= HOOK_COUNT) {
        programPrintError("%s: invalid hook ID: %d", opcodeName, hookId);
        return;
    }
    int startProcIndex = programFindProcedure(program, gScriptProcNames[SCRIPT_PROC_START]);
    if (startProcIndex == -1) {
        programPrintError("%s: 'start' procedure not found", opcodeName);
        return;
    }
    if (!scriptHooksRegister(program, static_cast<HookType>(hookId), startProcIndex)) {
        programPrintError("%s(%d, %d): failed", opcodeName, hookId, startProcIndex);
    }
}

static void op_register_hook_proc(Program* program)
{
    constexpr char opcodeName[] = "register_hook_proc";

    int procedureIndex = programStackPopInteger(program);
    int hookId = programStackPopInteger(program);
    if (hookId < 0 || hookId >= HOOK_COUNT) {
        programPrintError("%s: invalid hook ID: %d", opcodeName, hookId);
        return;
    }
    if (procedureIndex < 0 || procedureIndex >= program->procedureCount()) {
        programPrintError("%s: procedure index %d is out of range [0; %d]", opcodeName, procedureIndex, program->procedureCount());
        return;
    }

    // Note: in sfall, register_hook_proc by default adds the next hook to the beginning of the hook order.
    // Meaning the last script to be registered will be executed first.
    // There was a special opcode `register_hook_proc_spec` that adds to the end of hook order instead.
    // In CE we assume that this order shouldn't matter, and giving script a choice like that doesn't solve anything, since several scripts from different mods can use either opcode.

    // Global script order is entirely based off script file name sorting and when user installs scripts from different mods, there's no way to ensure a "proper" order,
    // without some kind of script-dependency system, which we don't have.
    // So let's just simply use the direct order.
    if (!scriptHooksRegister(program, static_cast<HookType>(hookId), procedureIndex)) {
        programPrintError("%s(%d, %d): failed", opcodeName, hookId, procedureIndex);
    }
}

// register_hook_proc_spec
// In sfall: inserts at end of hook order (spec = special/append) so the
// hook runs LAST. This is used by RPU/ETu for final-override hooks like
// armor-based weapon restrictions (HOOK_CANUSEWEAPON in gl_partyarmor.ssl).
//
// Hook iteration in ScriptHookCall::call() is reverse (size-1 down to 0),
// so inserting at the end (highest index) means this hook executes last.
// Default register_hook_proc inserts at beginning (index 0 = first executed).
static void op_register_hook_proc_spec(Program* program)
{
    constexpr char opcodeName[] = "register_hook_proc_spec";

    int procedureIndex = programStackPopInteger(program);
    int hookId = programStackPopInteger(program);
    if (hookId < 0 || hookId >= HOOK_COUNT) {
        programPrintError("%s: invalid hook ID: %d", opcodeName, hookId);
        return;
    }
    if (procedureIndex < 0 || procedureIndex >= program->procedureCount()) {
        programPrintError("%s: procedure index %d is out of range [0; %d]", opcodeName, procedureIndex, program->procedureCount());
        return;
    }

    if (!scriptHooksRegister(program, static_cast<HookType>(hookId), procedureIndex, true)) {
        programPrintError("%s(%d, %d): failed", opcodeName, hookId, procedureIndex);
    }
}

// ============================================================
// reg_anim_callback: called when registered animations complete.
// In sfall 4.x, this registers a procedure to be invoked upon
// animation completion. The callback receives the animated object
// and a pre-set argument.
// Currently stores the callback for future integration with the
// animation system. The opcode is registered so RPU/ET Tu scripts
// do not crash.
// ============================================================

static Program* sfallAnimCallbackProgram = nullptr;
static int sfallAnimCallbackProcedureIndex = -1;

static void op_reg_anim_callback(Program* program)
{
    int procedureIndex = programStackPopInteger(program);

    if (procedureIndex < 0 || procedureIndex >= program->procedureCount()) {
        programPrintError("reg_anim_callback: procedure index %d is out of range [0; %d]",
            procedureIndex, program->procedureCount());
        return;
    }

    sfallAnimCallbackProgram = program;
    sfallAnimCallbackProcedureIndex = procedureIndex;
}

void sfallAnimCallbackReset()
{
    sfallAnimCallbackProgram = nullptr;
    sfallAnimCallbackProcedureIndex = -1;
}

ScriptHookCall* hookOpcodeGetCurrentCall(const char* opcodeName)
{
    const auto hookCall = ScriptHookCall::current();
    if (hookCall == nullptr) {
        programPrintError("%s: called outside of a script hook", opcodeName);
    }
    return hookCall;
}

static void op_get_sfall_arg(Program* program)
{
    constexpr char opcodeName[] = "get_sfall_arg";

    const auto hookCall = hookOpcodeGetCurrentCall(opcodeName);
    programStackPushValue(program, hookCall != nullptr ? hookCall->getNextArgFromScript() : ProgramValue(0));
}

static void op_get_sfall_args(Program* program)
{
    constexpr char opcodeName[] = "get_sfall_args";

    const auto hookCall = hookOpcodeGetCurrentCall(opcodeName);
    ArrayId result = 0;
    if (hookCall != nullptr) {
        result = CreateTempArray(hookCall->numArgs(), 0);
        for (int i = 0; i < hookCall->numArgs(); ++i) {
            SetArray(result, i, hookCall->getArgAt(i), false, program);
        }
    }
    programStackPushInteger(program, static_cast<int>(result));
}

static void op_set_sfall_arg(Program* program)
{
    constexpr char opcodeName[] = "set_sfall_arg";

    const ProgramValue value = programStackPopValue(program);
    const int argNum = programStackPopInteger(program);

    const auto hookCall = hookOpcodeGetCurrentCall(opcodeName);
    if (hookCall == nullptr) return;

    if (argNum < 0 || argNum >= hookCall->numArgs()) {
        programPrintError("%s: argNum %d out of range [0, %d]", opcodeName, argNum, hookCall->numArgs() - 1);
        return;
    }
    hookCall->setArgAt(argNum, value);
}

static void op_set_sfall_return(Program* program)
{
    constexpr char opcodeName[] = "set_sfall_return";

    const ProgramValue value = programStackPopValue(program);

    const auto hookCall = hookOpcodeGetCurrentCall(opcodeName);
    if (hookCall == nullptr) return;

    if (hookCall->numScriptReturnValues() >= hookCall->maxReturnValues()) {
        programPrintError("%s: trying to add next return value while only %d is expected", opcodeName, hookCall->maxReturnValues());
        return;
    }
    hookCall->addReturnValueFromScript(value);
}

// ============================================================
// Skill point opcodes
// ============================================================

// set_critter_skill_points(object critter, int skill, int value)
// Sets the base skill points (proto-level) for a critter.
// Arguments are popped in reverse order: value, skill, critter.
static void op_set_critter_skill_points(Program* program)
{
    int value = programStackPopInteger(program);
    int skill = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == nullptr || FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER) {
        programPrintError("set_critter_skill_points: expected critter object");
        return;
    }

    if (!skillIsValid(skill)) {
        programPrintError("set_critter_skill_points: invalid skill %d (valid range 0-%d)",
            skill, SKILL_COUNT - 1);
        return;
    }

    Proto* proto;
    if (protoGetProto(critter->pid, &proto) == -1) {
        programPrintError("set_critter_skill_points: cannot get proto for pid %d", critter->pid);
        return;
    }

    proto->critter.data.skills[skill] = value;
}

// get_critter_skill_points(object critter, int skill) -> int
// Returns the base skill points (proto-level) for a critter.
static void op_get_critter_skill_points(Program* program)
{
    int skill = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == nullptr || FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER) {
        programPrintError("get_critter_skill_points: expected critter object");
        programStackPushInteger(program, 0);
        return;
    }

    if (!skillIsValid(skill)) {
        programPrintError("get_critter_skill_points: invalid skill %d (valid range 0-%d)",
            skill, SKILL_COUNT - 1);
        programStackPushInteger(program, 0);
        return;
    }

    programStackPushInteger(program, skillGetBaseValue(critter, skill));
}

// set_available_skill_points(int value)
// Sets the player's unspent skill points.
static void op_set_available_skill_points(Program* program)
{
    int value = programStackPopInteger(program);
    if (value < 0) {
        value = 0;
    }
    pcSetStat(PC_STAT_UNSPENT_SKILL_POINTS, value);
}

// get_available_skill_points() -> int
// Returns the player's current unspent skill points.
static void op_get_available_skill_points(Program* program)
{
    programStackPushInteger(program, pcGetStat(PC_STAT_UNSPENT_SKILL_POINTS));
}

// mod_skill_points_per_level(int value)
// Sets a modifier for skill points gained per level. The modifier is stored
// and consumed by characterEditorUpdateLevel() in character_editor.cc.
int gSkillPointsPerLevelMod = 0;

static void op_mod_skill_points_per_level(Program* program)
{
    gSkillPointsPerLevelMod = programStackPopInteger(program);
    sfall_gl_vars_store("SFSkillP", gSkillPointsPerLevelMod);
}

// ============================================================
// VOODOO memory write opcodes — safe no-ops in CE engine.
//
// sfall allowed scripts to read/write arbitrary memory addresses
// in the Fallout2.exe process space. CE's address space is completely
// different, so direct memory access is impossible. These opcodes are
// registered as safe no-ops that pop their arguments and log a debug
// message, preventing script crashes on unregistered opcode errors.
//
// The behavior these patches intended to fix (hit chance modifier,
// rest timer, talking head moods, encounter dialog suppression, etc.)
// is handled through CE-native config settings (Fallout1Behavior, etc.)
// and hook-based alternatives.
//
// Registration is gated behind AllowUnsafeScripting=1 in ddraw.ini
// [Misc] section. If not enabled, these handlers log a warning noting
// that the setting must be enabled for full VOODOO compatibility.
// ============================================================

static void op_write_byte(Program* program)
{
    int value = programStackPopInteger(program);
    int addr = programStackPopInteger(program);

    debugPrint("VOODOO write_byte(0x%08X, %d) — no-op in CE engine\n", addr, value);
}

static void op_write_short(Program* program)
{
    int value = programStackPopInteger(program);
    int addr = programStackPopInteger(program);

    debugPrint("VOODOO write_short(0x%08X, %d) — no-op in CE engine\n", addr, value);
}

static void op_write_int(Program* program)
{
    int value = programStackPopInteger(program);
    int addr = programStackPopInteger(program);

    debugPrint("VOODOO write_int(0x%08X, %d) — no-op in CE engine\n", addr, value);
}

static void op_write_string(Program* program)
{
    const char* value = programStackPopString(program);
    int addr = programStackPopInteger(program);

    debugPrint("VOODOO write_string(0x%08X, \"%s\") — no-op in CE engine\n", addr, value != nullptr ? value : "(null)");
}

// call_offset_v0-v4: call functions at arbitrary addresses with 0-4 args.
// CE cannot call arbitrary engine-internal functions. Registered as no-ops
// that pop arguments and push 0, allowing scripts to proceed without crash.
static void op_call_offset_v0(Program* program)
{
    int addr = programStackPopInteger(program);
    debugPrint("VOODOO call_offset_v0(0x%08X) — no-op in CE engine\n", addr);
    programStackPushInteger(program, 0);
}

static void op_call_offset_v1(Program* program)
{
    int arg1 = programStackPopInteger(program);
    int addr = programStackPopInteger(program);
    debugPrint("VOODOO call_offset_v1(0x%08X, %d) — no-op in CE engine\n", addr, arg1);
    programStackPushInteger(program, 0);
}

static void op_call_offset_v2(Program* program)
{
    int arg2 = programStackPopInteger(program);
    int arg1 = programStackPopInteger(program);
    int addr = programStackPopInteger(program);
    debugPrint("VOODOO call_offset_v2(0x%08X, %d, %d) — no-op in CE engine\n", addr, arg1, arg2);
    programStackPushInteger(program, 0);
}

static void op_call_offset_v3(Program* program)
{
    int arg3 = programStackPopInteger(program);
    int arg2 = programStackPopInteger(program);
    int arg1 = programStackPopInteger(program);
    int addr = programStackPopInteger(program);
    debugPrint("VOODOO call_offset_v3(0x%08X, %d, %d, %d) — no-op in CE engine\n", addr, arg1, arg2, arg3);
    programStackPushInteger(program, 0);
}

static void op_call_offset_v4(Program* program)
{
    int arg4 = programStackPopInteger(program);
    int arg3 = programStackPopInteger(program);
    int arg2 = programStackPopInteger(program);
    int arg1 = programStackPopInteger(program);
    int addr = programStackPopInteger(program);
    debugPrint("VOODOO call_offset_v4(0x%08X, %d, %d, %d, %d) — no-op in CE engine\n", addr, arg1, arg2, arg3, arg4);
    programStackPushInteger(program, 0);
}

// call_offset_r0-r4: call functions returning int with 0-4 args.
// Same as v0-v4 but with return values. Registered as no-ops pushing 0.
static void op_call_offset_r0(Program* program)
{
    int addr = programStackPopInteger(program);
    debugPrint("VOODOO call_offset_r0(0x%08X) — no-op in CE engine\n", addr);
    programStackPushInteger(program, 0);
}

static void op_call_offset_r1(Program* program)
{
    int arg1 = programStackPopInteger(program);
    int addr = programStackPopInteger(program);
    debugPrint("VOODOO call_offset_r1(0x%08X, %d) — no-op in CE engine\n", addr, arg1);
    programStackPushInteger(program, 0);
}

static void op_call_offset_r2(Program* program)
{
    int arg2 = programStackPopInteger(program);
    int arg1 = programStackPopInteger(program);
    int addr = programStackPopInteger(program);
    debugPrint("VOODOO call_offset_r2(0x%08X, %d, %d) — no-op in CE engine\n", addr, arg1, arg2);
    programStackPushInteger(program, 0);
}

static void op_call_offset_r3(Program* program)
{
    int arg3 = programStackPopInteger(program);
    int arg2 = programStackPopInteger(program);
    int arg1 = programStackPopInteger(program);
    int addr = programStackPopInteger(program);
    debugPrint("VOODOO call_offset_r3(0x%08X, %d, %d, %d) — no-op in CE engine\n", addr, arg1, arg2, arg3);
    programStackPushInteger(program, 0);
}

static void op_call_offset_r4(Program* program)
{
    int arg4 = programStackPopInteger(program);
    int arg3 = programStackPopInteger(program);
    int arg2 = programStackPopInteger(program);
    int arg1 = programStackPopInteger(program);
    int addr = programStackPopInteger(program);
    debugPrint("VOODOO call_offset_r4(0x%08X, %d, %d, %d, %d) — no-op in CE engine\n", addr, arg1, arg2, arg3, arg4);
    programStackPushInteger(program, 0);
}

// ============================================================
// Perk frequency override (set_perk_freq, 0x8247).
//
// Sets the number of levels between perk selections.
// Default: 0 (use engine default of 3 levels, or 4 with Skilled trait).
// When overridden to a positive value, the override is used directly.
// Integration: character_editor.cc:5745 must read this value in
// characterEditorUpdateLevel() — see sfall_opcodes.h for the extern.
// ============================================================
int gPerkFrequencyOverride = 0;

static void op_set_perk_freq(Program* program)
{
    int value = programStackPopInteger(program);
    if (value < 0) {
        value = 0;
    }
    gPerkFrequencyOverride = value;
    sfall_gl_vars_store("SFPerkFr", value);
}

// set_perk_level(int perkID, int value) — 0x817A
// Sets the minimum level requirement for a specific perk.
// value of 0 effectively disables the level gate (any level qualifies).
// ETu uses value 999 for Magnetic Personality to effectively disable it.
static void op_set_perk_level(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);

    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_level: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }

    perkSetMinLevel(perkID, value);
}

// set_skill_max(int value) — 0x81A2
// Sets the maximum skill value cap. Default is 300 (vanilla match).
// The cap must be enforced in skill increment paths (skillAddForce,
// character editor skill point allocation) — those integration points
// need to be updated separately to read this value.
int gSkillMaxCap = 300;

static void op_set_skill_max(Program* program)
{
    int value = programStackPopInteger(program);
    if (value < 0) {
        value = 300;
    }
    gSkillMaxCap = value;
    sfall_gl_vars_store("SFSkillM", value);
}

// ============================================================
// Stat max/min opcodes.
// Modify stat description limits in gStatDescriptions[] via accessors
// in stat.cc. critterGetStat() and related functions automatically
// respect the modified limits.
// ============================================================

// set_stat_max(int stat, int value) — 0x81B4
static void op_set_stat_max(Program* program)
{
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);

    if (!statIsValid(stat)) {
        programPrintError("set_stat_max: invalid stat %d (valid range 0-%d)", stat, STAT_COUNT - 1);
        return;
    }

    statSetMaxValue(stat, value);
}

// set_stat_min(int stat, int value) — 0x81B5
static void op_set_stat_min(Program* program)
{
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);

    if (!statIsValid(stat)) {
        programPrintError("set_stat_min: invalid stat %d (valid range 0-%d)", stat, STAT_COUNT - 1);
        return;
    }

    statSetMinValue(stat, value);
}

// set_pc_stat_max(int stat, int value) — 0x81B7
// ETu needs this for rad resist cap at 100.
static void op_set_pc_stat_max(Program* program)
{
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);

    if (!statIsValid(stat)) {
        programPrintError("set_pc_stat_max: invalid stat %d (valid range 0-%d)", stat, STAT_COUNT - 1);
        return;
    }

    statSetMaxValue(stat, value);
}

// set_pc_stat_min(int stat, int value) — 0x81B8
static void op_set_pc_stat_min(Program* program)
{
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);

    if (!statIsValid(stat)) {
        programPrintError("set_pc_stat_min: invalid stat %d (valid range 0-%d)", stat, STAT_COUNT - 1);
        return;
    }

    statSetMinValue(stat, value);
}

// set_npc_stat_max(int stat, int value) — 0x81B9
static void op_set_npc_stat_max(Program* program)
{
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);

    if (!statIsValid(stat)) {
        programPrintError("set_npc_stat_max: invalid stat %d (valid range 0-%d)", stat, STAT_COUNT - 1);
        return;
    }

    statSetMaxValue(stat, value);
}

// set_npc_stat_min(int stat, int value) — 0x81BA
static void op_set_npc_stat_min(Program* program)
{
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);

    if (!statIsValid(stat)) {
        programPrintError("set_npc_stat_min: invalid stat %d (valid range 0-%d)", stat, STAT_COUNT - 1);
        return;
    }

    statSetMinValue(stat, value);
}

// ============================================================
// set_perk_name (0x8189) — sets display name override for a perk.
// Stored in static table; perkGetName() needs integration to read it.
// ============================================================
static constexpr int kMaxPerkNameOverrides = 128;
static char* sfallPerkNameOverrides[kMaxPerkNameOverrides] = {};

static void op_set_perk_name(Program* program)
{
    const char* name = programStackPopString(program);
    int perkID = programStackPopInteger(program);

    if (!perkIsValid(perkID) || perkID >= kMaxPerkNameOverrides) {
        programPrintError("set_perk_name: invalid perk ID %d", perkID);
        return;
    }

    if (sfallPerkNameOverrides[perkID] != nullptr) {
        delete[] sfallPerkNameOverrides[perkID];
        sfallPerkNameOverrides[perkID] = nullptr;
    }

    if (name != nullptr && name[0] != '\0') {
        size_t len = strlen(name) + 1;
        sfallPerkNameOverrides[perkID] = new char[len];
        memcpy(sfallPerkNameOverrides[perkID], name, len);
    }
}

// ============================================================
// set_perk_desc (0x818A) — sets description override for a perk.
// ============================================================
static char* sfallPerkDescOverrides[kMaxPerkNameOverrides] = {};

static void op_set_perk_desc(Program* program)
{
    const char* desc = programStackPopString(program);
    int perkID = programStackPopInteger(program);

    if (!perkIsValid(perkID) || perkID >= kMaxPerkNameOverrides) {
        programPrintError("set_perk_desc: invalid perk ID %d", perkID);
        return;
    }

    if (sfallPerkDescOverrides[perkID] != nullptr) {
        delete[] sfallPerkDescOverrides[perkID];
        sfallPerkDescOverrides[perkID] = nullptr;
    }

    if (desc != nullptr && desc[0] != '\0') {
        size_t len = strlen(desc) + 1;
        sfallPerkDescOverrides[perkID] = new char[len];
        memcpy(sfallPerkDescOverrides[perkID], desc, len);
    }
}

// ============================================================
// get_perk_available (0x8190) — returns 1 if perk is selectable.
// Checks against perk availability criteria using the dude.
// ============================================================
static void op_get_perk_available(Program* program)
{
    int perk = programStackPopInteger(program);

    if (!perkIsValid(perk)) {
        programStackPushInteger(program, 0);
        return;
    }

    // Check if the perk has valid ranks and the dude meets level requirement.
    int result = 0;
    int maxRank = perkGetMaxRank(perk);
    if (maxRank >= 0) {
        int level = pcGetStat(PC_STAT_LEVEL);
        if (level >= perkGetMinLevel(perk)) {
            result = 1;
        }
    }
    programStackPushInteger(program, result);
}

// ============================================================
// Knockback opcodes (0x8195-0x819A).
// Store knockback settings; exposed via sfall_opcodes.h for combat
// integration in combat.cc (attackComputeDamage).
// ============================================================
int sfallWeaponKnockbackType = 0;
float sfallWeaponKnockbackValue = 0.0f;
int sfallTargetKnockbackType = 0;
float sfallTargetKnockbackValue = 0.0f;
int sfallAttackerKnockbackType = 0;
float sfallAttackerKnockbackValue = 0.0f;

static void op_set_weapon_knockback(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    int type = programStackPopInteger(program);
    Object* weapon = static_cast<Object*>(programStackPopPointer(program));

    if (weapon == nullptr) {
        return;
    }

    sfallWeaponKnockbackType = type;
    sfallWeaponKnockbackValue = value.isFloat() ? value.asFloat() : static_cast<float>(value.integerValue);
    debugPrint("set_weapon_knockback(obj=%p, type=%d) — knockback system integration pending\n",
        static_cast<void*>(weapon), type);
}

static void op_set_target_knockback(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    int type = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == nullptr) {
        return;
    }

    sfallTargetKnockbackType = type;
    sfallTargetKnockbackValue = value.isFloat() ? value.asFloat() : static_cast<float>(value.integerValue);
    debugPrint("set_target_knockback(obj=%p, type=%d) — knockback system integration pending\n",
        static_cast<void*>(critter), type);
}

static void op_set_attacker_knockback(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    int type = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == nullptr) {
        return;
    }

    sfallAttackerKnockbackType = type;
    sfallAttackerKnockbackValue = value.isFloat() ? value.asFloat() : static_cast<float>(value.integerValue);
    debugPrint("set_attacker_knockback(obj=%p, type=%d) — knockback system integration pending\n",
        static_cast<void*>(critter), type);
}

static void op_remove_weapon_knockback(Program* program)
{
    Object* weapon = static_cast<Object*>(programStackPopPointer(program));
    (void)weapon;
    sfallWeaponKnockbackType = 0;
    sfallWeaponKnockbackValue = 0.0f;
}

static void op_remove_target_knockback(Program* program)
{
    Object* critter = static_cast<Object*>(programStackPopPointer(program));
    (void)critter;
    sfallTargetKnockbackType = 0;
    sfallTargetKnockbackValue = 0.0f;
}

static void op_remove_attacker_knockback(Program* program)
{
    Object* critter = static_cast<Object*>(programStackPopPointer(program));
    (void)critter;
    sfallAttackerKnockbackType = 0;
    sfallAttackerKnockbackValue = 0.0f;
}

// ============================================================
// set_xp_mod (0x81AA) — sets percentage modifier on XP gain.
// Integration point: pcAddExperience() in stat.cc.
// ============================================================
int gXpModPercentage = 100;

static constexpr int kMaxXpModPercentage = 10000;

static void op_set_xp_mod(Program* program)
{
    gXpModPercentage = programStackPopInteger(program);
    if (gXpModPercentage < 0) {
        gXpModPercentage = 0;
    }
    if (gXpModPercentage > kMaxXpModPercentage) {
        gXpModPercentage = kMaxXpModPercentage;
    }
    sfall_gl_vars_store("SFXpMod%", gXpModPercentage);
}

// ============================================================
// Fake perk/trait opcodes (0x81BB-0x81C2).
// Allow scripts to register custom perks/traits on the perk/trait
// selection screen. Stored in static tables for now; full integration
// requires UI changes in perk_dialog.cc and character_editor.cc.
// ============================================================
static constexpr int kMaxFakePerks = 64;
struct FakePerkEntry {
    char* name;
    int level;
    int image;
    char* desc;
    bool active;
};
static FakePerkEntry sfallFakePerks[kMaxFakePerks] = {};
static int sfallFakePerkCount = 0;

static constexpr int kMaxFakeTraits = 16;
struct FakeTraitEntry {
    char* name;
    int active; // 0 = inactive, 1 = active  
    int image;
    char* desc;
};
static FakeTraitEntry sfallFakeTraits[kMaxFakeTraits] = {};
static int sfallFakeTraitCount = 0;

static char* sfallPerkboxTitle = nullptr;
static bool sfallHideRealPerks = false;

static void sfallFreeFakePerkEntry(FakePerkEntry& entry)
{
    delete[] entry.name;
    delete[] entry.desc;
    entry.name = nullptr;
    entry.desc = nullptr;
}

static void sfallFreeFakeTraitEntry(FakeTraitEntry& entry)
{
    delete[] entry.name;
    delete[] entry.desc;
    entry.name = nullptr;
    entry.desc = nullptr;
}

// set_fake_perk(name, level, image, desc)
static void op_set_fake_perk(Program* program)
{
    const char* desc = programStackPopString(program);
    int image = programStackPopInteger(program);
    int level = programStackPopInteger(program);
    const char* name = programStackPopString(program);

    if (sfallFakePerkCount >= kMaxFakePerks) {
        programPrintError("set_fake_perk: too many fake perks (max %d)", kMaxFakePerks);
        return;
    }

    FakePerkEntry& entry = sfallFakePerks[sfallFakePerkCount++];
    entry.active = true;
    entry.level = level;
    entry.image = image;

    if (name != nullptr && name[0] != '\0') {
        size_t len = strlen(name) + 1;
        entry.name = new char[len];
        memcpy(entry.name, name, len);
    }
    if (desc != nullptr && desc[0] != '\0') {
        size_t len = strlen(desc) + 1;
        entry.desc = new char[len];
        memcpy(entry.desc, desc, len);
    }
}

// set_fake_trait(name, active, image, desc)
static void op_set_fake_trait(Program* program)
{
    const char* desc = programStackPopString(program);
    int image = programStackPopInteger(program);
    int active = programStackPopInteger(program);
    const char* name = programStackPopString(program);

    if (sfallFakeTraitCount >= kMaxFakeTraits) {
        programPrintError("set_fake_trait: too many fake traits (max %d)", kMaxFakeTraits);
        return;
    }

    FakeTraitEntry& entry = sfallFakeTraits[sfallFakeTraitCount++];
    entry.active = (active != 0) ? 1 : 0;
    entry.image = image;

    if (name != nullptr && name[0] != '\0') {
        size_t len = strlen(name) + 1;
        entry.name = new char[len];
        memcpy(entry.name, name, len);
    }
    if (desc != nullptr && desc[0] != '\0') {
        size_t len = strlen(desc) + 1;
        entry.desc = new char[len];
        memcpy(entry.desc, desc, len);
    }
}

// set_selectable_perk(name, active, image, desc)
// Alias for set_fake_perk in sfall; registers a perk as selectable.
static void op_set_selectable_perk(Program* program)
{
    const char* desc = programStackPopString(program);
    int image = programStackPopInteger(program);
    int active = programStackPopInteger(program);
    const char* name = programStackPopString(program);

    // Simply delegate to set_fake_perk logic.
    if (!active || sfallFakePerkCount >= kMaxFakePerks) {
        return;
    }

    FakePerkEntry& entry = sfallFakePerks[sfallFakePerkCount++];
    entry.active = true;
    entry.level = 0;
    entry.image = image;

    if (name != nullptr && name[0] != '\0') {
        size_t len = strlen(name) + 1;
        entry.name = new char[len];
        memcpy(entry.name, name, len);
    }
    if (desc != nullptr && desc[0] != '\0') {
        size_t len = strlen(desc) + 1;
        entry.desc = new char[len];
        memcpy(entry.desc, desc, len);
    }
}

// set_perkbox_title(title)
static void op_set_perkbox_title(Program* program)
{
    const char* title = programStackPopString(program);

    delete[] sfallPerkboxTitle;
    sfallPerkboxTitle = nullptr;

    if (title != nullptr && title[0] != '\0') {
        size_t len = strlen(title) + 1;
        sfallPerkboxTitle = new char[len];
        memcpy(sfallPerkboxTitle, title, len);
    }
}

// hide_real_perks()
static void op_hide_real_perks(Program* program)
{
    sfallHideRealPerks = true;
}

// show_real_perks()
static void op_show_real_perks(Program* program)
{
    sfallHideRealPerks = false;
}

// has_fake_perk(name) -> int
static void op_has_fake_perk(Program* program)
{
    // Accept either string name or integer extraPerkID.
    ProgramValue arg = programStackPopValue(program);
    int result = 0;

    if (arg.isString()) {
        const char* name = arg.asString(program);
        for (int i = 0; i < sfallFakePerkCount; i++) {
            if (sfallFakePerks[i].name != nullptr
                && strcmp(sfallFakePerks[i].name, name) == 0
                && sfallFakePerks[i].active) {
                result = i + 1;
                break;
            }
        }
    } else {
        int extraPerkID = arg.integerValue;
        // extraPerkID is 1-indexed
        if (extraPerkID > 0 && extraPerkID <= sfallFakePerkCount
            && sfallFakePerks[extraPerkID - 1].active) {
            result = extraPerkID;
        }
    }

    programStackPushInteger(program, result);
}

// has_fake_trait(name) -> int
static void op_has_fake_trait(Program* program)
{
    const char* name = programStackPopString(program);
    int result = 0;

    for (int i = 0; i < sfallFakeTraitCount; i++) {
        if (sfallFakeTraits[i].name != nullptr
            && strcmp(sfallFakeTraits[i].name, name) == 0) {
            result = i + 1;
            break;
        }
    }

    programStackPushInteger(program, result);
}

// ============================================================
// remove_trait (0x8225) — removes a selected trait from the player (F-023).
// Previously commented out; now registered. ET Tu depends on this for
// its trait system. Uses the engine's traitsSetSelected/traitsGetSelected
// API to check if the trait is currently selected and replace it with
// TRAIT_COUNT (none). Note: this only handles the two slot-based traits;
// it does not reverse trait stat/skill effects (those are permanent).
// ============================================================
static void op_remove_trait(Program* program)
{
    int traitID = programStackPopInteger(program);

    if (traitID < 0 || traitID >= TRAIT_COUNT) {
        debugPrint("remove_trait(%d): traitID out of range [0, %d)\n", traitID, TRAIT_COUNT);
        return;
    }

    int trait1, trait2;
    traitsGetSelected(&trait1, &trait2);

    if (trait1 == traitID) {
        traitsSetSelected(TRAIT_COUNT, trait2);
        debugPrint("remove_trait(%d): removed from slot 1\n", traitID);
    } else if (trait2 == traitID) {
        traitsSetSelected(trait1, TRAIT_COUNT);
        debugPrint("remove_trait(%d): removed from slot 2\n", traitID);
    } else {
        debugPrint("remove_trait(%d): trait not currently selected (selected: %d, %d)\n",
            traitID, trait1, trait2);
    }
}

// ============================================================
// Perk/trait modifier opcodes (F-029, F-032). These opcodes are used
// by ET Tu for gameplay modifications. Values are stored in globals
// for future integration with the character/perk systems.
//  0x81C3 perk_add_mode, 0x81C4 clear_selectable_perks,
//  0x81CB set_pyromaniac_mod, 0x81CC apply_heaveho_fix,
//  0x81CD set_swiftlearner_mod, 0x81CE set_hp_per_level_mod.
// ============================================================

static int sfallPerkAddMode = 0;
static bool sfallClearSelectablePerks = false;
static int sfallPyromaniacMod = 0;
static int sfallSwiftLearnerMod = 0;
static int sfallHpPerLevelMod = 0;

static void op_perk_add_mode(Program* program)
{
    sfallPerkAddMode = programStackPopInteger(program);
}

static void op_clear_selectable_perks(Program* program)
{
    (void)program;
    sfallClearSelectablePerks = true;
}

static void op_set_pyromaniac_mod(Program* program)
{
    sfallPyromaniacMod = programStackPopInteger(program);
}

// apply_heaveho_fix: stubbed — the Heave Ho! perk fix is handled
// through CE configuration rather than a script opcode.
static void op_apply_heaveho_fix(Program* program)
{
    (void)program;
    debugPrint("apply_heaveho_fix: CE handles Heave Ho! via config; opcode is a no-op");
}

static void op_set_swiftlearner_mod(Program* program)
{
    sfallSwiftLearnerMod = programStackPopInteger(program);
}

static void op_set_hp_per_level_mod(Program* program)
{
    sfallHpPerLevelMod = programStackPopInteger(program);
}

// ============================================================
// set_critter_hit_chance_mod (0x81C5) — modifies hit chance.
// Stores max and mod values; exposed via sfall_opcodes.h for combat
// integration in combat.cc (attackDetermineToHit).
// ============================================================
int sfallHitChanceMod = 0;
int sfallHitChanceMax = 95;

static void op_set_critter_hit_chance_mod(Program* program)
{
    int mod = programStackPopInteger(program);
    int max = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == nullptr) {
        return;
    }

    sfallHitChanceMax = max;
    sfallHitChanceMod = mod;
    debugPrint("set_critter_hit_chance_mod(obj=%p, max=%d, mod=%d) — combat integration pending\n",
        static_cast<void*>(critter), max, mod);
}

// ============================================================
// set_hit_chance_max (0x81A1) — sets the maximum to-hit cap.
// Previously commented out; now registered (F-006). ET Tu uses
// this for Fallout 1 hit mechanics where the cap may differ from 95.
// Clamps to valid range [1, 100].
// ============================================================
static void op_set_hit_chance_max(Program* program)
{
    int max = programStackPopInteger(program);
    if (max < 1) {
        max = 1;
    }
    if (max > 100) {
        max = 100;
    }
    sfallHitChanceMax = max;
}

// ============================================================
// set_base_hit_chance_mod (0x81C6) — sets flat hit chance bonus/malus.
// Previously commented out; now registered (F-006). ET Tu uses this
// for Fallout 1 hit mechanics where base chance is adjusted globally.
// ============================================================
static void op_set_base_hit_chance_mod(Program* program)
{
    int mod = programStackPopInteger(program);
    int max = programStackPopInteger(program);
    if (max < 1) {
        max = 1;
    }
    if (max > 100) {
        max = 100;
    }
    sfallHitChanceMax = max;
    sfallHitChanceMod = mod;
}

// ============================================================
// Set skill/pickpocket/critter modifier opcodes (F-029).
// 0x81C7 set_critter_skill_mod, 0x81C8 set_base_skill_mod,
// 0x81C9 set_critter_pickpocket_mod, 0x81CA set_base_pickpocket_mod.
// These opcodes are used by ET Tu for Fallout 1 skill/pickpocket
// mechanics. Values are stored in globals for future integration
// with the combat and skill systems. Also includes:
// 0x81A0 set_pickpocket_max, 0x81AB set_perk_level_mod.
// ============================================================

// Pickpocket max percentage (0x81A0).
static int sfallPickpocketMax = 0;

static void op_set_pickpocket_max(Program* program)
{
    int percentage = programStackPopInteger(program);
    if (percentage < 0) {
        percentage = 0;
    }
    if (percentage > 100) {
        percentage = 100;
    }
    sfallPickpocketMax = percentage;
}

// Perk level mod (0x81AB) — reduces number of levels between perk
// selections. Integration point: characterEditorUpdateLevel().
static int sfallPerkLevelMod = 0;

static void op_set_perk_level_mod(Program* program)
{
    sfallPerkLevelMod = programStackPopInteger(program);
}

// Skill mod globals (0x81C7, 0x81C8).
static int sfallCritterSkillMod = 0;
static int sfallBaseSkillMod = 0;

static void op_set_critter_skill_mod(Program* program)
{
    int max = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));
    (void)critter;
    if (max < 1) {
        max = 1;
    }
    sfallCritterSkillMod = max;
}

static void op_set_base_skill_mod(Program* program)
{
    int max = programStackPopInteger(program);
    if (max < 1) {
        max = 1;
    }
    sfallBaseSkillMod = max;
}

// Pickpocket mod globals (0x81C9, 0x81CA).
static int sfallCritterPickpocketMod = 0;
static int sfallBasePickpocketMod = 0;
static int sfallCritterPickpocketMax = 0;
static int sfallBasePickpocketMax = 0;

static void op_set_critter_pickpocket_mod(Program* program)
{
    int mod = programStackPopInteger(program);
    int max = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));
    (void)critter;
    if (max < 1) {
        max = 1;
    }
    sfallCritterPickpocketMod = mod;
    if (max > 100) {
        max = 100;
    }
    sfallCritterPickpocketMax = max;
}

static void op_set_base_pickpocket_mod(Program* program)
{
    int mod = programStackPopInteger(program);
    int max = programStackPopInteger(program);
    if (max < 1) {
        max = 1;
    }
    if (max > 100) {
        max = 100;
    }
    sfallBasePickpocketMod = mod;
    sfallBasePickpocketMax = max;
}

// ============================================================
// sneak_success (0x826C) — returns 1 if sneaking is successful.
// Uses the engine's skill check on the dude's Sneak skill vs 0.
// Pushes 0 if not sneaking or check fails.
// ============================================================
static void op_sneak_success(Program* program)
{
    int result = 0;
    if (gDude != nullptr && dudeHasState(DUDE_STATE_SNEAKING)) {
        int sneakSkill = skillGetValue(gDude, SKILL_SNEAK);
        result = (randomBetween(1, 100) <= sneakSkill) ? 1 : 0;
    }
    programStackPushInteger(program, result);
}

// ============================================================
// create_spatial (0x8273) — creates a spatial script object at a tile.
// Spatial scripts execute when a critter enters their radius.
// Full implementation requires script system integration (scriptAdd,
// scriptSetScript, object creation, and tile placement).
// ============================================================
static void op_create_spatial(Program* program)
{
    int radius = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);
    int scriptID = programStackPopInteger(program);

    // Create a spatial script object — invisible object that
    // runs its spatial procedure when critters enter its radius.
    // TODO: Full implementation requires:
    //   1. scriptAdd(&sid, SCRIPT_TYPE_SPATIAL)
    //   2. scriptSetObjects(sid, nullptr, nullptr)
    //   3. objectCreateWithFidPid + objectSetLocation
    //   4. scr->sp.radius = radius
    //   5. scr->sp.built_tile = builtTileCreate(tile, elevation)
    debugPrint("create_spatial(script=%d, tile=%d, elev=%d, radius=%d) — spatial system integration pending\n",
        scriptID, tile, elevation, radius);
    programStackPushInteger(program, 0);
}

// ============================================================
// hero_select_win (0x8213) — opens hero selection window.
// The win parameter in sfall specifies which window type to open
// (0 = character sheet, 1 = perks, 2 = karma, 3 = skills).
// CE provides equivalent functionality through the character editor
// and interface bar; full window-type routing requires UI integration.
// ============================================================
static void op_hero_select_win(Program* program)
{
    int win = programStackPopInteger(program);

    // Map sfall win parameter to character editor folder:
    //   0 = character sheet (main stats/skills view),
    //   1 = perks tab,
    //   2 = karma tab,
    //   3 = kills (level/location history).
    // Silently no-ops if the character editor is not currently open
    // (characterEditorShow() is a blocking modal dialog — it cannot be
    // opened from within a script handler).
    if (characterEditorGetWindow() >= 0) {
        switch (win) {
        case 0:
            characterEditorSelectFolder(0);
            break;
        case 1:
            characterEditorSelectFolder(1);
            break;
        case 2:
            characterEditorSelectFolder(2);
            break;
        case 3:
            characterEditorSelectFolder(3);
            break;
        default:
            debugPrint("hero_select_win(win=%d) — unsupported window type\n", win);
            break;
        }
    } else {
        debugPrint("hero_select_win(win=%d) — character editor not open, navigation skipped\n", win);
    }
}

// ============================================================
// set_hero_race (0x8214) — stores custom hero race for Hero Appearance.
// ============================================================
static void op_set_hero_race(Program* program)
{
    int race = programStackPopInteger(program);
    sfall_gl_vars_store("HAp_Race", race);
}

// ============================================================
// set_hero_style (0x8215) — stores custom hero style for Hero Appearance.
// ============================================================
static void op_set_hero_style(Program* program)
{
    int style = programStackPopInteger(program);
    sfall_gl_vars_store("HApStyle", style);
}

// ============================================================
// set_pipboy_available (0x818B) — enables/disables pipboy at runtime (F-019).
// Previously commented out; now registered. Overrides the static
// config value pipboy_available_at_game_start (pipboy.cc:442).
// Integration point: pipboyOpen() in pipboy.cc should check
// gPipboyAvailableOverride before the engine's static config.
// ============================================================
int gPipboyAvailableOverride = -1; // -1 = not set, 0 = unavailable, 1 = available

static void op_set_pipboy_available(Program* program)
{
    gPipboyAvailableOverride = programStackPopInteger(program) != 0 ? 1 : 0;
}

// ============================================================
// get_last_target (0x8248) / get_last_attacker (0x8249).
// Returns the last target/attacker of a critter. Maintains both a
// per-critter map and a global fallback for backward compatibility.
// Combat.cc writes gLastAttacker/gLastTarget on each attack; the
// get functions lazily populate per-critter storage from the global
// when the critter parameter matches the most recent attacker.
// ============================================================
int gLastAttacker = -1;
int gLastTarget = -1;

// Per-critter last target/attacker storage (F-030, F-088).
// Keyed by critter Object::id.
static std::unordered_map<int, int> gCritterLastTarget;
static std::unordered_map<int, int> gCritterLastAttacker;

static void op_get_last_target(Program* program)
{
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter != nullptr) {
        int critterId = critter->id;

        // Lazy-populate per-critter storage from the global: if this critter
        // was the most recent attacker, cache its last target.
        if (gLastAttacker == critterId) {
            gCritterLastTarget[critterId] = gLastTarget;
        }

        // Check per-critter storage first.
        auto it = gCritterLastTarget.find(critterId);
        if (it != gCritterLastTarget.end() && it->second >= 0) {
            Object* obj = objectFindById(it->second);
            if (obj != nullptr) {
                programStackPushPointer(program, obj);
                return;
            }
            // Object no longer exists — remove stale entry.
            gCritterLastTarget.erase(it);
        }
    }

    // Global fallback for backward compatibility.
    if (gLastTarget >= 0) {
        Object* obj = objectFindById(gLastTarget);
        if (obj != nullptr) {
            programStackPushPointer(program, obj);
            return;
        }
    }
    programStackPushInteger(program, 0);
}

static void op_get_last_attacker(Program* program)
{
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter != nullptr) {
        int critterId = critter->id;

        // Lazy-populate per-critter storage from the global: if this critter
        // was the most recent target, cache its last attacker.
        if (gLastTarget == critterId) {
            gCritterLastAttacker[critterId] = gLastAttacker;
        }

        // Check per-critter storage first.
        auto it = gCritterLastAttacker.find(critterId);
        if (it != gCritterLastAttacker.end() && it->second >= 0) {
            Object* obj = objectFindById(it->second);
            if (obj != nullptr) {
                programStackPushPointer(program, obj);
                return;
            }
            // Object no longer exists — remove stale entry.
            gCritterLastAttacker.erase(it);
        }
    }

    // Global fallback for backward compatibility.
    if (gLastAttacker >= 0) {
        Object* obj = objectFindById(gLastAttacker);
        if (obj != nullptr) {
            programStackPushPointer(program, obj);
            return;
        }
    }
    programStackPushInteger(program, 0);
}

// ============================================================
// get_kill_counter (0x818C) — returns kill counter for critter type.
// mod_kill_counter (0x818D) — modifies kill counter for critter type.
// ET Tu uses these for tracking faction/enemy kill counts. The engine
// maintains kill counters internally; these opcodes provide script access.
// ============================================================
static std::unordered_map<int, int> gSfallKillCounters;

static void op_get_kill_counter(Program* program)
{
    int critterType = programStackPopInteger(program);
    int count = 0;
    auto it = gSfallKillCounters.find(critterType);
    if (it != gSfallKillCounters.end()) {
        count = it->second;
    }
    programStackPushInteger(program, count);
}

static void op_mod_kill_counter(Program* program)
{
    int amount = programStackPopInteger(program);
    int critterType = programStackPopInteger(program);
    gSfallKillCounters[critterType] += amount;
}

// ============================================================
// get_perk_owed (0x818E) — returns how many perk levels the player is
// behind. set_perk_owed (0x818F) — sets the perk-owed counter.
// ET Tu perk system depends on these for "missed perk" tracking.
// ============================================================
static int gSfallPerkOwed = 0;

static void op_get_perk_owed(Program* program)
{
    programStackPushInteger(program, gSfallPerkOwed);
}

static void op_set_perk_owed(Program* program)
{
    gSfallPerkOwed = programStackPopInteger(program);
}

// ============================================================
// sfallAnimCallbackInvoke — public API to invoke the registered
// sfall animation callback procedure on the given object.
// Should be called from animation completion paths in animation.cc.
// ============================================================
void sfallAnimCallbackInvoke(Object* object)
{
    if (sfallAnimCallbackProgram == nullptr || sfallAnimCallbackProcedureIndex < 0) {
        return;
    }

    if (object == nullptr) {
        return;
    }

    // Set up a minimal script execution context.
    // Push the animated object as the argument, then execute the procedure.
    Program* program = sfallAnimCallbackProgram;
    int procIndex = sfallAnimCallbackProcedureIndex;

    // Guard against use-after-free (I2-F-001): if the registered program has
    // exited (set via _updatePrograms() before programListNodeFree frees it),
    // or if its internal data has been invalidated, clear the dangling pointer
    // and return. This is a best-effort guard — the true fix requires clearing
    // sfallAnimCallbackProgram in programFree() (interpreter.cc), which is
    // outside this translation unit.
    if (program->exited || program->data == nullptr) {
        sfallAnimCallbackProgram = nullptr;
        sfallAnimCallbackProcedureIndex = -1;
        return;
    }

    if (procIndex < 0 || procIndex >= program->procedureCount()) {
        sfallAnimCallbackProgram = nullptr;
        sfallAnimCallbackProcedureIndex = -1;
        return;
    }

    programStackPushValue(program, ProgramValue(object));
    programExecuteProcedure(program, procIndex);
    // Reset after one-shot invocation (matches sfall behavior).
    sfallAnimCallbackProgram = nullptr;
    sfallAnimCallbackProcedureIndex = -1;
}

void sfallOpcodesReset()
{
    // Free and clear fake perk entries.
    for (int i = 0; i < sfallFakePerkCount; i++) {
        sfallFreeFakePerkEntry(sfallFakePerks[i]);
    }
    sfallFakePerkCount = 0;

    // Free and clear fake trait entries.
    for (int i = 0; i < sfallFakeTraitCount; i++) {
        sfallFreeFakeTraitEntry(sfallFakeTraits[i]);
    }
    sfallFakeTraitCount = 0;

    // Reset XP modifier to default.
    gXpModPercentage = 100;

    // Reset hit chance globals.
    sfallHitChanceMod = 0;
    sfallHitChanceMax = 95;

    // Reset knockback globals.
    sfallWeaponKnockbackType = 0;
    sfallWeaponKnockbackValue = 0.0f;
    sfallTargetKnockbackType = 0;
    sfallTargetKnockbackValue = 0.0f;
    sfallAttackerKnockbackType = 0;
    sfallAttackerKnockbackValue = 0.0f;

    // Free perk name/desc override entries.
    for (int i = 0; i < kMaxPerkNameOverrides; i++) {
        if (sfallPerkNameOverrides[i] != nullptr) {
            delete[] sfallPerkNameOverrides[i];
            sfallPerkNameOverrides[i] = nullptr;
        }
    }
    for (int i = 0; i < kMaxPerkNameOverrides; i++) {
        if (sfallPerkDescOverrides[i] != nullptr) {
            delete[] sfallPerkDescOverrides[i];
            sfallPerkDescOverrides[i] = nullptr;
        }
    }

    // Reset perkbox UI state.
    delete[] sfallPerkboxTitle;
    sfallPerkboxTitle = nullptr;
    sfallHideRealPerks = false;

    // Reset last target/attacker tracking.
    gLastAttacker = -1;
    gLastTarget = -1;
    gCritterLastTarget.clear();
    gCritterLastAttacker.clear();

    // Reset hero model overrides set via set_dm_model / set_df_model.
    gCustomMaleHeroModelNum = 0;
    gCustomFemaleHeroModelNum = 0;

    // Reset skill/perk modifiers set via metarule opcodes.
    gSkillMaxCap = 300;
    gPerkFrequencyOverride = 0;
    gSkillPointsPerLevelMod = 0;

    // Reset pipboy availability override set by set_pipboy_available (0x818B).
    gPipboyAvailableOverride = -1;

    // Reset kill counter map.
    gSfallKillCounters.clear();

    // Reset perk owed counter.
    gSfallPerkOwed = 0;

    // Reset pickpocket max override.
    sfallPickpocketMax = 0;

    // Reset perk level mod.
    sfallPerkLevelMod = 0;

    // Reset skill/pickpocket mod globals.
    sfallCritterSkillMod = 0;
    sfallBaseSkillMod = 0;
    sfallCritterPickpocketMod = 0;
    sfallBasePickpocketMod = 0;
    sfallCritterPickpocketMax = 0;
    sfallBasePickpocketMax = 0;

    // Reset perk add mode and selectable perks flag.
    sfallPerkAddMode = 0;
    sfallClearSelectablePerks = false;

    // Reset pyromaniac/swiftlearner/HP per level mods.
    sfallPyromaniacMod = 0;
    sfallSwiftLearnerMod = 0;
    sfallHpPerLevelMod = 0;
}

// Persist all opcode-related global state into the sfall global vars map
// before sfall_gl_vars_save() writes to sfallgv.sav.
// Keys match the 8-character sfall convention.
void sfallOpcodeStateSave()
{
    sfall_gl_vars_store("SFXpMod%", gXpModPercentage);
    sfall_gl_vars_store("SFSkillM", gSkillMaxCap);
    sfall_gl_vars_store("SFPerkFr", gPerkFrequencyOverride);
    sfall_gl_vars_store("SFSkillP", gSkillPointsPerLevelMod);
}

// Restore opcode globals from the sfall global vars map after
// sfall_gl_vars_load() has read sfallgv.sav.
void sfallOpcodeStateLoad()
{
    int val;
    if (sfall_gl_vars_fetch("SFXpMod%", val)) {
        gXpModPercentage = val;
    }
    if (sfall_gl_vars_fetch("SFSkillM", val)) {
        gSkillMaxCap = val;
    }
    if (sfall_gl_vars_fetch("SFPerkFr", val)) {
        gPerkFrequencyOverride = val;
    }
    if (sfall_gl_vars_fetch("SFSkillP", val)) {
        gSkillPointsPerLevelMod = val;
    }
}

// Note: opcodes should pop arguments off the stack in reverse order
void sfallOpcodesInit()
{
    // ref. https://github.com/sfall-team/sfall/blob/71ecec3d405bd5e945f157954618b169e60068fe/artifacts/scripting/sfall%20opcode%20list.txt#L145
    // Note: we can't really implement these since address space is different.
    // We can potentially special case some of them, but we should try to avoid that.
    // 0x8156 - int   read_byte(int address)
    interpreterRegisterOpcode(0x8156, op_read_byte);
    // 0x8157 - int   read_short(int address)
    interpreterRegisterOpcode(0x8157, op_read_short);
    // 0x8158 - int   read_int(int address)
    interpreterRegisterOpcode(0x8158, op_read_int);
    // 0x8159 - string read_string(int address) — deliberately NOT registered (inherently unsafe)

    // VOODOO memory write opcodes — registered as safe no-ops.
    // These pop their arguments and log a debug message, preventing script
    // crashes on unregistered opcode errors. The behavior these patches
    // intended to fix is handled through CE-native config and hooks.
    // 0x81cf - void  write_byte(int address, int value)
    interpreterRegisterOpcode(0x81CF, op_write_byte);
    // 0x81d0 - void  write_short(int address, int value)
    interpreterRegisterOpcode(0x81D0, op_write_short);
    // 0x81d1 - void  write_int(int address, int value)
    interpreterRegisterOpcode(0x81D1, op_write_int);
    // 0x821b - void  write_string(int address, string value)
    interpreterRegisterOpcode(0x821B, op_write_string);

    // VOODOO call_offset opcodes — registered as safe no-ops pushing 0.
    // CE cannot call arbitrary engine-internal functions at arbitrary
    // addresses. These push 0 so scripts that check return values can
    // skip the VOODOO patch and use CE-native alternatives.
    // 0x81d2 - void  call_offset_v0(int address)
    interpreterRegisterOpcode(0x81D2, op_call_offset_v0);
    // 0x81d3 - void  call_offset_v1(int address, int arg1)
    interpreterRegisterOpcode(0x81D3, op_call_offset_v1);
    // 0x81d4 - void  call_offset_v2(int address, int arg1, int arg2)
    interpreterRegisterOpcode(0x81D4, op_call_offset_v2);
    // 0x81d5 - void  call_offset_v3(int address, int arg1, int arg2, int arg3)
    interpreterRegisterOpcode(0x81D5, op_call_offset_v3);
    // 0x81d6 - void  call_offset_v4(int address, int arg1, int arg2, int arg3, int arg4)
    interpreterRegisterOpcode(0x81D6, op_call_offset_v4);
    // 0x81d7 - int   call_offset_r0(int address)
    interpreterRegisterOpcode(0x81D7, op_call_offset_r0);
    // 0x81d8 - int   call_offset_r1(int address, int arg1)
    interpreterRegisterOpcode(0x81D8, op_call_offset_r1);
    // 0x81d9 - int   call_offset_r2(int address, int arg1, int arg2)
    interpreterRegisterOpcode(0x81D9, op_call_offset_r2);
    // 0x81da - int   call_offset_r3(int address, int arg1, int arg2, int arg3)
    interpreterRegisterOpcode(0x81DA, op_call_offset_r3);
    // 0x81db - int   call_offset_r4(int address, int arg1, int arg2, int arg3, int arg4)
    interpreterRegisterOpcode(0x81DB, op_call_offset_r4);

    // 0x815a - void set_pc_base_stat(int StatID, int value)
    interpreterRegisterOpcode(0x815A, op_set_pc_base_stat);
    // 0x815b - void set_pc_extra_stat(int StatID, int value)
    interpreterRegisterOpcode(0x815B, op_set_pc_bonus_stat);
    // 0x815c - int  get_pc_base_stat(int StatID)
    interpreterRegisterOpcode(0x815C, op_get_pc_base_stat);
    // 0x815d - int  get_pc_extra_stat(int StatID)
    interpreterRegisterOpcode(0x815D, op_get_pc_bonus_stat);

    // 0x815e - void set_critter_base_stat(object, int StatID, int value)
    interpreterRegisterOpcode(0x815E, op_set_critter_base_stat);
    // 0x815f - void set_critter_extra_stat(object, int StatID, int value)
    interpreterRegisterOpcode(0x815F, op_set_critter_extra_stat);
    // 0x8160 - int  get_critter_base_stat(object, int StatID)
    interpreterRegisterOpcode(0x8160, op_get_critter_base_stat);
    // 0x8161 - int  get_critter_extra_stat(object, int StatID)
    interpreterRegisterOpcode(0x8161, op_get_critter_extra_stat);
    // 0x8242 - void set_critter_skill_points(object critter, int skill, int value)
    interpreterRegisterOpcode(0x8242, op_set_critter_skill_points);
    // 0x8243 - int  get_critter_skill_points(object critter, int skill)
    interpreterRegisterOpcode(0x8243, op_get_critter_skill_points);
    // 0x8244 - void set_available_skill_points(int value)
    interpreterRegisterOpcode(0x8244, op_set_available_skill_points);
    // 0x8245 - int  get_available_skill_points()
    interpreterRegisterOpcode(0x8245, op_get_available_skill_points);
    // 0x8246 - void mod_skill_points_per_level(int value)
    interpreterRegisterOpcode(0x8246, op_mod_skill_points_per_level);

    // 0x81b4 - void set_stat_max(int stat, int value)
    interpreterRegisterOpcode(0x81B4, op_set_stat_max);
    // 0x81b5 - void set_stat_min(int stat, int value)
    interpreterRegisterOpcode(0x81B5, op_set_stat_min);
    // 0x81b7 - void set_pc_stat_max(int stat, int value)
    interpreterRegisterOpcode(0x81B7, op_set_pc_stat_max);
    // 0x81b8 - void set_pc_stat_min(int stat, int value)
    interpreterRegisterOpcode(0x81B8, op_set_pc_stat_min);
    // 0x81b9 - void set_npc_stat_max(int stat, int value)
    interpreterRegisterOpcode(0x81B9, op_set_npc_stat_max);
    // 0x81ba - void set_npc_stat_min(int stat, int value)
    interpreterRegisterOpcode(0x81BA, op_set_npc_stat_min);

    // 0x816b - int  input_funcs_available() // deprecated; do not implement
    // 0x816c - int  key_pressed(int dxScancode)
    interpreterRegisterOpcode(0x816C, op_key_pressed);
    // 0x8162 - void tap_key(int dxScancode)
    interpreterRegisterOpcode(0x8162, op_tap_key);
    // 0x821c - int  get_mouse_x()
    interpreterRegisterOpcode(0x821C, op_get_mouse_x);
    // 0x821d - int  get_mouse_y()
    interpreterRegisterOpcode(0x821D, op_get_mouse_y);
    // 0x821e - int  get_mouse_buttons()
    interpreterRegisterOpcode(0x821E, op_get_mouse_buttons);
    // 0x821f - int  get_window_under_mouse()
    interpreterRegisterOpcode(0x821F, op_get_window_under_mouse);

    // 0x8163 - int get_year()
    interpreterRegisterOpcode(0x8163, op_get_year);

    // 0x8164 - bool game_loaded()
    interpreterRegisterOpcode(0x8164, op_game_loaded);

    // 0x8165 - bool graphics_funcs_available()
    // 0x8166 - int  load_shader(string path)
    // 0x8167 - void free_shader(int ID)
    // 0x8168 - void activate_shader(int ID)
    // 0x8169 - void deactivate_shader(int ID)
    // 0x816d - void set_shader_int(int ID, string param, int value)
    // 0x816e - void set_shader_float(int ID, string param, float value)
    // 0x816f - void set_shader_vector(int ID, string param, float f1, float f2, float f3, float f4)
    // 0x81ad - int get_shader_version()
    // 0x81ae - void set_shader_mode(int mode)
    // 0x81b0 - void force_graphics_refresh(bool enabled)
    // 0x81b1 - int get_shader_texture(int ID, int texture)
    // 0x81b2 - void set_shader_texture(int ID, string param, int texID)

    // 0x816a - void set_global_script_repeat(int frames)
    interpreterRegisterOpcode(0x816A, op_set_global_script_repeat);
    // 0x819b - void set_global_script_type(int type)
    interpreterRegisterOpcode(0x819B, op_set_global_script_type);
    // 0x819c - int available_global_script_types()
    interpreterRegisterOpcode(0x819c, op_available_global_script_types);

    // 0x8170 - bool in_world_map()
    interpreterRegisterOpcode(0x8170, op_in_world_map);

    // 0x8171 - void force_encounter(int map)
    interpreterRegisterOpcode(0x8171, op_force_encounter);
    // 0x8229 - void force_encounter_with_flags(int map, int flags)
    interpreterRegisterOpcode(0x8229, op_force_encounter_with_flags);
    // 0x822a - void set_map_time_multi(float multi)
    interpreterRegisterOpcode(0x822A, op_set_map_time_multi);

    // 0x8172 - void set_world_map_pos(int x, int y)
    interpreterRegisterOpcode(0x8172, op_set_world_map_pos);
    // 0x8173 - int get_world_map_x_pos()
    interpreterRegisterOpcode(0x8173, op_get_world_map_x_pos);
    // 0x8174 - int get_world_map_y_pos()
    interpreterRegisterOpcode(0x8174, op_get_world_map_y_pos);

    // 0x8175 - void set_dm_model(string name)
    interpreterRegisterOpcode(0x8175, op_set_dm_model);
    // 0x8176 - void set_df_model(string name)
    interpreterRegisterOpcode(0x8176, op_set_df_model);
    // 0x8177 - void set_movie_path(string filename, int movieid)

    // 0x8178 - void set_perk_image(int perkID, int value)
    // 0x8179 - void set_perk_ranks(int perkID, int value)
    // 0x817a - void set_perk_level(int perkID, int value)
    interpreterRegisterOpcode(0x817A, op_set_perk_level);
    // 0x817b - void set_perk_stat(int perkID, int value)
    // 0x817c - void set_perk_stat_mag(int perkID, int value)
    // 0x817d - void set_perk_skill1(int perkID, int value)
    // 0x817e - void set_perk_skill1_mag(int perkID, int value)
    // 0x817f - void set_perk_type(int perkID, int value)
    // 0x8180 - void set_perk_skill2(int perkID, int value)
    // 0x8181 - void set_perk_skill2_mag(int perkID, int value)
    // 0x8182 - void set_perk_str(int perkID, int value)
    // 0x8183 - void set_perk_per(int perkID, int value)
    // 0x8184 - void set_perk_end(int perkID, int value)
    // 0x8185 - void set_perk_chr(int perkID, int value)
    // 0x8196 - void set_perk_int(int perkID, int value)
    // 0x8187 - void set_perk_agl(int perkID, int value)
    // 0x8188 - void set_perk_lck(int perkID, int value)
    // 0x8189 - void set_perk_name(int perkID, string value)
    interpreterRegisterOpcode(0x8189, op_set_perk_name);
    // 0x818a - void set_perk_desc(int perkID, string value)
    interpreterRegisterOpcode(0x818A, op_set_perk_desc);
    // 0x8247 - void set_perk_freq(int value)
    interpreterRegisterOpcode(0x8247, op_set_perk_freq);

    // 0x818b - void set_pipboy_available(int available)
    interpreterRegisterOpcode(0x818B, op_set_pipboy_available);

    // 0x818c - int get_kill_counter(int critterType)
    interpreterRegisterOpcode(0x818C, op_get_kill_counter);
    // 0x818d - void mod_kill_counter(int critterType, int amount)
    interpreterRegisterOpcode(0x818D, op_mod_kill_counter);

    // 0x818e - int get_perk_owed()
    interpreterRegisterOpcode(0x818E, op_get_perk_owed);
    // 0x818f - void set_perk_owed(int value)
    interpreterRegisterOpcode(0x818F, op_set_perk_owed);
    // 0x8190 - int get_perk_available(int perk)
    interpreterRegisterOpcode(0x8190, op_get_perk_available);

    // 0x8191 - int get_critter_current_ap(object critter)
    interpreterRegisterOpcode(0x8191, op_get_critter_current_ap);
    // 0x8192 - void set_critter_current_ap(object critter, int ap)
    interpreterRegisterOpcode(0x8192, op_set_critter_current_ap);

    // 0x8193 - int  active_hand()
    interpreterRegisterOpcode(0x8193, op_active_hand);
    // 0x8194 - void toggle_active_hand()
    interpreterRegisterOpcode(0x8194, op_toggle_active_hand);

    // 0x8195 - void set_weapon_knockback(object weapon, int type, int/float value)
    interpreterRegisterOpcode(0x8195, op_set_weapon_knockback);
    // 0x8196 - void set_target_knockback(object critter, int type, int/float value)
    interpreterRegisterOpcode(0x8196, op_set_target_knockback);
    // 0x8197 - void set_attacker_knockback(object critter, int type, int/float value)
    interpreterRegisterOpcode(0x8197, op_set_attacker_knockback);
    // 0x8198 - void remove_weapon_knockback(object weapon)
    interpreterRegisterOpcode(0x8198, op_remove_weapon_knockback);
    // 0x8199 - void remove_target_knockback(object critter)
    interpreterRegisterOpcode(0x8199, op_remove_target_knockback);
    // 0x819a - void remove_attacker_knockback(object critter)
    interpreterRegisterOpcode(0x819A, op_remove_attacker_knockback);

    // 0x819d - void  set_sfall_global(string/int varname, int/float value)
    interpreterRegisterOpcode(0x819D, op_set_sfall_global);
    // 0x819e - int   get_sfall_global_int(string/int varname)
    interpreterRegisterOpcode(0x819E, op_get_sfall_global_int);
    // 0x819f - float get_sfall_global_float(string/int varname)
    interpreterRegisterOpcode(0x819f, op_get_sfall_global_float);
    // 0x822d - int   create_array(int element_count, int flags)
    interpreterRegisterOpcode(0x822D, op_create_array);
    // 0x822e - void  set_array(int array, any element, any value)
    interpreterRegisterOpcode(0x822E, op_set_array);
    // 0x822f - any   get_array(int array, any element)
    interpreterRegisterOpcode(0x822F, op_get_array);
    // 0x8230 - void  free_array(int array)
    interpreterRegisterOpcode(0x8230, op_free_array);
    // 0x8231 - int   len_array(int array)
    interpreterRegisterOpcode(0x8231, op_len_array);
    // 0x8232 - void  resize_array(int array, int new_element_count)
    interpreterRegisterOpcode(0x8232, op_resize_array);
    // 0x8233 - int   temp_array(int element_count, int flags)
    interpreterRegisterOpcode(0x8233, op_temp_array);
    // 0x8234 - void  fix_array(int array)
    interpreterRegisterOpcode(0x8234, op_fix_array);
    // 0x8239 - int   scan_array(int array, int/float var)
    interpreterRegisterOpcode(0x8239, op_scan_array);
    // 0x8254 - void  save_array(any key, int array)
    interpreterRegisterOpcode(0x8254, op_save_array);
    // 0x8255 - int   load_array(any key)
    interpreterRegisterOpcode(0x8255, op_load_array);
    // 0x8256 - int   array_key(int array, int index)
    interpreterRegisterOpcode(0x8256, op_get_array_key);
    // 0x8257 - int   arrayexpr(any key, any value)
    interpreterRegisterOpcode(0x8257, op_arrayexpr);

    // 0x81a0 - void set_pickpocket_max(int percentage)
    interpreterRegisterOpcode(0x81A0, op_set_pickpocket_max);
    // 0x81a1 - void set_hit_chance_max(int percentage)
    interpreterRegisterOpcode(0x81A1, op_set_hit_chance_max);
    // 0x81a2 - void set_skill_max(int value)
    interpreterRegisterOpcode(0x81A2, op_set_skill_max);
    // 0x81aa - void set_xp_mod(int percentage)
    interpreterRegisterOpcode(0x81AA, op_set_xp_mod);
    // 0x81ab - void set_perk_level_mod(int levels)
    interpreterRegisterOpcode(0x81AB, op_set_perk_level_mod);

    // 0x81c5 - void set_critter_hit_chance_mod(object, int max, int mod)
    interpreterRegisterOpcode(0x81C5, op_set_critter_hit_chance_mod);
    // 0x81c6 - void set_base_hit_chance_mod(int max, int mod)
    interpreterRegisterOpcode(0x81C6, op_set_base_hit_chance_mod);
    // 0x81c7 - void set_critter_skill_mod(object, int max)
    interpreterRegisterOpcode(0x81C7, op_set_critter_skill_mod);
    // 0x81c8 - void set_base_skill_mod(int max)
    interpreterRegisterOpcode(0x81C8, op_set_base_skill_mod);
    // 0x81c9 - void set_critter_pickpocket_mod(object, int max, int mod)
    interpreterRegisterOpcode(0x81C9, op_set_critter_pickpocket_mod);
    // 0x81ca - void set_base_pickpocket_mod(int max, int mod)
    interpreterRegisterOpcode(0x81CA, op_set_base_pickpocket_mod);

    // note: these are deprecated; do not implement
    // 0x81a3 - int  eax_available()
    // 0x81a4 - void set_eax_environment(int environment)

    // 0x81a5 - void inc_npc_level(int pid/string name)
    interpreterRegisterOpcode(0x81a5, op_inc_npc_level);
    // 0x8241 - int  get_npc_level(int pid/string name)
    interpreterRegisterOpcode(0x8241, op_get_npc_level);

    // 0x81a6 - int get_viewport_x()
    // 0x81a7 - int get_viewport_y()
    // 0x81a8 - void set_viewport_x(int view_x)
    // 0x81a9 - void set_viewport_y(int view_y)

    // 0x81ac - int   get_ini_setting(string setting)
    interpreterRegisterOpcode(0x81AC, op_get_ini_setting);
    // 0x81eb - string get_ini_string(string setting)
    interpreterRegisterOpcode(0x81EB, op_get_ini_string);

    // 0x81af - int get_game_mode()
    interpreterRegisterOpcode(0x81AF, op_get_game_mode);

    // 0x81b3 - int get_uptime()
    interpreterRegisterOpcode(0x81B3, op_get_uptime);

    // 0x81b6 - void set_car_current_town(int town)
    interpreterRegisterOpcode(0x81B6, op_set_car_current_town);

    // 0x81bb - void set_fake_perk(string name, int level, int image, string desc)
    interpreterRegisterOpcode(0x81BB, op_set_fake_perk);
    // 0x81bc - void set_fake_trait(string name, int active, int image, string desc)
    interpreterRegisterOpcode(0x81BC, op_set_fake_trait);
    // 0x81bd - void set_selectable_perk(string name, int active, int image, string desc)
    interpreterRegisterOpcode(0x81BD, op_set_selectable_perk);
    // 0x81be - void set_perkbox_title(string title)
    interpreterRegisterOpcode(0x81BE, op_set_perkbox_title);
    // 0x81bf - void hide_real_perks()
    interpreterRegisterOpcode(0x81BF, op_hide_real_perks);
    // 0x81c0 - void show_real_perks()
    interpreterRegisterOpcode(0x81C0, op_show_real_perks);
    // 0x81c1 - int has_fake_perk(string name/int extraPerkID)
    interpreterRegisterOpcode(0x81C1, op_has_fake_perk);
    // 0x81c2 - int has_fake_trait(string name)
    interpreterRegisterOpcode(0x81C2, op_has_fake_trait);
    // 0x81c3 - void perk_add_mode(int type)
    interpreterRegisterOpcode(0x81C3, op_perk_add_mode);
    // 0x81c4 - void clear_selectable_perks()
    interpreterRegisterOpcode(0x81C4, op_clear_selectable_perks);
    // 0x8225 - void remove_trait(int traitID)
    interpreterRegisterOpcode(0x8225, op_remove_trait);

    // 0x81cb - void set_pyromaniac_mod(int bonus)
    interpreterRegisterOpcode(0x81CB, op_set_pyromaniac_mod);
    // 0x81cc - void apply_heaveho_fix()
    interpreterRegisterOpcode(0x81CC, op_apply_heaveho_fix);
    // 0x81cd - void set_swiftlearner_mod(int bonus)
    interpreterRegisterOpcode(0x81CD, op_set_swiftlearner_mod);
    // 0x81ce - void set_hp_per_level_mod(int mod)
    interpreterRegisterOpcode(0x81CE, op_set_hp_per_level_mod);

    // 0x81dc - void show_iface_tag(int tag)
    interpreterRegisterOpcode(0x81DC, op_show_iface_tag);
    // 0x81dd - void hide_iface_tag(int tag)
    interpreterRegisterOpcode(0x81DD, op_hide_iface_tag);
    // 0x81de - int  is_iface_tag_active(int tag)
    interpreterRegisterOpcode(0x81DE, op_is_iface_tag_active);

    // 0x81df - int  get_bodypart_hit_modifier(int bodypart)
    interpreterRegisterOpcode(0x81DF, op_get_bodypart_hit_modifier);
    // 0x81e0 - void set_bodypart_hit_modifier(int bodypart, int value)
    interpreterRegisterOpcode(0x81E0, op_set_bodypart_hit_modifier);

    // 0x81e1 - void set_critical_table(int crittertype, int bodypart, int level, int valuetype, int value)
    interpreterRegisterOpcode(0x81E1, op_set_critical_table);
    // 0x81e2 - int  get_critical_table(int crittertype, int bodypart, int level, int valuetype)
    interpreterRegisterOpcode(0x81E2, op_get_critical_table);
    // 0x81e3 - void reset_critical_table(int crittertype, int bodypart, int level, int valuetype)
    interpreterRegisterOpcode(0x81E3, op_reset_critical_table);

    // 0x81e4 - int   get_sfall_arg()
    interpreterRegisterOpcode(0x81e4, op_get_sfall_arg);

    // 0x823c - array get_sfall_args()
    interpreterRegisterOpcode(0x823c, op_get_sfall_args);

    // 0x823d - void  set_sfall_arg(int argnum, int value)
    interpreterRegisterOpcode(0x823d, op_set_sfall_arg);

    // 0x81e5 - void  set_sfall_return(any value)
    interpreterRegisterOpcode(0x81e5, op_set_sfall_return);

    // 0x81ea - int   init_hook()  -> OBSOLETE, do not implement

    // 0x81e6 - void set_unspent_ap_bonus(int multiplier)
    interpreterRegisterOpcode(0x81E6, op_set_unspent_ap_bonus);
    // 0x81e7 - int  get_unspent_ap_bonus()
    interpreterRegisterOpcode(0x81E7, op_get_unspent_ap_bonus);
    // 0x81e8 - void set_unspent_ap_perk_bonus(int multiplier)
    interpreterRegisterOpcode(0x81E8, op_set_unspent_ap_perk_bonus);
    // 0x81e9 - int  get_unspent_ap_perk_bonus()
    interpreterRegisterOpcode(0x81E9, op_get_unspent_ap_perk_bonus);

    // 0x81ec - float sqrt(float)
    interpreterRegisterOpcode(0x81EC, op_sqrt);
    // 0x81ed - int/float abs(int/float)
    interpreterRegisterOpcode(0x81ED, op_abs);
    // 0x81ee - float sin(float)
    interpreterRegisterOpcode(0x81EE, op_sin);
    // 0x81ef - float cos(float)
    interpreterRegisterOpcode(0x81EF, op_cos);
    // 0x81f0 - float tan(float)
    interpreterRegisterOpcode(0x81F0, op_tan);
    // 0x81f1 - float arctan(float x, float y)
    interpreterRegisterOpcode(0x81F1, op_arctan);
    // 0x8263 - ^ operator (exponentiation)
    interpreterRegisterOpcode(0x8263, op_power);
    // 0x8264 - float log(float)
    interpreterRegisterOpcode(0x8264, op_log);
    // 0x8265 - float exponent(float)
    interpreterRegisterOpcode(0x8265, op_exponent);
    // 0x8266 - int ceil(float)
    interpreterRegisterOpcode(0x8266, op_ceil);
    // 0x8267 - int round(float)
    interpreterRegisterOpcode(0x8267, op_round);
    // 0x827f - div operator (signed integer division)
    interpreterRegisterOpcode(0x827F, op_div);

    // 0x81f2 - void set_palette(string path)

    // 0x81f3 - void remove_script(object)
    interpreterRegisterOpcode(0x81F3, op_remove_script);
    // 0x81f4 - void set_script(object, int scriptid)
    interpreterRegisterOpcode(0x81F4, op_set_script);
    // 0x81f5 - int get_script(object)
    interpreterRegisterOpcode(0x81F5, op_get_script);

    // 0x81f6 - int nb_create_char() // deprecated; do not implement

    // 0x81f7 - int   fs_create(string path, int size)
    interpreterRegisterOpcode(0x81f7, op_fs_create);
    // 0x81f8 - int   fs_copy(string path, string source)
    interpreterRegisterOpcode(0x81f8, op_fs_copy);
    // 0x81f9 - int   fs_find(string path)
    interpreterRegisterOpcode(0x81f9, op_fs_find);
    // 0x81fa - void  fs_write_byte(int id, int data)
    interpreterRegisterOpcode(0x81fa, op_fs_write_byte);
    // 0x81fb - void  fs_write_short(int id, int data)
    interpreterRegisterOpcode(0x81fb, op_fs_write_short);
    // 0x81fc - void  fs_write_int(int id, int data)
    interpreterRegisterOpcode(0x81fc, op_fs_write_int);
    // 0x81fd - void  fs_write_float(int id, int data)
    interpreterRegisterOpcode(0x81fd, op_fs_write_float);
    // 0x81fe - void  fs_write_string(int id, string data)
    interpreterRegisterOpcode(0x81fe, op_fs_write_string);
    // 0x8208 - void  fs_write_bstring(int id, string data)
    interpreterRegisterOpcode(0x8208, op_fs_write_bstring);
    // 0x8209 - int   fs_read_byte(int id)
    interpreterRegisterOpcode(0x8209, op_fs_read_byte);
    // 0x820a - int   fs_read_short(int id)
    interpreterRegisterOpcode(0x820a, op_fs_read_short);
    // 0x820b - int   fs_read_int(int id)
    interpreterRegisterOpcode(0x820b, op_fs_read_int);
    // 0x820c - float fs_read_float(int id)
    interpreterRegisterOpcode(0x820c, op_fs_read_float);
    // 0x81ff - void  fs_delete(int id)
    interpreterRegisterOpcode(0x81ff, op_fs_delete);
    // 0x8200 - int   fs_size(int id)
    interpreterRegisterOpcode(0x8200, op_fs_size);
    // 0x8201 - int   fs_pos(int id)
    interpreterRegisterOpcode(0x8201, op_fs_pos);
    // 0x8202 - void  fs_seek(int id, int pos)
    interpreterRegisterOpcode(0x8202, op_fs_seek);
    // 0x8203 - void  fs_resize(int id, int size)
    interpreterRegisterOpcode(0x8203, op_fs_resize);

    // 0x8204 - int  get_proto_data(int pid, int offset)
    interpreterRegisterOpcode(0x8204, op_get_proto_data);
    // 0x8205 - void set_proto_data(int pid, int offset, int value)
    interpreterRegisterOpcode(0x8205, op_set_proto_data);

    // 0x8206 - void set_self(object)
    interpreterRegisterOpcode(0x8206, op_set_self);
    // 0x8207 - void register_hook(int hook)
    interpreterRegisterOpcode(0x8207, op_register_hook);

    // 0x820d - int   list_begin(int type)
    interpreterRegisterOpcode(0x820D, op_list_begin);
    // 0x820e - int   list_next(int listid)
    interpreterRegisterOpcode(0x820E, op_list_next);
    // 0x820f - void  list_end(int listid)
    interpreterRegisterOpcode(0x820F, op_list_end);
    // 0x8236 - array list_as_array(int type)
    interpreterRegisterOpcode(0x8236, op_list_as_array);

    // 0x8210 - int sfall_ver_major()
    interpreterRegisterOpcode(0x8210, op_get_version_major);
    // 0x8211 - int sfall_ver_minor()
    interpreterRegisterOpcode(0x8211, op_get_version_minor);
    // 0x8212 - int sfall_ver_build()
    interpreterRegisterOpcode(0x8212, op_get_version_patch);

    // 0x8213 - void hero_select_win(int)
    interpreterRegisterOpcode(0x8213, op_hero_select_win);
    // 0x8214 - void set_hero_race(int race)
    interpreterRegisterOpcode(0x8214, op_set_hero_race);
    // 0x8215 - void set_hero_style(int style)
    interpreterRegisterOpcode(0x8215, op_set_hero_style);

    // 0x8216 - void set_critter_burst_disable(object critter, int disable)
    interpreterRegisterOpcode(0x8216, op_set_critter_burst_disable);

    // 0x8217 - int  get_weapon_ammo_pid(object weapon)
    interpreterRegisterOpcode(0x8217, op_get_weapon_ammo_pid);
    // 0x8218 - void set_weapon_ammo_pid(object weapon, int pid)
    interpreterRegisterOpcode(0x8218, op_set_weapon_ammo_pid);
    // 0x8219 - int  get_weapon_ammo_count(object weapon)
    interpreterRegisterOpcode(0x8219, op_get_weapon_ammo_count);
    // 0x821a - void set_weapon_ammo_count(object weapon, int count)
    interpreterRegisterOpcode(0x821A, op_set_weapon_ammo_count);

    // 0x8220 - int get_screen_width()
    interpreterRegisterOpcode(0x8220, op_get_screen_width);
    // 0x8221 - int get_screen_height()
    interpreterRegisterOpcode(0x8221, op_get_screen_height);

    // 0x8222 - void stop_game()
    // 0x8223 - void resume_game()
    // 0x8224 - void create_message_window(string message)
    interpreterRegisterOpcode(0x8224, op_create_message_window);

    // 0x8226 - int get_light_level()
    interpreterRegisterOpcode(0x8226, op_get_light_level);

    // 0x8227 - void refresh_pc_art()
    interpreterRegisterOpcode(0x8227, op_refresh_pc_art);

    // 0x8228 - int get_attack_type()
    interpreterRegisterOpcode(0x8228, op_get_attack_type);

    // 0x822b - int  play_sfall_sound(string file, int mode)
    interpreterRegisterOpcode(0x822B, op_play_sfall_sound);
    // 0x822c - void stop_sfall_sound(int soundID)
    interpreterRegisterOpcode(0x822C, op_stop_sfall_sound);

    // 0x8235 - array string_split(string string, string split)
    interpreterRegisterOpcode(0x8235, op_string_split);
    // 0x8237 - int   atoi(string string)
    interpreterRegisterOpcode(0x8237, op_parse_int);
    // 0x8238 - float atof(string string)
    interpreterRegisterOpcode(0x8238, op_atof);
    // 0x824e - string substr(string string, int start, int length)
    interpreterRegisterOpcode(0x824E, op_substr);
    // 0x824f - int   strlen(string string)
    interpreterRegisterOpcode(0x824F, op_get_string_length);
    // 0x8250 - string sprintf(string format, any value)
    interpreterRegisterOpcode(0x8250, op_sprintf);
    // 0x8251 - int   charcode(string string)
    interpreterRegisterOpcode(0x8251, op_charcode);
    // 0x8253 - int   typeof(any value)
    interpreterRegisterOpcode(0x8253, op_type_of);

    // 0x823a - int get_tile_fid(int tileData)
    interpreterRegisterOpcode(0x823A, op_get_tile_fid);

    // 0x823b - int modified_ini() // deprecated: do not implement

    // 0x823e - void force_aimed_shots(int pid)
    // 0x823f - void disable_aimed_shots(int pid)

    // 0x8240 - void mark_movie_played(int id)

    // 0x8248 - object get_last_target(object critter)
    interpreterRegisterOpcode(0x8248, op_get_last_target);
    // 0x8249 - object get_last_attacker(object critter)
    interpreterRegisterOpcode(0x8249, op_get_last_attacker);
    // 0x824a - void block_combat(int enable)

    // 0x824b - int tile_under_cursor()
    interpreterRegisterOpcode(0x824B, op_tile_under_cursor);
    // 0x824c - int gdialog_get_barter_mod()
    interpreterRegisterOpcode(0x824C, op_gdialog_get_barter_mod);
    // 0x824d - void set_inven_ap_cost(int cost)
    interpreterRegisterOpcode(0x824D, op_set_inven_ap_cost);

    // 0x825a - void reg_anim_destroy(object object)
    interpreterRegisterOpcode(0x825A, op_reg_anim_destroy);
    // 0x825b - void reg_anim_animate_and_hide(object object, int animID, int delay)
    interpreterRegisterOpcode(0x825B, op_reg_anim_animate_and_hide);
    // 0x825c - void reg_anim_combat_check(int enable)
    interpreterRegisterOpcode(0x825C, op_reg_anim_combat_check);
    // 0x825d - void reg_anim_light(object object, int radius, int delay)
    interpreterRegisterOpcode(0x825D, op_reg_anim_light);
    // 0x825e - void reg_anim_change_fid(object object, int FID, int delay)
    interpreterRegisterOpcode(0x825E, op_reg_anim_change_fid);
    // 0x825f - void reg_anim_take_out(object object, int holdFrameID, int delay)
    interpreterRegisterOpcode(0x825F, op_reg_anim_take_out);
    // 0x8260 - void reg_anim_turn_towards(object object, int tile/targetObj, int delay)
    interpreterRegisterOpcode(0x8260, op_reg_anim_turn_towards);

    // 0x8261 - int metarule2_explosions(object object)
    interpreterRegisterOpcode(0x8261, op_explosions_metarule);

    // 0x8262 - void register_hook_proc(int hook, procedure proc)
    interpreterRegisterOpcode(0x8262, op_register_hook_proc);

    // 0x826b - string message_str_game(int fileId, int messageId)
    interpreterRegisterOpcode(0x826B, op_get_message);
    // 0x826c - int sneak_success()
    interpreterRegisterOpcode(0x826C, op_sneak_success);
    // 0x826d - int tile_light(int elevation, int tileNum)
    interpreterRegisterOpcode(0x826D, op_tile_light);
    // 0x826e - object obj_blocking_line(object objFrom, int tileTo, int blockingType)
    interpreterRegisterOpcode(0x826E, op_make_straight_path);
    // 0x826f - object obj_blocking_tile(int tileNum, int elevation, int blockingType)
    interpreterRegisterOpcode(0x826F, op_obj_blocking_at);
    // 0x8270 - array tile_get_objs(int tileNum, int elevation)
    interpreterRegisterOpcode(0x8270, op_tile_get_objects);
    // 0x8271 - array party_member_list(int includeHidden)
    interpreterRegisterOpcode(0x8271, op_party_member_list);
    // 0x8272 - array path_find_to(object objFrom, int tileTo, int blockingType)
    interpreterRegisterOpcode(0x8272, op_make_path);
    // 0x8273 - object create_spatial(int scriptID, int tile, int elevation, int radius)
    interpreterRegisterOpcode(0x8273, op_create_spatial);
    // 0x8274 - int art_exists(int artFID)
    interpreterRegisterOpcode(0x8274, op_art_exists);
    // 0x8275 - int obj_is_carrying_obj(object invenObj, object itemObj)
    interpreterRegisterOpcode(0x8275, op_obj_is_carrying_obj);

    // 0x8276 - any sfall_func0(string funcName)
    interpreterRegisterOpcode(0x8276, op_sfall_func0);
    // 0x8277 - any sfall_func1(string funcName, arg1)
    interpreterRegisterOpcode(0x8277, op_sfall_func1);
    // 0x8278 - any sfall_func2(string funcName, arg1, arg2)
    interpreterRegisterOpcode(0x8278, op_sfall_func2);
    // 0x8279 - any sfall_func3(string funcName, arg1, arg2, arg3)
    interpreterRegisterOpcode(0x8279, op_sfall_func3);
    // 0x827a - any sfall_func4(string funcName, arg1, arg2, arg3, arg4)
    interpreterRegisterOpcode(0x827A, op_sfall_func4);
    // 0x827b - any sfall_func5(string funcName, arg1, arg2, arg3, arg4, arg5)
    interpreterRegisterOpcode(0x827B, op_sfall_func5);
    // 0x827c - any sfall_func6(string funcName, arg1, arg2, arg3, arg4, arg5, arg6)
    interpreterRegisterOpcode(0x827C, op_sfall_func6);
    // 0x8280 - any sfall_func7(string funcName, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
    interpreterRegisterOpcode(0x8280, op_sfall_func7);
    // 0x8281 - any sfall_func8(string funcName, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
    interpreterRegisterOpcode(0x8281, op_sfall_func8);

    // 0x827d - void register_hook_proc_spec(int hook, procedure proc)
    interpreterRegisterOpcode(0x827d, op_register_hook_proc_spec);
    // 0x827e - void reg_anim_callback(procedure proc)
    interpreterRegisterOpcode(0x827e, op_reg_anim_callback);
}

void sfallOpcodesExit()
{
    sfallAnimCallbackReset();
    sfallVfsCloseAll();
}

} // namespace fallout
