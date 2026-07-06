# ---- Test Framework ----
# doctest: single-header C++ testing framework
add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE "${CMAKE_SOURCE_DIR}/third_party/doctest")

# ---- Common test stubs ----
# Stub implementations of engine functions that source files reference but
# that are not needed for unit-test level correctness (debugPrint, compat_*).
add_library(test_stubs STATIC
    test_common_stubs.cc
)
target_include_directories(test_stubs PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_stubs PUBLIC doctest)

# ---- Source-under-test library ----
# Compiles only the translation units needed for unit tests.
# Links against SDL2/zlib (vendored) to satisfy header includes.
add_library(test_sources STATIC
    "${CMAKE_SOURCE_DIR}/src/config.cc"
    "${CMAKE_SOURCE_DIR}/src/dictionary.cc"
    "${CMAKE_SOURCE_DIR}/src/memory.cc"
    "${CMAKE_SOURCE_DIR}/src/sfall_global_vars.cc"
    "${CMAKE_SOURCE_DIR}/src/game_config_migration.cc"
    "${CMAKE_SOURCE_DIR}/src/sfall_config.cc"
    "${CMAKE_SOURCE_DIR}/src/sfall_arrays.cc"
    "${CMAKE_SOURCE_DIR}/src/sfall_ini.cc"
)
target_include_directories(test_sources PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_sources PUBLIC ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_sources PUBLIC test_stubs ${SDL2_LIBRARIES})

# ---- Test: config.cc ----
add_executable(test_config
    test_config.cc
)
target_link_libraries(test_config PRIVATE test_sources test_stubs doctest)
add_test(NAME test_config COMMAND test_config)

# ---- Test: sfall_global_vars.cc ----
add_executable(test_global_vars
    test_global_vars.cc
)
target_link_libraries(test_global_vars PRIVATE test_sources test_stubs doctest)
add_test(NAME test_global_vars COMMAND test_global_vars)

# ---- Test: criticals (combat.cc data structures) ----
# Self-contained test — does not link combat.cc (too many engine deps).
add_executable(test_criticals
    test_criticals.cc
)
target_link_libraries(test_criticals PRIVATE doctest)
add_test(NAME test_criticals COMMAND test_criticals)

# ---- Test: script hooks (header-level type validation) ----
# Tests HookType enums, constants, and sub-type definitions.
# Header-only test — does not link sfall_script_hooks.cc.
add_executable(test_script_hooks
    test_script_hooks.cc
)
target_include_directories(test_script_hooks PRIVATE "${CMAKE_SOURCE_DIR}/src")
# SDL2/zlib headers needed for transitive includes from interpreter.h
target_include_directories(test_script_hooks PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_script_hooks PRIVATE test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_script_hooks COMMAND test_script_hooks)

# ---- Test: game_config_migration.cc ----
add_executable(test_game_config_migration
    test_game_config_migration.cc
)
target_link_libraries(test_game_config_migration PRIVATE test_sources test_stubs doctest)
add_test(NAME test_game_config_migration COMMAND test_game_config_migration)

# ============================================================
# Stage 4 Implementation — New Test Executables (29 entries)
# Added by consolidation agent (s4-impl-cmake-consolidate)
# ============================================================

# ---- Test: combat data structures ----
# Self-contained — does not link combat.cc (40+ engine deps).
add_executable(test_combat
    test_combat.cc
)
target_include_directories(test_combat PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_combat PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_combat PRIVATE doctest ${SDL2_LIBRARIES})
add_test(NAME test_combat COMMAND test_combat)

# ---- Test: combat hooks ----
# Self-contained — does not link combat.cc.
add_executable(test_combat_hooks
    test_combat_hooks.cc
)
target_include_directories(test_combat_hooks PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_combat_hooks PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_combat_hooks PRIVATE doctest ${SDL2_LIBRARIES})
add_test(NAME test_combat_hooks COMMAND test_combat_hooks)

# ---- Test: interpreter core (data structures) ----
# Self-contained test — does not link interpreter.cc (50+ engine deps).
add_executable(test_interpreter_core
    test_interpreter_core.cc
)
target_link_libraries(test_interpreter_core PRIVATE doctest)
add_test(NAME test_interpreter_core COMMAND test_interpreter_core)

