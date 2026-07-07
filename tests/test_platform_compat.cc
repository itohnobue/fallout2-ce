// Unit tests for platform_compat.cc — path traversal security and overflow regression.
//
// path traversal tests: mirror compat_path_contains_traversal() from
// platform_compat.cc:446-480 (fork-added user-controlled path validation).
//
// overflow regression tests: compile-time sizeof guards for Stage 5 fixes
// (F-11: DirectoryFileFindData.pattern size regression).
//
// Self-contained test — does not link platform_compat.cc.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>
#include <cstddef>

// Stage 5 overflow regression headers — needed for sizeof/static_assert checks.
// These are header-only; no linking of platform_compat.cc required.
#include "dfile.h"
#include "file_find.h"
#include "platform_compat.h"

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

// ============================================================================
// Stage 5 Overflow Regression Tests
// ============================================================================
//
// F-11 (CRITICAL — crash root cause):
//   DirectoryFileFindData.pattern was COMPAT_MAX_FNAME (256 bytes) but
//   compat_makepath writes up to COMPAT_MAX_PATH (260 bytes) — deterministic
//   4-byte stack overflow corrupting the canary → SIGABRT on every startup.
//   Fixed by expanding pattern to COMPAT_MAX_PATH at file_find.h:45.
//
//   TEST: static_assert + runtime sizeof check — if pattern is ever shrunk
//   back (e.g., someone changes it back to COMPAT_MAX_FNAME), the test
//   binary fails to compile. This is the most robust regression guard.

TEST_CASE("F-11 regression: DirectoryFileFindData::pattern is COMPAT_MAX_PATH sized")
{
    // Compile-time guard: pattern must be at least COMPAT_MAX_PATH bytes.
    // If someone reverts the Stage 5 fix and shrinks pattern back to
    // COMPAT_MAX_FNAME (256), this static_assert fires.
    static_assert(sizeof(fallout::DirectoryFileFindData::pattern) >= COMPAT_MAX_PATH,
        "F-11 REGRESSION: DirectoryFileFindData::pattern must be >= COMPAT_MAX_PATH (260) "
        "because compat_makepath assumes all destination buffers are COMPAT_MAX_PATH-sized. "
        "See file_find.h:40-45 and the Stage 5 fix (s5-impl-report.md).");

    // Runtime confirmation — exact size is COMPAT_MAX_PATH (260).
    CHECK(sizeof(fallout::DirectoryFileFindData::pattern) == COMPAT_MAX_PATH);

    // Verify pattern is >= every component max that compat_makepath may write.
    CHECK(sizeof(fallout::DirectoryFileFindData::pattern) >= COMPAT_MAX_PATH);
    CHECK(sizeof(fallout::DirectoryFileFindData::pattern) >= COMPAT_MAX_DIR);
    CHECK(sizeof(fallout::DirectoryFileFindData::pattern) >= COMPAT_MAX_FNAME);
    CHECK(sizeof(fallout::DirectoryFileFindData::pattern) >= COMPAT_MAX_EXT);
}

TEST_CASE("F-11 regression: DFileFindData::pattern is also COMPAT_MAX_PATH sized")
{
    // DFileFindData.pattern (dfile.h:120) was already COMPAT_MAX_PATH and
    // served as the reference convention for the DirectoryFileFindData fix.
    // Verify it hasn't regressed either.
    static_assert(sizeof(fallout::DFileFindData::pattern) >= COMPAT_MAX_PATH,
        "DFileFindData::pattern must be >= COMPAT_MAX_PATH (260).");

    CHECK(sizeof(fallout::DFileFindData::pattern) == COMPAT_MAX_PATH);
    CHECK(sizeof(fallout::DFileFindData::pattern) >= COMPAT_MAX_PATH);
}
