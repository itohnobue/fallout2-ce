# Sfall Compatibility

This document tracks Fallout 2 CE compatibility with sfall.  This is for modders who need to know which Sfall features work in CE.

For now, this covers opcodes/metarules, and hooks.  In the future, it will include other ways of modifying the engine (like ini files), and other Sfall-specific behaviour.

## HRP EDG Scroll-Blocker Support

CE supports the `.edg` file format from the HRP (High Resolution Patch), which defines per-map scroll boundaries and square-level render clipping.

**How it works in CE:**

- On map load, `maps/<mapname>.edg` is read if present. Missing file = silent fallback to the scroll-blocker object system.
- The `.edg` file defines per-elevation rectangle boundary zones. Multiple chained zones per elevation are supported.
- When loaded, these zones are used for both scroll blocking and visible area clipping (black bars), replacing both vanilla scroll blocking and CE hi-res stencil system.
- v2 EDG files also contain a `SquareRect` that defines "Angled edges", or square-grid stencil. This is also supported.


## Settings (ddraw.ini → fallout2.cfg / game.cfg)

Settings previously read from `ddraw.ini` have been moved into standard CE config files.

Most settings that control game behavior (premade characters, extra message files, combat tweaks, worldmap, etc.) have been moved into [`<DAT>/config/game.cfg`](files/ce.dat/config/game.cfg), which is a content-mod config file intended to be overridden by mods. See that file for the full list with descriptions.

The following settings were moved into [`fallout2.cfg`](files/fallout2.cfg) instead:

| ddraw.ini section | ddraw.ini key | fallout2.cfg section | fallout2.cfg key |
| --- | --- | --- | --- |
| `Misc` | `SkipOpeningMovies` | `ui` | `skip_opening_movies` |
| `Misc` | `DisplayKarmaChanges` | `ui` | `display_karma_changes` |
| `Misc` | `DisplayBonusDamage` | `ui` | `display_bonus_damage` |
| `Misc` | `NumbersInDialogue` | `ui` | `numbers_in_dialogue` |
| `Misc` | `AutoQuickSave` | `ui` | `auto_quick_save` |
| `Main` | `EnableHighResolutionStencil` | `ui` | `enable_high_resolution_stencil` |
| `Misc` | `ConsoleOutputPath` | `debug` | `console_output_path` |
| `Misc` | `GaplessMusic` | `sound` | `gapless_music` |
| `Misc` | `ScreenshotsFormat` | `system` | `screenshots_format` |
| `Misc` | `UseWalkDistance` | `qol` | `use_walk_distance` |
| `Misc` | `AutoOpenDoors` | `qol` | `auto_open_doors` |

### Speed Control (`[Speed]` section of `ddraw.ini`)

CE supports sfall's game speed multiplier via the `[Speed]` section of `ddraw.ini`:

| Key | Default | Description |
| --- | --- | --- |
| `SpeedMultiInitial` | 100 | Initial speed multiplier percentage. Preferred key; falls back to `SpeedMulti` if absent. |
| `SpeedMulti` | 100 | Fallback speed multiplier. Used if `SpeedMultiInitial` is not present. |

**How it works:**

- On game init, the speed value is read from `ddraw.ini [Speed]` and stored in sfall global variable 0 (`src/game.cc:373-386`).
- The global speed multiplier is applied in `animationComputeTicksPerFrame()` at `src/animation.cc:3350-3364`, after combat speed adjustments and before the FPS-to-milliseconds conversion.
- The multiplier affects ALL animation types (walks, idles, attacks, etc.) — not just combat movement.
- Scripts can change it at runtime via `set_sfall_global(0, value)` and read it via `get_sfall_global_int(0)`.
- On game reset (`gameReset`), the value is re-initialized from `ddraw.ini` to match sfall behavior (`src/sfall_callbacks.cc:43-56`).
- Values ≤ 0 are clamped to 100 to prevent game freeze.
- SpeedMulti is independent of the FPS limiter (`fps_limiter.cc`) — it controls animation speed, not rendering frame rate.

## Opcodes / Metarules