# ---- Test: interpreter_extra (data structures + logic patterns) ----
# Self-contained test — does not link interpreter_extra.cc (50+ engine deps).
add_executable(test_interpreter_extra
    test_interpreter_extra.cc
)
target_link_libraries(test_interpreter_extra PRIVATE doctest)
add_test(NAME test_interpreter_extra COMMAND test_interpreter_extra)

# ---- Test: inventory.cc core logic patterns ----
# Self-contained test — does not link inventory.cc (60+ engine deps).
add_executable(test_inventory
    test_inventory.cc
)
target_link_libraries(test_inventory PRIVATE doctest)
add_test(NAME test_inventory COMMAND test_inventory)

# ---- Test: item.cc (addiction, power armor weight, Fast Shot AP) ----
# Self-contained test — does not link item.cc (70+ engine deps).
add_executable(test_item
    test_item.cc
)
target_link_libraries(test_item PRIVATE doctest)
add_test(NAME test_item COMMAND test_item)

# ---- Test: stat.cc (experience, stat min/max, XP modifier) ----
# Self-contained test — does not link stat.cc.
add_executable(test_stat
    test_stat.cc
)
target_link_libraries(test_stat PRIVATE doctest)
add_test(NAME test_stat COMMAND test_stat)

# ---- Test: game.cc (state machine, SpeedMulti, globalVarsRead fix) ----
# Self-contained test — does not link game.cc.
add_executable(test_game
    test_game.cc
)
target_link_libraries(test_game PRIVATE doctest)
add_test(NAME test_game COMMAND test_game)

# ---- Test: map.cc / map_edge.cc (coordinate math, aging fix) ----
# Self-contained test — does not link map.cc or map_edge.cc.
add_executable(test_map
    test_map.cc
)
target_link_libraries(test_map PRIVATE doctest)
add_test(NAME test_map COMMAND test_map)

# ---- Test: sfall_config (configGetInt/Double/IntList fork validation) ----
# Links test_sources (config.cc, sfall_config.cc) for config/sfall_config testing.
add_executable(test_config_sfall
    test_config_sfall.cc
)
target_link_libraries(test_config_sfall PRIVATE test_sources test_stubs doctest)
add_test(NAME test_config_sfall COMMAND test_config_sfall)

# ---- Test: misc engine (configGetBool/SetDouble/IntList edges, ScopedConfig) ----
# Links test_sources (config.cc) for config function testing.
add_executable(test_misc_engine
    test_misc_engine.cc
)
target_link_libraries(test_misc_engine PRIVATE test_sources test_stubs doctest)
add_test(NAME test_misc_engine COMMAND test_misc_engine)

# ---- Test: object.cc (data structures, RAII, pure logic) ----
# Self-contained test — does not link object.cc (40+ engine deps).
# Includes obj_types.h from src/ for real type definitions.
add_executable(test_object
    test_object.cc
)
target_include_directories(test_object PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_object PRIVATE doctest)
add_test(NAME test_object COMMAND test_object)

# ---- Test: party_member.h (self-contained test-local mirrors) ----
add_executable(test_party
    test_party.cc
)
target_link_libraries(test_party PRIVATE doctest)
add_test(NAME test_party COMMAND test_party)

# ---- Test: perk.h (self-contained test-local mirrors) ----
add_executable(test_perk
    test_perk.cc
)
target_link_libraries(test_perk PRIVATE doctest)
add_test(NAME test_perk COMMAND test_perk)

# ---- Test: critter.h (self-contained test-local mirrors) ----
add_executable(test_critter
    test_critter.cc
)
target_link_libraries(test_critter PRIVATE doctest)
add_test(NAME test_critter COMMAND test_critter)

# ---- Test: proto data structures ----
# Self-contained test — does not link proto.cc (50+ engine deps).
# Includes proto_types.h, obj_types.h from src/ for real type definitions.
add_executable(test_proto
    test_proto.cc
)
target_include_directories(test_proto PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_proto PRIVATE doctest)
add_test(NAME test_proto COMMAND test_proto)

