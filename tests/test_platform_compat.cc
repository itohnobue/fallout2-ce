// Unit tests for platform_compat.cc path traversal security.
//
// Tests the fork-added compat_path_contains_traversal() function which
// validates user-controlled paths before passing them to VFS operations.
//
// Production source: platform_compat.cc:446-480
// Called by: sfall_ext.cc:190 (GlobalScriptPaths validation)
//
// Self-contained test — does not link platform_compat.cc.
// Mirrors the production logic from platform_compat.cc:446-480.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

// ============================================================================
// Mirrored production logic from platform_compat.cc:446-480
// ============================================================================

static bool testPathContainsTraversal(const char* path)
{
    if (path == nullptr) {
        return false;
    }

    // Walk the path component by component.  If any component is exactly "..",
    // the path traverses above its intended root.
    const char* p = path;

    // Skip leading separators (absolute paths: "/foo", "\\foo").
    while (*p == '/' || *p == '\\') {
        p++;
    }

    while (*p != '\0') {
        // Find the end of the current component (next separator or NUL).
        const char* start = p;
        while (*p != '\0' && *p != '/' && *p != '\\') {
            p++;
        }

        size_t len = static_cast<size_t>(p - start);
        if (len == 2 && start[0] == '.' && start[1] == '.') {
            return true; // ".." component found
        }

        // Skip separators between components.
        while (*p == '/' || *p == '\\') {
            p++;
        }
    }

    return false;
}

// ============================================================================
// Test Cases
// ============================================================================

TEST_CASE("path traversal: safe paths rejected or accepted correctly")
{
    SUBCASE("null path returns false")
    {
        CHECK(testPathContainsTraversal(nullptr) == false);
    }

    SUBCASE("empty path returns false")
    {
        CHECK(testPathContainsTraversal("") == false);
    }

    SUBCASE("single component safe path")
    {
        CHECK(testPathContainsTraversal("test.txt") == false);
        CHECK(testPathContainsTraversal("file") == false);
        CHECK(testPathContainsTraversal("my_script.ssl") == false);
    }

    SUBCASE("multi-component safe path")
    {
        CHECK(testPathContainsTraversal("scripts/global") == false);
        CHECK(testPathContainsTraversal("data/scripts/test.ssl") == false);
        CHECK(testPathContainsTraversal("mods/my_mod/data/scripts") == false);
    }

    SUBCASE("windows backslash safe path")
    {
        CHECK(testPathContainsTraversal("scripts\\global") == false);
        CHECK(testPathContainsTraversal("data\\scripts\\test.ssl") == false);
    }

    SUBCASE("mixed slash safe path")
    {
        CHECK(testPathContainsTraversal("scripts/global\\test") == false);
        CHECK(testPathContainsTraversal("data\\scripts/test.ssl") == false);
    }

    SUBCASE("absolute path with multiple leading slashes")
    {
        CHECK(testPathContainsTraversal("/scripts/global/test.ssl") == false);
        CHECK(testPathContainsTraversal("//scripts/global") == false);
        CHECK(testPathContainsTraversal("\\\\scripts\\global") == false);
    }
}

TEST_CASE("path traversal: .. in paths is detected")
{
    SUBCASE("simple .. at start")
    {
        CHECK(testPathContainsTraversal("../etc/passwd") == true);
        CHECK(testPathContainsTraversal("..\\windows\\system32") == true);
    }

    SUBCASE(".. in middle of path")
    {
        CHECK(testPathContainsTraversal("scripts/../etc/passwd") == true);
        CHECK(testPathContainsTraversal("scripts\\..\\etc\\passwd") == true);
    }

    SUBCASE(".. at end of path")
    {
        CHECK(testPathContainsTraversal("scripts/..") == true);
        CHECK(testPathContainsTraversal("data\\scripts\\..") == true);
    }

    SUBCASE("multiple .. components")
    {
        CHECK(testPathContainsTraversal("../../etc/passwd") == true);
        CHECK(testPathContainsTraversal("scripts/../../etc/passwd") == true);
    }

    SUBCASE(".. immediately after leading slash")
    {
        CHECK(testPathContainsTraversal("/../etc/passwd") == true);
        CHECK(testPathContainsTraversal("\\..\\windows\\system32") == true);
    }

    SUBCASE(".. among multiple slashes")
    {
        CHECK(testPathContainsTraversal("///..") == true);
        CHECK(testPathContainsTraversal("scripts///../etc") == true);
    }

    SUBCASE(".. with mixed separators")
    {
        CHECK(testPathContainsTraversal("scripts/..\\etc") == true);
        CHECK(testPathContainsTraversal("scripts\\../etc") == true);
    }
}

TEST_CASE("path traversal: edge cases — not traversal")
{
    SUBCASE("dot component alone is safe")
    {
        CHECK(testPathContainsTraversal("./test.txt") == false);
        CHECK(testPathContainsTraversal(".") == false);
        CHECK(testPathContainsTraversal("scripts/./test.ssl") == false);
    }

    SUBCASE("component containing dots is safe")
    {
        // ".." must be an EXACT component, not part of a filename.
        CHECK(testPathContainsTraversal("test..txt") == false);
        CHECK(testPathContainsTraversal("..file.ssl") == false);
        CHECK(testPathContainsTraversal("file..") == false);
    }

    SUBCASE("three dots is NOT traversal")
    {
        CHECK(testPathContainsTraversal("...") == false);
        CHECK(testPathContainsTraversal("scripts/.../test.ssl") == false);
    }

    SUBCASE("..x is NOT traversal (traversal is exactly '..')")
    {
        CHECK(testPathContainsTraversal("scripts/..x/test.ssl") == false);
        CHECK(testPathContainsTraversal("scripts/x../test.ssl") == false);
    }

    SUBCASE("single dot with trailing content is safe")
    {
        CHECK(testPathContainsTraversal(".file") == false);
        CHECK(testPathContainsTraversal("scripts/.file") == false);
    }
}
