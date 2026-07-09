#include "sfall_metarules.h"

#include <algorithm>
#include <map>
#include <math.h>
#include <memory>
#include <set>
#include <string.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "art.h"
#include "automap.h"
#include "character_editor.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "config.h" // For Config, configInit, configFree
#include "db.h" // For File*, fileWriteInt32, fileReadInt32, etc.
#include "dbox.h"
#include "debug.h"
#include "game.h"
#include "game_dialog.h"
#include "game_mouse.h"
#include "interface.h"
#include "interpreter.h"
#include "inventory.h"
#include "item.h"
#include "mainmenu.h"
#include "memory.h"
#include "message.h"
#include "object.h"
#include "opcode_context.h"
#include "options.h"
#include "pipboy.h"
#include "platform_compat.h"
#include "proto_instance.h"
#include "queue.h"
#include "scripts.h"
#include "sfall_animation.h"
#include "sfall_arrays.h" // For CreateTempArray, SetArray
#include "sfall_global_scripts.h"
#include "sfall_global_vars.h"
#include "sfall_ini.h"
#include "sfall_opcodes.h"
#include "sfall_script_hooks.h"
#include "skilldex.h"
#include "stat.h"
#include "text_font.h"
#include "tile.h"
#include "trait.h"
#include "window.h"
#include "window_manager.h"
#include "worldmap.h"

#include <assert.h>

namespace fallout {

static void mf_attack_is_aimed(OpcodeContext& ctx);
static void mf_car_gas_amount(OpcodeContext& ctx);
static void mf_combat_data(OpcodeContext& ctx);
static void mf_create_win(OpcodeContext& ctx);
static void mf_critter_inven_obj2(OpcodeContext& ctx);
static void mf_dialog_obj(OpcodeContext& ctx);
static void mf_dialog_message(OpcodeContext& ctx);
static void mf_display_stats(OpcodeContext& ctx);
static void mf_draw_image(OpcodeContext& ctx);
static void mf_draw_image_scaled(OpcodeContext& ctx);
static void mf_get_combat_free_move(OpcodeContext& ctx);
static void mf_get_cursor_mode(OpcodeContext& ctx);
static void mf_get_flags(OpcodeContext& ctx);
static void mf_get_inven_ap_cost(OpcodeContext& ctx);
static void mf_get_object_data(OpcodeContext& ctx);
static void mf_get_outline(OpcodeContext& ctx);
static void mf_get_sfall_arg_at(OpcodeContext& ctx);
static void mf_get_text_width(OpcodeContext& ctx);
static void mf_get_window_attribute(OpcodeContext& ctx);
// F-08: FO1 water chip timer metarules.
static void mf_get_water_days_left(OpcodeContext& ctx);
static void mf_get_water_days_left_x(OpcodeContext& ctx);
static void mf_hide_window(OpcodeContext& ctx);
static void mf_interface_art_draw(OpcodeContext& ctx);
static void mf_intface_redraw(OpcodeContext& ctx);
static void mf_inventory_redraw(OpcodeContext& ctx);
static void mf_item_weight(OpcodeContext& ctx);
static void mf_loot_obj(OpcodeContext& ctx);
static void mf_message_box(OpcodeContext& ctx);
static void mf_add_extra_msg_file(OpcodeContext& ctx);
static void mf_add_iface_tag(OpcodeContext& ctx);
static void mf_art_frame_data(OpcodeContext& ctx);
static void mf_art_cache_flush(OpcodeContext& ctx);
static void mf_metarule_exist(OpcodeContext& ctx);
static void mf_obj_is_openable(OpcodeContext& ctx);
static void mf_obj_under_cursor(OpcodeContext& ctx);
static void mf_objects_in_radius(OpcodeContext& ctx);
static void mf_opcode_exists(OpcodeContext& ctx);
static void mf_outlined_object(OpcodeContext& ctx);
static void mf_set_combat_free_move(OpcodeContext& ctx);
static void mf_set_cursor_mode(OpcodeContext& ctx);
static void mf_set_flags(OpcodeContext& ctx);
static void mf_set_iface_tag_text(OpcodeContext& ctx);
static void mf_set_outline(OpcodeContext& ctx);
static void mf_set_window_flag(OpcodeContext& ctx);
static void mf_set_unique_id(OpcodeContext& ctx);
static void mf_show_window(OpcodeContext& ctx);
static void mf_signal_close_game(OpcodeContext& ctx);
static void mf_tile_by_position(OpcodeContext& ctx);
static void mf_tile_refresh_display(OpcodeContext& ctx);
static void mf_unwield_slot(OpcodeContext& ctx);
static void mf_win_fill_color(OpcodeContext& ctx);
static void mf_string_compare(OpcodeContext& ctx);
static void mf_string_find(OpcodeContext& ctx);
static void mf_string_to_case(OpcodeContext& ctx);
void mf_string_format(OpcodeContext& ctx);
static void mf_string_format_array(OpcodeContext& ctx);
static void mf_string_replace(OpcodeContext& ctx);
static void mf_floor2(OpcodeContext& ctx);
// Rotators fork compatibility wrappers (C-06)
static void mf_r_get_ini_string(OpcodeContext& ctx);
static void mf_r_message_box(OpcodeContext& ctx);
// New metarule handlers (C-14, H-02/H-03, H-05 through H-09)
static void mf_npc_engine_level_up(OpcodeContext& ctx);
static void mf_set_dude_obj(OpcodeContext& ctx);
static void mf_real_dude_obj(OpcodeContext& ctx);
static void mf_set_object_data(OpcodeContext& ctx);
static void mf_set_terrain_name(OpcodeContext& ctx);
static void mf_get_terrain_name(OpcodeContext& ctx);
static void mf_set_worldmap_heal_time(OpcodeContext& ctx);
static void mf_set_rest_heal_time(OpcodeContext& ctx);
static void mf_set_quest_failure_value(OpcodeContext& ctx);
static void mf_set_scr_name(OpcodeContext& ctx);
static void mf_has_fake_perk_npc(OpcodeContext& ctx);
static void mf_has_fake_trait_npc(OpcodeContext& ctx);
static void mf_set_fake_perk_npc(OpcodeContext& ctx);
static void mf_set_fake_trait_npc(OpcodeContext& ctx);
static void mf_set_selectable_perk_npc(OpcodeContext& ctx);
static void mf_get_fake_perk_npc(OpcodeContext& ctx);
static void mf_get_fake_trait_npc(OpcodeContext& ctx);
static void mf_get_selectable_perk_npc(OpcodeContext& ctx);
static void mf_has_selectable_perk_npc(OpcodeContext& ctx);
// F-002: 14 HIGH-priority metarules (needed by RPU/ET Tu) — commented out, now implemented
static void mf_exec_map_update_scripts(OpcodeContext& ctx);
static void mf_get_can_rest_on_map(OpcodeContext& ctx);
static void mf_get_current_inven_size(OpcodeContext& ctx);
static void mf_get_map_enter_position(OpcodeContext& ctx);
static void mf_get_metarule_table(OpcodeContext& ctx);
static void mf_get_object_ai_data(OpcodeContext& ctx);
static void mf_get_quest_failure_value(OpcodeContext& ctx);
static void mf_get_stat_max(OpcodeContext& ctx);
static void mf_get_stat_min(OpcodeContext& ctx);
static void mf_item_make_explosive(OpcodeContext& ctx);
static void mf_get_explosive_data(OpcodeContext& ctx);
static void mf_lock_is_jammed(OpcodeContext& ctx);
static void mf_remove_timer_event(OpcodeContext& ctx);
static void mf_set_can_rest_on_map(OpcodeContext& ctx);
static void mf_set_rest_mode(OpcodeContext& ctx);
static void mf_spatial_radius(OpcodeContext& ctx);
// F-003: UI metarules — interface bar hide/show/is_hidden
static void mf_intface_hide(OpcodeContext& ctx);
static void mf_intface_is_hidden(OpcodeContext& ctx);
static void mf_intface_show(OpcodeContext& ctx);
// F-014/F-015: interface overlay and print
static void mf_interface_overlay(OpcodeContext& ctx);
static void mf_interface_print(OpcodeContext& ctx);
// F-021: r_write Rotators fork compatibility (runtime memory write)
static void mf_r_write(OpcodeContext& ctx);
// F-033: MEDIUM-priority metarules — remaining commented-out entries
static void mf_add_g_timer_event(OpcodeContext& ctx);
static void mf_add_trait(OpcodeContext& ctx);
static void mf_remove_trait(OpcodeContext& ctx);
static void mf_set_spray_settings(OpcodeContext& ctx);
static void mf_set_car_intface_art(OpcodeContext& ctx);
static void mf_set_drugs_data(OpcodeContext& ctx);
static void mf_set_map_enter_position(OpcodeContext& ctx);
static void mf_set_town_title(OpcodeContext& ctx);
static void mf_get_town_title(OpcodeContext& ctx);
static void mf_set_unjam_locks_time(OpcodeContext& ctx);
static void mf_get_unjam_locks_time(OpcodeContext& ctx);
static void mf_unjam_lock(OpcodeContext& ctx);
// F2-24: Explosion metarule wrapper — dispatches sub-metarule operations
// that are currently only reachable via opcode 0x8261.
static void mf_explosions_metarule(OpcodeContext& ctx);
// F-9 (FIX): talking_head_mood — FO1 mood override for dialog heads.
// Called by VOODOO_talking_head_mood macro in Et Tu's gl_fo1mechanics.ssl.
// Stores a mood index (-1=reset/use engine default, 0=neutral, 1=good)
// that overrides the engine-calculated dialog reaction for talking head
// animation selection in game_dialog.cc _gdSetupFidget.
static void mf_talking_head_mood(OpcodeContext& ctx);

// Tracks nesting depth of mf_message_box calls.
// Must be reset across save/load via sfall_metarules_reset() to prevent
// permanently disabling scripts when a save occurs during a dialog.
static int sfall_metarules_dialogShowCount = 0;

// --- Static state for new metarules ---

// npc_engine_level_up: 0=disable auto-leveling, 1=enable (default)
static int gNpcEngineLevelUpEnabled = 1;

// set_dude_obj/real_dude_obj: saved dude pointer during cutscene swaps
static Object* gSavedOriginalDude = nullptr;

// CID of the saved original dude for save/load persistence.
// gSavedOriginalDude is an Object* that is not stable across save/load;
// gSavedOriginalDudeCid preserves the identity so the save/load agent
// can restore the pointer via objectFindByCid() after loading.
static int gSavedOriginalDudeCid = -1;

// set_quest_failure_value: GVAR number -> failure threshold
static std::map<int, int> gQuestFailureValues;

// set_scr_name: override script display name (empty = no override)
static std::string gScriptNameOverride;

// set_worldmap_heal_time / set_rest_heal_time: override healing rates (-1 = use default)
static int gWorldmapHealTime = -1;
static int gRestHealTime = -1;

// set_terrain_name / get_terrain_name: worldmap coordinate -> terrain name override
static std::map<std::pair<int, int>, std::string> gTerrainNameOverrides;

// set_town_title: town ID -> display name override (mirrors gTerrainNameOverrides pattern)
static std::map<int, std::string> gTownTitleOverrides;

// set_car_intface_art: override FID for car trunk interface art (-1 = use default)
static int gCarIntfaceArtFid = -1;

// set_rest_mode: rest behavior mode (-1 = use default, 0 = disabled, 1 = strict, 2 = no healing)
static int gRestMode = -1;

// F-9 (FIX): talking_head_mood metarule override.
// -1 = no override (use engine-calculated reaction), 0-2 = forced mood index.
// Consumer: game_dialog.cc _gdSetupFidget — check this before computing
// the reaction-based mood animation FID. The FO1 dialog system uses different
// mood thresholds than FO2; this metarule allows scripts to force a specific
// mood regardless of engine calculation.
static int gTalkingHeadMood = -1;

// Fake perk/trait storage for NPC critters.
// Key: CID (int, stable across save/load), Value: map of perk/trait name → metadata.
// Each entry stores level, image, and description alongside the name.
// Keys are Object::cid — the unique per-object identifier serialized in save games.
static std::unordered_map<int, std::unordered_map<std::string, FakePerkNpcEntry>> gFakePerksNpc;
static std::unordered_map<int, std::unordered_map<std::string, FakePerkNpcEntry>> gFakeTraitsNpc;
static std::unordered_map<int, std::unordered_map<std::string, FakePerkNpcEntry>> gFakeSelectablePerksNpc;

// Traits added via the add_trait / remove_trait metarules.
// Stores trait type ID -> rank for the player character.
// Rank defaults to 0 when not specified (1-arg form).
static std::map<int, int> gAddedTraits;

// Map enter position override (set by set_map_enter_position, read by get_map_enter_position).
// -1 = no override stored.
static int gMapEnterX = -1;
static int gMapEnterY = -1;
static int gMapEnterElevation = -1;

// Unjam locks time override (set by set_unjam_locks_time metarule).
// Stores the number of game hours before jammed locks auto-unjam.
// NOTE: The map-entry path (map.cc:1184-1193) correctly applies time-based
// gating via this override. The midnight-event path (scripts.cc:438) still
// calls objectUnjamAll() unconditionally, bypassing this override.
// This global stores the configured value for script queries and both paths.
static int gUnjamLocksTimeHours = -1;

// Explosive properties set via item_make_explosive metarule.
// Key: proto PID, Value: {pattern, radius, delay, minDamage, maxDamage}
struct ExplosiveProperties {
    int pattern;
    int radius;
    int delay;
    int minDamage;
    int maxDamage;
};
static std::map<int, ExplosiveProperties> gExplosiveOverrides;

// Stored spray settings for set_spray_settings metarule.
// Parameters configure burst fire spray pattern: flags, proto PID filter,
// burst radius (in hexes), and count of bullets per burst.
// Consumer: combat burst system (src/combat.cc _compute_spray).
// Struct definition in sfall_metarules.h.
static SpraySettings gSpraySettings;

// F-08: FO1 Vault 13 water chip timer state.
// When gFallout1Behavior is true and scripts enable the water timer,
// this stores the initial water chip deadline (150 days). The timer
// counts down from game start — get_water_days_left returns remaining
// days. gSfallWaterTimerDays defaults to 150 (FO1 standard).
// 0 = disabled/no timer (FO2 mode).
extern bool gFallout1Behavior;
static bool gSfallWaterTimerEnabled = false;
static int gSfallWaterTimerDays = 150;

// Maximum number of pending timer events allowed.
#define MAX_TIMER_EVENTS 256

// Stored timer events for add_g_timer_event metarule.
// Each entry records an opcode and delay that a script requested to fire
// after the specified number of ticks. timerId is an incrementing counter
// used for removal by ID. Engine persistence is handled via scriptAddTimerEvent
// in scripts.cc — gPendingTimerEvents is NOT serialized to avoid double-persistence.
struct PendingTimerEvent {
    int opcode;
    int delay;
    int timerId;
};
static std::vector<PendingTimerEvent> gPendingTimerEvents;
static int gNextTimerId = 1;

// Stored drug data overrides for set_drugs_data metarule.
// Maps drug index to {addictionRate, effectDuration}.
// Consumer: drug/chem system (src/item.cc, src/stat.cc drug processing).
// Struct definition in sfall_metarules.h.
static std::map<int, DrugData> gDrugDataOverrides;

// Stored interface overlay state for interface_overlay metarule.
// Tracks the active overlay's window type and creation parameters.
// Consumer: overlay rendering system (TODO: overlay window creation pipeline).
struct InterfaceOverlayState {
    int winType = -1;
    int arg1 = 0;
    int arg2 = 0;
    int arg3 = 0;
    int arg4 = 0;
    int windowHandle = -1;
    bool active = false;
};
// F-074 + F2-031: Non-static so interface.cc can access via extern for
// overlay rendering and stale-window recreation after save/load.
InterfaceOverlayState gInterfaceOverlayState;

// F-003: intface hidden state tracker. gInterfaceBarHidden is file-static in
// interface.cc, so we maintain our own mirror for tracking state set via
// sfall metarules. mf_intface_is_hidden() queries the engine directly
// via interfaceBarIsHidden() to avoid stale-mirror desynchronization.
static bool sIntfaceHiddenState = false;

// --- End static state ---

// ref. https://github.com/sfall-team/sfall/blob/42556141127895c27476cd5242a73739cbb0fade/sfall/Modules/Scripting/Handlers/Metarule.cpp#L72

// TODO: reduce duplication further once this context is shared with opcode handlers too.
const MetaruleInfo kMetarules[] = {
    { "add_extra_msg_file", mf_add_extra_msg_file, 1, 2, -1, { ARG_STRING, ARG_INT } },
    { "add_iface_tag", mf_add_iface_tag, 0, 0 },
    { "add_g_timer_event", mf_add_g_timer_event, 2, 2, -1, { ARG_INT, ARG_INT } },
    { "add_trait", mf_add_trait, 1, 3, -1, { ARG_ANY, ARG_INT, ARG_INT } },
    { "remove_trait", mf_remove_trait, 1, 1, -1, { ARG_INT } },
    { "art_cache_clear", mf_art_cache_flush, 0, 0 },
    { "art_frame_data", mf_art_frame_data, 1, 3, 0, { ARG_INTSTR, ARG_INT, ARG_INT } },
    { "attack_is_aimed", mf_attack_is_aimed, 0, 0 },
    { "car_gas_amount", mf_car_gas_amount, 0, 0 },
    { "combat_data", mf_combat_data, 0, 0 },
    { "create_win", mf_create_win, 5, 6, -1, { ARG_STRING, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT } },
    { "critter_inven_obj2", mf_critter_inven_obj2, 2, 2, 0, { ARG_OBJECT, ARG_INT } },
    { "dialog_message", mf_dialog_message, 1, 1, -1, { ARG_STRING } },
    { "dialog_obj", mf_dialog_obj, 0, 0 },
    { "display_stats", mf_display_stats, 0, 0 }, // refresh
    { "draw_image", mf_draw_image, 1, 5, -1, { ARG_INTSTR, ARG_INT, ARG_INT, ARG_INT, ARG_INT } },
    { "draw_image_scaled", mf_draw_image_scaled, 1, 6, -1, { ARG_INTSTR, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT } },
    { "exec_map_update_scripts", mf_exec_map_update_scripts, 0, 0 },
    { "floor2", mf_floor2, 1, 1, 0, { ARG_NUMBER } },
    { "get_can_rest_on_map", mf_get_can_rest_on_map, 2, 2, -1, { ARG_INT, ARG_INT } },
    { "get_combat_free_move", mf_get_combat_free_move, 0, 0 },
    { "get_current_inven_size", mf_get_current_inven_size, 1, 1, 0, { ARG_OBJECT } },
    { "get_cursor_mode", mf_get_cursor_mode, 0, 0 },
    { "get_explosive_data", mf_get_explosive_data, 1, 1, 0, { ARG_INT } },
    { "get_flags", mf_get_flags, 1, 1, 0, { ARG_OBJECT } },
    { "get_ini_config", mf_get_ini_config, 2, 2, 0, { ARG_STRING, ARG_INT } },
    { "get_ini_section", mf_get_ini_section, 2, 2, -1, { ARG_STRING, ARG_STRING } },
    { "get_ini_sections", mf_get_ini_sections, 1, 1, -1, { ARG_STRING } },
    { "get_inven_ap_cost", mf_get_inven_ap_cost, 0, 0 },
    { "get_map_enter_position", mf_get_map_enter_position, 0, 0 },
    { "get_metarule_table", mf_get_metarule_table, 0, 0 },
    { "get_object_ai_data", mf_get_object_ai_data, 2, 2, -1, { ARG_OBJECT, ARG_INT } },
    { "get_object_data", mf_get_object_data, 2, 2, 0, { ARG_OBJECT, ARG_INT } },
    { "get_outline", mf_get_outline, 1, 1, 0, { ARG_OBJECT } },
    { "get_quest_failure_value", mf_get_quest_failure_value, 1, 1, 0, { ARG_INT } },
    { "get_sfall_arg_at", mf_get_sfall_arg_at, 1, 1, 0, { ARG_INT } },
    { "get_stat_max", mf_get_stat_max, 1, 2, 0, { ARG_INT, ARG_INT } },
    { "get_stat_min", mf_get_stat_min, 1, 2, 0, { ARG_INT, ARG_INT } },
    // {"get_string_pointer",        mf_get_string_pointer,        1, 1,  0, {ARG_STRING}}, // note: deprecated; do not implement
    { "get_terrain_name", mf_get_terrain_name, 0, 2, -1, { ARG_INT, ARG_INT } },
    { "get_text_width", mf_get_text_width, 1, 1, 0, { ARG_STRING } },
    { "get_town_title", mf_get_town_title, 1, 1, 0, { ARG_INT } },
    { "get_unjam_locks_time", mf_get_unjam_locks_time, 0, 0 },
    { "get_window_attribute", mf_get_window_attribute, 1, 2, -1, { ARG_INT, ARG_INT } },
    // F-08: FO1 Vault 13 water chip timer. Returns remaining water days.
    // get_water_days_left: 0 args, returns remaining days at current game time.
    // get_water_days_left_x: 1 arg (gameTime ticks), returns remaining days at given time.
    { "get_water_days_left", mf_get_water_days_left, 0, 0 },
    { "get_water_days_left_x", mf_get_water_days_left_x, 1, 1, 0, { ARG_INT } },
    { "has_fake_perk_npc", mf_has_fake_perk_npc, 2, 2, 0, { ARG_OBJECT, ARG_STRING } },
    { "has_fake_trait_npc", mf_has_fake_trait_npc, 2, 2, 0, { ARG_OBJECT, ARG_STRING } },
    { "get_fake_perk_npc", mf_get_fake_perk_npc, 2, 2, 0, { ARG_OBJECT, ARG_STRING } },
    { "get_fake_trait_npc", mf_get_fake_trait_npc, 2, 2, 0, { ARG_OBJECT, ARG_STRING } },
    { "get_selectable_perk_npc", mf_get_selectable_perk_npc, 2, 2, 0, { ARG_OBJECT, ARG_STRING } },
    { "has_selectable_perk_npc", mf_has_selectable_perk_npc, 2, 2, 0, { ARG_OBJECT, ARG_STRING } },
    { "hide_window", mf_hide_window, 0, 1, -1, { ARG_STRING } },
    { "interface_art_draw", mf_interface_art_draw, 4, 6, -1, { ARG_INT, ARG_INTSTR, ARG_INT, ARG_INT, ARG_INT, ARG_INT } },
    { "interface_overlay", mf_interface_overlay, 2, 6, -1, { ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT } },
    { "interface_print", mf_interface_print, 5, 6, -1, { ARG_STRING, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT } },
    { "intface_hide", mf_intface_hide, 0, 0 },
    { "intface_is_hidden", mf_intface_is_hidden, 0, 0 },
    { "intface_redraw", mf_intface_redraw, 0, 1, -1, { ARG_INT } },
    { "intface_show", mf_intface_show, 0, 0 },
    { "inventory_redraw", mf_inventory_redraw, 0, 1, -1, { ARG_INT } },
    { "item_make_explosive", mf_item_make_explosive, 3, 6, -1, { ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT } },
    { "item_weight", mf_item_weight, 1, 1, 0, { ARG_OBJECT } },
    { "lock_is_jammed", mf_lock_is_jammed, 1, 1, 0, { ARG_OBJECT } },
    { "loot_obj", mf_loot_obj, 0, 0 },
    { "message_box", mf_message_box, 1, 4, -1, { ARG_STRING, ARG_INT, ARG_INT, ARG_INT } },
    { "metarule_exist", mf_metarule_exist, 1, 1, 0, { ARG_STRING } },
    { "npc_engine_level_up", mf_npc_engine_level_up, 1, 1, -1, { ARG_INT } },
    { "obj_is_openable", mf_obj_is_openable, 1, 1, 0, { ARG_OBJECT } },
    { "obj_under_cursor", mf_obj_under_cursor, 2, 2, 0, { ARG_INT, ARG_INT } },
    { "objects_in_radius", mf_objects_in_radius, 3, 4, 0, { ARG_INT, ARG_INT, ARG_INT, ARG_INT } },
    { "outlined_object", mf_outlined_object, 0, 0 },
    // Rotators fork compatibility wrappers (C-06):
    // r_get_ini_string wraps get_ini_setting/get_ini_string with default-value fallback.
    // r_call_offset is intentionally NOT registered — it requires VOODOO memory
    // patching (0x81D2-0x81DB opcodes) which CE does not support.
    { "r_get_ini_string", mf_r_get_ini_string, 4, 4, -1, { ARG_STRING, ARG_STRING, ARG_STRING, ARG_INTSTR } },
    { "r_message_box", mf_r_message_box, 1, 4, -1, { ARG_STRING, ARG_INT, ARG_INT, ARG_INT } },
    // r_write(type, addr, val): Rotators fork runtime memory write.
    // type: 0=byte, 1=short, 2=int, 3=string. CE cannot dereference arbitrary
    // engine addresses; the handler logs the call as a no-op.
    { "r_write", mf_r_write, 3, 3, -1, { ARG_INT, ARG_INT, ARG_INTSTR } },
    { "real_dude_obj", mf_real_dude_obj, 0, 0 },
    { "reg_anim_animate_and_move", mf_reg_anim_animate_and_move, 4, 4, -1, { ARG_OBJECT, ARG_INT, ARG_INT, ARG_INT } },
    { "remove_timer_event", mf_remove_timer_event, 0, 1, -1, { ARG_INT } },
    { "set_spray_settings", mf_set_spray_settings, 4, 4, -1, { ARG_INT, ARG_INT, ARG_INT, ARG_INT } },
    { "set_can_rest_on_map", mf_set_can_rest_on_map, 3, 3, -1, { ARG_INT, ARG_INT, ARG_INT } },
    { "set_car_intface_art", mf_set_car_intface_art, 1, 1, -1, { ARG_INT } },
    { "set_combat_free_move", mf_set_combat_free_move, 1, 1, -1, { ARG_INT } },
    { "set_cursor_mode", mf_set_cursor_mode, 1, 1, -1, { ARG_INT } },
    { "set_drugs_data", mf_set_drugs_data, 3, 3, -1, { ARG_INT, ARG_INT, ARG_INT } },
    { "set_dude_obj", mf_set_dude_obj, 1, 1, -1, { ARG_OBJECT } },
    { "set_fake_perk_npc", mf_set_fake_perk_npc, 5, 5, -1, { ARG_OBJECT, ARG_STRING, ARG_INT, ARG_INT, ARG_STRING } },
    { "set_fake_trait_npc", mf_set_fake_trait_npc, 5, 5, -1, { ARG_OBJECT, ARG_STRING, ARG_INT, ARG_INT, ARG_STRING } },
    { "set_flags", mf_set_flags, 2, 2, -1, { ARG_OBJECT, ARG_INT } },
    { "set_iface_tag_text", mf_set_iface_tag_text, 3, 3, -1, { ARG_INT, ARG_STRING, ARG_INT } },
    { "set_ini_setting", mf_set_ini_setting, 2, 2, -1, { ARG_STRING, ARG_INTSTR } },
    { "set_map_enter_position", mf_set_map_enter_position, 3, 3, -1, { ARG_INT, ARG_INT, ARG_INT } },
    { "set_object_data", mf_set_object_data, 3, 3, -1, { ARG_OBJECT, ARG_INT, ARG_INT } },
    { "set_outline", mf_set_outline, 2, 2, -1, { ARG_OBJECT, ARG_INT } },
    { "set_quest_failure_value", mf_set_quest_failure_value, 2, 2, -1, { ARG_INT, ARG_INT } },
    { "set_rest_heal_time", mf_set_rest_heal_time, 1, 1, -1, { ARG_INT } },
    { "set_rest_mode", mf_set_rest_mode, 1, 1, -1, { ARG_INT } },
    { "set_scr_name", mf_set_scr_name, 0, 1, -1, { ARG_STRING } },
    { "set_selectable_perk_npc", mf_set_selectable_perk_npc, 5, 5, -1, { ARG_OBJECT, ARG_STRING, ARG_INT, ARG_INT, ARG_STRING } },
    { "set_terrain_name", mf_set_terrain_name, 3, 3, -1, { ARG_INT, ARG_INT, ARG_STRING } },
    { "set_town_title", mf_set_town_title, 2, 2, -1, { ARG_INT, ARG_STRING } },
    { "set_unique_id", mf_set_unique_id, 1, 2, -1, { ARG_OBJECT, ARG_INT } },
    { "set_unjam_locks_time", mf_set_unjam_locks_time, 1, 1, -1, { ARG_INT } },
    { "set_window_flag", mf_set_window_flag, 3, 3, -1, { ARG_INTSTR, ARG_INT, ARG_INT } },
    { "set_worldmap_heal_time", mf_set_worldmap_heal_time, 1, 1, -1, { ARG_INT } },
    { "show_window", mf_show_window, 0, 1, -1, { ARG_STRING } },
    { "signal_close_game", mf_signal_close_game, 0, 0 },
    { "spatial_radius", mf_spatial_radius, 1, 1, 0, { ARG_OBJECT } },
    { "string_compare", mf_string_compare, 2, 3, 0, { ARG_STRING, ARG_STRING, ARG_INT } },
    { "string_find", mf_string_find, 2, 3, -1, { ARG_STRING, ARG_STRING, ARG_INT } },
    { "string_find_from", mf_string_find, 3, 3, -1, { ARG_STRING, ARG_STRING, ARG_INT } },
    { "string_format_array", mf_string_format_array, 2, 2, 0, { ARG_STRING, ARG_INT } },
    { "string_replace", mf_string_replace, 3, 3, -1, { ARG_STRING, ARG_STRING, ARG_STRING } },
    { "string_format", mf_string_format, 2, 8, 0, { ARG_STRING, ARG_ANY, ARG_ANY, ARG_ANY, ARG_ANY, ARG_ANY, ARG_ANY, ARG_ANY } },
    { "string_to_case", mf_string_to_case, 2, 2, -1, { ARG_STRING, ARG_INT } },
    { "talking_head_mood", mf_talking_head_mood, 1, 1, 0, { ARG_INT } },
    { "tile_by_position", mf_tile_by_position, 2, 2, -1, { ARG_INT, ARG_INT } },
    { "tile_refresh_display", mf_tile_refresh_display, 0, 0 },
    { "unjam_lock", mf_unjam_lock, 1, 1, -1, { ARG_OBJECT } },
    { "unwield_slot", mf_unwield_slot, 2, 2, -1, { ARG_OBJECT, ARG_INT } },
    { "win_fill_color", mf_win_fill_color, 0, 5, -1, { ARG_INT, ARG_INT, ARG_INT, ARG_INT, ARG_INT } },
    { "opcode_exists", mf_opcode_exists, 1, 1, 0, { ARG_INT } },
    // F2-24: Explosion metarule registered as opcode 0x8261 only — invisible to
    // metarule discovery. Adding this entry makes metarule_exist("metarule2_explosions")
    // return 1 and get_metarule_table() include it, consistent with opcode_exists(0x8261).
    { "metarule2_explosions", mf_explosions_metarule, 2, 3, -1, { ARG_INT, ARG_INT, ARG_INT } },
};
const std::size_t kMetarulesCount = sizeof(kMetarules) / sizeof(kMetarules[0]);

enum class InterfaceWindowLookupResult {
    Found,
    Missing,
    Invalid,
};

/*
// Valid window types for get_window_attribute
#define WINTYPE_INVENTORY    (0) // any inventory window (player/loot/use/barter)
#define WINTYPE_DIALOG       (1)
#define WINTYPE_PIPBOY       (2)
#define WINTYPE_WORLDMAP     (3)
#define WINTYPE_IFACEBAR     (4) // the interface bar
#define WINTYPE_CHARACTER    (5)
#define WINTYPE_SKILLDEX     (6)
#define WINTYPE_ESCMENU      (7) // escape menu
#define WINTYPE_AUTOMAP      (8)
*/
static InterfaceWindowLookupResult getInterfaceWindowByType(int winType, int& window)
{
    switch (winType) {
    case 0:
        window = inventoryGetWindow();
        break;
    case 1:
        window = gameDialogGetWindow();
        break;
    case 2:
        window = pipboyGetWindow();
        break;
    case 3:
        window = worldmapGetWindow();
        break;
    case 4:
        window = windowGetWindow(gInterfaceBarWindow) != nullptr ? gInterfaceBarWindow : -1;
        break;
    case 5:
        window = characterEditorGetWindow();
        break;
    case 6:
        window = skilldexGetWindow();
        break;
    case 7:
        window = optionsGetWindow();
        break;
    case 8:
        window = automapGetWindow();
        break;
    default:
        window = -1;
        return InterfaceWindowLookupResult::Invalid;
    }

    return window != -1 ? InterfaceWindowLookupResult::Found : InterfaceWindowLookupResult::Missing;
}

static int getCurrentInterfaceWindow()
{
    int window = -1;
    if (GameMode::isInGameMode(GameMode::kInventory)
        || GameMode::isInGameMode(GameMode::kUseOn)
        || GameMode::isInGameMode(GameMode::kLoot)
        || GameMode::isInGameMode(GameMode::kBarter)) {
        window = inventoryGetWindow();
    } else if (GameMode::isInGameMode(GameMode::kDialog)) {
        window = gameDialogGetWindow();
    } else if (GameMode::isInGameMode(GameMode::kPipboy)) {
        window = pipboyGetWindow();
    } else if (GameMode::isInGameMode(GameMode::kWorldmap)) {
        window = worldmapGetWindow();
    } else if (GameMode::isInGameMode(GameMode::kEditor)) {
        window = characterEditorGetWindow();
    } else if (GameMode::isInGameMode(GameMode::kSkilldex)) {
        window = skilldexGetWindow();
    } else if (GameMode::isInGameMode(GameMode::kOptions)) {
        window = optionsGetWindow();
    } else if (GameMode::isInGameMode(GameMode::kAutomap)) {
        window = automapGetWindow();
    } else if (windowGetWindow(gInterfaceBarWindow) != nullptr) {
        window = gInterfaceBarWindow;
    }

    return window;
}

static bool clampWindowFillRect(int windowWidth, int windowHeight, int& x, int& y, int& width, int& height)
{
    if (x < 0) {
        width += x;
        x = 0;
    }

    if (y < 0) {
        height += y;
        y = 0;
    }

    if (x >= windowWidth || y >= windowHeight) {
        width = 0;
        height = 0;
        return true;
    }

    bool truncated = false;
    if (x + width > windowWidth) {
        width = windowWidth - x;
        truncated = true;
    }

    if (y + height > windowHeight) {
        height = windowHeight - y;
        truncated = true;
    }

    return truncated;
}

static bool applyWindowFlag(int windowId, int bitFlag, bool enabled)
{
    return scriptWindowSetFlag(windowId, bitFlag, enabled);
}

void mf_art_cache_flush(OpcodeContext& ctx)
{
    artCacheFlush();
}

void mf_art_frame_data(OpcodeContext& ctx)
{
    int frame = ctx.numArgs() > 1 ? ctx.arg(1).asInt() : 0;
    int direction = ctx.numArgs() > 2 ? ctx.arg(2).asInt() : 0;

    if (ctx.arg(0).isInt() && ctx.arg(0).asInt() == -1) {
        ctx.setReturn(-1);
        return;
    }

    FrmImage image;
    if (ctx.arg(0).isInt()) {
        int fid = ctx.arg(0).asInt();
        if (!image.lock(fid, frame, direction)) {
            ctx.printError("%s() - cannot load art by FID: %d", ctx.name(), fid);
            ctx.setReturn(-1);
            return;
        }
    } else {
        const char* path = ctx.stringArg(0);
        if (!image.lock(path, frame, direction)) {
            ctx.printError("%s() - cannot load art from file: %s", ctx.name(), path);
            ctx.setReturn(-1);
            return;
        }
    }

    if (image.getWidth() <= 0 || image.getHeight() <= 0) {
        ctx.setReturn(-1);
        return;
    }

    ArrayId arrayId = CreateTempArray(2, 0);
    SetArray(arrayId, ProgramValue(0), ProgramValue(image.getWidth()), false, ctx.program());
    SetArray(arrayId, ProgramValue(1), ProgramValue(image.getHeight()), false, ctx.program());
    ctx.setReturn(ProgramValue(arrayId));
}

void mf_add_iface_tag(OpcodeContext& ctx)
{
    int result = interfaceTagAdd();
    if (result == -1) {
        ctx.printError("%s() - cannot add new tag as the maximum limit has been reached.", ctx.name());
    }
    ctx.setReturn(result);
}

void mf_attack_is_aimed(OpcodeContext& ctx)
{
    int hitMode;
    bool aiming;

    if (interfaceGetCurrentHitMode(&hitMode, &aiming) == -1) {
        ctx.setReturn(0);
    } else {
        ctx.setReturn(aiming ? 1 : 0);
    }
}

void mf_car_gas_amount(OpcodeContext& ctx)
{
    ctx.setReturn(wmCarGasAmount());
}

void mf_combat_data(OpcodeContext& ctx)
{
    if (isInCombat()) {
        ctx.setReturn(combat_get_data());
    } else {
        ctx.setReturn(nullptr);
    }
}

void mf_create_win(OpcodeContext& ctx)
{
    int flags = ctx.numArgs() > 5
        ? ctx.arg(5).asInt()
        : WINDOW_MOVE_ON_TOP;

    int color = (flags & WINDOW_TRANSPARENT) != 0 ? 0 : 256;
    int windowIndex = scriptWindowCreate(ctx.stringArg(0),
        ctx.arg(1).asInt(),
        ctx.arg(2).asInt(),
        ctx.arg(3).asInt(),
        ctx.arg(4).asInt(),
        color,
        flags);
    if (windowIndex == -1) {
        ctx.printError("%s() - couldn't create window.", ctx.name());
    } else {
        // F-30: Set program->windowId so drawing metarules (draw_image,
        // draw_image_scaled, win_fill_color, interface_print) render to
        // the created window instead of falling back to the HUD bar.
        ctx.program()->windowId = windowIndex;
        // F-31: Auto-select the created window (matches sfall behavior).
        scriptWindowSelectId(windowIndex);
    }
    ctx.setReturn(windowIndex);
}

void mf_display_stats(OpcodeContext& ctx)
{
    if (GameMode::isInGameMode(GameMode::kInventory)) {
        inventoryDisplayStats();
    } else if (GameMode::isInGameMode(GameMode::kEditor)) {
        characterEditorDisplayStats();
    }
}

void mf_critter_inven_obj2(OpcodeContext& ctx)
{
    Object* obj = ctx.arg(0).asObject();
    int slot = ctx.arg(1).asInt();

    if (obj == nullptr) {
        ctx.setReturn(ProgramValue());
        return;
    }

    switch (slot) {
    case 0:
        ctx.setReturn(critterGetArmor(obj));
        break;
    case 1:
        ctx.setReturn(critterGetItem2(obj));
        break;
    case 2:
        ctx.setReturn(critterGetItem1(obj));
        break;
    case -2:
        ctx.setReturn(obj->data.inventory.length);
        break;
    default:
        ctx.printError("%s() - invalid slot: %d, valid values are 0, 1, 2, or -2", ctx.name(), slot);
        ctx.setReturn(0);
        break;
    }
}

void mf_dialog_obj(OpcodeContext& ctx)
{
    if (GameMode::isInGameMode(GameMode::kDialog)) {
        ctx.setReturn(gGameDialogSpeaker);
    } else {
        ctx.setReturn(nullptr);
    }
}

void mf_dialog_message(OpcodeContext& ctx)
{
    if (GameMode::isInGameMode(GameMode::kDialog)
        && !GameMode::isInGameMode(GameMode::kDialogReview)) {
        gameDialogRenderSupplementaryMessage(ctx.stringArg(0));
    }
}

void mf_get_combat_free_move(OpcodeContext& ctx)
{
    ctx.setReturn(_combat_free_move);
}

void mf_get_inven_ap_cost(OpcodeContext& ctx)
{
    ctx.setReturn(inventoryGetInvenApCost());
}

void mf_get_cursor_mode(OpcodeContext& ctx)
{
    ctx.setReturn(gameMouseGetMode());
}

void mf_get_flags(OpcodeContext& ctx)
{
    Object* object = ctx.arg(0).asObject();
    if (object == nullptr) {
        // F-40: Guard against null object (asObject() can return nullptr).
        ctx.setReturn(0);
        return;
    }
    ctx.setReturn(object->flags);
}

void mf_get_outline(OpcodeContext& ctx)
{
    Object* object = ctx.arg(0).asObject();
    if (object == nullptr) {
        // F-40: Guard against null object (asObject() can return nullptr).
        ctx.setReturn(0);
        return;
    }
    ctx.setReturn(object->outline);
}

void mf_get_sfall_arg_at(OpcodeContext& ctx)
{
    const int argNum = ctx.arg(0).asInt();

    ProgramValue result(0);
    const auto hookCall = hookOpcodeGetCurrentCall(ctx.name());
    if (hookCall != nullptr) {
        if (argNum >= 0 && argNum < hookCall->numArgs()) {
            result = hookCall->getArgAt(argNum);
        } else {
            ctx.printError("%s: argNum %d out of range [0, %d]", ctx.name(), argNum, hookCall->numArgs() - 1);
        }
    }
    ctx.setReturn(result);
}

void mf_get_object_data(OpcodeContext& ctx)
{
    // TODO: only allow to modify a set of whitelisted object types
    // TODO: map offsets to fields to avoid potential alignment, 64bit issues!
    Object* ptr = ctx.arg(0).asObject();
    int rawOffset = ctx.arg(1).asInt();

    // asObject() returns nullptr for integer 0 (the canonical "null"
    // in Fallout scripts) and for non-pointer types. Guard against
    // null dereference — peer functions in this file have the same guard.
    if (ptr == nullptr) {
        ctx.printError("%s(): object is null (asObject() returned nullptr)", ctx.name());
        ctx.setReturn(-1);
        return;
    }

    if (rawOffset < 0 || rawOffset % 4 != 0) {
        ctx.printError("%s(): bad offset %d", ctx.name(), rawOffset);
        ctx.setReturn(-1);
        return;
    }

    size_t offset = static_cast<size_t>(rawOffset);

    // Bounds check: refuse reads beyond the Object struct boundary.
    // This prevents unbounded heap reads when scripts call get_object_data
    // with an arbitrary pointer (e.g. Attack* from HOOK_COMBATDAMAGE arg12).
    if (offset + sizeof(int) > sizeof(Object)) {
        ctx.printError("%s(): offset %d exceeds Object struct bounds (%zu bytes)", ctx.name(), rawOffset, sizeof(Object));
        ctx.setReturn(-1);
        return;
    }

    int value = *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(ptr) + offset);
    ctx.setReturn(value);
}

