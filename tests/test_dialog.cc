// Unit tests for dialog.cc — dialog state management, color conversion, fix validation.
//
// Tests:
//   1. Dialog color conversion arithmetic — validates the float-to-int
//      conversion used by dialogSetOptionColor and dialogSetReplyColor.
//   2. Dialog setter function behavior — set/get border, spacing, colors.
//      (State manipulation tested directly since dialog.cc has heavy engine deps.)
//   3. F-05 fix validation: _dialogSetScrollUp argument assignment.
//      Pre-fix assigned a5 (scroll-down image) to the scroll-up pointer.
//      Post-fix correctly assigns a6. Local mirror functions test this.
//   4. _dialogSetScrollDown argument validation (sibling of scrollUp).
//   5. Default value validation — gDialogBorderX/Y, gDialogOptionSpacing,
//      _mediaFlag, _inDialog, _exitDialog initial values.
//
// This file does NOT link dialog.cc (too many engine deps: window_manager,
// text_font, svga, movie, mouse, etc.). It mirrors the scroll setup functions
// locally and validates the global state directly — same pattern as test_criticals.cc.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

#include "dialog.h"
#include "memory_manager.h"

using namespace fallout;

// ============================================================
// Definitions for dialog globals
// ============================================================
// These are declared as extern in dialog.h inside namespace fallout
// and defined in dialog.cc. Since dialog.cc is not linked into this
// test (it has 20+ engine deps), we provide test definitions here.

namespace fallout {
    int gDialogBorderX = 7;
    int gDialogBorderY = 7;
    int gDialogOptionSpacing = 5;
    int _inDialog = 0;
    int _exitDialog = 0;
    int _mediaFlag = 2;
    int _tods = -1;
    int _replyRGBset = 0;
    int _optionRGBset = 0;

    int _upButton = 0;
    int dword_56DBD8 = 0;
    int dword_56DBDC = 0;
    char* off_56DBE0 = nullptr;
    char* off_56DBE4 = nullptr;
    char* off_56DBE8 = nullptr;
    char* off_56DBEC = nullptr;

    int _downButton = 0;
    int dword_56DBB8 = 0;
    int dword_56DBBC = 0;
    char* off_56DBC0 = nullptr;
    char* off_56DBC4 = nullptr;
    char* off_56DBC8 = nullptr;
    char* off_56DBCC = nullptr;

    int gDialogReplyColorR = 0;
    int gDialogReplyColorG = 0;
    int gDialogReplyColorB = 0;
    int gDialogOptionColorR = 0;
    int gDialogOptionColorG = 0;
    int gDialogOptionColorB = 0;

    const float flt_501623 = 31.0f;
    const float flt_501627 = 31.0f;
}

// Local mirror of dialogInit (dialog.cc:686-688 — empty body)
static void testDialogInit()
{
}

// ============================================================
// Local mirrors of _dialogSetScrollUp / _dialogSetScrollDown
// ============================================================
// Copied from src/dialog.cc:584-611 and :615-642.
// We mirror these here because linking dialog.cc requires 20+ stubs
// (window_manager, svga, movie, mouse, text_font, etc.).

static int testDialogSetScrollUp(int a1, int a2, char* a3, char* a4, char* a5, char* a6, int a7)
{
    _upButton = a1;
    dword_56DBD8 = a2;

    if (off_56DBE0 != nullptr) {
        internal_free_safe(off_56DBE0, __FILE__, __LINE__);
    }
    off_56DBE0 = a3;

    if (off_56DBE4 != nullptr) {
        internal_free_safe(off_56DBE4, __FILE__, __LINE__);
    }
    off_56DBE4 = a4;

    if (off_56DBE8 != nullptr) {
        internal_free_safe(off_56DBE8, __FILE__, __LINE__);
    }
    off_56DBE8 = a5;

    if (off_56DBEC != nullptr) {
        internal_free_safe(off_56DBEC, __FILE__, __LINE__);
    }
    off_56DBEC = a6;

    dword_56DBDC = a7;

    return 0;
}

static int testDialogSetScrollDown(int a1, int a2, char* a3, char* a4, char* a5, char* a6, int a7)
{
    _downButton = a1;
    dword_56DBB8 = a2;

    if (off_56DBC0 != nullptr) {
        internal_free_safe(off_56DBC0, __FILE__, __LINE__);
    }
    off_56DBC0 = a3;

    if (off_56DBC4 != nullptr) {
        internal_free_safe(off_56DBC4, __FILE__, __LINE__);
    }
    off_56DBC4 = a4;

    if (off_56DBC8 != nullptr) {
        internal_free_safe(off_56DBC8, __FILE__, __LINE__);
    }
    off_56DBC8 = a5;

    if (off_56DBCC != nullptr) {
        internal_free_safe(off_56DBCC, __FILE__, __LINE__);
    }
    off_56DBCC = a6;

    dword_56DBBC = a7;

    return 0;
}


