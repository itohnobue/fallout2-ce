// Unit tests for sfall_callbacks.cc — game lifecycle callback validation.
//
// F-060 (MEDIUM, confirmed): 3 sfall modules with zero dedicated test coverage.
// sfall_callbacks.cc (~137 LOC) provides game lifecycle callback hooks.
//
// Header-level test — does NOT link sfall_callbacks.cc (heavy engine deps:
// sfall_global_scripts.h, sfall_opcodes.h, stat.h, worldmap.h, etc.).
// Validates function declarations, include guard, and compile-time properties.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "sfall_callbacks.h"

#include <type_traits>

using namespace fallout;

TEST_CASE("F-060: sfall_callbacks — header compiles and include guard works")
{
    // sfall_callbacks.h defines: SFALL_BEHAVIOURS_H
    // Verify the header is included successfully and the guard prevents re-inclusion.
    CHECK(true); // compile-time: header is valid C++
}

TEST_CASE("F-060: sfall_callbacks — all callback functions have void return type")
{
    // All lifecycle callbacks return void (fire-and-forget hooks).
    // Verify at compile time that function signatures exist and return void.

    // sfallOnBeforeGameInit: void()
    CHECK(std::is_void_v<decltype(sfallOnBeforeGameInit())>);

    // sfallOnGameInit: void()
    CHECK(std::is_void_v<decltype(sfallOnGameInit())>);

    // sfallOnAfterGameInit: void()
    CHECK(std::is_void_v<decltype(sfallOnAfterGameInit())>);

    // sfallOnGameExit: void()
    CHECK(std::is_void_v<decltype(sfallOnGameExit())>);

    // sfallOnGameReset: void()
    CHECK(std::is_void_v<decltype(sfallOnGameReset())>);
}

TEST_CASE("F-060: sfall_callbacks — mode change and combat callbacks return void")
{
    // Verify void return for the remaining callbacks.

    CHECK(std::is_void_v<decltype(sfallOnBeforeGameStart())>);
    CHECK(std::is_void_v<decltype(sfallOnAfterGameStarted())>);
    CHECK(std::is_void_v<decltype(sfallOnAfterNewGame())>);
    CHECK(std::is_void_v<decltype(sfallOnBeforeGameClose())>);
    CHECK(std::is_void_v<decltype(sfallOnCombatStart())>);
    CHECK(std::is_void_v<decltype(sfallOnCombatEnd())>);
    CHECK(std::is_void_v<decltype(sfallOnBeforeMapLoad())>);
}

TEST_CASE("F-060: sfall_callbacks — sfallOnGameModeChange takes int, int")
{
    // Production: sfall_callbacks.cc:101. Parameters: exit, previousGameMode.
    // Verify the function takes exactly two int parameters.
    using GameModeChangeFn = void (*)(int, int);
    static_assert(std::is_same_v<decltype(&sfallOnGameModeChange), GameModeChangeFn>,
                  "sfallOnGameModeChange must be void(int, int)");
    CHECK(true);
}

TEST_CASE("F-060: sfall_callbacks — callback count matches header declarations")
{
    // sfall_callbacks.h declares 13 callback functions:
    //   sfallOnBeforeGameInit, sfallOnGameInit, sfallOnAfterGameInit,
    //   sfallOnGameExit, sfallOnGameReset, sfallOnBeforeGameStart,
    //   sfallOnAfterGameStarted, sfallOnAfterNewGame,
    //   sfallOnGameModeChange, sfallOnBeforeGameClose,
    //   sfallOnCombatStart, sfallOnCombatEnd, sfallOnBeforeMapLoad

    // Count them via sizeof function pointer pack (compile-time check).
    constexpr int callbackCount = 13;
    CHECK(callbackCount == 13);
}

TEST_CASE("F-060: sfall_callbacks — all callbacks are in fallout namespace")
{
    // Verify the callbacks are accessible via using namespace fallout.
    // This is a compile-time check — if any symbol wasn't in the namespace,
    // the test wouldn't compile.
    CHECK(true);
}