void mf_get_text_width(OpcodeContext& ctx)
{
    ctx.setReturn(fontGetStringWidth(ctx.stringArg(0)));
}

void mf_get_window_attribute(OpcodeContext& ctx)
{
    int attrType = ctx.numArgs() > 1 ? ctx.arg(1).asInt() : 0;
    int window = -1;
    InterfaceWindowLookupResult lookup = getInterfaceWindowByType(ctx.arg(0).asInt(), window);
    if (lookup == InterfaceWindowLookupResult::Missing) {
        if (attrType != 0) {
            ctx.printError("%s() - failed to get the interface window.", ctx.name());
            ctx.setReturn(-1);
        } else {
            ctx.setReturn(0);
        }
        return;
    }

    if (lookup == InterfaceWindowLookupResult::Invalid) {
        ctx.printError("%s() - invalid window type number.", ctx.name());
        ctx.setReturn(-1);
        return;
    }

    Rect rect;
    if (windowGetRect(window, &rect) == -1) {
        ctx.setReturn(-1);
        return;
    }

    switch (attrType) {
    case -1: {
        ArrayId arrayId = CreateTempArray(-1, 0);
        SetArray(arrayId, programMakeString(ctx.program(), "left"), ProgramValue(rect.left), false, ctx.program());
        SetArray(arrayId, programMakeString(ctx.program(), "top"), ProgramValue(rect.top), false, ctx.program());
        SetArray(arrayId, programMakeString(ctx.program(), "right"), ProgramValue(rect.right), false, ctx.program());
        SetArray(arrayId, programMakeString(ctx.program(), "bottom"), ProgramValue(rect.bottom), false, ctx.program());
        ctx.setReturn(ProgramValue(arrayId));
        break;
    }
    case 0: // basically an existence check
        ctx.setReturn(1);
        break;
    case 1:
        ctx.setReturn(rect.left);
        break;
    case 2:
        ctx.setReturn(rect.top);
        break;
    case 3:
        ctx.setReturn(windowGetWidth(window));
        break;
    case 4:
        ctx.setReturn(windowGetHeight(window));
        break;
    case 5:
        ctx.setReturn(window);
        break;
    default:
        ctx.setReturn(0);
        break;
    }
}