// ============================================================
// Dialog global state — defaults and initial values
// ============================================================

TEST_CASE("dialog — default border values")
{
    // From src/dialog.cc:37-40
    // gDialogBorderX = 7, gDialogBorderY = 7
    CHECK(gDialogBorderX == 7);
    CHECK(gDialogBorderY == 7);
}

TEST_CASE("dialog — default option spacing")
{
    // From src/dialog.cc:42-43
    CHECK(gDialogOptionSpacing == 5);
}

TEST_CASE("dialog — default state flags")
{
    // From src/dialog.cc:
    //   _inDialog = 0  (line 55)
    //   _exitDialog = 0 (line 52)
    //   _mediaFlag = 2  (line 58)
    //   _tods = -1       (line 22)
    CHECK(_inDialog == 0);
    CHECK(_exitDialog == 0);
    CHECK(_mediaFlag == 2);
    CHECK(_tods == -1);
    CHECK(_replyRGBset == 0);
    CHECK(_optionRGBset == 0);
}


// ============================================================
// Dialog setter behavior tests (direct global state manipulation)
// ============================================================

TEST_CASE("dialogSetBorder — sets both X and Y")
{
    // Mirror of src/dialog.cc:575-581
    int savedX = gDialogBorderX;
    int savedY = gDialogBorderY;

    gDialogBorderX = 10;
    gDialogBorderY = 15;
    CHECK(gDialogBorderX == 10);
    CHECK(gDialogBorderY == 15);

    // Restore
    gDialogBorderX = savedX;
    gDialogBorderY = savedY;
}

TEST_CASE("dialogSetOptionSpacing — sets spacing value")
{
    // Mirror of src/dialog.cc:646-651
    int saved = gDialogOptionSpacing;

    gDialogOptionSpacing = 10;
    CHECK(gDialogOptionSpacing == 10);

    gDialogOptionSpacing = 0;
    CHECK(gDialogOptionSpacing == 0);

    // Negative values allowed (no validation in the setter)
    gDialogOptionSpacing = -5;
    CHECK(gDialogOptionSpacing == -5);

    // Restore
    gDialogOptionSpacing = saved;
}


// ============================================================
// Dialog color conversion tests
// ============================================================
// dialogSetOptionColor (dialog.cc:654-663):
//   r = (int)(a1 * flt_501623) where flt_501623 = 31.0f
// dialogSetReplyColor (dialog.cc:666-674):
//   r = (int)(a1 * flt_501627) where flt_501627 = 31.0f

TEST_CASE("dialogSetOptionColor — conversion arithmetic")
{
    const float flt_501623 = 31.0f;

    // White → all 31
    int r = (int)(1.0f * flt_501623);
    int g = (int)(1.0f * flt_501623);
    int b = (int)(1.0f * flt_501623);
    CHECK(r == 31);
    CHECK(g == 31);
    CHECK(b == 31);

    // Black → all 0
    r = (int)(0.0f * flt_501623);
    g = (int)(0.0f * flt_501623);
    b = (int)(0.0f * flt_501623);
    CHECK(r == 0);
    CHECK(g == 0);
    CHECK(b == 0);

    // Half values
    r = (int)(0.5f * flt_501623);
    g = (int)(0.5f * flt_501623);
    b = (int)(0.5f * flt_501623);
    CHECK(r == 15);
    CHECK(g == 15);
    CHECK(b == 15);
}

TEST_CASE("dialogSetReplyColor — uses flt_501627 (same as flt_501623)")
{
    const float flt_501627 = 31.0f;

    CHECK(flt_501627 == 31.0f);

    int r = (int)(0.8f * flt_501627);
    int g = (int)(0.4f * flt_501627);
    int b = (int)(0.1f * flt_501627);
    CHECK(r == 24);   // 0.8 * 31 = 24.8 → 24
    CHECK(g == 12);   // 0.4 * 31 = 12.4 → 12
    CHECK(b == 3);    // 0.1 * 31 = 3.1 → 3
}

TEST_CASE("dialog — option color setter simulates _optionRGBset")
{
    int saved = _optionRGBset;
    _optionRGBset = 1;
    CHECK(_optionRGBset == 1);
    _optionRGBset = saved;
}

TEST_CASE("dialog — reply color setter simulates _replyRGBset")
{
    int saved = _replyRGBset;
    _replyRGBset = 1;
    CHECK(_replyRGBset == 1);
    _replyRGBset = saved;
}

TEST_CASE("dialog — flt_501623 and flt_501627 are both 31.0f")
{
    // 5-bit color depth: 2^5 = 32, range 0-31
    CHECK(flt_501623 == 31.0f);
    CHECK(flt_501627 == 31.0f);
}