# ---- Test: scripts.cc (data structures, accessors, sfallModsIni config) ----
# Links test_sources (config.cc) for sfallModsIni Config integration.
add_executable(test_scripts
    test_scripts.cc
)
target_link_libraries(test_scripts PRIVATE test_sources test_stubs doctest)
add_test(NAME test_scripts COMMAND test_scripts)

# ---- Test: skill.cc (data structures, table integrity, skill max cap) ----
# Self-contained — uses test-local stubs only.
add_executable(test_skill
    test_skill.cc
)
target_link_libraries(test_skill PRIVATE doctest)
add_test(NAME test_skill COMMAND test_skill)

# ---- Test: sfall_arrays.cc ----
# Links test_sources (sfall_arrays.cc) for array functions.
# Local stubs in test file override engine dependencies.
add_executable(test_sfall_arrays
    test_sfall_arrays.cc
)
target_link_libraries(test_sfall_arrays PRIVATE test_sources test_stubs doctest)
add_test(NAME test_sfall_arrays COMMAND test_sfall_arrays)

# ---- Test: sfall_ini.cc ----
# Links test_sources (sfall_ini.cc) for INI functions.
# Local stubs in test file override engine dependencies.
add_executable(test_sfall_ini
    test_sfall_ini.cc
)
target_link_libraries(test_sfall_ini PRIVATE test_sources test_stubs doctest)
add_test(NAME test_sfall_ini COMMAND test_sfall_ini)

# ---- Test: sfall_config lifecycle (from sfall-infra) ----
# Links test_sources (sfall_config.cc) for sfallConfigInit/Exit.
add_executable(test_sfall_config
    test_sfall_config.cc
)
target_link_libraries(test_sfall_config PRIVATE test_sources test_stubs doctest)
add_test(NAME test_sfall_config COMMAND test_sfall_config)

# ---- Test: sfall_opcodes (lifecycle, extern globals) ----
# Extern globals from sfall_opcodes.cc are defined in test_common_stubs.cc.
# Links test_sources for sfall_global_vars.cc (M-092 tests).
add_executable(test_sfall_opcodes
    test_sfall_opcodes.cc
)
target_link_libraries(test_sfall_opcodes PRIVATE test_sources test_stubs doctest)
target_include_directories(test_sfall_opcodes PRIVATE ${ZLIB_INCLUDE_DIRS})
add_test(NAME test_sfall_opcodes COMMAND test_sfall_opcodes)

# ---- Test: sfall metarules (header-level type validation) ----
# Header-level test — does not link sfall_metarules.cc.
# SDL2/zlib headers needed for transitive includes from sfall_metarules.h.
add_executable(test_sfall_metarules
    test_sfall_metarules.cc
)
target_include_directories(test_sfall_metarules PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_sfall_metarules PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_sfall_metarules PRIVATE test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_sfall_metarules COMMAND test_sfall_metarules)

# ---- Test: sfall hooks (extended hook type validation) ----
# Complements test_script_hooks.cc. Header-level test.
add_executable(test_sfall_hooks
    test_sfall_hooks.cc
)
target_include_directories(test_sfall_hooks PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_sfall_hooks PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_sfall_hooks PRIVATE test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_sfall_hooks COMMAND test_sfall_hooks)

# ---- Test: window.cc (word wrap, color conversion, enums) ----
# Self-contained — does NOT link test_sources or SDL2.
# window.h removed from test to avoid SDL initialization hang on headless macOS.
# Memory stubs (internal_malloc_safe, etc.) come from test_stubs.
# Enums from window.h defined locally in test_window.cc.
add_executable(test_window
    test_window.cc
)
target_include_directories(test_window PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_window PRIVATE test_stubs doctest)
add_test(NAME test_window COMMAND test_window)

# ---- Test: dbox.cc (enum validation, file dialog layout constants) ----
# Header-only test — self-contained with dbox.h constants.
add_executable(test_dbox
    test_dbox.cc
)
target_include_directories(test_dbox PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_dbox PRIVATE doctest)
add_test(NAME test_dbox COMMAND test_dbox)

# ---- Test: dialog.cc (dialog state, color conversion, F-05 fix) ----
# Links test_sources (memory.cc). SDL2/zlib for dialog.h cascade.
add_executable(test_dialog
    test_dialog.cc
)
target_include_directories(test_dialog PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_dialog PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_dialog PRIVATE test_sources test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_dialog COMMAND test_dialog)