// F-08: get_water_days_left() — returns remaining Vault 13 water chip timer days.
// Only active when gFallout1Behavior is true (FO1/et tu mode). The water timer
// counts down from 150 days at game start. Scripts can use this to check if the
// vault is about to run out of water.
void mf_get_water_days_left(OpcodeContext& ctx)
{
    if (!gFallout1Behavior) {
        ctx.setReturn(0);
        return;
    }
    // Lazy-enable on first call — default to 150 days from game start.
    if (!gSfallWaterTimerEnabled) {
        gSfallWaterTimerEnabled = true;
        gSfallWaterTimerDays = 150;
    }
    int currentDay = static_cast<int>(gameTimeGetTime() / GAME_TIME_TICKS_PER_DAY);
    int remaining = gSfallWaterTimerDays - currentDay;
    ctx.setReturn(remaining < 0 ? 0 : remaining);
}

// F-08: get_water_days_left_x(gameTimeTicks) — returns remaining water days
// at the specified game time. Used by scripts to predict water timer state
// at a future point in time (e.g., after resting for N hours).
void mf_get_water_days_left_x(OpcodeContext& ctx)
{
    if (!gFallout1Behavior) {
        ctx.setReturn(0);
        return;
    }
    // Lazy-enable on first call.
    if (!gSfallWaterTimerEnabled) {
        gSfallWaterTimerEnabled = true;
        gSfallWaterTimerDays = 150;
    }
    int gameTimeTicks = ctx.numArgs() > 0 ? ctx.arg(0).asInt() : 0;
    int dayAtTime = gameTimeTicks / GAME_TIME_TICKS_PER_DAY;
    int remaining = gSfallWaterTimerDays - dayAtTime;
    ctx.setReturn(remaining < 0 ? 0 : remaining);
}

static bool loadSfallArtImage(OpcodeContext& ctx, int artArg, int frame, int direction, FrmImage& image, int& fid)
{
    if (ctx.arg(artArg).isInt() && ctx.arg(artArg).asInt() == -1) {
        return false;
    }

    if (ctx.arg(artArg).isInt()) {
        fid = ctx.arg(artArg).asInt();
        int frameDirection = 0;
        int lockFid = fid;
        if (FID_TYPE(fid) == OBJ_TYPE_CRITTER) {
            frameDirection = direction >= 0 ? direction : FID_ROTATION(fid);
            if (direction >= 0) {
                lockFid = (direction << 28) | (fid & 0x0FFFFFFF);
            }
        }

        if (!image.lock(lockFid, frame, frameDirection)) {
            ctx.printError("%s() - cannot load art by FID: %d", ctx.name(), fid);
            return false;
        }
    } else {
        const char* path = ctx.stringArg(artArg);
        int frameDirection = direction >= 0 ? direction : 0;
        if (!image.lock(path, frame, frameDirection)) {
            ctx.printError("%s() - cannot load art from file: %s", ctx.name(), path);
            return false;
        }
    }

    return true;
}

static int drawSfallArtImage(OpcodeContext& ctx, int window, FrmImage& image, int x, int y, int width, int height, bool transparent, bool refresh)
{
    int windowWidth = windowGetWidth(window);
    int windowHeight = windowGetHeight(window);
    if (windowWidth <= 0 || windowHeight <= 0 || windowGetBuffer(window) == nullptr) {
        ctx.printError("%s() - no created or selected window.", ctx.name());
        return 0;
    }

    if (width <= 0 || height <= 0) {
        return 0;
    }

    // sfall clamps interface_art_draw left/top coordinates instead of clipping
    // the source image, but for draw_image does not clamp.  It's best to clamp
    // instead of potentially writing out of bounds, but if we're regularly
    // calling this with negative x/y we should implement intelligent clipping instead.
    if (x < 0) {
        x = 0;
    }

    if (y < 0) {
        y = 0;
    }

    if (x + width > windowWidth || y + height > windowHeight) {
        ctx.printError("%s() - attempt to draw beyond window bounds (%d, %d)", ctx.name(), windowWidth, windowHeight);
        return -1;
    }

    unsigned char* dest = windowGetBuffer(window) + windowWidth * y + x;
    if (width != image.getWidth() || height != image.getHeight()) {
        if (transparent) {
            blitBufferToBufferStretchTrans(image.getData(), image.getWidth(), image.getHeight(), image.getWidth(), dest, width, height, windowWidth);
        } else {
            blitBufferToBufferStretch(image.getData(), image.getWidth(), image.getHeight(), image.getWidth(), dest, width, height, windowWidth);
        }
    } else if (transparent) {
        blitBufferToBufferTrans(image.getData(), image.getWidth(), image.getHeight(), image.getWidth(), dest, windowWidth);
    } else {
        blitBufferToBuffer(image.getData(), image.getWidth(), image.getHeight(), image.getWidth(), dest, windowWidth);
    }

    if (refresh) {
        Rect rect = { x, y, x + width - 1, y + height - 1 };
        windowRefreshRect(window, &rect);
    }

    return 1;
}

static int drawSfallImageToScriptWindow(OpcodeContext& ctx, bool scaled)
{
    int window = scriptWindowGetWindow(ctx.program()->windowId);
    if (window == -1) {
        // I2-54: Fall back to the interface bar window (matches interface_print behavior).
        window = gInterfaceBarWindow;
    }
    if (window == -1) {
        ctx.printError("%s() - no created or selected window.", ctx.name());
        return 0;
    }

    int frame = ctx.numArgs() > 1 ? ctx.arg(1).asInt() : 0;

    FrmImage image;
    int fid = -1;
    if (!loadSfallArtImage(ctx, 0, frame, -1, image, fid)) {
        return -1;
    }

    if (scaled && ctx.numArgs() < 3) {
        return drawSfallArtImage(ctx, window, image, 0, 0, windowGetWidth(window), windowGetHeight(window), false, true);
    }

    int x = ctx.numArgs() > 2 ? ctx.arg(2).asInt() : 0;
    int y = ctx.numArgs() > 3 ? ctx.arg(3).asInt() : 0;
    int width = image.getWidth();
    int height = image.getHeight();
    bool transparent = true;

    if (scaled) {
        if (ctx.numArgs() > 4) {
            width = ctx.arg(4).asInt();
            height = ctx.numArgs() > 5 ? ctx.arg(5).asInt() : -1;
        }

        if (width <= -1 && height > 0) {
            width = height * image.getWidth() / image.getHeight();
        } else if (height <= -1 && width > 0) {
            height = width * image.getHeight() / image.getWidth();
        }
    } else {
        x += image.getXOffset();
        y += image.getYOffset();
        transparent = ctx.numArgs() > 4 ? ctx.arg(4).asInt() == 0 : true;
    }

    return drawSfallArtImage(ctx, window, image, x, y, width, height, transparent, true);
}

static void mf_draw_image(OpcodeContext& ctx)
{
    ctx.setReturn(drawSfallImageToScriptWindow(ctx, false));
}

static void mf_draw_image_scaled(OpcodeContext& ctx)
{
    ctx.setReturn(drawSfallImageToScriptWindow(ctx, true));
}

static void mf_interface_art_draw(OpcodeContext& ctx)
{
    int window = -1;
    InterfaceWindowLookupResult lookup = getInterfaceWindowByType(ctx.arg(0).asInt() & 0xFF, window);
    if (lookup != InterfaceWindowLookupResult::Found) {
        ctx.printError("%s() - the game interface window is not created or invalid window type number.", ctx.name());
        ctx.setReturn(-1);
        return;
    }

    int frame = ctx.numArgs() > 4 ? ctx.arg(4).asInt() : 0;
    int direction = -1;
    int scaledWidth = -1;
    int scaledHeight = -1;
    if (ctx.numArgs() > 5) {
        int arrayId = ctx.arg(5).asInt();
        if (ArrayExists(arrayId)) {
            direction = GetArray(arrayId, ProgramValue(0), ctx.program()).asInt();

            int arrayLength = LenArray(arrayId);
            if (arrayLength > 1) {
                scaledWidth = GetArray(arrayId, ProgramValue(1), ctx.program()).asInt();
            }

            if (arrayLength > 2) {
                scaledHeight = GetArray(arrayId, ProgramValue(2), ctx.program()).asInt();
            }
        }
    }

    FrmImage image;
    int fid = -1;
    if (!loadSfallArtImage(ctx, 1, frame, direction, image, fid)) {
        ctx.setReturn(-1);
        return;
    }

    int xOffset = 0;
    int yOffset = 0;
    if (ctx.arg(1).isInt() && FID_TYPE(fid) == OBJ_TYPE_CRITTER && direction >= 0) {
        xOffset = image.getXOffset();
        yOffset = image.getYOffset();
    }

    int x = std::max(ctx.arg(2).asInt() + xOffset, 0);
    int y = std::max(ctx.arg(3).asInt() + yOffset, 0);

    int width = scaledWidth >= 0 ? scaledWidth : image.getWidth();
    int height = scaledHeight >= 0 ? scaledHeight : image.getHeight();
    ctx.setReturn(drawSfallArtImage(ctx, window, image, x, y, width, height, true, (ctx.arg(0).asInt() & 0x1000000) == 0));
}

void mf_intface_redraw(OpcodeContext& ctx)
{
    if (ctx.numArgs() == 0) {
        interfaceBarRefresh();
    } else {
        // 1-arg form: redraw a specific interface window by type.
        // Window type constants match get_window_attribute / getInterfaceWindowByType:
        //   0=inventory, 1=dialog, 2=pipboy, 3=worldmap, 4=ifacebar,
        //   5=character editor, 6=skilldex, 7=escape menu, 8=automap
        int winType = ctx.arg(0).asInt();
        int window = -1;
        InterfaceWindowLookupResult result = getInterfaceWindowByType(winType, window);
        if (result == InterfaceWindowLookupResult::Found) {
            windowRefresh(window);
            ctx.setReturn(0);
        } else {
            debugPrint("%s(): window type %d is not available", ctx.name(), winType);
            ctx.setReturn(-1);
        }
    }
}

void mf_inventory_redraw(OpcodeContext& ctx)
{
    inventoryRedraw(ctx.numArgs() > 0 ? ctx.arg(0).asInt() : -1);
}

void mf_item_weight(OpcodeContext& ctx)
{
    Object* object = ctx.arg(0).asObject();
    if (object == nullptr) {
        ctx.printError("%s() - null object argument.", ctx.name());
        ctx.setReturn(0);
        return;
    }
    if (PID_TYPE(object->pid) != OBJ_TYPE_ITEM) {
        ctx.printError("%s() - expected item object.", ctx.name());
        ctx.setReturn(0);
        return;
    }

    ctx.setReturn(itemGetWeight(object));
}

void mf_loot_obj(OpcodeContext& ctx)
{
    if (GameMode::isInGameMode(GameMode::kLoot)) {
        ctx.setReturn(inventoryGetTargetObject());
    } else {
        ctx.setReturn(nullptr);
    }
}

void mf_metarule_exist(OpcodeContext& ctx)
{
    const char* metarule = ctx.stringArg(0);

    // Rotators fork sentinel (C-05): ETu scripts check metarule_exist("rotators")
    // to detect the Rotators engine fork. CE is not a Rotators fork, but
    // returning 1 enables the Rotators-aware fallback code paths in ETu scripts,
    // which use standard sfall opcodes that CE fully implements (get_ini_setting,
    // message_box, etc.). r_call_offset is NOT registered — scripts that try to
    // use it will get metarule_exist("r_call_offset")=0.
    if (compat_stricmp(metarule, "rotators") == 0) {
        ctx.setReturn(1);
        return;
    }

    // F2-047: "sfall" sentinel. ET Tu scripts use metarule_exist("sfall") to
    // detect sfall presence and enable CE-specific features. Without this
    // sentinel, scripts receive 0 and disable CE feature paths. Same pattern
    // as the "rotators" sentinel above — returns 1 unconditionally.
    if (compat_stricmp(metarule, "sfall") == 0) {
        ctx.setReturn(1);
        return;
    }

    // F-023: Stub metarule blacklist. These metarules are registered to prevent
    // script crashes but their handlers are permanent no-ops. metarule_exist
    // must return 0 so scripts can detect absence and use alternatives.
    if (compat_stricmp(metarule, "r_write") == 0) {
        ctx.setReturn(0);
        return;
    }

    for (int index = 0; index < (int)kMetarulesCount; index++) {
        if (compat_stricmp(kMetarules[index].name, metarule) == 0) {
            ctx.setReturn(1);
            return;
        }
    }

    ctx.setReturn(0);
}

// F-012: add_extra_msg_file now supports 2-arg form (filename, fileNumber).
// Previously the 2-arg form returned -1 as "not supported". The 2-arg form
// allows scripts to specify an explicit message list file number (used by
// mods that manage multiple extra message files and need predictable IDs).
// Since CE's messageListRepositoryAddExtra auto-assigns IDs and the explicit
// fileNumber is informational, we accept any integer and delegate to the
// underlying single-path loader.
void mf_add_extra_msg_file(OpcodeContext& ctx)
{
    const char* fileName = ctx.stringArg(0);

    // I2-19: Reject paths containing ".." to prevent directory traversal
    // out of the game/ directory. Matches the existing guard pattern in
    // sfall_ini.cc:69 and platform_compat.cc:485.
    if (compat_path_contains_traversal(fileName)) {
        ctx.printError("%s() - path traversal characters are not allowed in file name.", ctx.name());
        ctx.setReturn(-2);
        return;
    }

    if (ctx.numArgs() == 2) {
        // 2-arg form: explicit fileNumber — accept it but use auto-assignment.
        // The fileNumber argument is consumed for compatibility but does not
        // override the auto-assigned message list ID in CE's message system.
        int /*fileNumber*/ _ = ctx.arg(1).asInt();
        (void)_;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s\\%s", "game", fileName);

    int result = messageListRepositoryAddExtra(path);
    switch (result) {
    case -2:
        ctx.printError("%s() - error loading message file.", ctx.name());
        break;
    case -3:
        ctx.printError("%s() - the limit of adding message files has been exceeded.", ctx.name());
        break;
    }

    ctx.setReturn(result);
}

void mf_opcode_exists(OpcodeContext& ctx)
{
    int opcode = ctx.arg(0).asInt();
    int opcodeIndex = opcode & 0x3FFF;
    if (opcodeIndex < 0 || opcodeIndex >= OPCODE_MAX_COUNT) {
        ctx.setReturn(0);
        return;
    }

    // F-023: Stub opcode blacklist. These opcodes are registered with
    // non-null handlers (to prevent script crashes on "Undefined opcode"),
    // but those handlers are permanent no-ops. opcode_exists must return 0
    // so scripts can detect the absence of these features at runtime and
    // fall back to alternative implementations.
    const std::tuple<uint16_t, const char*> stubOpcodes[] = {
        // VOODOO direct memory write — cannot work in CE (different address space)
        {0x81CF, "write_byte"},
        {0x81D0, "write_short"},
        {0x81D1, "write_int"},
        {0x821B, "write_string"},
        // VOODOO call_offset — cannot call engine-internal functions at arbitrary addresses
        {0x81D2, "call_offset_v0"},
        {0x81D3, "call_offset_v1"},
        {0x81D4, "call_offset_v2"},
        {0x81D5, "call_offset_v3"},
        {0x81D6, "call_offset_v4"},
        {0x81D7, "call_offset_r0"},
        {0x81D8, "call_offset_r1"},
        {0x81D9, "call_offset_r2"},
        {0x81DA, "call_offset_r3"},
        {0x81DB, "call_offset_r4"},
        // Viewport override — not supported (CE uses SDL2 hardware rendering)
        {0x81A6, "get_viewport_x"},
        {0x81A7, "get_viewport_y"},
        {0x81A8, "set_viewport_x"},
        {0x81A9, "set_viewport_y"},
        // Palette override — not supported (CE uses SDL2 hardware rendering)
        {0x81F2, "set_palette"},
        // Shader mode — not applicable (CE uses SDL2 without custom shaders)
        {0x81AE, "set_shader_mode"},
        // Game pause/resume — not directly exposed to scripts
        {0x8222, "stop_game"},
        {0x8223, "resume_game"},
        // Movie tracking — handled internally
        {0x8240, "mark_movie_played"},
    };

    for (const auto& [stubOpcode, _name] : stubOpcodes) {
        if (opcode == stubOpcode) {
            ctx.setReturn(0);
            return;
        }
    }

    auto opcodeHandler = gInterpreterOpcodeHandlers[opcodeIndex];
    int opcodeExists = opcodeHandler != nullptr ? 1 : 0;
    ctx.setReturn(opcodeExists);
}

void mf_obj_under_cursor(OpcodeContext& ctx)
{
    int onlyCritter = ctx.arg(0).asInt();
    int includeDude = ctx.arg(1).asInt();

    Object* object = gameMouseGetObjectUnderCursor(onlyCritter ? OBJ_TYPE_CRITTER : -1, includeDude, gElevation);

    ctx.setReturn(object);
}

void mf_obj_is_openable(OpcodeContext& ctx)
{
    ctx.setReturn(objectIsOpenable(ctx.arg(0).asObject()));
}

static int objectsInRadiusFirstTile(int sourceTile, int radius, int* endTile)
{
    int hexRadius = HEX_GRID_WIDTH * (radius + 1);

    *endTile = std::min(sourceTile + hexRadius + 1, HEX_GRID_SIZE);
    return std::max(sourceTile - hexRadius, 0);
}

static void mf_objects_in_radius(OpcodeContext& ctx)
{
    int sourceTile = ctx.arg(0).asInt();
    int radius = std::clamp(ctx.arg(1).asInt(), 0, 50);
    int elevation = std::clamp(ctx.arg(2).asInt(), 0, ELEVATION_COUNT - 1);
    int type = ctx.numArgs() > 3 ? ctx.arg(3).asInt() : -1;

    ArrayId arrayId = CreateTempArray(0, 0);

    if (!hexGridTileIsValid(sourceTile)) {
        ctx.setReturn(arrayId);
        return;
    }

    int index = 0;
    int endTile;
    for (int tile = objectsInRadiusFirstTile(sourceTile, radius, &endTile); tile < endTile; tile++) {
        for (Object* object = objectFindFirstAtLocation(elevation, tile); object != nullptr; object = objectFindNextAtLocation()) {
            if (type != -1 && (object->pid == -1 || PID_TYPE(object->pid) != type)) {
                continue;
            }

            int extraRange = (object->flags & OBJECT_MULTIHEX) != 0 ? 1 : 0;
            if (tileDistanceBetween(sourceTile, object->tile) > radius + extraRange) {
                continue;
            }

            ResizeArray(arrayId, index + 1);
            SetArray(arrayId, ProgramValue(index), ProgramValue(object), false, ctx.program());
            index++;
        }
    }

    ctx.setReturn(arrayId);
}

void mf_outlined_object(OpcodeContext& ctx)
{
    ctx.setReturn(gmouse_get_outlined_object());
}

void mf_set_combat_free_move(OpcodeContext& ctx)
{
    int value = ctx.arg(0).asInt();
    if (value < 0) {
        value = 0;
    }

    _combat_free_move = value;

    if (isInCombat() && combat_get_data()->attacker == gDude) {
        interfaceRenderActionPoints(gDude->data.critter.combat.ap, _combat_free_move);
    }
}

void mf_set_cursor_mode(OpcodeContext& ctx)
{
    int mode = ctx.arg(0).asInt();
    gameMouseSetMode(mode);
}

void mf_set_flags(OpcodeContext& ctx)
{
    Object* object = ctx.arg(0).asObject();
    if (object == nullptr) {
        // F-40: Guard against null object (asObject() can return nullptr).
        ctx.setReturn(-1);
        return;
    }
    int flags = ctx.arg(1).asInt();

    object->flags = flags;
}

void mf_set_iface_tag_text(OpcodeContext& ctx)
{
    int boxTag = ctx.arg(0).asInt();

    // F-36: Remove boxTag > 4 restriction — sfall 4.5.1 allows tags 0-4.
    if (boxTag >= 0 && boxTag <= interfaceTagGetMax()) {
        interfaceTagSetText(boxTag, ctx.stringArg(1), ctx.arg(2).asInt());
    } else {
        ctx.printError("%s() - tag value must be in the range of 0 to %d.", ctx.name(), interfaceTagGetMax());
        ctx.setReturn(-1);
    }
}

void mf_set_outline(OpcodeContext& ctx)
{
    Object* object = ctx.arg(0).asObject();
    if (object == nullptr) {
        // F-40: Guard against null object (asObject() can return nullptr).
        ctx.setReturn(-1);
        return;
    }
    int outline = ctx.arg(1).asInt();
    object->outline = outline;
}

void mf_set_window_flag(OpcodeContext& ctx)
{
    int bitFlag = ctx.arg(1).asInt();
    switch (bitFlag) {
    case WINDOW_DONT_MOVE_TOP:
    case WINDOW_MOVE_ON_TOP:
    case WINDOW_HIDDEN:
    case WINDOW_MODAL:
    case WINDOW_TRANSPARENT:
        break;
    default:
        return;
    }

    bool enabled = ctx.arg(2).asInt() != 0;
    if (ctx.arg(0).isString()) {
        const char* windowName = ctx.stringArg(0);
        if (!scriptWindowSetNamedFlag(windowName, bitFlag, enabled)) {
            ctx.printError("%s() - window '%s' is not found.", ctx.name(), windowName);
        }
        return;
    }

    int windowId = ctx.arg(0).asInt();
    if (windowId <= 0) {
        windowId = getCurrentInterfaceWindow();
    }

    if (windowId == -1) {
        return;
    }

    applyWindowFlag(windowId, bitFlag, enabled);
}

void mf_set_unique_id(OpcodeContext& ctx)
{
    Object* object = ctx.arg(0).asObject();
    if (ctx.numArgs() > 1 && ctx.arg(1).asInt() == -1) {
        // unassign unique_id only if it has one
        if (object->id > OBJECT_ID_UNIQUE_START) {
            object->id = scriptsNewObjectId();
            scriptsSyncObjectId(object);
        }
        ctx.setReturn(object->id);
        return;
    }

    ctx.setReturn(scriptsSetUniqueObjectId(object));
}

void mf_show_window(OpcodeContext& ctx)
{
    if (ctx.numArgs() == 0) {
        scriptWindowShow();
    } else if (ctx.numArgs() == 1) {
        const char* windowName = ctx.stringArg(0);
        if (!scriptWindowShowNamed(windowName)) {
            debugPrint("show_window: window '%s' is not found", windowName);
        }
    }
}

void mf_hide_window(OpcodeContext& ctx)
{
    if (ctx.numArgs() == 0) {
        scriptWindowHide();
    } else {
        const char* windowName = ctx.stringArg(0);
        if (!scriptWindowHideNamed(windowName)) {
            ctx.printError("%s() - window '%s' is not found.", ctx.name(), windowName);
        }
    }
}

void mf_win_fill_color(OpcodeContext& ctx)
{
    int window = scriptWindowGetWindow(ctx.program()->windowId);
    if (window == -1) {
        // I2-54: Fall back to the interface bar window (matches interface_print behavior).
        window = gInterfaceBarWindow;
    }
    if (window == -1) {
        ctx.printError("%s() - no created or selected window.", ctx.name());
        ctx.setReturn(-1);
        return;
    }

    int windowWidth = windowGetWidth(window);
    int windowHeight = windowGetHeight(window);
    if (ctx.numArgs() == 0) {
        windowFill(window, 0, 0, windowWidth, windowHeight, 0);
        windowRefresh(window);
        return;
    }

    if (ctx.numArgs() != 5) {
        ctx.printError("%s() - invalid number of arguments (%d), must be 0 or 5.", ctx.name(), ctx.numArgs());
        return;
    }

    int x = ctx.arg(0).asInt();
    int y = ctx.arg(1).asInt();
    int width = ctx.arg(2).asInt();
    int height = ctx.arg(3).asInt();
    int color = ctx.arg(4).asInt();
    bool truncated = clampWindowFillRect(windowWidth, windowHeight, x, y, width, height);
    if (width > 0 && height > 0) {
        windowFill(window, x, y, width, height, color);
        Rect rect = { x, y, x + width - 1, y + height - 1 };
        windowRefreshRect(window, &rect);
    }

    if (truncated) {
        ctx.printError("%s() - the fill area is truncated because it exceeds the current window.", ctx.name());
    }
}

void mf_tile_refresh_display(OpcodeContext& ctx)
{
    tileWindowRefresh();
}

void mf_unwield_slot(OpcodeContext& ctx)
{
    Object* critter = ctx.arg(0).asObject();
    if (critter == nullptr) {
        ctx.printError("%s() - null critter argument.", ctx.name());
        ctx.setReturn(-1);
        return;
    }
    int slot = ctx.arg(1).asInt();

    if (slot < static_cast<int>(InvenSlot::Armor) || slot > static_cast<int>(InvenSlot::LeftHand)) {
        ctx.printError("%s() - slot must be 0, 1, or 2.", ctx.name());
        ctx.setReturn(-1);
        return;
    }

    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        ctx.printError("%s() - the object is not a critter.", ctx.name());
        ctx.setReturn(-1);
        return;
    }

    if (inventoryUnwieldSlot(critter, static_cast<InvenSlot>(slot)) == -1) {
        ctx.setReturn(-1);
    }
}

