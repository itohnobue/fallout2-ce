// Unit tests for the real word_wrap.cc production function.
//
// Unlike window.cc (which has 40+ engine dependencies), word_wrap.cc is a
// standalone translation unit with minimal dependencies — it only calls the
// extern font function pointers and strlen. It can therefore be linked
// directly, giving us a production-linked test for the H-10 null guard and
// the wrap algorithm.
//
// The extern font function pointers (text_font.h) are declared as variables;
// their storage is provided here and pointed at the controllable stubs below.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>
#include <cstdlib>

#include "text_font.h"
#include "word_wrap.h"

using namespace fallout;

// ============================================================
// Storage for extern font function pointers used by wordWrap
// ============================================================

namespace fallout {
    FontManagerGetCharacterWidthProc* fontGetCharacterWidth = nullptr;
    FontManagerGetLetterSpacingProc* fontGetLetterSpacing = nullptr;
    FontManagerGetStringWidthProc* fontGetStringWidth = nullptr;
    FontManagerGetMonospacedCharacterWidth* fontGetMonospacedCharacterWidth = nullptr;
}

// ============================================================
// Controllable font stubs
// ============================================================

static int gWrapCharacterWidth = 1;
static int gWrapLetterSpacing = 1;
static int gWrapMonospacedCharacterWidth = 1;

static int testWrapGetCharacterWidth(int /*ch*/)
{
    return gWrapCharacterWidth;
}

static int testWrapGetLetterSpacing()
{
    return gWrapLetterSpacing;
}

static int testWrapGetStringWidth(const char* string)
{
    return (int)strlen(string) * (gWrapCharacterWidth + gWrapLetterSpacing);
}

static int testWrapGetMonospacedCharacterWidth()
{
    return gWrapMonospacedCharacterWidth;
}

static void installFontStubs()
{
    fontGetCharacterWidth = testWrapGetCharacterWidth;
    fontGetLetterSpacing = testWrapGetLetterSpacing;
    fontGetStringWidth = testWrapGetStringWidth;
    fontGetMonospacedCharacterWidth = testWrapGetMonospacedCharacterWidth;

    gWrapCharacterWidth = 1;
    gWrapLetterSpacing = 1;
    gWrapMonospacedCharacterWidth = 1;
}

// ============================================================
// wordWrap tests (real production function)
// ============================================================

TEST_CASE("wordWrap — null string returns -1 (H-10)")
{
    // H-10: traitGetDescription/traitGetName return nullptr for out-of-range
    // trait ids (e.g. the TRAIT_COUNT sentinel fed by the character editor);
    // pre-fix this dereferenced nullptr inside fontGetStringWidth. The caller
    // contract treats a non-zero return as "cannot wrap" and bails.
    installFontStubs();

    short breakpoints[WORD_WRAP_MAX_COUNT];
    short count = 99;
    int rc = wordWrap(nullptr, 100, breakpoints, &count);
    CHECK(rc == -1);
    CHECK(count == 0);
}

TEST_CASE("wordWrap — short string fits on one line")
{
    installFontStubs();

    char text[] = "hello";
    short breakpoints[WORD_WRAP_MAX_COUNT];
    short count = 0;
    int rc = wordWrap(text, 100, breakpoints, &count);
    CHECK(rc == 0);
    CHECK(count == 2);
    CHECK(breakpoints[1] == 5);
}

TEST_CASE("wordWrap — width too small for monospaced char returns -1")
{
    installFontStubs();
    gWrapMonospacedCharacterWidth = 50;

    char text[] = "hello";
    short breakpoints[WORD_WRAP_MAX_COUNT];
    short count = 0;
    int rc = wordWrap(text, 10, breakpoints, &count);
    CHECK(rc == -1);
}

TEST_CASE("wordWrap — word wrapping with spaces")
{
    installFontStubs();

    // Each char is (width 1 + spacing 1) = 2 units. "hello world" = 22 units.
    // With width = 12: "hello " (12 units) fits, "world" (10 units) fits on
    // the second line. breakpoints are positions into the string: 0, 6
    // (after "hello "), and 12 (one past the terminating NUL, matching the
    // engine's +1 convention).
    char text[] = "hello world";
    short breakpoints[WORD_WRAP_MAX_COUNT];
    short count = 0;
    int rc = wordWrap(text, 12, breakpoints, &count);
    CHECK(rc == 0);
    CHECK(count == 3);
    CHECK(breakpoints[0] == 0);
    CHECK(breakpoints[1] == 6);  // after "hello "
    CHECK(breakpoints[2] == 12); // one past "world"
}

TEST_CASE("wordWrap — character wrapping when a single word exceeds width")
{
    installFontStubs();

    // 15-char word = 30 units with width 20: no space to backtrack to, so
    // the wrap breaks mid-word (character wrap).
    char text[] = "abcdefghijklmno";
    short breakpoints[WORD_WRAP_MAX_COUNT];
    short count = 0;
    int rc = wordWrap(text, 20, breakpoints, &count);
    CHECK(rc == 0);
    CHECK(count >= 2);
}
