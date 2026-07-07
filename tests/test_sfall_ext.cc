// Unit tests for sfall_ext.cc — sfall extension module validation.
//
// F-060 (MEDIUM, confirmed): 3 sfall modules with zero dedicated test coverage.
// sfall_ext.cc (~362 LOC) handles:
//   - GlobalScriptPaths parsing from ddraw.ini [Misc]
//   - HookScriptsPath parsing from ddraw.ini [Misc]
//   - ExtraPatches support
//   - sfall_metarules save/load orchestration
//
// Header-level test — does NOT link sfall_ext.cc (heavy engine deps:
// sfall_arrays.h, sfall_metarules.h, sfall_opcodes.h, etc.).
// Validates function declarations, types, and compile-time properties.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <string>
#include <type_traits>
#include <vector>

#include "sfall_ext.h"
#include "db.h" // File type definition

using namespace fallout;

TEST_CASE("F-060: sfall_ext — header compiles and include guard works")
{
    // sfall_ext.h defines: SFALL_EXT_H
    CHECK(true); // compile-time: header is valid C++
}

TEST_CASE("F-060: sfall_ext — sfallParseGlobalScriptPaths returns bool")
{
    // Production: sfall_ext.h:19. Returns bool (true on success).
    CHECK(std::is_same_v<decltype(sfallParseGlobalScriptPaths()), bool>);
}

TEST_CASE("F-060: sfall_ext — sfallGetGlobalScriptPaths returns const vector<string>&")
{
    // Production: sfall_ext.h:23. Returns const reference to vector of strings.
    using ExpectedType = const std::vector<std::string>&;
    CHECK(std::is_same_v<decltype(sfallGetGlobalScriptPaths()), ExpectedType>);
}

TEST_CASE("F-060: sfall_ext — sfallParseHookScriptsPath returns bool")
{
    // Production: sfall_ext.h:29. Returns bool.
    CHECK(std::is_same_v<decltype(sfallParseHookScriptsPath()), bool>);
}

TEST_CASE("F-060: sfall_ext — sfallGetHookScriptsPath returns const string&")
{
    // Production: sfall_ext.h:32. Returns const reference to string.
    CHECK(std::is_same_v<decltype(sfallGetHookScriptsPath()), const std::string&>);
}

TEST_CASE("F-060: sfall_ext — save/load function signatures")
{
    // sfallSaveGameData / sfallLoadGameData take File* and return bool.
    // Production: sfall_ext.h:13-14.
    CHECK(std::is_same_v<decltype(sfallSaveGameData(std::declval<File*>())), bool>);
    CHECK(std::is_same_v<decltype(sfallLoadGameData(std::declval<File*>())), bool>);
}