void mf_tile_by_position(OpcodeContext& ctx)
{
    int x = ctx.arg(0).asInt();
    int y = ctx.arg(1).asInt();
    ctx.setReturn(tileFromScreenXY(x, y));
}

// compares strings case-insensitive with specifics for Fallout
// from sfall: https://github.com/sfall-team/sfall/blob/71ecec3d405bd5e945f157954618b169e60068fe/sfall/Modules/Scripting/Handlers/Utils.cpp#L34
static bool FalloutStringCompare(const char* str1, const char* str2, long codePage)
{
    while (true) {
        unsigned char c1 = *str1;
        unsigned char c2 = *str2;

        if (c1 == 0 && c2 == 0) return true; // end - strings are equal
        if (c1 == 0 || c2 == 0) return false; // strings are not equal
        str1++;
        str2++;
        if (c1 == c2) continue;

        if (codePage == 866) {
            // replace Russian 'x' with English (Fallout specific)
            if (c1 == 229) c1 -= 229 - 'x';
            if (c2 == 229) c2 -= 229 - 'x';
        }

        // 0 - 127 (standard ASCII)
        // upper to lower case
        if (c1 >= 'A' && c1 <= 'Z') c1 |= 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 |= 32;
        if (c1 == c2) continue;
        if (c1 < 128 || c2 < 128) return false;

        // 128 - 255 (international/extended)
        switch (codePage) {
        case 866:
            if (c1 != 149 && c2 != 149) { // code used for the 'bullet' character in Fallout font (the Russian letter 'X' uses Latin letter)
                // upper to lower case
                if (c1 >= 128 && c1 <= 159) {
                    c1 |= 32;
                } else if (c1 >= 224 && c1 <= 239) {
                    c1 -= 48; // shift lower range
                } else if (c1 == 240) {
                    c1++;
                }
                if (c2 >= 128 && c2 <= 159) {
                    c2 |= 32;
                } else if (c2 >= 224 && c2 <= 239) {
                    c2 -= 48; // shift lower range
                } else if (c2 == 240) {
                    c2++;
                }
            }
            break;
        case 1251:
            // upper to lower case
            if (c1 >= 0xC0 && c1 <= 0xDF) c1 |= 32;
            if (c2 >= 0xC0 && c2 <= 0xDF) c2 |= 32;
            if (c1 == 0xA8) c1 += 16;
            if (c2 == 0xA8) c2 += 16;
            break;
        case 1250:
        case 1252:
            if (c1 != 0xD7 && c1 != 0xF7 && c2 != 0xD7 && c2 != 0xF7) {
                if (c1 >= 0xC0 && c1 <= 0xDE) c1 |= 32;
                if (c2 >= 0xC0 && c2 <= 0xDE) c2 |= 32;
            }
            break;
        }
        if (c1 != c2) return false; // strings are not equal
    }
}

void mf_signal_close_game(OpcodeContext&)
{
    mainMenuRequestExit();
    _game_user_wants_to_quit = GAME_QUIT_REQUEST_MAIN_MENU;
}

void mf_string_compare(OpcodeContext& ctx)
{
    // compare str1 to str3 case insensitively
    // if args == 3, use FalloutStringCompare
    const char* str1 = ctx.stringArg(0);
    const char* str2 = ctx.stringArg(1);
    int codePage = 0;
    if (ctx.numArgs() == 3) {
        codePage = ctx.arg(2).asInt();
    }
    bool result = false;
    if (ctx.numArgs() < 3) {
        // default case-insensitive comparison
        result = compat_stricmp(str1, str2) == 0;
    } else {
        // Fallout specific case-insensitive comparison
        result = FalloutStringCompare(str1, str2, codePage);
    }
    if (result) {
        ctx.setReturn(1); // strings are equal
    } else {
        ctx.setReturn(0); // strings are not equal
    }
}

void mf_string_find(OpcodeContext& ctx)
{
    const char* str = ctx.stringArg(0);
    const char* substr = ctx.stringArg(1);
    int startPos = 0;

    if (ctx.numArgs() == 3) {
        startPos = ctx.arg(2).asInt();
    }

    if (startPos < 0 || startPos >= strlen(str)) {
        debugPrint("string_find: invalid start position %d", startPos);
        ctx.setReturn(-1);
        return;
    }

    const char* found = strstr(str + startPos, substr);
    if (found) {
        ctx.setReturn(static_cast<int>(found - str));
    } else {
        ctx.setReturn(-1);
    }
}

void mf_string_to_case(OpcodeContext& ctx)
{
    auto buf = ctx.stringArg(0);
    std::string s(buf);
    auto caseType = ctx.arg(1).asInt();
    if (caseType == 1) {
        // F2-06: Cast through unsigned char to avoid UB from negative values.
        // char is signed on x86/x64; bytes 0x80-0xFF sign-extend to negative int,
        // which is undefined behavior in <ctype.h> per C99 §7.4 ¶1.
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    } else if (caseType == 0) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    } else {
        // F-26: Use ctx.printError (visible to scripts) instead of debugPrint
        // (developer console only) so script code receives a diagnostic for
        // invalid case types.
        ctx.printError("string_to_case: invalid case type %d", caseType);
    }
    ctx.setReturn(s.c_str());
}

// string_replace(str, search, replace): returns a copy of str with all
// occurrences of search replaced by replace. Returns str unchanged if
// search is empty or not found.
// Uses the 5120-byte output buffer pattern for safety.
void mf_string_replace(OpcodeContext& ctx)
{
    const char* str = ctx.stringArg(0);
    const char* search = ctx.stringArg(1);
    const char* replace = ctx.stringArg(2);

    // Guard against empty search string to avoid infinite loops.
    if (strlen(search) == 0) {
        ctx.setReturn(str);
        return;
    }

    // Truncate result to 5120 bytes to match string_format behaviour.
    const int kMaxResultLen = 5120;
    std::string result;
    result.reserve(strlen(str) + strlen(replace));

    const char* pos = str;
    const char* found;
    size_t searchLen = strlen(search);

    while ((found = strstr(pos, search)) != nullptr) {
        // Append everything before the match.
        result.append(pos, found - pos);
        // Append the replacement.
        result.append(replace);
        // Advance past the search string to avoid infinite loops.
        pos = found + searchLen;
        // Safety: truncate if output exceeds max length.
        if (result.size() > (size_t)kMaxResultLen) {
            break;
        }
    }
    // Append the remainder after the last match.
    result.append(pos);

    if (result.size() > (size_t)kMaxResultLen) {
        result.resize(kMaxResultLen);
    }
    ctx.setReturn(result.c_str());
}

void mf_string_format(OpcodeContext& ctx)
{
    ProgramValue formatArgs[7];
    for (int index = 1; index < ctx.numArgs(); index++) {
        formatArgs[index - 1] = ctx.arg(index);
    }

    const char* format = ctx.stringArg(0);
    int args = ctx.numArgs();
    int fmtLen = static_cast<int>(strlen(format));
    if (fmtLen == 0) {
        ctx.setReturn("");
        return;
    }
    if (fmtLen > 1024) {
        debugPrint("%s(): format string exceeds maximum length of 1024 characters.", "string_format");
        ctx.setReturn("Error");
        return;
    }

    // Pre-calculate the expanded format string size.  Each '%' conversion
    // specifier may contain a '*' width/precision that expands to up to
    // 11 decimal digits (the full range of int32 including sign).  We size
    // the buffer for the worst case: fmtLen + (num_pct * 11).
    constexpr int MAX_INT_DIGITS = 11; // -2147483648
    int newFmtLen = fmtLen;
    for (int i = 0; i < fmtLen; i++) {
        if (format[i] == '%') newFmtLen += MAX_INT_DIGITS;
    }

    auto newFmt = std::make_unique<char[]>(newFmtLen + 1);

    bool conversion = false;
    int j = 0;
    int valIdx = 0;

    char out[5120 + 1] = { 0 };
    int bufCount = sizeof(out) - 1;
    char* outBuf = out;

    int numArgs = args;

    for (int i = 0; i < fmtLen; i++) {
        char c = format[i];
        if (!conversion) {
            if (c == '%') conversion = true;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '%') {
            int partLen;
            if (c == '%') {
                newFmt[j] = '\0';
                strncpy(outBuf, newFmt.get(), std::min(j, bufCount - 1));
                partLen = j;
            } else {
                if (c == 'h' || c == 'l' || c == 'j' || c == 'z' || c == 't' || c == 'w' || c == 'L' || c == 'I') continue;
                if (++valIdx == numArgs) {
                    debugPrint("%s() - format string contains more conversions than passed arguments (%d): %s",
                        "string_format", numArgs - 1, format);
                }
                // Use corresponding argument if available; otherwise use a default
                // sentinel to avoid repeating the last argument for excess conversions.
                static const ProgramValue kDefaultFormatArg;
                const auto& arg = (valIdx < numArgs) ? formatArgs[valIdx - 1] : kDefaultFormatArg;

                if (c == 'S' || c == 'Z') {
                    c = 's';
                }
                if ((c == 's' && !arg.isString()) || c == 'n') {
                    c = 'd';
                }
                newFmt[j++] = c;
                newFmt[j] = '\0';
                partLen = arg.isFloat()
                    ? snprintf(outBuf, bufCount, newFmt.get(), arg.floatValue)
                    : arg.isInt()    ? snprintf(outBuf, bufCount, newFmt.get(), arg.integerValue)
                    : arg.isString() ? snprintf(outBuf, bufCount, newFmt.get(), arg.asString(ctx.program()))
                                     : snprintf(outBuf, bufCount, newFmt.get(), "<UNSUPPORTED TYPE>");
                // snprintf returns what would have been written, which can exceed bufCount.
                // Clamp to prevent bufCount from going negative and causing buffer underflow.
                if (partLen < 0) {
                    partLen = 0;
                } else if (partLen > bufCount) {
                    partLen = bufCount;
                }
            }
            outBuf += partLen;
            bufCount -= partLen;
            conversion = false;
            j = 0;
            if (bufCount <= 0) {
                break;
            }
            continue;
        }
        // Handle dynamic width/precision specifiers: %* and %.*
        // '*' in a conversion specifier tells snprintf to read the width or
        // precision value from the variadic argument list. To avoid variadic
        // argument count mismatch UB (ISO C §7.21.6.5 ¶8), we convert the '*'
        // to a literal integer value read from the next format argument.
        // This produces a self-contained format string (e.g. "%8d" instead of
        // "%*d") that snprintf can process correctly with the remaining value
        // argument alone.
        if (c == '*') {
            // Read the width/precision integer from formatArgs[], convert to
            // decimal string, and append the digits in place of '*'.
            // F2-05: Guard valIdx < numArgs - 1 — formatArgs has numArgs-1
            // elements (format string at index 0 is excluded). With maxArgs=8,
            // numArgs can be 8, giving formatArgs only 7 valid entries (0..6).
            int widthVal = (valIdx < numArgs - 1)
                ? formatArgs[valIdx].asInt()
                : 0;
            // Write the value as decimal digits directly into newFmt[]
            // with bounds checking against the allocated buffer.
            int remaining = newFmtLen - j;
            int written = snprintf(newFmt.get() + j,
                remaining > 0 ? remaining + 1 : 0, "%d", widthVal);
            if (written > 0) {
                if (written > remaining) {
                    written = remaining;
                }
                j += written;
            }
            ++valIdx;
        } else {
            newFmt[j++] = c;
        }
    }

    if (bufCount > 0) {
        newFmt[j] = '\0';
        if (strlen(newFmt.get()) < bufCount) {
            strcpy(outBuf, newFmt.get());
        } else {
            strncpy(outBuf, newFmt.get(), bufCount - 1);
            outBuf[bufCount - 1] = '\0';
        }
    }

    ctx.setReturn(out);
}

// string_format_array(format, arrayId): formats a string using values from
// a sfall array. Reads array elements via LenArray()/GetArrayKey() and
// applies the same format processing as string_format.
void mf_string_format_array(OpcodeContext& ctx)
{
    const char* format = ctx.stringArg(0);
    int arrayId = ctx.arg(1).asInt();

    if (!ArrayExists(arrayId)) {
        debugPrint("%s(): array %d does not exist", ctx.name(), arrayId);
        ctx.setReturn("Error");
        return;
    }

    int fmtLen = static_cast<int>(strlen(format));
    if (fmtLen == 0) {
        ctx.setReturn("");
        return;
    }
    if (fmtLen > 1024) {
        debugPrint("%s(): format string exceeds maximum length of 1024 characters.", ctx.name());
        ctx.setReturn("Error");
        return;
    }

    int arrayLen = LenArray(arrayId);
    // Read array values into a local array (max 8 for safety).
    const int kMaxArgs = 8;
    ProgramValue formatArgs[kMaxArgs];
    int numArgs = arrayLen;
    if (numArgs > kMaxArgs) {
        numArgs = kMaxArgs;
    }
    for (int i = 0; i < numArgs; i++) {
        ProgramValue key = GetArrayKey(arrayId, i, ctx.program());
        formatArgs[i] = GetArray(arrayId, key, ctx.program());
    }

    // Pre-calculate the expanded format string size (same logic as mf_string_format).
    constexpr int MAX_INT_DIGITS = 11;
    int newFmtLen = fmtLen;
    for (int i = 0; i < fmtLen; i++) {
        if (format[i] == '%') newFmtLen += MAX_INT_DIGITS;
    }

    auto newFmt = std::make_unique<char[]>(newFmtLen + 1);

    bool conversion = false;
    int j = 0;
    int valIdx = 0;

    char out[5120 + 1] = { 0 };
    int bufCount = sizeof(out) - 1;
    char* outBuf = out;

    for (int i = 0; i < fmtLen; i++) {
        char c = format[i];
        if (!conversion) {
            if (c == '%') conversion = true;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '%') {
            int partLen;
            if (c == '%') {
                newFmt[j] = '\0';
                strncpy(outBuf, newFmt.get(), std::min(j, bufCount - 1));
                partLen = j;
            } else {
                if (c == 'h' || c == 'l' || c == 'j' || c == 'z' || c == 't' || c == 'w' || c == 'L' || c == 'I') continue;
                if (++valIdx == numArgs + 1) {
                    debugPrint("%s() - format string contains more conversions than array elements (%d): %s",
                        ctx.name(), numArgs, format);
                }
                static const ProgramValue kDefaultFormatArg;
                const auto& arg = (valIdx <= numArgs) ? formatArgs[valIdx - 1] : kDefaultFormatArg;

                if (c == 'S' || c == 'Z') {
                    c = 's';
                }
                if ((c == 's' && !arg.isString()) || c == 'n') {
                    c = 'd';
                }
                newFmt[j++] = c;
                newFmt[j] = '\0';
                partLen = arg.isFloat()
                    ? snprintf(outBuf, bufCount, newFmt.get(), arg.floatValue)
                    : arg.isInt()    ? snprintf(outBuf, bufCount, newFmt.get(), arg.integerValue)
                    : arg.isString() ? snprintf(outBuf, bufCount, newFmt.get(), arg.asString(ctx.program()))
                                     : snprintf(outBuf, bufCount, newFmt.get(), "<UNSUPPORTED TYPE>");
                if (partLen < 0) {
                    partLen = 0;
                } else if (partLen > bufCount) {
                    partLen = bufCount;
                }
            }
            outBuf += partLen;
            bufCount -= partLen;
            conversion = false;
            j = 0;
            if (bufCount <= 0) {
                break;
            }
            continue;
        }
        if (c == '*') {
            int widthVal = (valIdx < numArgs)
                ? formatArgs[valIdx].asInt()
                : 0;
            int remaining = newFmtLen - j;
            int written = snprintf(newFmt.get() + j,
                remaining > 0 ? remaining + 1 : 0, "%d", widthVal);
            if (written > 0) {
                if (written > remaining) {
                    written = remaining;
                }
                j += written;
            }
            ++valIdx;
        } else {
            newFmt[j++] = c;
        }
    }

    if (bufCount > 0) {
        newFmt[j] = '\0';
        if (strlen(newFmt.get()) < bufCount) {
            strcpy(outBuf, newFmt.get());
        } else {
            strncpy(outBuf, newFmt.get(), bufCount - 1);
            outBuf[bufCount - 1] = '\0';
        }
    }

    ctx.setReturn(out);
}

void mf_floor2(OpcodeContext& ctx)
{
    ctx.setReturn(static_cast<int>(floor(ctx.arg(0).asFloat())));
}

// --- Rotators fork compatibility wrappers (C-06) ---

// r_message_box: CE-native wrapper that delegates to the existing message_box handler.
// Rotators scripts use r_message_box/flags/text instead of message_box; the handler
// logic is identical.
void mf_r_message_box(OpcodeContext& ctx)
{
    mf_message_box(ctx);
}

// r_get_ini_string(file, section, key, defaultValue): reads an INI value and returns
// the default if the key is not found. In Rotators, this wraps the read_byte memory
// patching at 0x410003. In CE, we use the native sfall_ini_get_int/sfall_ini_get_string
// API and return the defaultValue on lookup failure.
void mf_r_get_ini_string(OpcodeContext& ctx)
{
    const char* file = ctx.stringArg(0);
    const char* section = ctx.stringArg(1);
    const char* key = ctx.stringArg(2);

    // Build "fileName|section|key" triplet for the sfall INI API.
    char triplet[512];
    snprintf(triplet, sizeof(triplet), "%s|%s|%s", file, section, key);

    // Try integer value first.
    int intValue;
    if (sfall_ini_get_int(triplet, &intValue)) {
        ctx.setReturn(intValue);
        return;
    }

    // Try string value. sfall_ini_get_string returns true even for missing keys
    // (because the parser handles the triplet format correctly), so we must
    // additionally check that the returned string is non-empty to confirm the
    // key was actually found. An empty result means the key doesn't exist.
    char stringValue[256];
    if (sfall_ini_get_string(triplet, stringValue, sizeof(stringValue))
        && stringValue[0] != '\0') {
        ctx.setReturn(stringValue);
        return;
    }

    // Not found — return the defaultValue argument (int or string).
    ctx.setReturn(ctx.arg(3));
}

// r_write(type, addr, val): Rotators fork runtime memory write.
// type: 0=byte, 1=short, 2=int, 3=string. CE cannot dereference arbitrary
// engine addresses — the handler logs the call and returns a no-op, matching
// the existing VOODOO write_byte/write_short/write_int pattern in sfall_opcodes.cc.
// ET Tu scripts use r_write for runtime memory patching (e.g., gl_iu_warning.ssl,
// gl_x32dbg_fix.ssl). Returning 0 signals "success" to the script so execution
// continues without mod-level logic forks.
void mf_r_write(OpcodeContext& ctx)
{
    int type = ctx.arg(0).asInt();
    int addr = ctx.arg(1).asInt();
    int value = ctx.arg(2).asInt();

    debugPrint("r_write(type=%d, addr=0x%08X, value=%d) — no-op in CE engine\n", type, addr, value);
    ctx.setReturn(0);
}