# ---- Test: worldmap (self-contained data structure tests) ----
# Self-contained test — does not link worldmap.cc (45+ engine deps).
add_executable(test_worldmap
    test_worldmap.cc
)
target_link_libraries(test_worldmap PRIVATE doctest)
add_test(NAME test_worldmap COMMAND test_worldmap)

# ---- Test: character_editor.cc (pure logic mirrors) ----
# Self-contained test — does not link character_editor.cc (40+ engine deps).
add_executable(test_character_editor
    test_character_editor.cc
)
target_link_libraries(test_character_editor PRIVATE doctest)
add_test(NAME test_character_editor COMMAND test_character_editor)

# ============================================================
# Stage 6 Implementation — New Test Executables (8 entries)
# ============================================================

# ---- Test: loadsave.cc (H-013/H-014/H-015 fork change mirrors) ----
# Self-contained test — does not link loadsave.cc (engine file I/O deps).
add_executable(test_loadsave
    test_loadsave.cc
)
target_link_libraries(test_loadsave PRIVATE doctest)
add_test(NAME test_loadsave COMMAND test_loadsave)

# ---- Test: sfall_animation.cc (M-057: 8-function module mirrors) ----
# Self-contained test — does not link sfall_animation.cc (50+ engine deps).
add_executable(test_sfall_animation
    test_sfall_animation.cc
)
target_link_libraries(test_sfall_animation PRIVATE doctest)
add_test(NAME test_sfall_animation COMMAND test_sfall_animation)

# ---- Test: endgame.cc (N2-H-001/N2-044/N2-045/N2-046 fork change mirrors) ----
# Self-contained test — does not link endgame.cc (40+ engine deps).
# Includes obj_types.h and proto_types.h from src/.
add_executable(test_endgame
    test_endgame.cc
)
target_include_directories(test_endgame PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_endgame PRIVATE doctest)
add_test(NAME test_endgame COMMAND test_endgame)

# ---- Test: pipboy.cc (M-081/N2-026/N2-027 bounds-checking mirrors) ----
# Self-contained test — does not link pipboy.cc (30+ engine deps).
add_executable(test_pipboy
    test_pipboy.cc
)
target_link_libraries(test_pipboy PRIVATE doctest)
add_test(NAME test_pipboy COMMAND test_pipboy)

# ---- Test: game_dialog.cc (N2-001/N2-049 fork change mirrors) ----
# Self-contained test — does not link game_dialog.cc (40+ engine deps).
add_executable(test_game_dialog
    test_game_dialog.cc
)
target_link_libraries(test_game_dialog PRIVATE doctest)
add_test(NAME test_game_dialog COMMAND test_game_dialog)

# ---- Test: game_mouse.cc (M-089/N2-047 fork change mirrors) ----
# Self-contained test — does not link game_mouse.cc (40+ engine deps).
add_executable(test_game_mouse
    test_game_mouse.cc
)
target_link_libraries(test_game_mouse PRIVATE doctest)
add_test(NAME test_game_mouse COMMAND test_game_mouse)

# ---- Test: sound.cc (N2-003 preloadBuffers error-propagation mirrors) ----
# Self-contained test — does not link sound.cc.
add_executable(test_sound
    test_sound.cc
)
target_link_libraries(test_sound PRIVATE doctest)
add_test(NAME test_sound COMMAND test_sound)

# ---- Test: animation.cc (H-009/H-010/N2-018 fork change mirrors) ----
# Self-contained test — does not link animation.cc.
add_executable(test_animation
    test_animation.cc
)
target_link_libraries(test_animation PRIVATE doctest)
add_test(NAME test_animation COMMAND test_animation)

# ---- Test: platform_compat.cc (path traversal security) ----
# Self-contained test — does not link platform_compat.cc.
# Tests compat_path_contains_traversal from platform_compat.cc:446-480.
add_executable(test_platform_compat
    test_platform_compat.cc
)
target_link_libraries(test_platform_compat PRIVATE doctest)
add_test(NAME test_platform_compat COMMAND test_platform_compat)
