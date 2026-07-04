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
)
target_include_directories(test_sources PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_sources PUBLIC ${SDL2_INCLUDE_DIRS})
target_include_directories(test_sources PUBLIC ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_sources PUBLIC test_stubs)

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
target_include_directories(test_script_hooks PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(test_script_hooks PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_script_hooks PRIVATE test_stubs doctest)
add_test(NAME test_script_hooks COMMAND test_script_hooks)