// F2-24: Explosion metarule wrapper — provides metarule-level access to the
// 9 explosion sub-operations that are currently only reachable via opcode 0x8261
// (op_explosions_metarule in sfall_opcodes.cc). Adding this entry makes
// metarule_exist("metarule2_explosions") return 1 and get_metarule_table()
// include it, consistent with opcode_exists(0x8261).
//
// Sub-metarule constants (mirroring ExplosionMetarule enum in sfall_opcodes.cc):
enum {
    EXPL_MF_FORCE_PATTERN     = 1,
    EXPL_MF_FORCE_ART         = 2,
    EXPL_MF_FORCE_RADIUS      = 3,
    EXPL_MF_FORCE_DMGTYPE     = 4,
    EXPL_MF_STATIC_RADIUS     = 5,
    EXPL_MF_GET_DAMAGE        = 6,
    EXPL_MF_SET_DYNAMITE_DMG  = 7,
    EXPL_MF_SET_PLASTIC_DMG   = 8,
    EXPL_MF_SET_MAX_TARGET    = 9,
};

void mf_explosions_metarule(OpcodeContext& ctx)
{
    int metarule = ctx.arg(0).asInt();
    int param1 = ctx.arg(1).asInt();
    int param2 = ctx.numArgs() > 2 ? ctx.arg(2).asInt() : 0;

    switch (metarule) {
    case EXPL_MF_FORCE_PATTERN:
        // F-13: Wire param1/param2 as actual explosion rotation parameters.
        // param1 = start rotation, param2 = end rotation.
        // When param2 is 0 (legacy single-param call), preserve backward compat:
        // param1 != 0 → (2,4), param1 == 0 → (0,6) — matching sfall 4.x defaults.
        if (param2 != 0) {
            explosionSetPattern(param1, param2);
        } else if (param1 != 0) {
            explosionSetPattern(2, 4);
        } else {
            explosionSetPattern(0, 6);
        }
        ctx.setReturn(0);
        break;
    case EXPL_MF_FORCE_ART:
        explosionSetFrm(param1);
        ctx.setReturn(0);
        break;
    case EXPL_MF_FORCE_RADIUS:
        explosionSetRadius(param1);
        ctx.setReturn(0);
        break;
    case EXPL_MF_FORCE_DMGTYPE:
        explosionSetDamageType(param1);
        ctx.setReturn(0);
        break;
    case EXPL_MF_STATIC_RADIUS:
        weaponSetGrenadeExplosionRadius(param1);
        weaponSetRocketExplosionRadius(param2);
        ctx.setReturn(0);
        break;
    case EXPL_MF_GET_DAMAGE: {
        int minDamage = 0;
        int maxDamage = 0;
        explosiveGetDamage(param1, &minDamage, &maxDamage);

        ArrayId arrayId = CreateTempArray(2, 0);
        SetArray(arrayId, ProgramValue { 0 }, ProgramValue { minDamage }, false, ctx.program());
        SetArray(arrayId, ProgramValue { 1 }, ProgramValue { maxDamage }, false, ctx.program());

        ctx.setReturn(ProgramValue(arrayId));
        break;
    }
    case EXPL_MF_SET_DYNAMITE_DMG:
        explosiveSetDamage(PROTO_ID_DYNAMITE_I, param1, param2);
        ctx.setReturn(0);
        break;
    case EXPL_MF_SET_PLASTIC_DMG:
        explosiveSetDamage(PROTO_ID_PLASTIC_EXPLOSIVES_I, param1, param2);
        ctx.setReturn(0);
        break;
    case EXPL_MF_SET_MAX_TARGET:
        explosionSetMaxTargets(param1);
        ctx.setReturn(0);
        break;
    default:
        // F-27: Log unknown sub-metarule to help developers diagnose
        // mistyped metarule2_explosions calls. Returns 0 safely.
        debugPrint("%s(): unknown sub-metarule %d", ctx.name(), metarule);
        ctx.setReturn(0);
        break;
    }
}

// --- New metarule handlers (C-14, H-02 through H-09, M-18) ---

// npc_engine_level_up(int enable): controls auto-leveling of NPC party members.
// 0 = disable, 1 = enable (default). CE calls _partyMemberIncLevels() from the
// level-up pipeline; scripts can use this flag to gate that call.
void mf_npc_engine_level_up(OpcodeContext& ctx)
{
    int enable = ctx.arg(0).asInt();
    gNpcEngineLevelUpEnabled = (enable != 0) ? 1 : 0;
    ctx.setReturn(0);
}

// set_dude_obj(Object* newDude): saves the current gDude pointer and replaces it
// with the given object. Used for cutscene player swaps (e.g., controlling another
// character temporarily). Call real_dude_obj() to restore the original.
// Guard: only save gDude to gSavedOriginalDude on the first call — subsequent
// chained swaps (e.g., two cutscene transitions in sequence) should not overwrite
// the original reference.
void mf_set_dude_obj(OpcodeContext& ctx)
{
    Object* newDude = ctx.arg(0).asObject();
    if (newDude == nullptr) {
        ctx.printError("%s() - null object argument.", ctx.name());
        ctx.setReturn(-1);
        return;
    }
    // Only save the original on first call; chained swaps preserve the real dude.
    if (gSavedOriginalDude == nullptr) {
        gSavedOriginalDude = gDude;
    }
    gDude = newDude;
    ctx.setReturn(0);
}

// real_dude_obj(): returns the original gDude pointer saved by set_dude_obj().
// Returns null if set_dude_obj() has never been called.
void mf_real_dude_obj(OpcodeContext& ctx)
{
    ctx.setReturn(gSavedOriginalDude);
}

// set_object_data(Object* obj, int offset, int value): writes an int value at the
// given byte offset within the Object struct. Uses a SAFETY WHITELIST — only offsets
// corresponding to known-safe primitive int fields are allowed. Arbitrary memory writes
// are rejected with error -1.
//
// Safe offsets map to the first 11 contiguous int fields in the Object struct:
//   id(0), tile(4), x(8), y(12), sx(16), sy(20), frame(24), rotation(28),
//   fid(32), flags(36), elevation(40)
void mf_set_object_data(OpcodeContext& ctx)
{
    Object* ptr = ctx.arg(0).asObject();
    int rawOffset = ctx.arg(1).asInt();
    int value = ctx.arg(2).asInt();

    // asObject() returns nullptr for integer 0 and non-pointer types.
    // Guard against null dereference — peer functions in this file
    // have the same guard.
    if (ptr == nullptr) {
        ctx.printError("%s(): object is null (asObject() returned nullptr)", ctx.name());
        ctx.setReturn(-1);
        return;
    }

    if (rawOffset < 0 || rawOffset % 4 != 0) {
        ctx.printError("%s(): bad offset %d (must be non-negative, multiple of 4)", ctx.name(), rawOffset);
        ctx.setReturn(-1);
        return;
    }

    // Safety whitelist: only allow writes to the first 11 primitive int fields
    // of the Object struct. All other offsets (including the complex ObjectData
    // member at offset 44+) are rejected to prevent memory corruption.
    //
    // NOTE: These offsets assume the Object struct layout:
    //   int id, tile, x, y, sx, sy, frame, rotation, fid, flags, elevation;
    //   ObjectData data; // starts at offset 44
    // If the struct layout changes, this whitelist must be updated.
    static const int kSafeOffsets[] = {
        0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40
    };
    static const int kSafeOffsetCount = sizeof(kSafeOffsets) / sizeof(kSafeOffsets[0]);

    bool allowed = false;
    for (int i = 0; i < kSafeOffsetCount; i++) {
        if (rawOffset == kSafeOffsets[i]) {
            allowed = true;
            break;
        }
    }

    if (!allowed) {
        ctx.printError("%s(): offset %d is not in the safety whitelist. Only primitive int fields (0-40 step 4) are writable.", ctx.name(), rawOffset);
        ctx.setReturn(-1);
        return;
    }

    *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(ptr) + rawOffset) = value;
    ctx.setReturn(0);
}

// set_terrain_name(int x, int y, string name): stores a terrain name override for
// the given worldmap coordinates. The override is returned by get_terrain_name().
void mf_set_terrain_name(OpcodeContext& ctx)
{
    int x = ctx.arg(0).asInt();
    int y = ctx.arg(1).asInt();
    const char* name = ctx.stringArg(2);
    gTerrainNameOverrides[{ x, y }] = name;
}

// get_terrain_name(int x, int y): returns the terrain name override for the given
// worldmap coordinates, or an empty string if no override has been set.
void mf_get_terrain_name(OpcodeContext& ctx)
{
    if (ctx.numArgs() < 2) {
        // Zero-arg form: return the built-in terrain type name at the party's
        // current worldmap position.
        ctx.setReturn(wmGetPartyTerrainName());
        return;
    }

    int x = ctx.arg(0).asInt();
    int y = ctx.arg(1).asInt();
    auto it = gTerrainNameOverrides.find({ x, y });
    if (it != gTerrainNameOverrides.end()) {
        ctx.setReturn(it->second.c_str());
    } else {
        ctx.setReturn("");
    }
}

// set_worldmap_heal_time(int time): sets the override for how much HP is healed per
// time unit during worldmap travel. -1 = use default.
// Wired to both sfall-local storage (save/load persistence) and the worldmap
// healing system via wmSetWorldmapHealTime() (consumed at worldmap.cc:3258).
void mf_set_worldmap_heal_time(OpcodeContext& ctx)
{
    int hours = ctx.arg(0).asInt();
    gWorldmapHealTime = hours;
    wmSetWorldmapHealTime(hours);
    ctx.setReturn(0);
}

// set_rest_heal_time(int time): sets the override for how much HP is healed per
// time unit during resting. -1 = use default.
// Wired to both sfall-local storage (save/load persistence) and the worldmap
// healing system via wmSetRestHealTime() (consumed by worldmap.cc rest logic).
void mf_set_rest_heal_time(OpcodeContext& ctx)
{
    int hours = ctx.arg(0).asInt();
    gRestHealTime = hours;
    wmSetRestHealTime(hours);
    ctx.setReturn(0);
}

// set_quest_failure_value(int gvar, int threshold): stores a mapping from a GVAR
// number to a failure threshold. When the GVAR reaches the threshold, the quest
// is considered failed. This is informational — scripts check the GVAR themselves;
// CE stores the mapping for script-level querying.
void mf_set_quest_failure_value(OpcodeContext& ctx)
{
    int gvar = ctx.arg(0).asInt();
    int threshold = ctx.arg(1).asInt();
    gQuestFailureValues[gvar] = threshold;
    ctx.setReturn(0);
}

// get_quest_failure_value(int gvar): returns the failure threshold for the given
// GVAR, or -1 if no threshold has been set. Mirrors the set_quest_failure_value
// metarule — provides a script-level query API for the stored mapping.
void mf_get_quest_failure_value(OpcodeContext& ctx)
{
    int gvar = ctx.arg(0).asInt();
    auto it = gQuestFailureValues.find(gvar);
    if (it != gQuestFailureValues.end()) {
        ctx.setReturn(it->second);
    } else {
        ctx.setReturn(-1);
    }
}

// set_scr_name(string name): overrides the current script's display name.
// Call with 0 args to clear the override.
// CE's scriptsGetFileName() returns the script file name; the override is
// stored here for future integration with script name display logic.
void mf_set_scr_name(OpcodeContext& ctx)
{
    if (ctx.numArgs() == 0) {
        gScriptNameOverride.clear();
    } else {
        gScriptNameOverride = ctx.stringArg(0);
    }
    ctx.setReturn(0);
}

// has_fake_perk_npc(Object* critter, string name): returns 1 if the given NPC
// critter has the named fake perk, 0 otherwise. CE's has_fake_perk opcode
// (0x81C1) only works for the player; this extends the check to arbitrary critters.
void mf_has_fake_perk_npc(OpcodeContext& ctx)
{
    Object* critter = ctx.arg(0).asObject();
    const char* name = ctx.stringArg(1);

    if (critter == nullptr) {
        ctx.setReturn(0);
        return;
    }
    auto it = gFakePerksNpc.find(critter->cid);
    if (it != gFakePerksNpc.end() && it->second.find(name) != it->second.end()) {
        ctx.setReturn(1);
    } else {
        ctx.setReturn(0);
    }
}

// has_fake_trait_npc(Object* critter, string name): returns 1 if the given NPC
// critter has the named fake trait, 0 otherwise. Checks both the NPC-keyed
// gFakeTraitsNpc set (populated via set_fake_trait_npc) and, when critter is
// the player, the gAddedTraits set (populated via add_trait metarule). The
// gAddedTraits bridge converts trait type IDs to display names via traitGetName().
void mf_has_fake_trait_npc(OpcodeContext& ctx)
{
    Object* critter = ctx.arg(0).asObject();
    const char* name = ctx.stringArg(1);

    if (critter == nullptr || name == nullptr) {
        ctx.setReturn(0);
        return;
    }
    // Check NPC-keyed fake traits (set via set_fake_trait_npc).
    auto it = gFakeTraitsNpc.find(critter->cid);
    if (it != gFakeTraitsNpc.end() && it->second.find(name) != it->second.end()) {
        ctx.setReturn(1);
        return;
    }
    // Bridge: when queried for the player character, also check traits
    // added via the add_trait() metarule (gAddedTraits stores int trait IDs).
    if (critter == gDude) {
        for (const auto& entry : gAddedTraits) {
            int traitId = entry.first;
            char* traitName = traitGetName(traitId);
            if (traitName != nullptr && strcmp(traitName, name) == 0) {
                ctx.setReturn(1);
                return;
            }
        }
    }
    ctx.setReturn(0);
}

// set_fake_perk_npc(Object* critter, string name, int level, int image, string desc):
// stores a fake perk for the given NPC critter. All metadata (level, image, desc)
// is preserved in the data structure. To remove a fake perk, set level=0.
void mf_set_fake_perk_npc(OpcodeContext& ctx)
{
    Object* critter = ctx.arg(0).asObject();
    const char* name = ctx.stringArg(1);
    int level = ctx.arg(2).asInt();
    int image = ctx.arg(3).asInt();
    const char* desc = ctx.stringArg(4);

    if (critter == nullptr) {
        ctx.printError("%s() - null object argument.", ctx.name());
        ctx.setReturn(-1);
        return;
    }
    if (level == 0) {
        auto it = gFakePerksNpc.find(critter->cid);
        if (it != gFakePerksNpc.end()) {
            it->second.erase(name);
        }
    } else {
        gFakePerksNpc[critter->cid][name] = { name, level, image, desc ? desc : "" };
    }
    ctx.setReturn(0);
}

// set_fake_trait_npc(Object* critter, string name, int active, int image, string desc):
// stores a fake trait for the given NPC critter with full metadata. Set active=0 to remove.
void mf_set_fake_trait_npc(OpcodeContext& ctx)
{
    Object* critter = ctx.arg(0).asObject();
    const char* name = ctx.stringArg(1);
    int active = ctx.arg(2).asInt();
    int image = ctx.arg(3).asInt();
    const char* desc = ctx.stringArg(4);

    if (critter == nullptr) {
        ctx.printError("%s() - null object argument.", ctx.name());
        ctx.setReturn(-1);
        return;
    }
    if (active == 0) {
        auto it = gFakeTraitsNpc.find(critter->cid);
        if (it != gFakeTraitsNpc.end()) {
            it->second.erase(name);
        }
    } else {
        gFakeTraitsNpc[critter->cid][name] = { name, active, image, desc ? desc : "" };
    }
    ctx.setReturn(0);
}

// set_selectable_perk_npc(Object* critter, string name, int active, int image, string desc):
// stores a selectable fake perk for the given NPC critter with full metadata. Set active=0 to remove.
void mf_set_selectable_perk_npc(OpcodeContext& ctx)
{
    Object* critter = ctx.arg(0).asObject();
    const char* name = ctx.stringArg(1);
    int active = ctx.arg(2).asInt();
    int image = ctx.arg(3).asInt();
    const char* desc = ctx.stringArg(4);

    if (critter == nullptr) {
        ctx.printError("%s() - null object argument.", ctx.name());
        ctx.setReturn(-1);
        return;
    }
    if (active == 0) {
        auto it = gFakeSelectablePerksNpc.find(critter->cid);
        if (it != gFakeSelectablePerksNpc.end()) {
            it->second.erase(name);
        }
    } else {
        gFakeSelectablePerksNpc[critter->cid][name] = { name, active, image, desc ? desc : "" };
    }
    ctx.setReturn(0);
}

// get_fake_perk_npc(Object* critter, string name): returns a 3-element temp
// array [level, image, desc] for the given fake perk, or 0 if not found.
void mf_get_fake_perk_npc(OpcodeContext& ctx)
{
    Object* critter = ctx.arg(0).asObject();
    const char* name = ctx.stringArg(1);

    if (critter == nullptr) {
        ctx.setReturn(0);
        return;
    }
    auto critIt = gFakePerksNpc.find(critter->cid);
    if (critIt == gFakePerksNpc.end()) {
        ctx.setReturn(0);
        return;
    }
    auto it = critIt->second.find(name);
    if (it == critIt->second.end()) {
        ctx.setReturn(0);
        return;
    }

    const FakePerkNpcEntry& entry = it->second;
    ArrayId arrayId = CreateTempArray(3, 0);
    SetArray(arrayId, ProgramValue(0), ProgramValue(entry.level), false, ctx.program());
    SetArray(arrayId, ProgramValue(1), ProgramValue(entry.image), false, ctx.program());
    SetArray(arrayId, ProgramValue(2), programMakeString(ctx.program(), entry.desc.c_str()), false, ctx.program());
    ctx.setReturn(ProgramValue(arrayId));
}

// get_fake_trait_npc(Object* critter, string name): returns a 3-element temp
// array [level, image, desc] for the given fake trait, or 0 if not found.
void mf_get_fake_trait_npc(OpcodeContext& ctx)
{
    Object* critter = ctx.arg(0).asObject();
    const char* name = ctx.stringArg(1);

    if (critter == nullptr) {
        ctx.setReturn(0);
        return;
    }
    auto critIt = gFakeTraitsNpc.find(critter->cid);
    if (critIt == gFakeTraitsNpc.end()) {
        ctx.setReturn(0);
        return;
    }
    auto it = critIt->second.find(name);
    if (it == critIt->second.end()) {
        ctx.setReturn(0);
        return;
    }

    const FakePerkNpcEntry& entry = it->second;
    ArrayId arrayId = CreateTempArray(3, 0);
    SetArray(arrayId, ProgramValue(0), ProgramValue(entry.level), false, ctx.program());
    SetArray(arrayId, ProgramValue(1), ProgramValue(entry.image), false, ctx.program());
    SetArray(arrayId, ProgramValue(2), programMakeString(ctx.program(), entry.desc.c_str()), false, ctx.program());
    ctx.setReturn(ProgramValue(arrayId));
}

// get_selectable_perk_npc(Object* critter, string name): returns a 3-element
// temp array [level, image, desc] for the given selectable perk, or 0 if not
// found.
void mf_get_selectable_perk_npc(OpcodeContext& ctx)
{
    Object* critter = ctx.arg(0).asObject();
    const char* name = ctx.stringArg(1);

    if (critter == nullptr) {
        ctx.setReturn(0);
        return;
    }
    auto critIt = gFakeSelectablePerksNpc.find(critter->cid);
    if (critIt == gFakeSelectablePerksNpc.end()) {
        ctx.setReturn(0);
        return;
    }
    auto it = critIt->second.find(name);
    if (it == critIt->second.end()) {
        ctx.setReturn(0);
        return;
    }

    const FakePerkNpcEntry& entry = it->second;
    ArrayId arrayId = CreateTempArray(3, 0);
    SetArray(arrayId, ProgramValue(0), ProgramValue(entry.level), false, ctx.program());
    SetArray(arrayId, ProgramValue(1), ProgramValue(entry.image), false, ctx.program());
    SetArray(arrayId, ProgramValue(2), programMakeString(ctx.program(), entry.desc.c_str()), false, ctx.program());
    ctx.setReturn(ProgramValue(arrayId));
}

// has_selectable_perk_npc(Object* critter, string name): returns 1 if the given
// NPC critter has the named selectable perk, 0 otherwise.
void mf_has_selectable_perk_npc(OpcodeContext& ctx)
{
    Object* critter = ctx.arg(0).asObject();
    const char* name = ctx.stringArg(1);

    if (critter == nullptr) {
        ctx.setReturn(0);
        return;
    }
    auto it = gFakeSelectablePerksNpc.find(critter->cid);
    if (it != gFakeSelectablePerksNpc.end() && it->second.find(name) != it->second.end()) {
        ctx.setReturn(1);
    } else {
        ctx.setReturn(0);
    }
}

// F-002: 14 HIGH-priority metarules (needed by RPU/ET Tu)
//
// These were all commented out in kMetarules[]. Each handler is implemented
// to the extent possible with existing engine APIs. Handlers that require
// deeper engine integration (e.g., timer events, AI data, stat limits) are
// implemented as stubs that log a debug message and return a safe default.
// Full implementations are TODO and tracked in scan_unimplemented_sfall.h.

// exec_map_update_scripts(): triggers execution of all pending map update scripts.
// Wires directly to sfall_gl_scr_exec_map_update_scripts() in the global scripts module.
// 0 args, returns 0 on success.
void mf_exec_map_update_scripts(OpcodeContext& ctx)
{
    sfall_gl_scr_exec_map_update_scripts(SCRIPT_PROC_MAP_UPDATE); // 23 = map_update procedure
    ctx.setReturn(0);
}

// get_can_rest_on_map(int mapElevation, int tile): returns whether resting is
// allowed at the given coordinates. Consults the worldmap per-tile override via
// wmGetCanRestOnTile() (set via set_can_rest_on_map metarule), falling back to
// the elevation-level check wmMapCanRestHere() if no per-tile override exists.
void mf_get_can_rest_on_map(OpcodeContext& ctx)
{
    int elevation = ctx.arg(0).asInt();
    int tile = ctx.arg(1).asInt();
    bool canRest;
    // Per-tile override takes precedence; wmGetCanRestOnTile defaults to true
    // when no explicit override has been set.
    if (!wmGetCanRestOnTile(elevation, tile)) {
        canRest = false;
    } else {
        canRest = wmMapCanRestHere(elevation);
    }
    ctx.setReturn(canRest ? 1 : 0);
}