See [`https://sfall-team.github.io/sfall/`](https://sfall-team.github.io/sfall/) for documentation on specific functions.

| Group | Opcodes In Group | Compatibility | Notes |
| --- | --- | --- | --- |
| Direct memory access| read_byte,short,int,string<br>write_byte,short,int,string<br>call_offset_vX | partially (no-ops) | read_byte supports specific addresses (0x56D38C combat highlight, 0x410003 Rotators fork detection → 0xF4). read_short/int/string return -1 stub. write_byte/short/int/string and all call_offset_v0-v4 / call_offset_r0-r4 are registered as safe no-ops (requires `AllowUnsafeScripting=1`). |
| Stats | get/set_pc_base_stat<br>get/set_pc_extra_stat<br>get/set_critter_base_stat<br>get/set_critter_extra_stat | ✅ | CE uses engine stat helpers here instead of sfall's direct proto-field behavior, so derived-stat update behavior can differ. |
| Stats / Alter min/max | get/set_stat_min/max<br>set_pc_stat_min/max<br>set_npc_stat_min/max | ✅ | get_stat_max/get_stat_min implemented (sfall_metarules.cc:2095,2111). set_pc_stat_max/min and set_npc_stat_max/min registered and implemented (sfall_opcodes.cc). All use engine stat helpers. |
| Skills | get/set_critter_skill_points<br>get/set_available_skill_points<br>set_skill_max<br>set_critter_skill_mod<br>set_base_skill_mod<br>mod_skill_points_per_level | ✅ | set_skill_max wired into skill.cc. mod_skill_points_per_level stored; consumed by characterEditorUpdateLevel() (character_editor.cc:5758). get_critter_skill_points, get_available_skill_points registered and implemented (sfall_opcodes.cc). set_critter_skill_mod (0x81C7) and set_base_skill_mod (0x81C8): fully integrated — consumed in skillGetValue() at skill.cc:252,267,270 via sfallGetBaseSkillMod() / sfallGetCritterSkillMod(). |
| Graphics | graphics_funcs_available<br>force_graphics_refresh<br>get_screen_width<br>get_screen_height<br>set_palette | ✅ | get_screen_width, get_screen_height fully implemented. set_palette (0x81F2) registered as safe no-op (SDL2 rendering does not support palette overrides). graphics_funcs_available, shader ops not implemented. |
| Shaders | load_shader<br>free_shader<br>activate_shader<br>deactivate_shader<br>set/get_shader_* | 🚫 | likely will not implement direct compatibility
| Perks and traits | set_perk_image<br>set_perk_*<br>set_pyromaniac_mod<br>apply_heaveho_fix<br>set_swiftlearner_mod<br>has/set_fake_perk<br>has/set_fake_trait<br>set_selectable_perk<br>set_perkbox_title<br>show/hide_real_perks<br>perk_add_mode<br>clear_selectable_perks<br>add/remove_trait<br>seq_perk_freq<br>set_perk_name<br>set_perk_desc<br>set_hp_per_level_mod | ✅ | set_perk_freq is fully integrated (gPerkFrequencyOverride wired into character_editor.cc). set_perk_level override fully integrated via perkSetMinLevel (perk.cc:588). set_pyromaniac_mod consumed at combat.cc:4858. set_swiftlearner_mod consumed at stat.cc:822. set_hp_per_level_mod consumed at stat.cc:859,912. set_critter_skill_mod and set_base_skill_mod consumed at skill.cc:252,267,270. perk_add_mode, clear_selectable_perks, hide_real_perks integrated via sfallGet* accessors at character_editor.cc. add_trait/remove_trait fully implemented (sfall_metarules.cc:2984) with 1-arg and 3-arg forms; display integrated via gAddedTraits at character_editor.cc:2164. set_perk_name, set_perk_desc stored in perk override arrays and consumed in perk.cc via sfallGetPerk*Override() accessors. set_perk_image, apply_heaveho_fix, seq_perk_freq not yet implemented. |
| Virtual file system | fs_create<br>fs_copy<br>fs_find<br>fs_read/write_*<br>fs_delete<br>fs_size<br>fs_pos<br>fs_seek<br>fs_resize | ✅ | All 18 fs_* opcodes are registered and implemented (see `sfall_opcodes.cc:2892-2927`). |
| Combat / Knockback | set_weapon_knockback<br>set_target_knockback<br>set_attacker_knockback<br>remove_weapon_knockback<br>remove_target_knockback<br>remove_attacker_knockback | ✅ | All 6 knockback opcodes registered. Knockback modifiers consumed in _compute_dmg_damage at combat.cc:4837-4857 for all 3 types (weapon, target, attacker) with absolute (type=1) and additive (type=2) modes. |
| Maps and encounters | in_world_map<br>force_encounter{_with_flags}<br>set_map_time_multi<br>get/set_map_enter_position<br>exec_map_update_scripts<br>get/set_terrain_name<br>set_town_title<br>get/set_can_rest_on_map<br>set_rest_heal_time<br>set_rest_mode<br>set_worldmap_heal_time | implemented: in_world_map, force_encounter, force_encounter_with_flags, set_map_time_multi, exec_map_update_scripts, get/set_can_rest_on_map, get/set_terrain_name, get/set_town_title, set_rest_heal_time, set_worldmap_heal_time, set_rest_mode | - |
| Maps and encounters / Worldmap | get_world_map_x/y_pos<br>set_world_map_pos | ✅ | - |
| Audio | play_sfall_sound<br>stop_sfall_sound | ✅ | `play_sfall_sound` currently supports `.acm`, `.wav`, `.ogg` formats, and can load from `.dat` archives. `.mp3` is not yet supported. |
| Combat / Weapons and ammo | get/set_weapon_ammo_pid<br>get/set_weapon_ammo_count | ✅ | - |
| Sfall / Version | sfall_ver_major<br>sfall_ver_minor<br>sfall_ver_build | ✅ | CE currently reports `4.5.1` |
| Utility / Math | log, exponent, round, sqrt, abs, sin, cos, tan, arctan, ceil, ^, floor2, div | ✅ | - |
| Keyboard and mouse | key_pressed<br>tap_key<br>get_mouse_x/y<br>get_mouse_buttons | ✅ | - |
| Lists | list_begin<br>list_next<br>list_end<br>list_as_array<br>party_member_list | ✅ | - |
| Explosions | set_attack_explosion_pattern<br>set_attack_explosion_art<br>set_attack_explosion_radius<br>set_attack_is_explosion_fire<br>set_explosion_radius<br>set_dynamite_damage<br>set_plastic_damage<br>get_explosion_damage<br>set_explosion_max_targets<br>item_make_explosive | ✅ | item_make_explosive registered and implemented (sfall_metarules.cc:2129, stores in gExplosiveOverrides). |
| Animations | reg_anim_combat_check<br>reg_anim_destroy<br>reg_anim_animate_and_hide<br>reg_anim_light<br>reg_anim_change_fid<br>reg_anim_take_out<br>reg_anim_turn_towards<br>reg_anim_callback<br>reg_anim_animate_and_move | ✅ | - |
| Art and appearance | art_exists<br>art_frame_data<br>refresh_pc_art<br>art_cache_clear | ✅ | - |
| Tiles and paths | get_tile_fid<br>tile_under_cursor<br>tile_light<br>tile_get_objs<br>tile_refresh_display<br>obj_blocking_tile<br>tile_by_position<br>get_tile_ground_fid<br>get_tile_roof_fid<br>obj_blocking_line<br>path_find_to<br>objects_in_radius | ✅ | `get_tile_ground_fid` and `get_tile_roof_fid` are sfall.h convenience wrappers around `get_tile_fid` (with mode parameter: 0=ground, 1=roof). The underlying opcode provides the full functionality. |
| Utility | sprintf<br>typeof<br>atoi<br>atof | ✅ | - |
| Utility / Strings | string_split<br>substr<br>strlen<br>charcode<br>get_string_pointer<br>string_find<br>string_find_from<br>string_format<br>string_format_array<br>string_replace<br>string_to_case<br>string_compare | ✅ except get_string_pointer (🚫) | `get_string_pointer` is deprecated and intentionally omitted. `string_find_from` is available as the 3-arg form of `string_find`. |
| Interface / Tags | show_iface_tag<br>hide_iface_tag<br>is_iface_tag_active<br>set_iface_tag_text<br>add_iface_tag | ✅ | Legacy `BoxBarCount`, `BoxBarColors` ddraw.ini settings not supported. |
| Global variables | set_sfall_global<br>get_sfall_global_int<br>get_sfall_global_float | ✅ | Current CE storage is int-backed; `set_sfall_global` stores integer values. `get_sfall_global_float` is registered and implemented (sfall_opcodes.cc:490). |
| Hooks / Hook functions | init_hook<br>get_sfall_arg<br>get_sfall_args<br>get_sfall_arg_at<br>set_sfall_return<br>set_sfall_arg<br>register_hook<br>register_hook_proc<br>register_hook_proc_spec | ✅ | See below for implemented hooks. `init_hook` is deprecated and will not be implemented. Both register_hook_proc and register_hook_proc_spec add hooks to the *beginning* of the hook list (see `sfall_script_hooks.cc:150`). |
| Arrays / Array functions | create_array<br>temp_array<br>fix_array<br>get/set_array<br>resize_array<br>free_array<br>scan_array<br>len_array<br>save/load_array<br>array_key<br>arrayexpr | ✅ | - |
| Perks and traits / NPC perks | set_fake_perk_npc<br>set_fake_trait_npc<br>set_selectable_perk_npc<br>has_fake_perk_npc<br>has_fake_trait_npc | ✅ (metarules) | All five NPC perk metarules are registered and implemented (see `sfall_metarules.cc`). |
| Global scripts / Global script functions | set_global_script_repeat<br>set_global_script_type<br>available_global_script_types | ✅ | available_global_script_types registered and implemented (sfall_opcodes.cc:442). |
| Combat | attack_is_aimed<br>block_combat<br>force_aimed_shots<br>disable_aimed_shots<br>get_attack_type<br>get/set_bodypart_hit_modifier<br>combat_data<br>get/set/reset_critical_table<br>get_last_target<br>get_last_attacker<br>set_critter_burst_disable<br>get/set_critter_current_ap<br>set_spray_settings<br>get/set_combat_free_move | ✅ | block_combat (0x824A) registered as safe no-op (combat control is internal). force_aimed_shots (0x823E), disable_aimed_shots (0x823F) registered and implemented; combat.cc checks per-PID flags via sfallGetForceAimedShots/sfallGetDisableAimedShots. set_spray_settings fully wired: _compute_spray (combat.cc:3852-3882) consumes per-weapon PID filtering, count/radius overrides, and ammo clamping. |
| Car | set_car_current_town<br>car_gas_amount<br>set_car_intface_art | ✅ | set_car_intface_art registered and implemented (sfall_metarules.cc:2424, stores FID in gCarIntfaceArtFid). |
| Interface / Windows and images | interface_art_draw<br>interface_print<br>draw_image{_scaled}<br>get_window_under_mouse<br>create_win<br>get_window_attribute<br>message_box<br>set_window_flag<br>win_fill_color<br>interface_overlay<br>dialog_message<br>get_text_width<br>hide_window<br>show_window<br>create_message_window | ✅ | interface_overlay implemented (sfall_metarules.cc:2814) — supports create/destroy/clear actions. interface_print registered and implemented (sfall_metarules.cc:2319). |
| Interface / Outline | outlined_object<br>get_outline<br>set_outline | ✅ | - |
| Interface / Main interface | intface_is_hidden<br>intface_redraw<br>intface_hide<br>intface_show<br>set_quest_failure_value | ✅ | `intface_redraw` supports both 0-arg (redraw entire bar) and 1-arg (redraw window by type) forms (sfall_metarules.cc:1065). `intface_hide` (sfall_metarules.cc:2264), `intface_show` (sfall_metarules.cc:2271), `intface_is_hidden` (sfall_metarules.cc:2278) registered and implemented. `set_quest_failure_value` fully implemented with setter (sfall_metarules.cc:1884) and getter (sfall_metarules.cc:1895). |
| Interface / Inventory | display_stats<br>inventory_redraw<br>critter_inven_obj2<br>get_current_inven_size<br>item_weight | ✅ | get_current_inven_size registered and implemented (sfall_metarules.cc:2019, returns obj->data.inventory.length). |
| Interface / Cursor | get/set_cursor_mode | ✅ | - |
| Locks | lock_is_jammed<br>unjam_lock<br>set_unjam_locks_time | partial | lock_is_jammed registered and implemented (sfall_metarules.cc:2167, checks OBJ_JAMMED flag). unjam_lock fully implemented (sfall_metarules.cc:2661, wraps engine's objectUnjamLock). set_unjam_locks_time: registered and consumed in mapLoadSaved (map.cc) — overrides default 24-hour unjam threshold. |
| INI settings | get_ini_setting<br>get_ini_string<br>get_ini_section<br>get_ini_sections<br>get_ini_config<br>get_ini_config_db<br>set_ini_setting | ✅ | `modified_ini` is intentionally omitted as deprecated. |
| Objects and scripts | set_self<br>set_dude_obj<br>real_dude_obj<br>remove_script<br>get/set_script<br>obj_is_carrying_obj<br>loot_obj<br>dialog_obj<br>obj_under_cursor<br>get/set_object_data<br>get/set_flags<br>set_unique_id<br>set_scr_name<br>obj_is_openable<br>get/set_proto_data<br>get_object_ai_data | implemented: set_self, set_dude_obj, real_dude_obj, get/set/remove_script, obj_is_carrying_obj, loot_obj, dialog_obj, obj_under_cursor, get_object_data, set_object_data (metarule), get_flags, set_flags, set_unique_id, set_scr_name, obj_is_openable, get_proto_data, set_proto_data, get_object_ai_data (type 0) | set_dude_obj/real_dude_obj/set_object_data/set_scr_name are implemented as metarules. get_object_ai_data type 0 (AI packet number) implemented; types 1-2 (AI flags, procedure) implemented via aiPacketGetFlags/aiPacketGetProcedure accessors. |
| Other / Game management | set_movie_path<br>stop/resume_game<br>mark_movie_played<br>game_loaded<br>get_game_mode<br>get_uptime<br>signal_close_game | ✅ | mark_movie_played (0x8240) registered as safe no-op (CE movie tracking is internal). stop/resume_game (0x8222,0x8223) registered as safe no-ops. game_loaded, get_game_mode, get_uptime, signal_close_game fully implemented. |
| Gameplay tweaks | set_pickpocket_max<br>set_hit_chance_max<br>set_xp_mod<br>set_critter_hit_chance_mod<br>set_base_hit_chance_mod<br>set_hp_per_level_mod<br>gdialog_get_barter_mod<br>get/set_unspent_ap_bonus<br>get/set_unspent_ap_perk_bonus<br>set_base_pickpocket_mod<br>set_critter_pickpocket_mod<br>get/set_inven_ap_cost<br>set_drugs_data<br>get_kill_counter<br>mod_kill_counter<br>set_pipboy_available | ✅ | gdialog_get_barter_mod, get/set_unspent_ap{_perk}_bonus, get/set_inven_ap_cost, set_xp_mod, set_hit_chance_max, set_base_hit_chance_mod fully implemented. set_critter_hit_chance_mod (0x81C5) implemented: per-critter modifier consumed in attackDetermineToHit() (combat.cc) via sfallGetCritterHitChanceMod(), additive with global set_base_hit_chance_mod. set_hp_per_level_mod consumed at stat.cc:859,912. Pickpocket modifiers (0x81A0, 0x81C9, 0x81CA) fully integrated — consumed in skillsPerformStealing() (skill.cc) via sfallGetPickpocket*() accessors. Cap uses sfallGetPickpocketMax() with 95 fallback. |
| NPCs | inc_npc_level<br>get_npc_level<br>npc_engine_level_up | implemented: inc_npc_level (0x81A5), get_npc_level (0x8241), npc_engine_level_up (metarule) | get_npc_level delegates to partyMemberGetLevel. npc_engine_level_up controls auto-leveling. |
| Hero Appearance | set_dm/df_model<br>hero_select_win<br>set_hero_race<br>set_hero_style | implemented: set_dm_model (0x8175), set_df_model (0x8176), hero_select_win (0x8213), set_hero_race (0x8214), set_hero_style (0x8215) | set_hero_race/set_hero_style implemented (sfall_opcodes.cc:3764,3773) — store values via sfall global vars. No config flag needed — feature is always-on. |
| Events | add_g_timer_event<br>remove_timer_event<br>create_spatial<br>spatial_radius | ✅ | All 4 opcodes registered and fully implemented. add_g_timer_event (sfall_metarules.cc:2496), remove_timer_event (sfall_metarules.cc:2267), create_spatial (sfall_opcodes.cc:4141), spatial_radius (sfall_metarules.cc:2233). |
| Other | get_year<br>active_hand<br>toggle_active_hand<br>get/set_viewport_x/y<br>get_light_level<br>message_str_game<br>sneak_success<br>unwield_slot<br>add_extra_msg_file<br>get_metarule_table<br>metarule_exist<br> | ✅ | get/set_viewport_x/y (0x81A6-0x81A9) registered as safe stubs (CE renders with SDL2, scroll is engine-managed). `input_funcs_available`, `nb_create_char` are deprecated in sfall and intentionally absent in CE. `sneak_success` registered and implemented (sfall_opcodes.cc:3683). `add_extra_msg_file` supports the 2-arg form (filename, fileNumber). |

## Hooks

| Hook | ID | Compatibility | Notes |
| --- | --- | --- | --- |
| ToHit | `HOOK_TOHIT` | ✅ | - |
| AfterHitRoll | `HOOK_AFTERHITROLL` | ✅ | Overriding `defender` leaves a lot of attack variables in previous state (e.g. distance, ->oops, roundsHitMainTarget) |
| CalcAPCost | `HOOK_CALCAPCOST` | ✅ | - |
| DeathAnim1 | `HOOK_DEATHANIM1` | 🚫 | Use DEATHANIM2 instead |
| DeathAnim2 | `HOOK_DEATHANIM2` | ✅ | - |
| CombatDamage | `HOOK_COMBATDAMAGE` | ✅ | - |
| OnDeath | `HOOK_ONDEATH` | ✅ | - |
| FindTarget | `HOOK_FINDTARGET` | ✅ | Fires at 3 combat_ai.cc call sites (lines 1679, 1706, 1760) via static_cast<HookType>(7). Hook fully operational (sfall_script_hooks.h:41). |
| UseObjOn | `HOOK_USEOBJON` | ✅ | - |
| UseObj | `HOOK_USEOBJ` | ✅ | CE notes an sfall-matching inconsistency around return code `2` behavior between interface contexts. |
| RemoveInvenObj | `HOOK_REMOVEINVENOBJ` | 🚫 | Deliberately absent: requires RMOBJ_* constants and destination object tracking not present in CE's itemRemove. Would require significant refactoring of the item removal code path. |
| BarterPrice | `HOOK_BARTERPRICE` | ✅ | - |
| ItemDamage | `HOOK_ITEMDAMAGE` | ✅ | - |
| MoveCost | `HOOK_MOVECOST` | ✅ | - |
| AmmoCost | `HOOK_AMMOCOST` | ✅ | Requires `check_weapon_ammo_cost=1` if you want pre-attack ammo validation to respect per-shot/per-round overrides. |
| KeyPress | `HOOK_KEYPRESS` | ✅ | Third hook arg is SDL_Keycode (SDL keysym, not Windows VK_ codes). Differs numerically from sfall VK_ for letters and function keys. See VK→SDL mapping table below. |
| MouseClick | `HOOK_MOUSECLICK` | ✅ | - |
| UseSkill | `HOOK_USESKILL` | ✅ | - |
| Steal | `HOOK_STEAL` | ✅ | - |
| WithinPerception | `HOOK_WITHINPERCEPTION` | ✅ | - |
| InventoryMove | `HOOK_INVENTORYMOVE` | ✅ | - |
| InvenWield | `HOOK_INVENWIELD` | ✅ | - |
| AdjustFID | `HOOK_ADJUSTFID` | ✅ | Second hook arg currently matches the first because CE has no internal FID modifiers like Hero Appearance. |
| CombatTurn | `HOOK_COMBATTURN` | ✅ | - |
| StdProcedure | `HOOK_STDPROCEDURE` | ✅ | - |
| StdProcedureEnd | `HOOK_STDPROCEDURE_END` | ✅ | - |
| CarTravel | `HOOK_CARTRAVEL` | ✅ | Fires once per worldmap tick during car travel. Speed is CE step count (3-8) matching sfall scale (3-8); fuel default is 100/tick. Override via ret0 (steps, -1 to keep) and ret1 (fuel, -1 to keep). |
| SetGlobalVar | `HOOK_SETGLOBALVAR` | ✅ | Fires on op_set_global_var for integer values only (not pointer/string values). ret0 overrides the stored value. |
| RestTimer | `HOOK_RESTTIMER` | ✅ | CE is slightly more strict: only `ret0 == 1` interrupts. Ticks wrap every 6.8y; do not rely on them for absolute game time. |
| GameModeChange | `HOOK_GAMEMODECHANGE` | ✅ | - |
| UseAnimObj | `HOOK_USEANIMOBJ` | ✅ | Fires on animate_stand_obj and animate_stand_reverse_obj |
| ExplosiveTimer | `HOOK_EXPLOSIVETIMER` | ✅ | - |
| DescriptionObj | `HOOK_DESCRIPTIONOBJ` | ✅ | Supports sfall 4.4.0+ direct string return for description override |
| UseSkillOn | `HOOK_USESKILLON` | ✅ | - |
| OnExplosion | `HOOK_ONEXPLOSION` | ✅ | Fires on explosive detonation — item timers and script-triggered explosions. |
| SubCombatDamage | `HOOK_SUBCOMBATDAMAGE` | 🚫 | (maybe) |
| SetLighting | `HOOK_SETLIGHTING` | ✅ | Fires on objectSetLight for per-object lighting changes |
| Sneak | `HOOK_SNEAK` | ✅ | Fires after each sneak check (via sneakEventProcess). arg0=result (1 success, 0 failure), arg1=duration in ticks, arg2=critter. ret0 overrides result, ret1 overrides duration. |
| TargetObject | `HOOK_TARGETOBJECT` | ✅ | Fires at the start of `_combat_attack`, when attack execution begins (after target selection by AI, before hit computation). arg0=attacker, arg1=defender, arg2=hitMode, arg3=hitLocation. |
| Dialog | `HOOK_DIALOG` | ✅ | Fires on dialog start (arg0=speaker, arg1=headFid, arg2=reaction) and exit (arg1=-1, arg2=-1, arg0=speaker). |
| DialogReaction | `HOOK_DIALOGREACTION` | ✅ | Fires when a dialog reaction is triggered (`_talk_to_critter_reacts`). arg0=speaker, arg1=reaction (-2, -1, or 0). |
| Encounter | `HOOK_ENCOUNTER` | ✅ | - |
| AdjustPoison | `HOOK_ADJUSTPOISON` | 🚫 | (maybe) |
| AdjustRads | `HOOK_ADJUSTRADS` | 🚫 | (maybe) |
| RollCheck | `HOOK_ROLLCHECK` | 🚫 | Deliberately absent: randomRoll() has 30+ call sites with no event_type context. Adding context to every call site is too invasive; pass-through hook on every roll would be too expensive. |
| BestWeapon | `HOOK_BESTWEAPON` | 🚫 | Deliberately absent: _ai_best_weapon() has 10+ return points with complex comparison logic. Object lifetime concerns with return value override. |
| CanUseWeapon | `HOOK_CANUSEWEAPON` | ✅ | - |
| BuildSfxWeapon | `HOOK_BUILDSFXWEAPON` | 🚫 | Deliberately absent: sfxBuildWeaponName() returns char* to static buffer (_sfx_file_name). String return from scripts requires buffer management and lifetime semantics. |
| StatLevelUp | `HOOK_STATLEVELUP` | ✅ | Fires in stat.cc pcAddExperienceWithOptions() and character_editor.cc characterEditorUpdateLevel() |
| Barter | `HOOK_BARTER` | ✅ | Fires in game_dialog.cc gameDialogBarter() |
| Message | `HOOK_MESSAGE` | ✅ | Fires in display_monitor.cc displayMonitorAddMessage() |

### VK → SDL Keycode Mapping

CE uses SDL2 rendering and input, not DirectInput. `HOOK_KEYPRESS` and `key_pressed()/tap_key()` receive **SDL_Keycode** values (SDL keysym), not Windows VK_ codes. The numeric values differ significantly for common keys used in RPU/Et Tu scripts.

| Key | Windows VK_ (hex) | VK_ (dec) | SDL_Keycode | SDL_SCANCODE | Notes |
| --- | --- | --- | --- | --- | --- |
| A | `VK_A` = 0x41 | 65 | SDLK_a = 97 | 4 | Letter keys: VK_ is uppercase ASCII; SDL_Keycode is lowercase |
| B | `VK_B` = 0x42 | 66 | SDLK_b = 98 | 5 | |
| ... | ... | ... | ... | ... | |
| Z | `VK_Z` = 0x5A | 90 | SDLK_z = 122 | 29 | |
| 0 | `VK_0` = 0x30 | 48 | SDLK_0 = 48 | 39 | Digit keys match between VK_ and SDL_Keycode |
| 1-9 | 0x31-0x39 | 49-57 | SDLK_1-9 = 49-57 | 30-38 | Digit keys: VK_ and SDL_Keycode are identical |
| Escape | `VK_ESCAPE` = 0x1B | 27 | SDLK_ESCAPE = 27 | 41 | Escape: VK_ and SDL_Keycode match (numerically) |
| Return | `VK_RETURN` = 0x0D | 13 | SDLK_RETURN = 13 | 40 | Return/Enter: match |
| Space | `VK_SPACE` = 0x20 | 32 | SDLK_SPACE = 32 | 44 | Space: match |
| Tab | `VK_TAB` = 0x09 | 9 | SDLK_TAB = 9 | 43 | Tab: match |
| Backspace | `VK_BACK` = 0x08 | 8 | SDLK_BACKSPACE = 8 | 42 | Backspace: match |
| Shift | `VK_SHIFT` = 0x10 | 16 | SDLK_LSHIFT/SDLK_RSHIFT = 1073742049/1073742050 | 225/229 | Shift/Control/Alt: VK_ uses modifier codes; SDL uses left/right-specific keys |
| Control | `VK_CONTROL` = 0x11 | 17 | SDLK_LCTRL/SDLK_RCTRL = 1073742048/1073742051 | 224/228 | |
| Alt | `VK_MENU` = 0x12 | 18 | SDLK_LALT/SDLK_RALT = 1073742050/1073742051 | 226/230 | |
| F1 | `VK_F1` = 0x70 | 112 | SDLK_F1 = 1073741882 | 58 | Function keys: SDL_Keycode is in 0x40000000+ range |
| F2-F12 | 0x71-0x7B | 113-123 | SDLK_F2-12 = 1073741883-93 | 59-69 | |
| Numpad 0-9 | 0x60-0x69 | 96-105 | SDLK_KP_0-9 = 1073741922-1931 | | Numpad: SDL uses separate keycodes |
| Left/Right/Up/Down | 0x25-0x28 | 37-40 | SDLK_LEFT/RIGHT/UP/DOWN = 1073741904-1907 | 80/79/82/81 | Arrow keys: SDL_Keycode is in 0x40000000+ range |

**How scripts should handle this:**

1. **Use SDL_Keycode values directly** — for mod scripts targeting CE, use SDL_ constants (e.g., `SDLK_a` = 97 instead of `VK_A` = 65).
2. **Use `key_pressed()` / `tap_key()` with SDL codes** — these functions pass the argument directly to `sfall_kb_is_key_pressed()` which expects SDL codes.
3. **In `HOOK_KEYPRESS` handlers** — the third argument (`args[2]`) is always the SDL_Keycode. If a `key_pressed()` trampoline is needed, pass the hook's arg[2] directly — no conversion required.
4. **RPU/Et Tu compatibility** — sfall scripts using VK_ constants will receive incorrect key detection. To bridge this, CE's `sfall_kb_helpers.cc` provides partial VK→SDL mapping for common keys. For unmapped keys, scripts must use SDL codes directly.

**Additional hook notes:** Registering a hook type that has no engine fire site (HOOK_DEATHANIM1, HOOK_REMOVEINVENOBJ, HOOK_SUBCOMBATDAMAGE, HOOK_ADJUSTPOISON, HOOK_ADJUSTRADS, HOOK_ROLLCHECK, HOOK_BESTWEAPON, HOOK_BUILDSFXWEAPON, and the obsolete HEX*BLOCKING hooks) will now emit a `debugPrint` warning. The hooks table above marks these as 🚫 with explanations.
