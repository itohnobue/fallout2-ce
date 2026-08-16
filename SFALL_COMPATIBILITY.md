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

The following settings were moved into [`<DAT>/config/game.cfg`](files/ce.dat/config/game.cfg):

| ddraw.ini section | ddraw.ini key | game.cfg section | game.cfg key |
| --- | --- | --- | --- |
| `Misc` | `StartGDialogFix` | `dialog` | `start_gdialog_fix` |
| `Misc` | `StartingMap` | `start` | `map` |
| `Misc` | `StartYear` | `start` | `year` |
| `Misc` | `StartMonth` | `start` | `month` |
| `Misc` | `StartDay` | `start` | `day` |
| `Misc` | `StartTime` | `start` | `time` |
| `Misc` | `StartXPos` | `start` | `worldmap_x` |
| `Misc` | `StartYPos` | `start` | `worldmap_y` |
| `Misc` | `ViewXPos` | `start` | `worldmap_view_x` |
| `Misc` | `ViewYPos` | `start` | `worldmap_view_y` |
| `Misc` | `XPTable` | `stats` | `xp_table` |
| `Misc` | `DisableSpecialMapIDs` | `maps` | `disable_special_map_ids` |
| `Misc` | `Movie1` - `Movie32` | `movies` | `movie1` - `movie32` |
| `Misc` | `Fallout1Behavior` movie behavior | `movies` | `endgame_play_after_slideshow`, `endgame_movie_male`, `endgame_movie_female` |
| `Sound` | `MainMenuMusic` | `sound` | `main_menu_music` |
| `Sound` | `WorldMapMusic` | `sound` | `worldmap_music` |
| `Sound` | `WorldMapCarMusic` | `sound` | `worldmap_car_music` |
| `Sound` | `EndGameMovieMusic0` | `sound` | `endgame_movie_music0` |
| `Sound` | `EndGameMovieMusic1` | `sound` | `endgame_movie_music1` |
| `Sound` | `MapLoadingSound` | `sound` | `map_loading_sound` |
| `Misc` | `Movie1` - `Movie32` | `movies` | `movie1` - `movie32` |
| `Misc` | `Fallout1Behavior` movie behavior | `movies` | `endgame_play_after_slideshow`, `endgame_movie_male`, `endgame_movie_female` |
| `Interface` | `WorldMapTerrainInfo` | `worldmap` | `terrain_info` |
| `Misc` | `WorldMapFPSPatch` + `WorldMapDelay2` | `worldmap` | `travel_delay` |

Unlike sfall, `travel_delay` throttles only world-map travel simulation. Input,
rendering, and world-map scripts continue at the normal frame rate.

CE does not provide a single `Fallout1Behavior` compatibility switch. The movie
portion can be configured directly: set `endgame_play_after_slideshow=0`,
`endgame_movie_male=10`, and `endgame_movie_female=11` for the sfall Fallout 1
movie sequence. Time-limit and item-weight behavior are separate features.

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
| Stats / Alter min/max | get/set_stat_min/max<br>set_pc_stat_min/max<br>set_npc_stat_min/max | ✅ | get_stat_max/get_stat_min implemented (sfall_metarules.cc:2095,2111). set_pc_stat_max/min and set_npc_stat_min/max registered and implemented (sfall_opcodes.cc). All use engine stat helpers. |
| Skills | get/set_critter_skill_points<br>get/set_available_skill_points<br>set_skill_max<br>set_critter_skill_mod<br>set_base_skill_mod<br>mod_skill_points_per_level | ✅ | set_skill_max wired into skill.cc. mod_skill_points_per_level stored; consumed by characterEditorUpdateLevel() (character_editor.cc:5758). get_critter_skill_points, get_available_skill_points registered and implemented (sfall_opcodes.cc). set_critter_skill_mod (0x81C7) and set_base_skill_mod (0x81C8): fully integrated — consumed in skillGetValue() at skill.cc:252,267,270 via sfallGetBaseSkillMod() / sfallGetCritterSkillMod(). |
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