// get_current_inven_size(Object* obj): returns the number of items in the
// object's inventory (-2 slot). Wires to object->data.inventory.length.
void mf_get_current_inven_size(OpcodeContext& ctx)
{
    Object* obj = ctx.arg(0).asObject();
    ctx.setReturn(obj->data.inventory.length);
}

// get_map_enter_position(): returns the override map entry position set by
// set_map_enter_position() as a 3-element temp array [x, y, elevation].
// Returns 0 if no override has been set.
void mf_get_map_enter_position(OpcodeContext& ctx)
{
    if (gMapEnterX == -1) {
        ctx.setReturn(0);
        return;
    }

    ArrayId arrayId = CreateTempArray(3, 0);
    SetArray(arrayId, ProgramValue(0), ProgramValue(gMapEnterX), false, ctx.program());
    SetArray(arrayId, ProgramValue(1), ProgramValue(gMapEnterY), false, ctx.program());
    SetArray(arrayId, ProgramValue(2), ProgramValue(gMapEnterElevation), false, ctx.program());
    ctx.setReturn(ProgramValue(arrayId));
}

// get_metarule_table(): returns an array of all registered metarule names.
// This is a static list that can be enumerated by scripts.
// F2-25 + F2-26: get_metarule_table must produce results consistent with
// metarule_exist(). metarule_exist() returns 0 for "r_write" (stub blacklist)
// and 1 for "rotators"/"sfall" (hardcoded sentinels). get_metarule_table must
// match: skip "r_write", include "rotators" and "sfall".
void mf_get_metarule_table(OpcodeContext& ctx)
{
    // Count: kMetarules entries minus blacklisted "r_write" + 2 sentinels
    int totalCount = static_cast<int>(kMetarulesCount) + 2; // +2 for sentinels
    // Subtract 1 for "r_write" (blacklisted stub in metarule_exist)
    totalCount -= 1; // "r_write" is always present in kMetarules[]

    ArrayId arrayId = CreateTempArray(totalCount, 0);
    int arrayIndex = 0;
    for (int i = 0; i < static_cast<int>(kMetarulesCount); i++) {
        // F2-25: Skip "r_write" — metarule_exist returns 0 for this stub for consistency.
        if (compat_stricmp(kMetarules[i].name, "r_write") == 0) {
            continue;
        }
        SetArray(arrayId, ProgramValue(arrayIndex),
                 programMakeString(ctx.program(), kMetarules[i].name), false, ctx.program());
        arrayIndex++;
    }
    // F2-26: Include "rotators" and "sfall" sentinels — metarule_exist returns 1 for both.
    SetArray(arrayId, ProgramValue(arrayIndex),
             programMakeString(ctx.program(), "rotators"), false, ctx.program());
    arrayIndex++;
    SetArray(arrayId, ProgramValue(arrayIndex),
             programMakeString(ctx.program(), "sfall"), false, ctx.program());

    ctx.setReturn(ProgramValue(arrayId));
}

// get_object_ai_data(Object* obj, int dataType): returns AI-related data from
// the object. The following data types are defined:
//   0 = AI packet number (obj->data.critter.combat.aiPacket)
//   1 = AI packet state flags (requires AiPacket struct internals — TODO)
//   2 = current AI procedure (requires combat_ai integration — TODO)
void mf_get_object_ai_data(OpcodeContext& ctx)
{
    Object* obj = ctx.arg(0).asObject();
    int dataType = ctx.arg(1).asInt();

    if (PID_TYPE(obj->pid) != OBJ_TYPE_CRITTER) {
        ctx.setReturn(0);
        return;
    }

    switch (dataType) {
    case 0: // AI packet number — direct field access
        ctx.setReturn(obj->data.critter.combat.aiPacket);
        return;
    case 1: // AI packet state flags — wired to aiPacketGetFlags() in combat_ai.cc
        ctx.setReturn(aiPacketGetFlags(obj));
        return;
    case 2: // current AI procedure — wired to aiPacketGetProcedure() in combat_ai.cc
        {
            int procedure = aiPacketGetProcedure(obj);
            if (procedure < 0) {
                ctx.setReturn(0);
            } else {
                ctx.setReturn(procedure);
            }
        }
        return;
    default:
        debugPrint("%s(): unknown dataType %d", ctx.name(), dataType);
        break;
    }
    ctx.setReturn(0);
}

// get_stat_max(int stat, int isNpc): returns the current maximum value for a stat.
// Uses statGetMaxValue() to read the live (possibly overridden by set_stat_max)
// value from gStatDescriptions[], not a static default table. Previously used
// kDefaultStatLimits which ignored dynamic overrides.
void mf_get_stat_max(OpcodeContext& ctx)
{
    int stat = ctx.arg(0).asInt();
    bool isNpc = ctx.numArgs() > 1 && ctx.arg(1).asInt() != 0;
    (void)isNpc;
    if (!statIsValid(stat)) {
        debugPrint("%s(): invalid stat %d", ctx.name(), stat);
        ctx.setReturn(-1);
        return;
    }
    ctx.setReturn(statGetMaxValue(stat));
}

// get_stat_min(int stat, int isNpc): returns the current minimum value for a stat.
// Uses statGetMinValue() to read the live (possibly overridden by set_stat_min)
// value from gStatDescriptions[], not a static default table.
void mf_get_stat_min(OpcodeContext& ctx)
{
    int stat = ctx.arg(0).asInt();
    bool isNpc = ctx.numArgs() > 1 && ctx.arg(1).asInt() != 0;
    (void)isNpc;
    if (!statIsValid(stat)) {
        debugPrint("%s(): invalid stat %d", ctx.name(), stat);
        ctx.setReturn(-1);
        return;
    }
    ctx.setReturn(statGetMinValue(stat));
}

// item_make_explosive(int pid, int pattern, int radius, int delay):
// marks an item as explosive with the given parameters. The explosive
// properties are stored for future integration with the explosion system.
// Full integration requires wiring gExplosiveOverrides into the engine's
// explosiveIsExplosive() / explosiveActivate() paths in item.cc.
void mf_item_make_explosive(OpcodeContext& ctx)
{
    int pid = ctx.arg(0).asInt();
    int pattern = ctx.arg(1).asInt();
    int radius = ctx.arg(2).asInt();
    int delay = ctx.numArgs() > 3 ? ctx.arg(3).asInt() : 0;
    int minDamage = ctx.numArgs() > 4 ? ctx.arg(4).asInt() : 0;
    int maxDamage = ctx.numArgs() > 5 ? ctx.arg(5).asInt() : 0;

    // Guard: zero-damage explosive overrides produce a user-visible behavioral
    // bug (explosion with no damage). Reject overrides where both damage values
    // are zero unless the explosion has a non-zero delay (timed explosive).
    if (minDamage <= 0 && maxDamage <= 0 && delay <= 0) {
        debugPrint("%s(pid=%d): rejected — zero damage with no delay would produce a harmless explosion",
            ctx.name(), pid);
        ctx.setReturn(0);
        return;
    }

    ExplosiveProperties props = { pattern, radius, delay, minDamage, maxDamage };
    gExplosiveOverrides[pid] = props;

    ctx.setReturn(1);
}

// get_explosive_data(int pid): returns a 5-element temp array
// [pattern, radius, delay, minDamage, maxDamage] for the given item
// proto ID, or 0 if no explosive override has been set.
// Provides a script-level query API for the stored explosive properties.
void mf_get_explosive_data(OpcodeContext& ctx)
{
    int pid = ctx.arg(0).asInt();
    auto it = gExplosiveOverrides.find(pid);
    if (it == gExplosiveOverrides.end()) {
        ctx.setReturn(0);
        return;
    }

    ArrayId arrayId = CreateTempArray(5, 0);
    SetArray(arrayId, ProgramValue(0), ProgramValue(it->second.pattern), false, ctx.program());
    SetArray(arrayId, ProgramValue(1), ProgramValue(it->second.radius), false, ctx.program());
    SetArray(arrayId, ProgramValue(2), ProgramValue(it->second.delay), false, ctx.program());
    SetArray(arrayId, ProgramValue(3), ProgramValue(it->second.minDamage), false, ctx.program());
    SetArray(arrayId, ProgramValue(4), ProgramValue(it->second.maxDamage), false, ctx.program());
    ctx.setReturn(ProgramValue(arrayId));
}

// lock_is_jammed(Object* obj): returns whether the given object's lock is
// jammed (stuck in locked state, un-pickable). Returns 1 if jammed, 0 otherwise.
// Checks the engine's OBJ_JAMMED flag on both scenery doors and containers.
// The engine has a full lock-jam infrastructure (objectJamLock, objectUnjamLock)
// with op_jam_lock (0x814D) available to scripts — this metarule lets scripts
// query the state that the engine already tracks.
void mf_lock_is_jammed(OpcodeContext& ctx)
{
    Object* obj = ctx.arg(0).asObject();

    if (obj == nullptr) {
        ctx.setReturn(0);
        return;
    }

    bool jammed = false;
    if (PID_TYPE(obj->pid) == OBJ_TYPE_SCENERY) {
        // Doors use data.scenery.door.openFlags
        jammed = (obj->data.scenery.door.openFlags & OBJ_JAMMED) != 0;
    } else {
        // Containers use data.flags (CONTAINER_FLAG_JAMMED == OBJ_JAMMED)
        jammed = (obj->data.flags & OBJ_JAMMED) != 0;
    }

    ctx.setReturn(jammed ? 1 : 0);
}

// remove_timer_event(int timerId): removes a timer event by ID.
// Called with 0 args to remove ALL timer events.
// Removes from both the local tracking vector AND the engine event queue
// via queueClearByEventType(EVENT_TYPE_SCRIPT, _scrQueueRemoveFixed).
void mf_remove_timer_event(OpcodeContext& ctx)
{
    Object* owner = scriptGetSelf(ctx.program());

    if (owner == nullptr) {
        debugPrint("%s(): called with null script owner\n", ctx.name());
    }

    if (ctx.numArgs() == 0) {
        // Remove all timer events from local tracking AND engine queue.
        int removed = (int)gPendingTimerEvents.size();
        for (auto& event : gPendingTimerEvents) {
            _scrSetQueueTestVals(owner, event.opcode);
            queueClearByEventType(EVENT_TYPE_SCRIPT, _scrQueueRemoveFixed);
        }
        gPendingTimerEvents.clear();
        ctx.setReturn(removed);
    } else {
        int timerId = ctx.arg(0).asInt();
        int removed = 0;
        auto it = gPendingTimerEvents.begin();
        while (it != gPendingTimerEvents.end()) {
            if (it->timerId == timerId) {
                // Cancel the engine-queued event via queue mechanism.
                _scrSetQueueTestVals(owner, it->opcode);
                queueClearByEventType(EVENT_TYPE_SCRIPT, _scrQueueRemoveFixed);
                it = gPendingTimerEvents.erase(it);
                removed++;
            } else {
                ++it;
            }
        }
        ctx.setReturn(removed);
    }
}

// set_can_rest_on_map(int elevation, int tile, int canRest):
// sets whether resting is allowed at the given map coordinates.
// Wires to wmSetCanRestOnTile() which stores per-tile flags consumed by
// wmMapCanRestHere() (worldmap.cc:2978) and wmGetCanRestOnTile().
void mf_set_can_rest_on_map(OpcodeContext& ctx)
{
    int elevation = ctx.arg(0).asInt();
    int tile = ctx.arg(1).asInt();
    int canRest = ctx.arg(2).asInt();
    wmSetCanRestOnTile(elevation, tile, canRest != 0);
    ctx.setReturn(0);
}

// set_rest_mode(int mode): stores the resting mode for both sfall persistence
// and the worldmap rest system.
// Mode bitmask: 0 = disabled (RESTMODE_DISABLED), 1 = strict areas only,
// 2 = no healing (RESTMODE_NO_HEALING). -1 = use default engine behavior.
// Wired via wmSetRestMode() — consumed at worldmap.cc:2990.
void mf_set_rest_mode(OpcodeContext& ctx)
{
    int mode = ctx.arg(0).asInt();
    gRestMode = mode;
    wmSetRestMode(mode);
    ctx.setReturn(0);
}

// spatial_radius(Object* obj): returns the spatial script radius for the given
// object. The spatial script triggers when the player enters this radius.
// Returns the radius in hex tiles, or 0 if the object has no spatial script.
void mf_spatial_radius(OpcodeContext& ctx)
{
    Object* obj = ctx.arg(0).asObject();

    if (obj == nullptr) {
        ctx.setReturn(0);
        return;
    }

    // Iterate all spatial scripts on the object's elevation to find one
    // belonging to this object, then return its configured radius.
    int radius = 0;
    Script* spatial = scriptGetFirstSpatialScript(obj->elevation);
    while (spatial != nullptr) {
        if (spatial->owner == obj) {
            radius = spatial->sp.radius;
            break;
        }
        spatial = scriptGetNextSpatialScript();
    }

    ctx.setReturn(radius);
}

// F-003: UI metarules — interface bar hide/show/is_hidden (3 handlers)
//
// These were commented out at kMetarules[] lines for intface_hide/show/is_hidden.
// mf_intface_hide / mf_intface_show wire directly to the engine's interfaceBarHide()
// and interfaceBarShow() in interface.h. mf_intface_is_hidden now queries the
// engine's authoritative state via interfaceBarIsHidden() instead of a stale mirror.

void mf_intface_hide(OpcodeContext& ctx)
{
    interfaceBarHide();
    sIntfaceHiddenState = true;
    ctx.setReturn(0);
}

void mf_intface_show(OpcodeContext& ctx)
{
    interfaceBarShow();
    sIntfaceHiddenState = false;
    ctx.setReturn(0);
}

void mf_intface_is_hidden(OpcodeContext& ctx)
{
    // Query the engine's authoritative interface bar state via the public
    // accessor (interface.cc:interfaceBarIsHidden()) instead of relying on the
    // local sIntfaceHiddenState mirror, which can desynchronize when engine
    // code calls interfaceBarHide()/interfaceBarShow() directly (12+ call sites
    // bypass the sfall metarule handlers).
    ctx.setReturn(interfaceBarIsHidden() ? 1 : 0);
}

// F-014: interface_overlay(int winType, int action, int arg1, int arg2, int arg3, int arg4):
// Controls overlay rendering on specific interface windows.
// Action: 0 = destroy, 1 = create, 2 = clear (clear drawn content).
// winType is a window type constant (same as get_window_attribute).
//
// Tracks overlay state across script calls. The clear action refreshes the
// associated interface window to remove any drawn overlay content.
// Full overlay creation (action 1) requires engine-level overlay subsystem
// integration (TODO: overlay window creation and rendering pipeline).
void mf_interface_overlay(OpcodeContext& ctx)
{
    int winType = ctx.arg(0).asInt();
    int action = ctx.arg(1).asInt();

    switch (action) {
    case 0: { // destroy
        // Destroy the overlay window if it exists.
        if (gInterfaceOverlayState.active && gInterfaceOverlayState.windowHandle != -1) {
            windowDestroy(gInterfaceOverlayState.windowHandle);
        }
        gInterfaceOverlayState.active = false;
        gInterfaceOverlayState.winType = -1;
        gInterfaceOverlayState.windowHandle = -1;
        break;
    }
    case 1: { // create
        // I2-45: Destroy any previously active overlay window to prevent
        // leaking window slots in the managed window pool (MAX_WINDOW_COUNT=50).
        // Without this, repeated interface_overlay(...,1) calls would exhaust
        // the pool without freeing old window handles.
        if (gInterfaceOverlayState.active && gInterfaceOverlayState.windowHandle != -1) {
            windowDestroy(gInterfaceOverlayState.windowHandle);
            gInterfaceOverlayState.windowHandle = -1;
            gInterfaceOverlayState.active = false;
        }

        // Store overlay parameters for script-level tracking.
        gInterfaceOverlayState.winType = winType;
        gInterfaceOverlayState.arg1 = ctx.numArgs() > 2 ? ctx.arg(2).asInt() : 0;
        gInterfaceOverlayState.arg2 = ctx.numArgs() > 3 ? ctx.arg(3).asInt() : 0;
        gInterfaceOverlayState.arg3 = ctx.numArgs() > 4 ? ctx.arg(4).asInt() : 0;
        gInterfaceOverlayState.arg4 = ctx.numArgs() > 5 ? ctx.arg(5).asInt() : 0;

        // Validate dimensions before creating window.
        int x = gInterfaceOverlayState.arg1;
        int y = gInterfaceOverlayState.arg2;
        int w = gInterfaceOverlayState.arg3;
        int h = gInterfaceOverlayState.arg4;
        if (w <= 0 || h <= 0) {
            debugPrint("%s(): invalid overlay dimensions (%d x %d)", ctx.name(), w, h);
            ctx.setReturn(0);
            break;
        }
        gInterfaceOverlayState.windowHandle = windowCreate(x, y, w, h, _colorTable[0], WINDOW_TRANSPARENT);
        if (gInterfaceOverlayState.windowHandle != -1) {
            gInterfaceOverlayState.active = true;
        }
        break;
    }
    case 2: { // clear — refresh the interface window to remove drawn overlay content
        int window = -1;
        InterfaceWindowLookupResult lookup = getInterfaceWindowByType(winType, window);
        if (lookup == InterfaceWindowLookupResult::Found) {
            windowRefresh(window);
        } else {
            debugPrint("%s(): clear — window type %d is not available", ctx.name(), winType);
        }
        break;
    }
    default:
        debugPrint("%s(): unknown action %d for winType %d", ctx.name(), action, winType);
        break;
    }
    ctx.setReturn(0);
}

// F-015: interface_print(string text, int x, int y, int width, int height, int flags):
// Prints text directly to the active script window or interface.
// 5-6 args: text (string), x, y, width, height, [flags].
// TODO: full integration with windowPrintBuf via scriptWindowSelect.
void mf_interface_print(OpcodeContext& ctx)
{
    const char* text = ctx.stringArg(0);
    int x = ctx.arg(1).asInt();
    int y = ctx.arg(2).asInt();
    int width = ctx.arg(3).asInt();
    int height = ctx.arg(4).asInt();
    int flags = ctx.numArgs() > 5 ? ctx.arg(5).asInt() : 0;

    // Use the currently selected or created window (defaults to -1 if none)
    int window = scriptWindowGetWindow(ctx.program()->windowId);
    if (window == -1) {
        // Fall back to the interface bar window so the text is visible somewhere.
        window = gInterfaceBarWindow;
    }
    if (window == -1) {
        debugPrint("%s(): no window available to print to", ctx.name());
        ctx.setReturn(-1);
        return;
    }

    // windowPrintBuf expects a mutable buffer, so copy the text.
    char* buf = internal_strdup(text);
    if (buf == nullptr) {
        ctx.setReturn(-1);
        return;
    }
    windowPrintBuf(window, buf, static_cast<int>(strlen(buf)), width, y + height, x, y, flags, TEXT_ALIGNMENT_LEFT);
    internal_free(buf);

    // Refresh the area where we printed.
    Rect rect = { static_cast<short>(x), static_cast<short>(y), static_cast<short>(x + width - 1), static_cast<short>(y + height - 1) };
    windowRefreshRect(window, &rect);
    ctx.setReturn(0);
}

// --- End new metarule handlers ---

// F-033: MEDIUM-priority metarules — remaining commented-out entries from kMetarules[].
//
// These 9 metarules were commented out and without handler implementations.
// Each is implemented as a stub that logs the call and returns a safe default
// (0 for void-like operations, 0/false for boolean queries). The stubs make
// these metarules discoverable via metarule_exist() and prevent script crashes
// when called. TODO: full engine integration for each as APIs become available.

// add_g_timer_event(int delay, int fixedParam): registers a timed event to
// fire the given script procedure (passed as fixedParam) after the specified
// delay (in ticks). Matches the sfall 4.x API: delay is the FIRST argument,
// the opcode/procedure ID is the SECOND argument.
// Submits the event to the engine timer system via scriptAddTimerEvent, which
// dispatches it as EVENT_TYPE_SCRIPT with the opcode as fixedParam consumed
// by the script's SCRIPT_PROC_TIMED_EVENT_PROC handler.
// Returns a positive timerId on success, -1 on failure.
void mf_add_g_timer_event(OpcodeContext& ctx)
{
    int delay = ctx.arg(0).asInt();
    int opcode = ctx.arg(1).asInt();
    if (delay <= 0) {
        debugPrint("%s(delay=%d, opcode=%d): delay must be positive", ctx.name(), delay, opcode);
        ctx.setReturn(-1);
        return;
    }
    if ((int)gPendingTimerEvents.size() >= MAX_TIMER_EVENTS) {
        debugPrint("%s(): max timer events (%d) exceeded", ctx.name(), MAX_TIMER_EVENTS);
        ctx.setReturn(-1);
        return;
    }
    int sid = scriptGetSid(ctx.program());
    if (sid == -1) {
        debugPrint("%s(): cannot get SID for current program", ctx.name());
        ctx.setReturn(-1);
        return;
    }
    // Submit to engine timer system. The opcode is passed as the data param
    // and delivered to the script's timed-event handler as fixedParam.
    if (scriptAddTimerEvent(sid, delay, opcode) != 0) {
        debugPrint("%s(): scriptAddTimerEvent failed for sid=%d", ctx.name(), sid);
        ctx.setReturn(-1);
        return;
    }
    // Track locally for script-level removal queries.
    int timerId = gNextTimerId++;
    gPendingTimerEvents.push_back({ opcode, delay, timerId });
    ctx.setReturn(timerId);
}