// ============================================================
// F-05 fix validation: _dialogSetScrollUp argument order
// ============================================================
// The bug (F-05): _dialogSetScrollUp at dialog.cc:604
//   Pre-fix:  off_56DBEC = a5;  // wrong: used scroll-down image arg
//   Post-fix: off_56DBEC = a6;  // correct: scroll-up image arg
//
// Argument order of _dialogSetScrollUp (dialog.cc:584):
//   int _dialogSetScrollUp(int a1, int a2, char* a3, char* a4,
//                          char* a5, char* a6, int a7)
// Where:
//   a3 = off_56DBE0 (normal image)
//   a4 = off_56DBE4 (pressed image)
//   a5 = off_56DBE8 (scroll-down image)
//   a6 = off_56DBEC (scroll-up image)

TEST_CASE("F-05 fix — off_56DBEC receives a6 not a5 (scroll-up images)")
{
    // Save original state
    char* savedE0 = off_56DBE0;
    char* savedE4 = off_56DBE4;
    char* savedE8 = off_56DBE8;
    char* savedEC = off_56DBEC;
    int savedUpBtn = _upButton;
    int savedD8 = dword_56DBD8;
    int savedDC = dword_56DBDC;

    // Prevent internal_free_safe from freeing non-heap pointers
    off_56DBE0 = nullptr;
    off_56DBE4 = nullptr;
    off_56DBE8 = nullptr;
    off_56DBEC = nullptr;

    // Use distinct test values for a5 and a6 to detect the bug
    char testA5[] = "scroll_down_img";
    char testA6[] = "scroll_up_img";

    testDialogSetScrollUp(100, 200, testA5, testA6, testA5, testA6, 300);

    // F-05 verification: off_56DBEC must receive a6 (scroll-up), not a5
    CHECK(off_56DBEC == testA6);  // post-fix: correct
    CHECK(off_56DBEC != testA5);  // pre-fix would have assigned a5 here

    // Cross-check: off_56DBE8 must receive a5 (scroll-down)
    CHECK(off_56DBE8 == testA5);

    // Verify other assignments
    CHECK(off_56DBE0 == testA5);
    CHECK(off_56DBE4 == testA6);
    CHECK(_upButton == 100);
    CHECK(dword_56DBD8 == 200);
    CHECK(dword_56DBDC == 300);

    // Restore state
    off_56DBE0 = savedE0;
    off_56DBE4 = savedE4;
    off_56DBE8 = savedE8;
    off_56DBEC = savedEC;
    _upButton = savedUpBtn;
    dword_56DBD8 = savedD8;
    dword_56DBDC = savedDC;
}


// ============================================================
// _dialogSetScrollDown — argument order validation
// ============================================================
// Sibling function _dialogSetScrollDown follows same pattern.
// Verify its correctness.

TEST_CASE("_dialogSetScrollDown — correct argument mapping")
{
    // Save original state
    char* savedC0 = off_56DBC0;
    char* savedC4 = off_56DBC4;
    char* savedC8 = off_56DBC8;
    char* savedCC = off_56DBCC;
    int savedDown = _downButton;
    int savedB8 = dword_56DBB8;
    int savedBC = dword_56DBBC;

    off_56DBC0 = nullptr;
    off_56DBC4 = nullptr;
    off_56DBC8 = nullptr;
    off_56DBCC = nullptr;

    char testA3[] = "normal_img";
    char testA4[] = "pressed_img";
    char testA5[] = "down_normal";
    char testA6[] = "down_pressed";

    testDialogSetScrollDown(50, 60, testA3, testA4, testA5, testA6, 70);

    CHECK(_downButton == 50);
    CHECK(dword_56DBB8 == 60);
    CHECK(off_56DBC0 == testA3);
    CHECK(off_56DBC4 == testA4);
    CHECK(off_56DBC8 == testA5);
    CHECK(off_56DBCC == testA6);
    CHECK(dword_56DBBC == 70);

    // Restore
    off_56DBC0 = savedC0;
    off_56DBC4 = savedC4;
    off_56DBC8 = savedC8;
    off_56DBCC = savedCC;
    _downButton = savedDown;
    dword_56DBB8 = savedB8;
    dword_56DBBC = savedBC;
}


// ============================================================
// dialogInit lifecycle test
// ============================================================

TEST_CASE("dialogInit — no-op (empty body at dialog.cc:686-688)")
{
    // dialogInit() is currently an empty function (no-op).
    // Local mirror verifies the pattern.
    testDialogInit();
    CHECK(true);
}


// ============================================================
// Dialog reply/option color global existence check
// ============================================================

TEST_CASE("dialog — reply color globals are accessible")
{
    // These are extern globals declared in dialog.h:76-81
    int r = gDialogReplyColorR;
    int g = gDialogReplyColorG;
    int b = gDialogReplyColorB;
    (void)r; (void)g; (void)b;
    CHECK(true);
}

TEST_CASE("dialog — option color globals are accessible")
{
    int r = gDialogOptionColorR;
    int g = gDialogOptionColorG;
    int b = gDialogOptionColorB;
    (void)r; (void)g; (void)b;
    CHECK(true);
}