| Utility / Strings | string_split<br>substr<br>strlen<br>charcode<br>get_string_pointer<br>string_find<br>string_find_from<br>string_format<br>string_format_array<br>string_replace<br>string_to_case<br>string_compare | ✅ | `get_string_pointer` is deprecated and intentionally omitted. |
| Interface / Tags | show_iface_tag<br>hide_iface_tag<br>is_iface_tag_active<br>set_iface_tag_text<br>add_iface_tag | ✅ | Legacy `BoxBarCount`, `BoxBarColors` ddraw.ini settings not supported.<br> `show_iface_tag` and `hide_iface_tag` do not not work for tag values `1` (Poisoned) and `2` (Radiated). <br> `set_iface_tag_text` and `add_iface_tag` works only for custom tags `>= 5`. <br> `is_iface_tag_active` is supporting all the tag values. |
| Global variables | set_sfall_global<br>get_sfall_global_int<br>get_sfall_global_float | ✅ except get_sfall_global_float | Current CE storage is int-backed; `set_sfall_global` stores integer values |
| Hooks / Hook functions | init_hook<br>get_sfall_arg<br>get_sfall_args<br>get_sfall_arg_at<br>set_sfall_return<br>set_sfall_arg<br>register_hook<br>register_hook_proc<br>register_hook_proc_spec | ✅ | See below for implemented hooks. `init_hook` is deprecated and will not be implemented. register_hook_proc and register_hook_proc_spec both add hooks to the *end* of the hook list, instead of beginning and end, respectively. |
| Arrays / Array functions | create_array<br>temp_array<br>fix_array<br>get/set_array<br>resize_array<br>free_array<br>scan_array<br>len_array<br>save/load_array<br>array_key<br>arrayexpr | ✅ | - |
| Perks and traits / NPC perks | set_fake_perk_npc<br>set_fake_trait_npc<br>set_selectable_perk_npc<br>has_fake_perk_npc<br>has_fake_trait_npc | not implemented | - |
| Global scripts / Global script functions | set_global_script_repeat<br>set_global_script_type<br>available_global_script_types | ✅ except available_global_script_types | - |
| Combat | attack_is_aimed<br>block_combat<br>force_aimed_shots<br>disable_aimed_shots<br>get_attack_type<br>get/set_bodypart_hit_modifier<br>combat_data<br>get/set/reset_critical_table<br>get_last_target<br>get_last_attacker<br>set_critter_burst_disable<br>get/set_critter_current_ap<br>set_spray_settings<br>get/set_combat_free_move<br>set_fo1_hit_chance | ✅ except block_combat, force_aimed_shots, disable_aimed_shots, get_last_target, get_last_attacker, set_spray_settings | - |
| Car | set_car_current_town<br>car_gas_amount<br>set_car_intface_art | ✅ | - |
| Interface / Outline | outlined_object<br>get_outline<br>set_outline | ✅ | - |
| Interface / Main interface | intface_is_hidden<br>intface_redraw<br>intface_hide<br>intface_show<br>set_quest_failure_value | ✅ | `intface_redraw` supports both 0-arg (redraw entire bar) and 1-arg (redraw window by type) forms (sfall_metarules.cc:1065). `intface_hide` (sfall_metarules.cc:2264), `intface_show` (sfall_metarules.cc:2271), `intface_is_hidden` (sfall_metarules.cc:2278) registered and implemented. `set_quest_failure_value` fully implemented with setter (sfall_metarules.cc:1884) and getter (sfall_metarules.cc:1895). |
| Interface / Inventory | display_stats<br>inventory_redraw<br>critter_inven_obj2<br>get_current_inven_size<br>item_weight | ✅ | get_current_inven_size registered and implemented (sfall_metarules.cc:2019, returns obj->data.inventory.length). |
| Interface / Cursor | get/set_cursor_mode | ✅ | - |
| Locks | lock_is_jammed<br>unjam_lock<br>set_unjam_locks_time | partial | lock_is_jammed registered and implemented (sfall_metarules.cc:2167, checks OBJ_JAMMED flag). unjam_lock fully implemented (sfall_metarules.cc:2661, wraps engine's objectUnjamLock). set_unjam_locks_time: registered and consumed in mapLoadSaved (map.cc) — overrides default 24-hour unjam threshold. |
| INI settings | get_ini_setting<br>get_ini_string<br>get_ini_section<br>get_ini_sections<br>get_ini_config<br>get_ini_config_db<br>set_ini_setting | ✅ | `modified_ini` is intentionally omitted as deprecated. |
| Objects and scripts | set_self<br>set_dude_obj<br>real_dude_obj<br>remove_script<br>get/set_script<br>obj_is_carrying_obj<br>loot_obj<br>dialog_obj<br>obj_under_cursor<br>get/set_object_data<br>get/set_flags<br>set_unique_id<br>set_scr_name<br>obj_is_openable<br>get/set_proto_data<br>get_object_ai_data | implemented: set_self, set_dude_obj, real_dude_obj, get/set/remove_script, obj_is_carrying_obj, loot_obj, dialog_obj, obj_under_cursor, get_object_data, set_object_data (metarule), get_flags, set_flags, set_unique_id, set_scr_name, obj_is_openable, get_proto_data, set_proto_data, get_object_ai_data (type 0) | set_dude_obj/real_dude_obj/set_object_data/set_scr_name are implemented as metarules. get_object_ai_data type 0 (AI packet number) implemented; types 1-2 (AI flags, procedure) implemented via aiPacketGetFlags/aiPacketGetProcedure accessors. **Caveat:** `get_object_data`/`set_object_data` read/write raw bytes at script-supplied offsets against `sizeof(Object)` bounds (`sfall_metarules.cc:861-896`). sfall's `C_ATTACK_*` constants are 32-bit `Attack`-struct offsets; CE's 64-bit `Attack` places `defenderFlags` at byte 68, not 48 (`combat_defs.h:102-124`), so `get_object_data(combat_data, C_ATTACK_FLAGS_TARGET)` reads the wrong field (see the RPU section below). |
| Other / Game management | set_movie_path<br>stop/resume_game<br>mark_movie_played<br>game_loaded<br>get_game_mode<br>get_uptime<br>signal_close_game | ✅ | set_movie_path (0x8177) implemented — runtime override + game.cfg `[movies] movie1..movie32` config (68ff38e). mark_movie_played (0x8240) fully implemented via gameMovieMarkSeen. stop/resume_game (0x8222,0x8223) registered as safe no-ops. game_loaded, get_game_mode, get_uptime, signal_close_game fully implemented. |
| Gameplay tweaks | set_pickpocket_max<br>set_hit_chance_max<br>set_xp_mod<br>set_critter_hit_chance_mod<br>set_base_hit_chance_mod<br>set_hp_per_level_mod<br>gdialog_get_barter_mod<br>get/set_unspent_ap_bonus<br>get/set_unspent_ap_perk_bonus<br>set_base_pickpocket_mod<br>set_critter_pickpocket_mod<br>get/set_inven_ap_cost<br>set_drugs_data<br>get_kill_counter<br>mod_kill_counter<br>set_pipboy_available | ✅ | gdialog_get_barter_mod, get/set_unspent_ap{_perk}_bonus, get/set_inven_ap_cost, set_xp_mod, set_hit_chance_max, set_base_hit_chance_mod fully implemented. set_critter_hit_chance_mod (0x81C5) implemented: per-critter modifier consumed in attackDetermineToHit() (combat.cc) via sfallGetCritterHitChanceMod(), additive with global set_base_hit_chance_mod. set_hp_per_level_mod consumed at stat.cc:859,912. Pickpocket modifiers (0x81A0, 0x81C9, 0x81CA) fully integrated — consumed in skillsPerformStealing() (skill.cc) via sfallGetPickpocket*() accessors. Cap uses sfallGetPickpocketMax() with 95 fallback. |
| NPCs | inc_npc_level<br>get_npc_level<br>npc_engine_level_up | implemented: inc_npc_level (0x81A5), get_npc_level (0x8241), npc_engine_level_up (metarule) | get_npc_level delegates to partyMemberGetLevel. npc_engine_level_up controls auto-leveling. |
| Hero Appearance | set_dm/df_model<br>hero_select_win<br>set_hero_race<br>set_hero_style | implemented: set_dm_model (0x8175), set_df_model (0x8176), hero_select_win (0x8213), set_hero_race (0x8214), set_hero_style (0x8215) | set_hero_race/set_hero_style implemented (sfall_opcodes.cc:5969-5982, registered 8807-8808) — store values via sfall global vars. No config flag needed — feature is always-on. |
| Events | add_g_timer_event<br>remove_timer_event<br>create_spatial<br>spatial_radius | ✅ | All 4 opcodes registered and fully implemented. add_g_timer_event (sfall_metarules.cc:2496), remove_timer_event (sfall_metarules.cc:2267), create_spatial (sfall_opcodes.cc:4141), spatial_radius (sfall_metarules.cc:2233). |
| Other | get_year<br>active_hand<br>toggle_active_hand<br>get/set_viewport_x/y<br>get_light_level<br>message_str_game<br>sneak_success<br>unwield_slot<br>add_extra_msg_file<br>get_metarule_table<br>metarule_exist<br> | ✅ | get/set_viewport_x/y (0x81A6-0x81A9) registered as safe stubs (CE renders with SDL2, scroll is engine-managed). `input_funcs_available`, `nb_create_char` are deprecated in sfall and intentionally absent in CE. `sneak_success` registered and implemented (sfall_opcodes.cc:3683). `add_extra_msg_file` supports the 2-arg form (filename, fileNumber). |

### CE-only metarules

CE defines several metarules that are not supported in Sfall. Include [ce.h](files/ce.h) for the #defines.

| Name | Definition |
| --- | --- |
| `encounter_intros(toggle)` | Enable or disable the display-monitor random encounter intro message, for example `You encounter: ...`. This does not affect the separate encounter detection dialog. |
| `set_reaction_thresholds(neutral, good)` | Set thresholds for reactions considered "neutral" and "good". Defaults: FO1 -25/25, FO2 -51/49 (fork keeps the original per-game thresholds). |
| `set_party_member_cc_msg_ids(pid, start_msg_id, end_msg_id)` | Override party-member combat-control update messages for a pid. Picks randomly from the inclusive contiguous range. Default fallback ranges are 670-674 for humans and 677-678 for the hardcoded dog pid list. |
| `rest_option_msgs(base_msg_id)` | Change the base message id used for Pip-Boy rest option labels. CE reads the rest labels from `base_msg_id` through `base_msg_id + 13`; the default Fallout 2 range is 302-315 (FO1 mode: 321-334). |
| `set_rest_option(rest_option, value)` | Change the wake hour for Pip-Boy rest options 8-11: morning, noon, evening, and midnight. `value` is an hour from 0-23. Defaults are 8, 12, 18, and 0 (FO1 morning default: 6). |

### CE-only metarules

CE defines several metarules that are not supported in Sfall.  Include [ce.h](files/ce.h) for the #defines.

| Name | Definition |
| --- | --- |
| `encounter_intros(toggle)` | Enable or disable the display-monitor random encounter intro message, for example `You encounter: ...`. This does not affect the separate encounter detection dialog. |
| `rest_option_msgs(base_msg_id)` | Change the base message id used for Pip-Boy rest option labels. CE reads the rest labels from `base_msg_id` through `base_msg_id + 13`; the default Fallout 2 range is 302-315. |
| `set_party_member_cc_msg_ids(pid, start_msg_id, end_msg_id)` | Override party-member combat-control update messages for a pid. Picks randomly from the inclusive contiguous range. Default fallback ranges are 670-674 for humans and 677-678 for the hardcoded dog pid list. |
| `set_rest_option(rest_option, value)` | Change the wake hour for Pip-Boy rest options 8-11: morning, noon, evening, and midnight. `value` is an hour from 0-23. Defaults are 8, 12, 18, and 0. |

## Hooks

| Hook | ID | Compatibility | Notes |
| --- | --- | --- | --- |
| ToHit | `HOOK_TOHIT` | ✅ | - |
| AfterHitRoll | `HOOK_AFTERHITROLL` | ✅ | Overriding `defender` leaves a lot of attack variables in previous state (e.g. distance, ->oops, roundsHitMainTarget) |
| CalcAPCost | `HOOK_CALCAPCOST` | ✅ | - |
| DeathAnim1 | `HOOK_DEATHANIM1` | 🚫 | Use DEATHANIM2 instead |
| DeathAnim2 | `HOOK_DEATHANIM2` | ✅ | - |
| CombatDamage | `HOOK_COMBATDAMAGE` | ✅ | - |
| OnDeath | `HOOK_ONDEATH` | ✅ | **Notification-only hook:** fires after `DAM_DEAD` is set; `maxReturnValues=0` — scripts cannot prevent or modify death through this hook. Use for cleanup/notification only. Fires exactly once at `critter.cc:917` in `critterKill()` for all death paths (combat, non-combat, environmental, script kill, poison, radiation). The duplicate fire site at `combat.cc:5264` was removed as part of the F-68 fix — only the single canonical fire site in `critterKill()` remains. |
| FindTarget | `HOOK_FINDTARGET` | ✅ | **Contract difference from sfall:** CE uses a simplified 2-arg layout: arg0=attacker (Object), arg1=target (Object), 1 return value. sfall uses a 5-arg layout (arg0=attacker, arg1=combat group index, arg2=current target, arg3=previous target, arg4=area attack mode). CE fires at 4 `combat_ai.cc` call sites: line 1776 (cooperative combat pre-selection redirect — player-controlled attacker with existing target), line 1864 (area-attack target iteration — per-candidate check), line 1902 ("who hit me" retaliation override), line 1963 (final post-selection override — last chance to replace engine-chosen target). Return value: if non-null and a valid critter, overrides the engine-selected target; null means "keep engine choice." Scripts written for sfall's 5-arg layout will receive different values in arg0/arg1 than expected. |
| UseObjOn | `HOOK_USEOBJON` | ✅ | - |
| UseObj | `HOOK_USEOBJ` | ✅ | CE notes an sfall-matching inconsistency around return code `2` behavior between interface contexts. |
| RemoveInvenObj | `HOOK_REMOVEINVENOBJ` | 🚫 | Deliberately absent: requires RMOBJ_* constants and destination object tracking not present in CE's itemRemove. Would require significant refactoring of the item removal code path. |
| BarterPrice | `HOOK_BARTERPRICE` | ✅ | - |
| ItemDamage | `HOOK_ITEMDAMAGE` | ✅ | - |
| MoveCost | `HOOK_MOVECOST` | ✅ | - |
| AmmoCost | `HOOK_AMMOCOST` | ✅ | Requires `check_weapon_ammo_cost=1` if you want pre-attack ammo validation to respect per-shot/per-round overrides. |
| KeyPress | `HOOK_KEYPRESS` | ✅ | **Sfall-compatible 3-arg layout** (`src/sfall_kb_helpers.cc:702-716`): arg0=pressed state (1=pressed, 0=released), arg1=DIK key code, arg2=VK_ Virtual Key code (converted from the raw SDL_Keycode via `sdl_keycode_to_vk`). ret0=255 swallows the key (et tu's TMA idiom, `sfall_kb_helpers.cc:741-743`); ret0 in 1-263 remaps the key to that DIK code. See VK→SDL mapping notes below. |
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
| Dialog | `HOOK_DIALOG` (49) | ✅ [CE] | CE-specific. Fires on dialog start (arg0=speaker, arg1=headFid, arg2=reaction) and exit (arg1=-1, arg2=-1, arg0=speaker). |
| DialogReaction | `HOOK_DIALOGREACTION` (50) | ✅ [CE] | CE-specific. Fires when a dialog reaction is triggered (`_talk_to_critter_reacts`). arg0=speaker, arg1=reaction (-2, -1, or 0). |
| Encounter | `HOOK_ENCOUNTER` | ✅ | **Sfall-compatible 5-arg layout** (`src/sfall_script_hooks.cc:737-796`): arg0=event type (0=random encounter, 1=local-map-enter from worldmap), arg1=mapId, arg2=isSpecial (1 if special encounter — specials are encoded in arg2, never arg0), arg3=tableId (encounter table number, -1 if not an encounter), arg4=entryId (entry index in table, -1 if not an encounter). **Note:** arg0 uses sfall's encoding (0/1). The earlier CE "arg0=2 for local-map-enter / arg0=1 for special" scheme was a regression (86e6c4d) and was reverted — the doc previously described the reverted scheme. Forced encounters do **not** fire the hook (sfall N-01, `src/sfall_script_hooks.cc:769-771`) — the map load proceeds directly. Return values: ret0 overrides mapId (-1 cancels for event type 0, or the map to load); ret1 (event type 0 only) returns 1 to cancel the encounter and directly load the map from ret0. |
| AdjustPoison | `HOOK_ADJUSTPOISON` | 🚫 | (maybe) |
| AdjustRads | `HOOK_ADJUSTRADS` | 🚫 | (maybe) |
| RollCheck | `HOOK_ROLLCHECK` | 🚫 | Deliberately absent: randomRoll() has 30+ call sites with no event_type context. Adding context to every call site is too invasive; pass-through hook on every roll would be too expensive. |
| BestWeapon | `HOOK_BESTWEAPON` | 🚫 | Deliberately absent: _ai_best_weapon() has 10+ return points with complex comparison logic. Object lifetime concerns with return value override. |
| CanUseWeapon | `HOOK_CANUSEWEAPON` | ✅ | - |
| BuildSfxWeapon | `HOOK_BUILDSFXWEAPON` | 🚫 | Deliberately absent: sfxBuildWeaponName() returns char* to static buffer (_sfx_file_name). String return from scripts requires buffer management and lifetime semantics. |
| StatLevelUp | `HOOK_STATLEVELUP` (51) | ✅ [CE] | CE-specific. Fires in stat.cc pcAddExperienceWithOptions() and character_editor.cc characterEditorUpdateLevel() |
| Barter | `HOOK_BARTER` (52) | ✅ [CE] | CE-specific. Fires in game_dialog.cc gameDialogBarter() |
| Message | `HOOK_MESSAGE` (53) | ✅ [CE] | CE-specific. Fires in display_monitor.cc displayMonitorAddMessage() |

### VK → SDL Keycode Mapping

CE uses SDL2 rendering and input, not DirectInput. `HOOK_KEYPRESS` passes sfall-compatible codes: arg0=pressed state, arg1=DIK, arg2=VK (converted from the raw SDL_Keycode; `src/sfall_kb_helpers.cc:702-716`). `key_pressed()`/`tap_key()` accept DIK codes (0-255) or VK codes with the `0x80000000` flag and translate them to SDL scancodes internally (`get_scancode_from_key`, `sfall_kb_helpers.cc:608-615`). The numeric values differ significantly from SDL_Keycode for common keys used in RPU/Et Tu scripts.

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

1. **Use sfall-style key codes** — for mod scripts targeting CE, pass DIK codes (0-255, e.g. `DIK_A` = 30) or VK codes with the `0x80000000` flag (e.g. `0x80000041` for VK_A) to `key_pressed()`/`tap_key()`. SDL_Keycode values in the 0-255 range would be misinterpreted as DIK codes (SDLK_a = 97 is not DIK_A = 30).
2. **Use `key_pressed()` / `tap_key()` with DIK or VK codes** — these functions translate the argument to an SDL scancode via `get_scancode_from_key()` (`sfall_kb_helpers.cc:608-615`): DIK (0-255) via `kDiks[]`, VK (0x80000000 flag) via the fully-populated `kVkToSdl[256]` table.
3. **In `HOOK_KEYPRESS` handlers** — the layout is sfall-compatible: arg0=pressed state, arg1=DIK key code, arg2=VK code (converted from the raw SDL_Keycode; `sfall_kb_helpers.cc:702-716`). If a `key_pressed()` trampoline is needed, pass the hook's arg1 (DIK) directly — no conversion required.
4. **RPU/Et Tu compatibility** — sfall scripts using DIK and VK_ constants get correct key detection: DIK codes map through `kDiks[]` for common keys, and VK codes (0x80000000 flag) map through the fully-populated `kVkToSdl[256]` static lookup table (see VK→SDL Mapping Scope below). For unmapped DIK codes, scripts must use the VK_ form (0x80000000 flag) where a `kVkToSdl` entry exists.

### DIK→SDL Mapping Scope

CE's `sfall_kb_helpers.cc` (`kDiks[]` array) maps 256 DIK (DirectInput Key) entries to SDL scancodes. The following describes which DIK ranges are mapped and which are not:

**Mapped (common keys):**
- DIK 1-13: Escape, digits 1-0, minus, equals, backspace (`SDL_SCANCODE_ESCAPE` through `SDL_SCANCODE_BACKSPACE`)
- DIK 14: Tab (`SDL_SCANCODE_TAB`)
- DIK 15-27: Q through right bracket (`SDL_SCANCODE_Q` through `SDL_SCANCODE_RIGHTBRACKET`)
- DIK 28: Return (`SDL_SCANCODE_RETURN`)
- DIK 29: Left Ctrl (`SDL_SCANCODE_LCTRL`)
- DIK 30-43: A through backslash (`SDL_SCANCODE_A` through `SDL_SCANCODE_BACKSLASH`)
- DIK 44-53: Z through slash (`SDL_SCANCODE_Z` through `SDL_SCANCODE_SLASH`)
- DIK 54: Right Shift (`SDL_SCANCODE_RSHIFT`)
- DIK 55: Numpad `*` (`SDL_SCANCODE_KP_MULTIPLY`)
- DIK 56: Left Alt (`SDL_SCANCODE_LALT`)
- DIK 57: Space (`SDL_SCANCODE_SPACE`)
- DIK 58: Caps Lock (`SDL_SCANCODE_CAPSLOCK`)
- DIK 59-68: F1-F10 (`SDL_SCANCODE_F1` through `SDL_SCANCODE_F10`)
- DIK 69: Num Lock (`SDL_SCANCODE_NUMLOCKCLEAR`)
- DIK 70: Scroll Lock (`SDL_SCANCODE_SCROLLLOCK`)
- DIK 71-83: Numpad 7-0, minus/plus/period (`SDL_SCANCODE_KP_7` through `SDL_SCANCODE_KP_PERIOD`)
- DIK 87-88: F11-F12 (`SDL_SCANCODE_F11` through `SDL_SCANCODE_F12`)
- DIK 141: Numpad `=` (`SDL_SCANCODE_KP_EQUALS`)
- DIK 156: Numpad Enter (`SDL_SCANCODE_KP_ENTER`)
- DIK 157: Right Ctrl (`SDL_SCANCODE_RCTRL`)
- DIK 179: Numpad `,` (`SDL_SCANCODE_KP_COMMA`)
- DIK 181: Numpad `/` (`SDL_SCANCODE_KP_DIVIDE`)
- DIK 183: SysRq (`SDL_SCANCODE_SYSREQ`)
- DIK 184: Right Alt (`SDL_SCANCODE_RALT`)
- DIK 199: Home (`SDL_SCANCODE_HOME`)
- DIK 200: Up arrow (`SDL_SCANCODE_UP`)
- DIK 201: Page Up (`SDL_SCANCODE_PAGEUP`)
- DIK 203: Left arrow (`SDL_SCANCODE_LEFT`)
- DIK 205: Right arrow (`SDL_SCANCODE_RIGHT`)
- DIK 207: End (`SDL_SCANCODE_END`)
- DIK 208: Down arrow (`SDL_SCANCODE_DOWN`)
- DIK 209: Page Down (`SDL_SCANCODE_PAGEDOWN`)
- DIK 210: Insert (`SDL_SCANCODE_INSERT`)
- DIK 211: Delete (`SDL_SCANCODE_DELETE`)
- DIK 219-221: Left Win, Right Win, Apps (`SDL_SCANCODE_LGUI`, `SDL_SCANCODE_RGUI`, `SDL_SCANCODE_APPLICATION`)

**Not mapped (return `SDL_SCANCODE_UNKNOWN`):**
- DIK 0: Reserved (no DIK_0 constant)
- DIK 84-86: Reserved/unused (gap between F10 and F11)
- DIK 89-140, 142-155: Unassigned DIK range — includes OEM-specific keys (DIK_AT, DIK_COLON, DIK_UNDERLINE, DIK_KANA, DIK_CONVERT, DIK_NOCONVERT, DIK_YEN, DIK_KANJI, DIK_PREVTRACK, DIK_STOP, DIK_AX, DIK_UNLABELED, DIK_OEM_102) and unassigned slots. All return `SDL_SCANCODE_UNKNOWN`.
- DIK 158-178, 180: Unassigned (gap between RCtrl and NumpadComma, plus unmapped slot at 180)
- DIK 185-198, 212-218, 222-255: Unassigned gaps and reserved ranges

**VK (Virtual Key) codes:** Values with the `0x80000000` flag set are treated as VK codes. VK→SDL translation **is implemented** via a fully-populated `kVkToSdl[256]` static lookup table at `sfall_kb_helpers.cc:284-563`, mapping Windows `VK_*` constants to `SDL_Scancode` values. The table is consumed by `get_scancode_from_key()` at `sfall_kb_helpers.cc:567-571`, which is called by `sfall_kb_is_key_pressed()` (used from `sfall_opcodes.cc:315` via `key_pressed()`/`tap_key()`). Most commonly-used VK keys are mapped (VK_BACK through VK_OEM_CLEAR); a few entries remain `SDL_SCANCODE_UNKNOWN` for keys without direct SDL equivalents (e.g., VK_LBUTTON, VK_MBUTTON).

**Additional hook notes:** Registering a hook type that has no engine fire site (HOOK_DEATHANIM1, HOOK_REMOVEINVENOBJ, HOOK_SUBCOMBATDAMAGE, HOOK_ADJUSTPOISON, HOOK_ADJUSTRADS, HOOK_ROLLCHECK, HOOK_BESTWEAPON, HOOK_BUILDSFXWEAPON, and the obsolete HEX*BLOCKING hooks) will now emit a `debugPrint` warning. The hooks table above marks these as 🚫 with explanations.

## Et Tu (FO1-in-FO2) Compatibility

This section tracks Fallout Et Tu (https://github.com/rotators/Fo1in2) compatibility with CE. Et Tu runs as a total conversion: its 17 global scripts, 1000+ map scripts, FO1 content data, and three config files (`Fallout2.cfg`, `data/config/game#patch.cfg`, `ddraw.ini`) all run on the CE engine. Analysis snapshot: et tu master `c154bb8` (2026-08-11).

### Requirements

| Requirement | Status | Evidence / notes |
| --- | --- | --- |
| sfall version gate (`sfall_ver_major < 5` in gl_0.ssl:27-29) | ✅ | CE reports 4.5.1 (`src/sfall_opcodes.cc:83-85`) |
| Rotators detection (`read_byte(0x410003)==0xF4` + `metarule_exist("rotators")`) | ✅ | `src/sfall_opcodes.cc:158-160`, `src/sfall_metarules.cc:1268-1271` |
| CE detection (`ce_enabled` = `opcode_exists(0x823B)==false`) | ✅ | 0x823B `modified_ini` intentionally not implemented |
| Startup gate (`get_ini_setting("ddraw.ini\|...")` for AllowUnsafeScripting, DisableHorrigan, UseFileSystemOverride, Fallout1Behavior) | ⚠️ | With CE's shipped `files/ddraw.ini` all four resolve to 0 → `gl_0.int` shows a warning, force-writes the keys to 1 via `set_ini_setting`, then calls `signal_close_game` followed by a deliberate `force_crash` (gl_0.ssl:90-93; on CE the crash resolves to an error print, not a hard crash). One-time first-run restart. Add the four keys to ddraw.ini or adjust the config-bridge defaults to remove the friction. |
| FO1 hit chance (`set_fo1_hit_chance(true)`, gl_fo1mechanics.ssl:115-117) | ✅ | Metarule H-04 (`sfall_metarules.cc:2379`); consumed in to-hit at `combat.cc:4833-4839` together with `gFallout1Behavior`. The et tu call is conditional on `fo1in2_fo2_hitchance_enabled == false` — it does not fire when the user enables "FO2 hit chance". |
| FO1 worldmap labels (`remove_wm_town_names(true)`, gl_classic_wm.ssl:30) | ✅ | Metarule H-05 (`sfall_metarules.cc:2396`); live circle-overlay label gated at `worldmap.cc:6435-6437` |
| Encounter handling (`encounter_detection(false)`, gl_fo1mechanics.ssl:109) | ✅ | Metarule → `wmSetEncounterDetection` |
| HOOK_ENCOUNTER (gl_worldmap.ssl:51-63 handler) | ✅ | **CE now uses the sfall arg0 encoding**: arg0=0 random / arg0=1 local-map-enter, arg2=isSpecial; forced encounters do not fire the hook (`src/sfall_script_hooks.cc:737-796`). Ret0 overrides map id, ret0=-1 cancels (event type 0). See the HOOK_ENCOUNTER row in the Hooks table above. |
| Reaction thresholds (`set_reaction_thresholds(25, 75)`, gl_fo1mechanics.ssl:112) | ✅ | Metarule + `reaction.cc:40-46`; FO1/FO2 defaults preserved on game reset |
| Rest hours/strings (`rest_option_msgs(320)`, `set_rest_option(REST_OPTION_MORNING, 6)`, gl_0_settings.ssl:123-124) | ✅ | CE metarules (`sfall_metarules.cc:2416,2427`); FO1 defaults 6:00 wake / base 321. Both calls are gated on `not(fo1in2_0800_resting_enabled)` |
| FO1 XP progression (`XPTable` in ddraw.ini; `[stats] xp_table` in game#patch.cfg) | ⚠️ | ddraw.ini `[Misc] XPTable` parsed and applied (`combat.cc:2095-2118`, `stat.cc:772-810`). game.cfg `[stats] xp_table` has no consumer — if only game#patch.cfg is deployed, the FO2 XP table applies. |
| FO1 start position (`[start] worldmap_x/worldmap_y` in game#patch.cfg) | ❌ | No consumer in CE; player starts at the FO2 default worldmap position (173,122). ddraw.ini `StartXPos/StartYPos` (migrated to `[worldmap] start_x_pos/start_y_pos`, `worldmap.cc:1188`) works as an alternative source. |
| FO1 worldmap viewport (`[start] worldmap_view_x/worldmap_view_y`) | ❌ | No consumer (cosmetic). |
| Worldmap terrain info (`[worldmap] terrain_info=1`) | ❌ | Not implemented (upstream de1ade9 feature absent). Cosmetic. |
| Worldmap travel speed (`WorldMapFPSPatch`/`WorldMapDelay2`; `[worldmap] travel_delay`) | ✅ | sfall boolean+ms semantics (M-64, `worldmap.cc:1154-1163`); `travel_delay` (`worldmap.cc:1150`) |
| Travel markers (`WorldMapTravelMarkers`/`trail_markers`) | ✅ | `content_config.cc:115`, `worldmap.cc:1152` |
| Special-map-ID disabling (`DisableSpecialMapIDs`; `[maps] disable_special_map_ids`) | ⚠️ | ddraw.ini key honored (`combat.cc:2077`, `worldmap.cc:4372-4375`); game.cfg `[maps] disable_special_map_ids` has no consumer |
| QuickPockets AP reduction (`QuickPocketsApCostReduction`; `[combat] quick_pockets_ap_cost_reduction`) | ⚠️ | ddraw.ini key honored (`sfall_callbacks.cc:76`); game.cfg key has no consumer |
| PA weight (FO1 = not halved) | ⚠️ | **Fork conflict:** CE gates the halving on `!gFallout1Behavior` (`item.cc:845`) while upstream CE halves unconditionally; et tu compensates by doubling protos in `gl_fo1mechanics.ssl` `adjust_pa_weight`. On CE-in-FO1-mode the two combine to 2× weight. Verify and pick one side (gate off, or document). |
| Party member dialog (`set_party_member_cc_msg_ids`, Dogmeat in `_Dogs`) | ✅ | `game_dialog.cc:186`, metarule |
| Party armor appearance / weapon restrictions (gl_partyarmor.ssl) | ✅ | HOOK_INVENWIELD/ADJUSTFID/INVENTORYMOVE/CANUSEWEAPON, `art_exists`, `get_object_data`, `real_dude_obj` |
| TMA ("Tell Me About", gl_tma.ssl) | ✅ | HOOK_KEYPRESS DIK arg1 + ret0=255 swallow (`sfall_kb_helpers.cc:741-743`); `get_ini_section("config\keymap.ini", language)` |
| Motorcycle (gl_car.ssl, MOTRCYCL.ssl) | ✅ | HOOK_CARTRAVEL, HOOK_MOUSECLICK, `set_car_intface_art` |
| Auto doors / auto push / armor destroy | ✅ | HOOK_COMBATTURN, HOOK_STDPROCEDURE, `set_proto_data` (0x8205), arrays, `set_flags` |
| Ammo INI loader (gl_ammomod.ssl) | ✅ | `get_ini_setting("ddraw.ini\|Misc\|DamageFormula")` bridge; `set_proto_data` ammo offsets |
| `get/set_proto_data` offsets used by et tu (PROTO_CR_FLAGS, PROTO_IT_WEIGHT, PROTO_WP_DMG_TYPE, PROTO_CR_BONUS_HP, PROTO_AM_*, PROTO_AR_DR_PLASMA, PROTO_FID, PROTO_WP_ANIM/RANGE, PROTO_CN_MAX_SIZE) | ✅ | Raw-offset impl with bounds+alignment checks (`sfall_opcodes.cc:1034-1093`) |
| `game#patch.cfg` overlay mechanism | ✅ | `content_config.cc:15,31-34` (config\game.cfg + config\game#patch.cfg, VFS-aware) |
| et tu `Fallout2.cfg` | ✅ | All keys CE-native (system paths, ui, sound, qol, screen) |
| Perks.ini (`PerksFile=config\Perks.ini`) | ❌ | No consumer; et tu perk tuning (`[PerksTweak]` NightVision/Survivalist/MrFixit/Medic/etc.) silently not applied. FO2 default perk bonuses remain. |
| NPC combat control (`mods/sfall-mods.ini [CombatControl] Mode=3`) | ❌ | sfall engine feature, absent in CE. Optional (et tu README feature). |
| Item highlighting (`mods/sfall-mods.ini [Highlighting]`) | ❌ | sfall engine feature, absent in CE. Optional QoL. |
| `WorldMapTimeMod`, `WorldMapEncounterFix/Rate`, `UseScrollingQuestsList`, `ItemCounterAutoCaps`, `DeathScreenFontPatch`, `EnableMusicInDialogue` | ❌ | sfall engine settings absent in CE; et tu ships neutral/disabled values for most (low impact); no et tu script reads them. |
| Rotators-only metarules (`r_call_offset*`, `r_hrp*`) | ❌ (unused) | Missing from kMetarules but **not called by any shipped et tu script** — zero runtime impact. |

### Remaining work for full et tu support

Prioritized (P1 = blocks/degrades core et tu experience; P2 = config parity; P3 = optional/QoL):

1. **P1 — Fix the first-run startup gate** (`gl_0.ssl`): ship an et tu-compatible `files/ddraw.ini` (or a content-config fallback) so `get_ini_setting("ddraw.ini|Debugging|AllowUnsafeScripting" | "Misc|DisableHorrigan" | "Misc|UseFileSystemOverride" | "Misc|Fallout1Behavior")` resolves non-zero out of the box, avoiding the forced-restart loop.
2. **P1 — Resolve the FO1 power-armor weight double-count**: `src/item.cc:845` gates halving on `!gFallout1Behavior`; et tu's `gl_fo1mechanics.ssl` `adjust_pa_weight` doubles protos assuming the engine always halves (upstream CE behavior). Align with upstream (halve unconditionally) or document the deviation so et tu's compensation is not doubled.
3. **P1 — Implement `[start] worldmap_x` / `worldmap_y`** (start worldmap marker position) so et tu's `game#patch.cfg` (823, 72) places the player at the FO1 start; upstream CE reads these via `wmGetStartWorldMapConfigValue` (upstream `worldmap.cc:1261-1314`).
4. **P2 — Implement `[worldmap] terrain_info`** (worldmap terrain display, upstream de1ade9) — et tu ships `terrain_info=1`.
5. **P2 — Read `[stats] xp_table` from game.cfg** (upstream 2223f7c) as a second source next to ddraw.ini `[Misc] XPTable`, so game#patch.cfg-only deployments get the FO1 XP table.
6. **P2 — Read `[maps] disable_special_map_ids` and `[combat] quick_pockets_ap_cost_reduction` from game.cfg** (ddraw.ini paths already work; add the game.cfg keys for full game#patch.cfg parity).
7. **P2 — Implement `[start] worldmap_view_x` / `worldmap_view_y`** (viewport start; cosmetic).
8. **P2 — Implement `PerksFile` (config/Perks.ini `[PerksTweak]`)** so et tu's perk tuning (NightVision 10, Survivalist 0, MrFixit 20, Medic 20/20, MasterThief 0, Speaker 0, Ghost, …) applies; or document that FO2 perk defaults remain.
9. **P3 — Optional sfall engine features** et tu ships config for: NPC combat control (`sfall-mods.ini [CombatControl]`), key-driven item highlighting (`[Highlighting]`), `WorldMapTimeMod`, `WorldMapEncounterFix`/`WorldMapEncounterRate`, `UseScrollingQuestsList`, `ItemCounterAutoCaps`, `DeathScreenFontPatch`, `EnableMusicInDialogue`. None break et tu scripts; add only if the fork wants full sfall QoL parity.
10. **P3 — Register the 5 rotators-only metarules** (`r_call_offset`, `r_call_offset_cdecl`, `r_call_offset_push`, `r_hrp`, `r_hrp_offset`) as safe no-ops so `metarule_exist("r_...")` probes and any third-party script referencing them do not error.
11. **P3 — Verify at runtime (build + test)**: (a) `set_reaction_thresholds(25,75)` persistence across game reset vs `gl_fo1mechanics.ssl` re-application; (b) FastShotFix=3 + `gl_apcost.ssl` CE-path double AP reduction for melee/unarmed with Fast Shot.
12. **Docs — Verify the merged HOOK_ENCOUNTER/HOOK_KEYPRESS rows stay accurate when the hook code next changes** (both rows are current: HOOK_ENCOUNTER arg0=0/1, arg2=special, no forced-fire; HOOK_KEYPRESS arg1=DIK, arg2=VK).

## RPU (Fallout 2 Restoration Project) Compatibility

CE supports the [Fallout 2 Restoration Project, updated (RPU)](https://github.com/BGforgeNet/Fallout2_Restoration_Project) (requires sfall 4.5; CE reports 4.5.1). RPU's own scripting surface is small: 4 sfall hooks, ~25 opcodes/metarules, and sfall ddraw.ini config keys. All hooks and opcodes/metarules are implemented; the config keys are handled as shown in the table below (four are unimplemented/unsupported). The table below is a requirement-by-requirement audit against RPU master `f7c10859` (2026-08-10).

| Requirement | Status | Evidence / Notes |
| --- | --- | --- |
| Hooks: HOOK_USEOBJON, HOOK_USEOBJ, HOOK_GAMEMODECHANGE, HOOK_COMBATDAMAGE | ✅ | RPU registers only these 4 (`scripts_src/global/gl_k_alcohl.ssl:102-103`, `gl_k_dogmeat_fix.ssl:20`, `gl_k_wpnchk.ssl:53,61`). HOOK_COMBATDAMAGE 13-arg layout matches RPU's sequential `get_sfall_arg` reads (target, attacker, dmg_target, dmg_attacker, flags_target, flags_attacker, weapon, body_part). |
| `get_ini_setting` / `set_ini_setting` / `get_ini_string` (incl. `mods\rpu.ini`, `mods\upu.ini`, ddraw.ini keys) | ✅ | `src/sfall_ini.cc` opcodes + content-config bridge + gSfallConfig fallback. RPU reads `mods\*.ini` via relative paths and `WorldMapSlots`/`BoostScriptDialogLimit`/`EnableHeroAppearanceMod`/`UseFileSystemOverride` from ddraw.ini. |
| `WorldMapSlots=21` (RPU gate: `gl_k_modini.ssl:14`, `!= 21` → `signal_end_game`) | ✅ | H-06: gSfallConfig default 21 (`src/sfall_config.cc:64-71`) flows through `op_get_ini_setting` fallback (`src/sfall_ini.cc:651-656`). |
| `BoostScriptDialogLimit=1` (RPU gate: `gl_k_modini.ssl:18`, `== 0` → `signal_end_game`) | ✅ | CE returns -1 for the absent key (never 0) (`src/sfall_ini.cc:658`); dialog message capacity is 10000 (`src/scripts.cc:56`), so the boost is inherently satisfied. |
| `EnableHeroAppearanceMod=1` (`epai37.ssl:101`) | ✅ | CE's Hero Appearance feature is always-on (matches RPU default 1). |
| `ElevatorsFile=mods\elevators.ini` + elevators.ini format | ✅ | CE reads `gSfallConfig [Misc] ElevatorsFile` (`src/elevator.cc:686-746`) and parses `[N] Image/IDn/Elevationn/Tilen` — RPU `mods/elevators.ini` sections [24]-[28] match. |
| `UseFileSystemOverride=1` (`upu.h:14-20` gate) | ⚠️ | CE default is 0; install RPU's own ddraw.ini (ships `UseFileSystemOverride=1`) or set `[start] use_filesystem_override=1` in game.cfg to avoid the UPU "EXIT AND RE-LAUNCH" warning. Even with the gate passing, see fs_copy below. |
| `ExtraSaveSlots=1` | ✅ | WIRED — 100 save pages / 1000 slots (`src/loadsave.cc:519`, gExtraSaveSlots). |
| `KarmaFRMs` / `KarmaPoints` | ✅ | Migrated to `[karma] frms/points`, consumed at `src/character_editor.cc:7844-7857`. |
| `FemaleDialogMsgs=2` | ❌ | Not implemented in CE. Affects only non-English female dialog/cutscene message variants; English RPU ships no `dialog_female` dir. |
| `OverrideArtCacheSize=1` | ⚠️ | Not wired; CE controls art cache via `settings.system.art_cache_size`. No functional impact. |
| `BoxBarColours=11111` | ⚠️ | Legacy interface-bar colour setting, documented unsupported (cosmetic). |
| `ProcessorIdle=1` | ❌ | Not implemented (CPU-idle perf setting; no gameplay impact). |
| `[Scripts] IniConfigFolder=mods` | ✅ | CE uses it as the ini base path (`src/game.cc:438-439`, `sfall_ini_set_base_path`). |
| Global scripts `scripts\gl_k_*.int` | ✅ | CE auto-discovers `scripts\gl*.int` + `scripts\sfall\gl*.int` incl. inside .dat mods (`src/sfall_global_scripts.cc:118-149`). |
| `scripts.lst` override in mod (1558 scripts, `# local_vars=` annotations) | ✅ | Dynamic list load (`src/scripts.cc:1595-1654`); parses `local_vars=`. |
| Worldmap content: city.txt (61 areas), worldmap.txt (20 tiles), maps.txt (173 maps) | ✅ | CE loads all dynamically (`src/worldmap.cc`). Pipboy automap clamps at 160 maps (see remaining work). |
| Elevator content via `mods/elevators.ini` | ✅ | See ElevatorsFile row. |
| `.edg` files (175) | ✅ | CE `.edg` scroll-block/stencil support (`edg_support=1`, `src/map.cc:1030`). |
| Data files: ai.txt, party.txt, quests.txt, karmavar.txt, endgame.txt, vault13.gam | ✅ | All read by CE (`src/combat_ai.cc:398`, `src/party_member.cc:136`, `src/pipboy.cc:2860`, `src/character_editor.cc:7664`, `src/endgame.cc:1074`, `src/game.cc:1066`). |
| Hero appearance opcodes `set_hero_style`/`set_hero_race` (`epai37.ssl`) | ✅ | Implemented (`src/sfall_opcodes.cc:5969-5982`, registered 8807-8808), store `HApStyle`/`HAp_Race` globals RPU reads. |
| `get_sfall_global_int` on unset keys | ✅ | M-03 returns 0 for missing keys, matching sfall (`src/sfall_opcodes.cc:690-697`). |
| `fs_copy(path, path)` in-place FRM patch (UPU Goris de-robing FPS, critters walk faster) | ⚠️ | CE rejects identical source/dest (C-06, `src/sfall_opcodes.cc:2579-2589`); the two UPU animation-FPS features silently do nothing on CE. Use the alternative pre-patched dats (`walk_speed_fix_low_fps.dat`, `goris_fast_derobing_low_fps.dat`) for equivalent behavior. |
| `get_object_data(combat_data, C_ATTACK_*)` (boxing KO check `ncprzftr.ssl:143`) | ⚠️ | sfall's `C_ATTACK_*` offsets are 32-bit Attack-struct offsets; CE's 64-bit `Attack` has different field positions (`defenderFlags` at 68, not 48), so `C_ATTACK_FLAGS_TARGET` reads the wrong bytes. Single call site; other boxing win paths still work. |
| Mod loading: `mods_order.txt` + `mods/rpu.dat` etc. | ✅ | CE `sfallLoadMods` (`src/sfall_ext.cc:187-267`); RPU installer places `mods_order.txt` in `mods/`. |

### Remaining work for full RPU support

Prioritized (P1 = affects bundled RPU features; P2 = minor/edge):

1. **P1 — Support `fs_copy(src, src)` (same-path copy) for sfall-compatible in-memory patching.** RPU's bundled UPU scripts (`gl_k_goris_derobing.ssl:40`, `gl_k_walking_speed.ssl:88`) call `fs_copy(path, path)` to open an FRM for read-modify-write. CE's C-06 guard (`src/sfall_opcodes.cc:2579-2589`) rejects identical paths to avoid disk truncation; the sfall semantic (memory-only copy handle) means same-path copy is valid and the scripts' FPS-patch features should work. Fix approach: implement fs_copy as a memory-backed handle (read source fully, return handle over the in-memory buffer, write-back on fs_close) — preserving the C-06 safety intent (no truncation) while accepting identical paths. Without this, UPU `goris_derobing_speed` and `critters_walk_faster` are inert on CE.
2. **P1 — Map sfall `C_ATTACK_*` offsets to CE's 64-bit `Attack` layout in `get_object_data(combat_data, ...)`.** RPU `ncprzftr.ssl:143` reads `C_ATTACK_FLAGS_TARGET` (=0x30, 32-bit offset) and gets garbage on CE's 64-bit struct. Fix approach: special-case `combat_data` pointers in `mf_get_object_data`/`mf_set_object_data` with a translated offset table (sfall constant → CE `Attack` field), or expose a dedicated combat-data accessor metarule. Verifies the boxing-match KO check.
3. **P2 — Make `UseFileSystemOverride` gate pass by default.** Either default `UseFileSystemOverride=1` in gSfallConfig (`src/sfall_config.cc:61`) or populate the `[start] use_filesystem_override` content bridge default, so RPU's `check_filesystem_override` (`upu.h:14-20`) does not show the "EXIT AND RE-LAUNCH" float_msg on CE-default configs. (RPU's own ddraw.ini already sets 1, so this only matters for CE-default config users.)
4. **P2 — Raise pipboy automap capacity above 160 for RPU's 173 maps.** RPU maps 160-172 (EPA sublevels, SF Sheng's, Slaver Camp, safehouses, etc.) are excluded from the pipboy automap by `AUTOMAP_MAP_COUNT` clamps (`src/pipboy.cc:1806,1934`). Consider a dynamic automap entry store or a configurable `AUTOMAP_MAP_COUNT` so RPU maps ≥160 get automap entries.
5. **P3 — `FemaleDialogMsgs` support for non-English RPU translations.** Implement sfall's female dialog message selection (`dialog_female`/`cuts_female` dirs) if non-English RPU support is a goal; English RPU is unaffected.
6. **P3 — `ProcessorIdle` / `BoxBarColours` parity.** Perf/cosmetic settings; accept as documented unsupported or implement as trivial config passthroughs.