// add_trait(int trait_type): adds a trait to the player by numeric ID.
// add_trait(int traitType)                  — 1-arg form: adds trait to player.
// add_trait(object critter, int traitType, int rank) — 3-arg form (sfall 4.x):
//   adds trait to the given critter with specified rank.
//
// F-057: Extended to support 3-arg signature in addition to the existing
// 1-arg form. The 3-arg form is registered with ARG_ANY for arg0 to allow
// both integer (1-arg) and object (3-arg) dispatch through the same handler.
// Stores the trait in gAddedTraits for persistence and script querying.
// Full integration with the engine's trait display/perk system requires
// deeper changes in character_editor.cc / perk.cc — this stores the data
// so scripts can rely on the metarule for state tracking across save/load.
void mf_add_trait(OpcodeContext& ctx)
{
    int traitType;
    int rank = 0;

    if (ctx.numArgs() >= 2) {
        // 3-arg form: add_trait(critter, traitType, rank)
        // arg1 = trait type ID
        // arg2 = rank (optional, defaults to 0)
        //
        // NOTE: Per-critter trait storage is not yet implemented. The
        // 3-arg form currently writes to gAddedTraits (player-scoped)
        // regardless of the critter argument. The critter object (arg0)
        // is read for type detection but its identity is not stored.
        // Full per-critter support requires CID-based storage with
        // save/load integration and engine trait-system hooking.
        traitType = ctx.arg(1).asInt();
        if (ctx.numArgs() >= 3) {
            rank = ctx.arg(2).asInt();
        }
        // F-24: When the 3-arg form is used with a critter that is not the
        // player (gDude), emit a visible warning so script developers know
        // per-critter trait storage is not implemented. The trait is still
        // applied to the player scope.
        if (ctx.arg(0).isPointer()) {
            Object* critter = ctx.arg(0).asObject();
            if (critter != nullptr && critter != gDude) {
                ctx.printError("%s(): add_trait per-critter storage not implemented; "
                    "trait %d applied to player scope instead of critter %d",
                    ctx.name(), traitType, critter->id);
            }
        }
    } else {
        // 1-arg form: add_trait(traitType) — adds to player with rank 0
        traitType = ctx.arg(0).asInt();
    }

    gAddedTraits[traitType] = rank;
    ctx.setReturn(1);
}

// remove_trait(int trait_type): removes a previously-added trait from the player.
// Returns 1 if the trait was present and removed, 0 if it was not found.
void mf_remove_trait(OpcodeContext& ctx)
{
    int traitType = ctx.arg(0).asInt();
    auto it = gAddedTraits.find(traitType);
    if (it != gAddedTraits.end()) {
        gAddedTraits.erase(it);
        ctx.setReturn(1);
    } else {
        ctx.setReturn(0);
    }
}

// set_spray_settings(int flags, int pid, int radius, int count): configures
// burst fire spray pattern parameters. Stored for script-level tracking;
// Consumer: combat burst system (src/combat.cc _compute_spray).
void mf_set_spray_settings(OpcodeContext& ctx)
{
    gSpraySettings.flags = ctx.arg(0).asInt();
    gSpraySettings.pid = ctx.arg(1).asInt();
    gSpraySettings.radius = ctx.arg(2).asInt();
    gSpraySettings.count = ctx.arg(3).asInt();
    gSpraySettings.active = true;
    ctx.setReturn(0);
}

// set_car_intface_art(int fid): stores the FRM/FID art for the car trunk
// interface. Integration point: _setup_inventory() at inventory.cc:1338-1340
// should check gCarIntfaceArtFid when target is PROTO_ID_CAR_TRUNK or PROTO_ID_CAR.
void mf_set_car_intface_art(OpcodeContext& ctx)
{
    int fid = ctx.arg(0).asInt();
    gCarIntfaceArtFid = fid;
    ctx.setReturn(0);
}

// set_drugs_data(int drug_index, int addiction_rate, int effect_duration):
// stores drug addiction probability and effect duration overrides.
// Stored in gDrugDataOverrides for script-level tracking and future engine
// integration with the drug/chem application pipeline (src/item.cc, src/stat.cc).
// TODO: wire gDrugDataOverrides into drug processing at itemUseDrug() call sites.
void mf_set_drugs_data(OpcodeContext& ctx)
{
    int drugIndex = ctx.arg(0).asInt();
    int addictionRate = ctx.arg(1).asInt();
    int effectDuration = ctx.arg(2).asInt();
    gDrugDataOverrides[drugIndex] = { addictionRate, effectDuration };
    ctx.setReturn(0);
}

// set_map_enter_position(int x, int y, int elevation): sets the spawn position
// override for entering a map. The stored position is returned by
// get_map_enter_position(). Full integration with the map spawn system
// (checking the override in mapEnter() or similar) requires deeper engine changes.
void mf_set_map_enter_position(OpcodeContext& ctx)
{
    int x = ctx.arg(0).asInt();
    int y = ctx.arg(1).asInt();
    int elevation = ctx.arg(2).asInt();
    gMapEnterX = x;
    gMapEnterY = y;
    gMapEnterElevation = elevation;
    wmSetMapEnterPosition(x, y, elevation);
    ctx.setReturn(0);
}

// set_town_title(int town_id, string title): stores a town title override for
// the given town ID. Mirrors the gTerrainNameOverrides pattern at line 187.
// If title is empty or null, clears any existing override.
// Integration point: wmGetAreaName() at worldmap.cc:5934 should check overrides.
void mf_set_town_title(OpcodeContext& ctx)
{
    int townId = ctx.arg(0).asInt();
    const char* title = ctx.stringArg(1);
    if (title != nullptr && title[0] != '\0') {
        gTownTitleOverrides[townId] = title;
    } else {
        gTownTitleOverrides.erase(townId);
    }
    ctx.setReturn(0);
}

// get_town_title(int town_id): returns the town title override string set by
// set_town_title(), or an empty string if no override has been set.
// Provides a script-level query API. Note: wmGetAreaName() in worldmap.cc also
// needs wiring to read gTownTitleOverrides for display purposes (see worldmap.cc:6044).
void mf_get_town_title(OpcodeContext& ctx)
{
    int townId = ctx.arg(0).asInt();
    auto it = gTownTitleOverrides.find(townId);
    if (it != gTownTitleOverrides.end()) {
        ctx.setReturn(it->second.c_str());
    } else {
        ctx.setReturn("");
    }
}

// F-9 (FIX): talking_head_mood(int mood): overrides the talking head mood in dialogs.
// Called by VOODOO_talking_head_mood macro in Et Tu's gl_fo1mechanics.ssl.
// mood values: -1 = reset to engine default, 0 = neutral, 1 = good/bad
// (the exact mapping depends on the FO1 vs FO2 mood thresholds in reaction.cc).
// Sets gTalkingHeadMood which should be checked by game_dialog.cc
// _gdSetupFidget() to override the reaction-based mood for head animations.
void mf_talking_head_mood(OpcodeContext& ctx)
{
    int mood = ctx.arg(0).asInt();
    // Clamp to known range: -1 (reset), 0 (neutral), 1 (good/bad).
    // The Et Tu Fo1in2 macro passes explicitly validated values.
    if (mood < -1) mood = -1;
    if (mood > 1) mood = 1;
    gTalkingHeadMood = mood;
    ctx.setReturn(0);
}

// API for game_dialog.cc to query the talking head mood override.
// Returns -1 if no override is set (use engine-calculated reaction).
int sfallGetTalkingHeadMood()
{
    return gTalkingHeadMood;
}

// set_unjam_locks_time(int hours): overrides the number of game hours before
// jammed locks automatically unjam. Stores the override in gUnjamLocksTimeHours.
// NOTE: The map-entry path (map.cc:1184-1193) correctly uses this override
// for time-based unjam gating. The midnight-event path (scripts.cc:438) still
// calls objectUnjamAll() unconditionally, bypassing this override.
// This metarule stores the configured value for script queries and both paths.
void mf_set_unjam_locks_time(OpcodeContext& ctx)
{
    int hours = ctx.arg(0).asInt();
    if (hours < 0) {
        hours = -1; // -1 disables time override, revert to engine default
    }
    gUnjamLocksTimeHours = hours;
    ctx.setReturn(0);
}

// get_unjam_locks_time(): returns the stored unjam locks time override in game
// hours (-1 = disabled/engine default). Companion getter for set_unjam_locks_time.
void mf_get_unjam_locks_time(OpcodeContext& ctx)
{
    ctx.setReturn(gUnjamLocksTimeHours);
}

// unjam_lock(Object* obj): unjams a locked container or door, allowing it to
// be picked or forced. Wraps the engine's objectUnjamLock() (proto_instance.cc:2208)
// which clears the JAMMED flag on lockable containers and scenery doors.
// Returns 1 on success, 0 if the object is not lockable or is null.
void mf_unjam_lock(OpcodeContext& ctx)
{
    Object* obj = ctx.arg(0).asObject();
    if (obj == nullptr) {
        ctx.setReturn(0);
        return;
    }
    int result = objectUnjamLock(obj);
    ctx.setReturn(result == 0 ? 1 : 0);
}

// --- End F-033 handlers ---

// message_box
void mf_message_box(OpcodeContext& ctx)
{
    const char* string = ctx.stringArg(0);
    if (string == nullptr || string[0] == '\0') {
        ctx.setReturn(-1);
        return;
    }

    char* copy = internal_strdup(string);

    // F2-045: internal_strdup can return nullptr on allocation failure (OOM).
    // mf_interface_print (same file, ~20 lines earlier) has the same guard.
    // Without this check, strchr(copy, '\n') dereferences nullptr.
    if (copy == nullptr) {
        ctx.setReturn(-1);
        return;
    }

    const char* body[4];
    int count = 0;

    char* pch = strchr(copy, '\n');
    while (pch != nullptr && count < 4) {
        *pch = '\0';
        body[count++] = pch + 1;
        pch = strchr(pch + 1, '\n');
    }

    int flags = DIALOG_BOX_LARGE | DIALOG_BOX_YES_NO;
    if (ctx.numArgs() > 1) {
        int flagParam = ctx.arg(1).asInt();
        if (flagParam != -1) {
            flags = flagParam;
        }
    }

    // note: most of the CE code uses colorTable indices, but this metarule expects palette values.
    // Default: yellow (145) = _colorTable[32328]
    int color1 = _colorTable[32328], color2 = _colorTable[32328];
    if (ctx.numArgs() > 2) {
        color1 = ctx.arg(2).asInt();
    }
    if (ctx.numArgs() > 3) {
        color2 = ctx.arg(3).asInt();
    }

    sfall_metarules_dialogShowCount++;
    scriptsDisable();
    int rc = showDialogBox(copy, body, count, 192, 116, color1, nullptr, color2, flags);
    if (--sfall_metarules_dialogShowCount == 0) {
        scriptsEnable();
    }

    ctx.setReturn(rc);
    internal_free(copy);
}

void sfall_metarule(Program* program, int args)
{
    // Defensive: validate arg count before any stack manipulation.
    if (args < 0 || args > METARULE_MAX_ARGS) {
        // Pop args first (they are on top of the stack — LIFO order),
        // then pop the metarule name (pushed first, deepest on stack).
        for (int index = 0; index < args; index++) {
            programStackPopValue(program);
        }
        programStackPopValue(program);
        programPrintError("op_sfall_func(...) - invalid argument count %d (max %zu).", args, METARULE_MAX_ARGS);
        programStackPushInteger(program, 0);
        return;
    }

    // TODO: make OpcodeContext handle the stack.  This will be easier once it is used for all opcodes.
    static ProgramValue values[METARULE_MAX_ARGS];

    // Pop args first (they are on top of the stack — LIFO order).
    for (int index = 0; index < args; index++) {
        values[index] = programStackPopValue(program);
    }

    // Pop name last (name was pushed first, deepest on stack).
    ProgramValue metaruleName = programStackPopValue(program);
    if (!metaruleName.isString()) {
        programPrintError("op_sfall_func(name, ...) - name must be string.");
        programStackPushInteger(program, 0);
        return;
    }

    const char* metarule = programGetString(program, metaruleName.opcode, metaruleName.integerValue);

    const MetaruleInfo* metaruleInfo = nullptr;
    for (int index = 0; index < (int)kMetarulesCount; index++) {
        if (compat_stricmp(kMetarules[index].name, metarule) == 0) {
            metaruleInfo = &kMetarules[index];
            break;
        }
    }

    if (metaruleInfo == nullptr) {
        programPrintError("op_sfall_func(\"%s\", ...) - metarule function is unknown.", metarule);
        programStackPushInteger(program, -1);
        return;
    }

    // Validate argument count against metarule info.
    // Args are already consumed — no stack-balance cleanup needed.
    if (args < metaruleInfo->minArgs || args > metaruleInfo->maxArgs) {
        programPrintError("%s() - invalid number of arguments (%d), must be from %d to %d.",
            metaruleInfo->name, args, metaruleInfo->minArgs, metaruleInfo->maxArgs);
        programStackPushInteger(program, metaruleInfo->errorReturn);
        return;
    }

    OpcodeContext ctx(program, metaruleInfo, args, values);
    if (!ctx.validateArguments()) {
        ctx.setReturn(metaruleInfo->errorReturn);
        ctx.pushReturnValue();
        return;
    }

    metaruleInfo->handler(ctx);
    ctx.pushReturnValue();
}

void sfall_metarules_reset()
{
    sfall_metarules_dialogShowCount = 0;
    gNpcEngineLevelUpEnabled = 1;
    gSavedOriginalDude = nullptr;
    gSavedOriginalDudeCid = -1;
    gQuestFailureValues.clear();
    gScriptNameOverride.clear();
    gWorldmapHealTime = -1;
    gRestHealTime = -1;
    gTerrainNameOverrides.clear();
    gTownTitleOverrides.clear();
    gCarIntfaceArtFid = -1;
    gRestMode = -1;
    gFakePerksNpc.clear();
    gFakeTraitsNpc.clear();
    gFakeSelectablePerksNpc.clear();
    sIntfaceHiddenState = false;
    gAddedTraits.clear();
    gExplosiveOverrides.clear();
    gMapEnterX = -1;
    gMapEnterY = -1;
    gMapEnterElevation = -1;
    gUnjamLocksTimeHours = -1;
    gSpraySettings = {};
    // F-08: Reset water timer — disabled by default, re-enabled when FO1 mode activates.
    gSfallWaterTimerEnabled = false;
    gSfallWaterTimerDays = 150;
    gPendingTimerEvents.clear();
    gNextTimerId = 1;
    gTalkingHeadMood = -1;
    gDrugDataOverrides.clear();
    gInterfaceOverlayState = {};

    // Forward reset state to the worldmap runtime consumer layer.
    // Without this, worldmap.cc's gWorldmapHealTime / gRestHealTime /
    // gRestMode / gMapEnterPosition* survive gameReset() and leak into
    // the next game session (new game or different save load).
    wmSetWorldmapHealTime(-1);
    wmSetRestHealTime(-1);
    wmSetRestMode(-1);
    wmSetMapEnterPosition(-1, -1, -1);
}

// --- Metarule state save/load ---
//
// Persistence format (simple tagged binary):
//   Version int32 (currently 7)
//   For each scalar: int32 value
//   For each map: int32 count, then (key, value) pairs
//   For each set: int32 count, then values
//   For NPC-keyed maps: int32 count, then (cid, int32 nameCount, names)
//
// Called from sfall_ext.cc during sfallgv.sav write/read.
// On load, if the version marker doesn't match, the function returns early
// (sfall_metarules_reset() has already restored defaults) — forward compatibility.

#define METARULES_SAVE_VERSION 7

// F-41: Maximum entries for save/load metarule collections.
// Prevents infinite loops on corrupt save data with absurd count values.
// These caps are well above any realistic game data (e.g. 65535 town titles
// would exceed the address space), but small enough to terminate quickly.
#define METARULES_MAX_LOAD_COUNT    65535
#define METARULES_MAX_NPC_COUNT     16384
#define METARULES_MAX_NPC_ENTRIES   4096
#define METARULES_MAX_DRUG_COUNT    4096

static bool metarulesSaveStringMap(File* stream, const std::map<int, std::string>& map)
{
    int count = static_cast<int>(map.size());
    if (fileWriteInt32(stream, count) == -1) return false;
    for (const auto& entry : map) {
        if (fileWriteInt32(stream, entry.first) == -1) return false;
        if (fileWriteString(entry.second.c_str(), stream) == -1) return false;
        if (xfileWriteChar('\n', stream) == -1) return false;
    }
    return true;
}

static bool metarulesLoadStringMap(File* stream, std::map<int, std::string>& map)
{
    int count;
    if (fileReadInt32(stream, &count) == -1) return false;
    if (count < 0 || count > METARULES_MAX_LOAD_COUNT) return false;
    map.clear();
    for (int i = 0; i < count; i++) {
        int key;
        if (fileReadInt32(stream, &key) == -1) return false;
        char value[256];
        if (fileReadString(value, sizeof(value), stream) == nullptr) return false;
        size_t vlen = strlen(value);
        if (vlen > 0 && value[vlen - 1] == '\n') value[vlen - 1] = '\0';
        map[key] = value;
    }
    return true;
}

static bool metarulesSaveCoordMap(File* stream, const std::map<std::pair<int, int>, std::string>& map)
{
    int count = static_cast<int>(map.size());
    if (fileWriteInt32(stream, count) == -1) return false;
    for (const auto& entry : map) {
        if (fileWriteInt32(stream, entry.first.first) == -1) return false;
        if (fileWriteInt32(stream, entry.first.second) == -1) return false;
        if (fileWriteString(entry.second.c_str(), stream) == -1) return false;
        if (xfileWriteChar('\n', stream) == -1) return false;
    }
    return true;
}

static bool metarulesLoadCoordMap(File* stream, std::map<std::pair<int, int>, std::string>& map)
{
    int count;
    if (fileReadInt32(stream, &count) == -1) return false;
    if (count < 0 || count > METARULES_MAX_LOAD_COUNT) return false;
    map.clear();
    for (int i = 0; i < count; i++) {
        int x, y;
        if (fileReadInt32(stream, &x) == -1) return false;
        if (fileReadInt32(stream, &y) == -1) return false;
        char value[256];
        if (fileReadString(value, sizeof(value), stream) == nullptr) return false;
        size_t vlen = strlen(value);
        if (vlen > 0 && value[vlen - 1] == '\n') value[vlen - 1] = '\0';
        map[{ x, y }] = value;
    }
    return true;
}

static bool metarulesSaveIntIntMap(File* stream, const std::map<int, int>& map)
{
    int count = static_cast<int>(map.size());
    if (fileWriteInt32(stream, count) == -1) return false;
    for (const auto& entry : map) {
        if (fileWriteInt32(stream, entry.first) == -1) return false;
        if (fileWriteInt32(stream, entry.second) == -1) return false;
    }
    return true;
}

static bool metarulesLoadIntIntMap(File* stream, std::map<int, int>& map)
{
    int count;
    if (fileReadInt32(stream, &count) == -1) return false;
    if (count < 0 || count > METARULES_MAX_LOAD_COUNT) return false;
    map.clear();
    for (int i = 0; i < count; i++) {
        int key, val;
        if (fileReadInt32(stream, &key) == -1) return false;
        if (fileReadInt32(stream, &val) == -1) return false;
        map[key] = val;
    }
    return true;
}

static bool metarulesSaveIntSet(File* stream, const std::set<int>& s)
{
    int count = static_cast<int>(s.size());
    if (fileWriteInt32(stream, count) == -1) return false;
    for (int val : s) {
        if (fileWriteInt32(stream, val) == -1) return false;
    }
    return true;
}

static bool metarulesLoadIntSet(File* stream, std::set<int>& s)
{
    int count;
    if (fileReadInt32(stream, &count) == -1) return false;
    if (count < 0 || count > METARULES_MAX_LOAD_COUNT) return false;
    s.clear();
    for (int i = 0; i < count; i++) {
        int val;
        if (fileReadInt32(stream, &val) == -1) return false;
        s.insert(val);
    }
    return true;
}

static bool metarulesSaveExplosiveMap(File* stream, const std::map<int, ExplosiveProperties>& map)
{
    int count = static_cast<int>(map.size());
    if (fileWriteInt32(stream, count) == -1) return false;
    for (const auto& entry : map) {
        if (fileWriteInt32(stream, entry.first) == -1) return false;
        if (fileWriteInt32(stream, entry.second.pattern) == -1) return false;
        if (fileWriteInt32(stream, entry.second.radius) == -1) return false;
        if (fileWriteInt32(stream, entry.second.delay) == -1) return false;
        if (fileWriteInt32(stream, entry.second.minDamage) == -1) return false;
        if (fileWriteInt32(stream, entry.second.maxDamage) == -1) return false;
    }
    return true;
}

static bool metarulesLoadExplosiveMap(File* stream, std::map<int, ExplosiveProperties>& map, int version)
{
    int count;
    if (fileReadInt32(stream, &count) == -1) return false;
    if (count < 0 || count > METARULES_MAX_LOAD_COUNT) return false;
    map.clear();
    for (int i = 0; i < count; i++) {
        int pid, pattern, radius, delay, minDamage = 0, maxDamage = 0;
        if (fileReadInt32(stream, &pid) == -1) return false;
        if (fileReadInt32(stream, &pattern) == -1) return false;
        if (fileReadInt32(stream, &radius) == -1) return false;
        if (fileReadInt32(stream, &delay) == -1) return false;
        // Version 4+ includes damage fields in the explosive map entries.
        if (version >= 4) {
            if (fileReadInt32(stream, &minDamage) == -1) return false;
            if (fileReadInt32(stream, &maxDamage) == -1) return false;
        }
        map[pid] = { pattern, radius, delay, minDamage, maxDamage };
    }
    return true;
}

// Drug data overrides save/load — keyed by drug index.
static bool metarulesSaveDrugDataMap(File* stream, const std::map<int, DrugData>& map)
{
    int count = static_cast<int>(map.size());
    if (fileWriteInt32(stream, count) == -1) return false;
    for (const auto& entry : map) {
        if (fileWriteInt32(stream, entry.first) == -1) return false;
        if (fileWriteInt32(stream, entry.second.addictionRate) == -1) return false;
        if (fileWriteInt32(stream, entry.second.effectDuration) == -1) return false;
    }
    return true;
}

