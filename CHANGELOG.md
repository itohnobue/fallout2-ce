# Changelog

## [Unreleased] — Session 2

### Added
- **SpeedMulti / SpeedMultiInitial** — Global game speed multiplier (`[Speed]` section of `ddraw.ini`), applied to all animation timing via sfall global var 0. Scripts can change it at runtime with `set_sfall_global(0, value)`. Implemented at `src/game.cc:373-386` (init), `src/animation.cc:3350-3364` (application), `src/sfall_callbacks.cc:43-56` (gameReset re-init). `src/sfall_config.h:15-16`.
- **HOOK_CARTRAVEL** (28) — Fires once per worldmap tick during car travel. Allows overriding speed (steps per tick) and fuel consumption. `src/sfall_script_hooks.cc:1169-1186`, fire point at `src/worldmap.cc:3091`.
- **HOOK_SETGLOBALVAR** (29) — Fires on `op_set_global_var` for integer values. Allows overriding the stored value before it's written. `src/sfall_script_hooks.cc:1196-1212`, fire point at `src/interpreter_extra.cc:1238`.
- **HOOK_SNEAK** (39) — Fires after each sneak check (via `sneakEventProcess`). Allows overriding result and duration. `src/sfall_script_hooks.cc:1222-1246`, fire point at `src/critter.cc:1238`.
- **Emscripten CMake preset** — `emscripten-debug` and `emscripten-release` configure presets in `CMakePresets.json` with toolchain file `cmake/toolchain/Emscripten.cmake`.

### Fixed
- **gameReset SpeedMulti** — SpeedMulti is now re-initialized from `ddraw.ini` after game reset (`src/sfall_callbacks.cc:43-56`). Previously, `sfall_gl_vars_reset()` cleared SpeedMulti (key 0) and it defaulted to 100% in subsequent new games.
- **Dead include** — Removed unused `#include "scan_unimplemented.h"` from `src/sfall_config.cc`.
- **Unused constexpr** — Removed duplicate `kMetarulesMax` from `src/sfall_metarules.cc`, replaced usages with `kMetarulesCount`.
- **Scan tool false positive** — Removed `"metarule_exist"` from unimplemented list in `src/scan_unimplemented_sfall.h` (was already implemented at `src/sfall_metarules.cc:851-862`).

### Deliberately Absent
- **HOOK_REMOVEINVENOBJ** — CE's `itemRemove()` does not track RMOBJ_* reason codes or destination object tracking. Would require significant refactoring of the item removal code path.
- **HOOK_ROLLCHECK** — `randomRoll()` has 30+ call sites with no event_type context. Pass-through hook would be too expensive.
- **HOOK_BESTWEAPON** — `_ai_best_weapon()` has 10+ return points with complex comparison logic. Object lifetime concerns with return value override.
- **HOOK_BUILDSFXWEAPON** — `sfxBuildWeaponName()` returns char* to static 16-byte buffer. String override requires buffer management.

---

## [Session 1] — sfall 4.3.4 → 4.4.9 Upgrade

### Added
- **sfall version upgraded** from 4.3.4 to 4.4.9 (`src/sfall_opcodes.cc:62-64`). `sfall_ver_major/minor/build` opcodes now report 4.4.9.
- **HOOK_USEANIMOBJ** (32) — Fires on `opAnimateStand` and `opAnimateStandReverse` before `reg_anim_begin()`. Observe-only hook. `src/sfall_script_hooks.cc:1078-1091`, fire point at `src/interpreter_extra.cc:1358,1383`.
- **HOOK_DESCRIPTIONOBJ** (34) — Fires when an object is examined by the player. Supports sfall 4.4.0+ direct string return for description override. `src/sfall_script_hooks.cc:1105-1122`, fire point at `src/proto_instance.cc:274,280`.
- **HOOK_SETLIGHTING** (38) — Fires on `objectSetLight` for per-object lighting changes. Allows script override of light intensity and distance (maxReturnValues=2). `src/sfall_script_hooks.cc:1135-1164`, fire point at `src/object.cc:1749,1761`.

### Fixed (32 confirmed MEDIUM+ bugs)
- **Bounds checks:** `mf_get_object_data` (`src/sfall_metarules.cc:529-554`) — offset validation, alignment error handling.
- **Null pointer guards:** `opCreateObject` (`src/interpreter_extra.cc:904-907`), `opDestroyMultipleObjects` (`src/interpreter_extra.cc:4511-4515`), `opCritterHeal` (`src/interpreter_extra.cc:2232-2236`), `opMetarule` DROP_ALL_INVEN/INVEN_UNWIELD_WHO (`src/interpreter_extra.cc:3269,3283`).
- **Procedure index bounds:** `opDelayedCall` (`src/interpreter.cc:794`), `opConditionalCall` (`src/interpreter.cc:821`).
- **Stack imbalance fix:** `opCritterRemoveTrait` (`src/interpreter_extra.cc:2953-2956`) — push -1 on null-object early return path.
- **Off-by-one fix:** `opTokenize` (`src/interpreter_lib.cc:278-334`) — boundary conditions, string allocation fixes.
- **Error handling improvements:** `op_read_byte` (`src/sfall_opcodes.cc:102-117`) — now prints error for unknown addresses instead of silently returning 0.
- **Input validation:** `op_set_weapon_ammo_pid` (`src/sfall_opcodes.cc:815-845`) — ammo PID validation before changing weapon ammo type.
- **Behavioral match fix:** `game_loaded` (`src/sfall_global_scripts.cc:213-216`) — returns false for non-global scripts (sfall 4.4.5 behavioral fix).
- **Save/load guards:** Negative count before `reserve()` in `sfall_gl_vars_load` (`src/sfall_global_vars.cc:80-82`); parity check for associative array load (`src/sfall_arrays.cc:1114-1118`).
- Full list verified across 15 source files (see Session 1 synthesis reports for complete file:line inventory).
