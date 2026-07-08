# Changelog

All notable changes to this fork (Fallout 2 CE Extended) by Nobu. This fork extends the upstream [alexbatalov/fallout2-ce](https://github.com/alexbatalov/fallout2-ce) engine with comprehensive RPU / Et Tu mod compatibility and production hardening.

---

## Fork Summary

**Total fork commits:** 29 | **Base:** `alexbatalov/fallout2-ce` upstream (merge point at `481cb9e`)

The fork implements full sfall script compatibility for RPU (Restoration Project Updated) and Et Tu (Fallout 1 in Fallout 2 engine) mods, covering opcodes, metarules, hooks, configuration, and engine behavior parity. Five production audit passes hardened the codebase across 10+ domains.

---

## [Fork] — 2024-06 to 2026-07

### sfall Version

CE reports **sfall 4.5.1** to scripts (`src/sfall_opcodes.cc:73-79`). Version bumped from 4.4.9 to 4.5.1 for RPU/Et Tu compatibility (`src/sfall_metarules.cc:1374`).

### RPU / Et Tu Compatibility (16 commits)

**Opcodes and Metarules:**
- 121+ sfall opcodes registered and implemented: stats, skills, perks, combat, knockback, explosions, worldmap, audio, virtual file system, lists, arrays, objects, scripts, animations, tiles, paths, UI, inventory, dialogs, events, global scripts, car travel, INI settings, keyboard, mouse, graphics
- `sfall_ver_major/minor/build` opcodes report version 4.5.1
- `set_hero_race` and `set_hero_style` opcodes fully implemented (always-on, no config flag needed)
- `SpeedMulti` / `SpeedMultiInitial` from `ddraw.ini [Speed]` — applied to all animation timing, re-initialized on game reset
- `set_perk_freq`, `set_perk_level`, `set_pyromaniac_mod`, `set_swiftlearner_mod`, `set_hp_per_level_mod` — fully integrated with consumer code
- `set_critter_skill_mod`, `set_base_skill_mod` — consumed via sfall accessors in `skillGetValue()`
- `perk_add_mode`, `clear_selectable_perks`, `hide_real_perks` — integrated via sfall accessors
- `add_trait` / `remove_trait` — fully implemented with 1-arg and 3-arg forms
- `set_perk_name`, `set_perk_desc` — stored in perk override arrays, consumed at `perk.cc`
- `set_perk_image` — fully implemented with validation, consumed at `perk.cc:626`
- `apply_heaveho_fix` — registered as safe no-op (handled via engine config)
- All 18 `fs_*` virtual file system opcodes registered and implemented
- All 6 knockback opcodes consumed in `_compute_dmg_damage` (weapon, target, attacker; absolute and additive modes)
- `set_spray_settings` fully wired with per-weapon PID filtering, count/radius overrides, ammo clamping
- `force_encounter`, `force_encounter_with_flags`, `set_map_time_multi`, `exec_map_update_scripts` implemented
- `gdialog_get_barter_mod`, `get/set_unspent_ap{_perk}_bonus`, `get/set_inven_ap_cost` implemented
- Pickpocket modifiers fully integrated — consumed in `skillsPerformStealing()` with sfall accessors
- `inc_npc_level`, `get_npc_level`, `npc_engine_level_up` implemented
- `set_car_intface_art`, `car_gas_amount` implemented
- `sneak_success`, `interface_print`, `interface_overlay` implemented
- `intface_hide/show/is_hidden/redraw` implemented
- `get_current_inven_size`, `item_weight`, `lock_is_jammed`, `unjam_lock` implemented
- `set_unjam_locks_time` consumed in `mapLoadSaved`
- `get_object_ai_data` (types 0-2) implemented via AI packet accessors
- `set_dude_obj`, `real_dude_obj`, `set_object_data`, `set_scr_name` implemented as metarules
- `create_spatial`, `spatial_radius`, `add_g_timer_event`, `remove_timer_event` implemented
- `dialog_message` implemented
- String function suite: `string_split`, `substr`, `strlen`, `charcode`, `string_find`, `string_find_from`, `string_format`, `string_replace`, `string_to_case`, `string_compare` implemented
- `get_ini_setting`, `get_ini_string`, `get_ini_section`, `get_ini_sections`, `set_ini_setting` implemented
- `display_stats`, `inventory_redraw`, `critter_inven_obj2` registered and implemented
- All array opcodes (`create_array`, `temp_array`, `fix_array`, `get/set_array`, `resize_array`, `free_array`, `scan_array`, `len_array`, `save/load_array`, `array_key`, `arrayexpr`) fully implemented

**Hooks:**
- 43 of 62 hook types implemented (24 standard sfall hooks + 5 CE-specific)
- **CE-specific hooks** (not present in upstream sfall): `HOOK_DIALOG` (49), `HOOK_DIALOGREACTION` (50), `HOOK_STATLEVELUP` (51), `HOOK_BARTER` (52), `HOOK_MESSAGE` (53)
- `HOOK_CARTRAVEL` (28) — speed and fuel override during worldmap car travel
- `HOOK_SETGLOBALVAR` (29) — fires on `op_set_global_var`, allows overriding stored value
- `HOOK_SNEAK` (39) — fires after each sneak check, result and duration overridable
- `HOOK_COMBATDAMAGE` (5), `HOOK_ONDEATH` (6), `HOOK_FINDTARGET` (7) — all verified operational
- `HOOK_KEYPRESS` (19) — uses SDL_Keycode (not VK_); DIK→SDL scancode bridge in `sfall_kb_helpers.cc`
- `HOOK_BARTERPRICE` (10), `HOOK_ITEMDAMAGE` (16), `HOOK_AMMOCOST` (17), `HOOK_MOVECOST` (11) — fully implemented
- `HOOK_TARGETOBJECT` (42) — fires at `_combat_attack` start
- `HOOK_DESCRIPTIONOBJ` (34) — supports direct string return for description override
- `HOOK_SETLIGHTING` (38), `HOOK_USEANIMOBJ` (32), `HOOK_ONEXPLOSION` (36) — implemented
- `HOOK_CANUSEWEAPON` (48) — allows preventing weapon use by PC or NPC
- `HOOK_ENCOUNTER` (43) — random encounter override
- `HOOK_GAMEMODECHANGE` (31), `HOOK_RESTTIMER` (30) — implemented
- Deliberately absent hooks documented with rationale: `HOOK_DEATHANIM1`, `HOOK_REMOVEINVENOBJ`, `HOOK_SUBCOMBATDAMAGE`, `HOOK_ADJUSTPOISON`, `HOOK_ADJUSTRADS`, `HOOK_ROLLCHECK`, `HOOK_BESTWEAPON`, `HOOK_BUILDSFXWEAPON`, HEX*BLOCKING hooks

**Config:**
- `ddraw.ini` settings migrated to `fallout2.cfg` and `game.cfg`
- `[Speed]` section of `ddraw.ini` parsed for `SpeedMultiInitial` / `SpeedMulti` with proper fallback
- Settings file: `<DAT>/config/game.cfg` with full descriptions
- HRP EDG scroll-blocker file format (`.edg`) fully supported

**Engine Behavior:**
- Game reset re-initializes SpeedMulti from `ddraw.ini` (matching sfall behavior)
- `game_loaded` returns false for non-global scripts (sfall 4.4.5 behavioral fix)
- Safe no-ops for incompatible opcodes: DMA access (partial), `set_palette`, `block_combat`, `stop/resume_game`, `get/set_viewport_x/y`
- 256-entry DIK→SDL scancode mapping table covering all standard keyboard keys
- VK (Virtual Key) codes with `0x80000000` flag explicitly rejected (not supported)

### Production Audits & Bug Fixes (5 audit passes)

Each audit verified by adversarial pipeline with 2-iteration convergence:

**Audit 1** (commit `a342f63`): 44 verified fixes — 2 CRITICAL null derefs, 6 HIGH safety/security, 36 MEDIUM robustness

**Audit 2** (commit `d13a00a`): 25 verified fixes — 3 HIGH (FO1 level cap, perk save, game_loaded order), 22 MEDIUM (opcode bugs, save integrity, build system, test coverage)

**Audit 3** (commit `3db72b1`): test suite expanded for maximum post-fork code coverage

**Audit 4** (commit `650b8b6`): 42 verified fixes — 3 HIGH (stat bounds, invisibility check, hook call stack drain), 39 MEDIUM (opcode/meta/hook bugs, save integrity, config parsing, strtol bounds)

**Fix categories across all audits:**
- Bounds checks on opcode handlers and metarule functions
- Null pointer guards on object manipulation paths
- Procedure index bounds on `opDelayedCall` / `opConditionalCall`
- Stack imbalance fix in `opCritterRemoveTrait` (push -1 on early null return)
- Off-by-one fixes in `opTokenize` boundary conditions and string allocation
- Error handling in `op_read_byte` (prints error for unknown addresses)
- Input validation in `op_set_weapon_ammo_pid` (ammo PID validation)
- Save/load integrity: negative count guards, parity checks for associative arrays
- Config parsing bounds and `strtol` error handling
- Release-only startup crash fix: pattern buffer overflow in `DirectoryFileFindData`

### Build System & Testing

- **macOS ARM64 release build script** (`build_mac_arm_release.sh`) — production-ready RelWithDebInfo configuration
- **CMake Presets**: `macos` (universal), `macos-arm64-debug`, `macos-arm64-release`, `windows-x64-debug`, `windows-x64-release`, `emscripten-debug`, `emscripten-release`
- **Tests**: 67 test targets with comprehensive sfall integration, engine state, and save/load round-trip coverage
- Production build verified with 67/67 tests passing on macOS ARM64

### Documentation

- `SFALL_COMPATIBILITY.md` — complete sfall opcode/hook/config compatibility reference with DIK→SDL mapping table
- `README.md` — updated mod compatibility status, standalone Mod Compatibility section
- `INSTALL.md` — build instructions for all platforms
- Cleanup: removed outdated fork notice, CONTRIBUTING.md, and stale README sections

---

## [Upstream] — alexbatalov/fallout2-ce (pre-fork)

All changes prior to `481cb9e` are from the upstream [fallout2-ce](https://github.com/alexbatalov/fallout2-ce) project. See that repository's changelog for pre-fork history.