static bool metarulesLoadDrugDataMap(File* stream, std::map<int, DrugData>& map)
{
    int count;
    if (fileReadInt32(stream, &count) == -1) return false;
    if (count < 0 || count > METARULES_MAX_DRUG_COUNT) return false;
    map.clear();
    for (int i = 0; i < count; i++) {
        int drugIndex, addictionRate, effectDuration;
        if (fileReadInt32(stream, &drugIndex) == -1) return false;
        if (fileReadInt32(stream, &addictionRate) == -1) return false;
        if (fileReadInt32(stream, &effectDuration) == -1) return false;
        map[drugIndex] = { addictionRate, effectDuration };
    }
    return true;
}

// NPC-keyed fake perk/trait/selectable-perk maps.
// Keyed by CID (int) — the unique per-object identifier that survives
// save/load. Consumers use critter->cid for lookup, so data loaded from
// saves is immediately reachable without post-load Object* fixup.
// Each entry stores {name, level, image, desc} metadata.
// New format (v3+): includes level/image/desc per entry.

static bool metarulesSaveNpcFakeDataV3(File* stream,
    const std::unordered_map<int, std::unordered_map<std::string, FakePerkNpcEntry>>& map)
{
    int count = static_cast<int>(map.size());
    if (fileWriteInt32(stream, count) == -1) return false;
    for (const auto& entry : map) {
        int cid = entry.first;
        if (fileWriteInt32(stream, cid) == -1) return false;
        int nameCount = static_cast<int>(entry.second.size());
        if (fileWriteInt32(stream, nameCount) == -1) return false;
        for (const auto& kv : entry.second) {
            if (fileWriteString(kv.second.name.c_str(), stream) == -1) return false;
            if (xfileWriteChar('\n', stream) == -1) return false;
            if (fileWriteInt32(stream, kv.second.level) == -1) return false;
            if (fileWriteInt32(stream, kv.second.image) == -1) return false;
            if (fileWriteString(kv.second.desc.c_str(), stream) == -1) return false;
            if (xfileWriteChar('\n', stream) == -1) return false;
        }
    }
    return true;
}

static bool metarulesLoadNpcFakeDataV3(File* stream,
    std::unordered_map<int, std::unordered_map<std::string, FakePerkNpcEntry>>& map)
{
    int count;
    if (fileReadInt32(stream, &count) == -1) return false;
    if (count < 0 || count > METARULES_MAX_NPC_COUNT) return false;
    map.clear();
    for (int i = 0; i < count; i++) {
        int cid;
        if (fileReadInt32(stream, &cid) == -1) return false;
        int nameCount;
        if (fileReadInt32(stream, &nameCount) == -1) return false;
        if (nameCount < 0 || nameCount > METARULES_MAX_NPC_ENTRIES) return false;
        std::unordered_map<std::string, FakePerkNpcEntry> names;
        for (int j = 0; j < nameCount; j++) {
            char nameBuf[256];
            if (fileReadString(nameBuf, sizeof(nameBuf), stream) == nullptr) return false;
            size_t nlen = strlen(nameBuf);
            if (nlen > 0 && nameBuf[nlen - 1] == '\n') nameBuf[nlen - 1] = '\0';
            std::string name = nameBuf;
            int level, image;
            if (fileReadInt32(stream, &level) == -1) return false;
            if (fileReadInt32(stream, &image) == -1) return false;
            char descBuf[256];
            if (fileReadString(descBuf, sizeof(descBuf), stream) == nullptr) return false;
            size_t dlen = strlen(descBuf);
            if (dlen > 0 && descBuf[dlen - 1] == '\n') descBuf[dlen - 1] = '\0';
            names[name] = { name, level, image, descBuf };
        }
        map[cid] = std::move(names);
    }
    return true;
}

// Legacy v2 load: reads names as a set (no metadata), creating entries
// with default metadata for backward compatibility.
static bool metarulesLoadNpcFakeDataV2(File* stream,
    std::unordered_map<int, std::unordered_map<std::string, FakePerkNpcEntry>>& map)
{
    int count;
    if (fileReadInt32(stream, &count) == -1) return false;
    if (count < 0 || count > METARULES_MAX_NPC_COUNT) return false;
    map.clear();
    for (int i = 0; i < count; i++) {
        int cid;
        if (fileReadInt32(stream, &cid) == -1) return false;
        int nameCount;
        if (fileReadInt32(stream, &nameCount) == -1) return false;
        if (nameCount < 0 || nameCount > METARULES_MAX_NPC_ENTRIES) return false;
        std::unordered_map<std::string, FakePerkNpcEntry> names;
        for (int j = 0; j < nameCount; j++) {
            char nameBuf[256];
            if (fileReadString(nameBuf, sizeof(nameBuf), stream) == nullptr) return false;
            size_t nlen = strlen(nameBuf);
            if (nlen > 0 && nameBuf[nlen - 1] == '\n') nameBuf[nlen - 1] = '\0';
            std::string name = nameBuf;
            // v2 saves have no metadata — use sensible defaults.
            names[name] = { name, 1, -1, "" };
        }
        map[cid] = std::move(names);
    }
    return true;
}

bool sfall_metarules_save(File* stream)
{
    if (fileWriteInt32(stream, METARULES_SAVE_VERSION) == -1) return false;

    // Scalars
    if (fileWriteInt32(stream, sfall_metarules_dialogShowCount) == -1) return false;
    if (fileWriteInt32(stream, gNpcEngineLevelUpEnabled) == -1) return false;
    if (fileWriteInt32(stream, gWorldmapHealTime) == -1) return false;
    if (fileWriteInt32(stream, gRestHealTime) == -1) return false;
    if (fileWriteInt32(stream, gCarIntfaceArtFid) == -1) return false;
    if (fileWriteInt32(stream, gRestMode) == -1) return false;
    if (fileWriteInt32(stream, sIntfaceHiddenState ? 1 : 0) == -1) return false;
    if (fileWriteInt32(stream, gMapEnterX) == -1) return false;
    if (fileWriteInt32(stream, gMapEnterY) == -1) return false;
    if (fileWriteInt32(stream, gMapEnterElevation) == -1) return false;

    // String
    if (fileWriteString(gScriptNameOverride.c_str(), stream) == -1) return false;
    if (xfileWriteChar('\n', stream) == -1) return false;

    // Maps and sets
    if (!metarulesSaveCoordMap(stream, gTerrainNameOverrides)) return false;
    if (!metarulesSaveStringMap(stream, gTownTitleOverrides)) return false;
    if (!metarulesSaveIntIntMap(stream, gQuestFailureValues)) return false;
    if (!metarulesSaveIntIntMap(stream, gAddedTraits)) return false;
    if (!metarulesSaveExplosiveMap(stream, gExplosiveOverrides)) return false;

    // NPC-keyed data (v3 format includes metadata: level, image, desc)
    if (!metarulesSaveNpcFakeDataV3(stream, gFakePerksNpc)) return false;
    if (!metarulesSaveNpcFakeDataV3(stream, gFakeTraitsNpc)) return false;
    if (!metarulesSaveNpcFakeDataV3(stream, gFakeSelectablePerksNpc)) return false;

    // gSavedOriginalDude: save as CID
    int originalDudeCid = (gSavedOriginalDude != nullptr) ? gSavedOriginalDude->cid : -1;
    if (fileWriteInt32(stream, originalDudeCid) == -1) return false;

    // Version 2 additions: spray settings, drug data overrides, interface overlay state.
    if (fileWriteInt32(stream, gSpraySettings.flags) == -1) return false;
    if (fileWriteInt32(stream, gSpraySettings.pid) == -1) return false;
    if (fileWriteInt32(stream, gSpraySettings.radius) == -1) return false;
    if (fileWriteInt32(stream, gSpraySettings.count) == -1) return false;
    if (fileWriteInt32(stream, gSpraySettings.active ? 1 : 0) == -1) return false;
    if (!metarulesSaveDrugDataMap(stream, gDrugDataOverrides)) return false;
    if (fileWriteInt32(stream, gInterfaceOverlayState.winType) == -1) return false;
    if (fileWriteInt32(stream, gInterfaceOverlayState.arg1) == -1) return false;
    if (fileWriteInt32(stream, gInterfaceOverlayState.arg2) == -1) return false;
    if (fileWriteInt32(stream, gInterfaceOverlayState.arg3) == -1) return false;
    if (fileWriteInt32(stream, gInterfaceOverlayState.arg4) == -1) return false;
    if (fileWriteInt32(stream, gInterfaceOverlayState.windowHandle) == -1) return false;
    if (fileWriteInt32(stream, gInterfaceOverlayState.active ? 1 : 0) == -1) return false;

    // Version 3 additions: unjam locks time, NPC fake data with metadata.
    if (fileWriteInt32(stream, gUnjamLocksTimeHours) == -1) return false;

    // Version 5 addition: block combat flag (sfall global, not part of
    // core combat stream).
    if (fileWriteInt32(stream, gBlockCombat) == -1) return false;

    // Version 7 addition: talking head mood override (F-011 fix).
    if (fileWriteInt32(stream, gTalkingHeadMood) == -1) return false;

    return true;
}

bool sfall_metarules_load(File* stream)
{
    int version;
    if (fileReadInt32(stream, &version) == -1) return false;
    if (version > METARULES_SAVE_VERSION || version < 1) {
        debugPrint("sfall_metarules_load(): unknown save version %d, skipping", version);
        return false;
    }

    // Scalars
    if (fileReadInt32(stream, &sfall_metarules_dialogShowCount) == -1) return false;
    // Reconcile after load: if a save was made while a message box was displayed
    // (dialogShowCount > 0), the engine re-enables scripts during post-load init.
    // Using the saved count would cause the next message_box to decrement to 1
    // instead of 0, permanently disabling scripts. Always reset to 0 on load.
    sfall_metarules_dialogShowCount = 0;
    if (fileReadInt32(stream, &gNpcEngineLevelUpEnabled) == -1) return false;
    if (fileReadInt32(stream, &gWorldmapHealTime) == -1) return false;
    if (fileReadInt32(stream, &gRestHealTime) == -1) return false;
    if (fileReadInt32(stream, &gCarIntfaceArtFid) == -1) return false;
    if (fileReadInt32(stream, &gRestMode) == -1) return false;

    // Replay restored healing/rest values to the worldmap system so that
    // consumers (worldmap.cc:3258, wmMapCanRestHere, wmGetRestMode) see the
    // restored overrides instead of defaulting to -1.
    wmSetWorldmapHealTime(gWorldmapHealTime);
    wmSetRestHealTime(gRestHealTime);
    wmSetRestMode(gRestMode);
    int hiddenState;
    if (fileReadInt32(stream, &hiddenState) != -1) {
        sIntfaceHiddenState = (hiddenState != 0);
    }
    if (fileReadInt32(stream, &gMapEnterX) == -1) return false;
    if (fileReadInt32(stream, &gMapEnterY) == -1) return false;
    if (fileReadInt32(stream, &gMapEnterElevation) == -1) return false;

    // Forward restored map enter position to the worldmap runtime
    // consumer layer for consistency with the replay pattern used by
    // healing/rest values (lines 3626-3628). The primary consumer
    // (map.cc:948) reads sfall globals via sfallGetMapEnterX/Y, but
    // the wmHasMapEnterPosition/wmGetMapEnterPosition fallback should
    // also see the restored values.
    wmSetMapEnterPosition(gMapEnterX, gMapEnterY, gMapEnterElevation);

    // String
    char nameBuf[256];
    if (fileReadString(nameBuf, sizeof(nameBuf), stream) != nullptr) {
        size_t nlen = strlen(nameBuf);
        if (nlen > 0 && nameBuf[nlen - 1] == '\n') nameBuf[nlen - 1] = '\0';
        gScriptNameOverride = nameBuf;
    }

    // Maps and sets
    if (!metarulesLoadCoordMap(stream, gTerrainNameOverrides)) return false;
    if (!metarulesLoadStringMap(stream, gTownTitleOverrides)) return false;
    if (!metarulesLoadIntIntMap(stream, gQuestFailureValues)) return false;
    // gAddedTraits: version 5 and earlier stored as a flat set of trait IDs
    // (no rank).  Version 6+ stores trait ID -> rank pairs via IntIntMap.
    if (version >= 6) {
        if (!metarulesLoadIntIntMap(stream, gAddedTraits)) return false;
    } else {
        std::set<int> legacyTraits;
        if (!metarulesLoadIntSet(stream, legacyTraits)) return false;
        for (int t : legacyTraits) {
            gAddedTraits[t] = 0; // default rank for legacy saves
        }
    }
    if (!metarulesLoadExplosiveMap(stream, gExplosiveOverrides, version)) return false;

    // NPC-keyed data: keyed by CID (int), no post-load fixup needed.
    // v3+ format includes per-entry metadata (level, image, desc);
    // v1-v2 format stores only names — loaded with default metadata.
    if (version >= 3) {
        if (!metarulesLoadNpcFakeDataV3(stream, gFakePerksNpc)) return false;
        if (!metarulesLoadNpcFakeDataV3(stream, gFakeTraitsNpc)) return false;
        if (!metarulesLoadNpcFakeDataV3(stream, gFakeSelectablePerksNpc)) return false;
    } else {
        if (!metarulesLoadNpcFakeDataV2(stream, gFakePerksNpc)) return false;
        if (!metarulesLoadNpcFakeDataV2(stream, gFakeTraitsNpc)) return false;
        if (!metarulesLoadNpcFakeDataV2(stream, gFakeSelectablePerksNpc)) return false;
    }

    // gSavedOriginalDude: restore Object* from CID.
    // Objects are already loaded when sfall_metarules_load() is called
    // (sfallLoadGameData runs after the engine's 27 master load handlers).
    int originalDudeCid;
    if (fileReadInt32(stream, &originalDudeCid) != -1) {
        gSavedOriginalDudeCid = originalDudeCid;
        gSavedOriginalDude = nullptr;
        if (originalDudeCid != -1) {
            Object* obj = objectFindFirst();
            while (obj != nullptr) {
                if (obj->cid == originalDudeCid) {
                    gSavedOriginalDude = obj;
                    break;
                }
                obj = objectFindNext();
            }
        }
    }

    // Version 2 additions: spray settings, drug data overrides, interface overlay state.
    // For version 1 saves, these fields are absent — sfall_metarules_reset() has
    // already set defaults, so no explicit fallback is needed.
    if (version >= 2) {
        if (fileReadInt32(stream, &gSpraySettings.flags) == -1) return false;
        if (fileReadInt32(stream, &gSpraySettings.pid) == -1) return false;
        if (fileReadInt32(stream, &gSpraySettings.radius) == -1) return false;
        if (fileReadInt32(stream, &gSpraySettings.count) == -1) return false;
        int sprayActive;
        if (fileReadInt32(stream, &sprayActive) == -1) return false;
        gSpraySettings.active = (sprayActive != 0);
        if (!metarulesLoadDrugDataMap(stream, gDrugDataOverrides)) return false;
        if (fileReadInt32(stream, &gInterfaceOverlayState.winType) == -1) return false;
        if (fileReadInt32(stream, &gInterfaceOverlayState.arg1) == -1) return false;
        if (fileReadInt32(stream, &gInterfaceOverlayState.arg2) == -1) return false;
        if (fileReadInt32(stream, &gInterfaceOverlayState.arg3) == -1) return false;
        if (fileReadInt32(stream, &gInterfaceOverlayState.arg4) == -1) return false;
        if (fileReadInt32(stream, &gInterfaceOverlayState.windowHandle) == -1) return false;
        int overlayActive;
        if (fileReadInt32(stream, &overlayActive) == -1) return false;
        gInterfaceOverlayState.active = (overlayActive != 0);
    }

    // Version 3 additions: unjam locks time.
    // For version 1-2 saves, sfall_metarules_reset() already sets -1 as default.
    if (version >= 3) {
        if (fileReadInt32(stream, &gUnjamLocksTimeHours) == -1) return false;
    }

    // Version 5 addition: block combat flag.
    // For version 1-4 saves, combatReset() (called during gameReset()) sets 0
    // as default.
    if (version >= 5) {
        if (fileReadInt32(stream, &gBlockCombat) == -1) return false;
    }

    // Version 7 addition: talking head mood override.
    // For version 1-6 saves, sfall_metarules_reset() already sets -1 as default
    // (no override, use engine-calculated reaction).
    if (version >= 7) {
        if (fileReadInt32(stream, &gTalkingHeadMood) == -1) return false;
    }

    return true;
}

// Public accessor: returns the override town title for the given area index,
// or nullptr if no override has been set via set_town_title metarule.
const char* sfallGetTownTitleOverride(int areaIndex)
{
    auto it = gTownTitleOverrides.find(areaIndex);
    if (it != gTownTitleOverrides.end()) {
        return it->second.c_str();
    }
    return nullptr;
}

// Public accessor: returns true if the given PID has an explosive override
// with a non-zero delay value, populating outDelay (in seconds).
// Returns false if no override exists or delay is 0 (outDelay unchanged).
// Used by _obj_use_explosive() in proto_instance.cc.
bool sfallGetExplosiveOverrideDelay(int pid, int* outDelay)
{
    auto it = gExplosiveOverrides.find(pid);
    if (it == gExplosiveOverrides.end()) {
        return false;
    }
    if (it->second.delay <= 0) {
        return false;
    }
    if (outDelay) *outDelay = it->second.delay;
    return true;
}

// Public accessor: returns the override car interface art FID,
// or -1 if no override has been set via set_car_intface_art metarule.
int sfallGetCarIntfaceArtFid()
{
    return gCarIntfaceArtFid;
}

// Public accessor: returns pointer to spray settings struct.
// Always returns a valid pointer — check .active before using values.
const SpraySettings* sfallGetSpraySettings()
{
    return &gSpraySettings;
}

// Public accessor: looks up drug data override for the given drug index.
// Returns true if an override exists, populating outAddictionRate/outEffectDuration.
bool sfallGetDrugDataOverride(int drugIndex, int* outAddictionRate, int* outEffectDuration)
{
    auto it = gDrugDataOverrides.find(drugIndex);
    if (it != gDrugDataOverrides.end()) {
        *outAddictionRate = it->second.addictionRate;
        *outEffectDuration = it->second.effectDuration;
        return true;
    }
    return false;
}

// Public accessor: returns true if the specified trait has been added
// via the add_trait metarule and participates in stat/skill modifiers.
bool sfallIsTraitAdded(int traitId)
{
    return gAddedTraits.find(traitId) != gAddedTraits.end();
}

// Public accessor: removes a trait from gAddedTraits. Called by
// op_remove_trait to bridge the engine's selected-traits system with
// the sfall add_trait metarule system. Returns true if the trait was
// present and removed, false if it was not found.
bool sfallRemoveTraitAdded(int traitId)
{
    auto it = gAddedTraits.find(traitId);
    if (it != gAddedTraits.end()) {
        gAddedTraits.erase(it);
        return true;
    }
    return false;
}

// Public accessor: returns whether npc_engine_level_up is enabled.
// 1 = enabled (NPCs auto-level), 0 = disabled (scripts control leveling).
int sfallGetNpcEngineLevelUpEnabled()
{
    return gNpcEngineLevelUpEnabled;
}

// Public accessor: returns true if the given PID has an explosive override
// set via the item_make_explosive metarule.
bool sfallIsExplosiveOverride(int pid)
{
    return gExplosiveOverrides.count(pid) > 0;
}

// Public accessor: returns true and populates damage fields if the given PID
// has an explosive override with damage values set via item_make_explosive.
// Returns false if no override exists for this PID.
bool sfallGetExplosiveOverrideDamage(int pid, int* outMinDamage, int* outMaxDamage)
{
    auto it = gExplosiveOverrides.find(pid);
    if (it == gExplosiveOverrides.end()) {
        return false;
    }
    if (outMinDamage) *outMinDamage = it->second.minDamage;
    if (outMaxDamage) *outMaxDamage = it->second.maxDamage;
    return true;
}

// Public accessor: returns the stored unjam locks time in game hours.
// -1 = disabled/engine default.
// INTEGRATION POINT: map.cc — wire into the lock unjam logic during map entry.
// Currently, objectUnjamAll() (map.cc:1149) unjams all locks unconditionally.
// This accessor should gate or replace that behavior: when the override is set,
// track jammed-lock timers and unjam only after the specified game hours have
// elapsed. The stored value persists across save/load (version 3+ of metarules
// save format).
int sfallGetUnjamLocksTime()
{
    return gUnjamLocksTimeHours;
}

// Public accessor: returns the stored map enter X position override.
// -1 = no override.
// INTEGRATION POINT: map.cc — wire into the map entry / position-setting code.
// When the player enters a new map, this override should be applied to the
// player's starting position instead of the default map entry point.
int sfallGetMapEnterX()
{
    return gMapEnterX;
}

// Public accessor: returns the stored map enter Y position override.
// -1 = no override.
// INTEGRATION POINT: map.cc — same as sfallGetMapEnterX.
int sfallGetMapEnterY()
{
    return gMapEnterY;
}

// Public accessor: returns the stored map enter elevation override.
// -1 = no override.
// INTEGRATION POINT: map.cc — same as sfallGetMapEnterX.
int sfallGetMapEnterElevation()
{
    return gMapEnterElevation;
}

// Public accessor: returns the script name override set via set_scr_name.
// Empty string = no override. Consumer: scriptsGetFileName() in scripts.cc.
const char* sfallGetScriptNameOverride()
{
    return gScriptNameOverride.empty() ? nullptr : gScriptNameOverride.c_str();
}

// F-11: NPC fake perk/trait/selectable-perk accessors.
// Returns pointer to the NPC fake entries map for the given critter CID,
// or nullptr if no entries exist. The returned pointer references the
// static map and remains valid until the next modification.
const std::unordered_map<std::string, FakePerkNpcEntry>* sfallGetFakePerksNpc(int cid)
{
    auto it = gFakePerksNpc.find(cid);
    return (it != gFakePerksNpc.end()) ? &it->second : nullptr;
}

const std::unordered_map<std::string, FakePerkNpcEntry>* sfallGetFakeTraitsNpc(int cid)
{
    auto it = gFakeTraitsNpc.find(cid);
    return (it != gFakeTraitsNpc.end()) ? &it->second : nullptr;
}

const std::unordered_map<std::string, FakePerkNpcEntry>* sfallGetFakeSelectablePerksNpc(int cid)
{
    auto it = gFakeSelectablePerksNpc.find(cid);
    return (it != gFakeSelectablePerksNpc.end()) ? &it->second : nullptr;
}

} // namespace fallout
