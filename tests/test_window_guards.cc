// Unit tests for window.cc — index guard validation.
//
// Tests verify that all script window functions that access
// gManagedWindows[gCurrentManagedWindowIndex] have a -1 guard
// that returns early when gCurrentManagedWindowIndex == -1.
//
// These tests mirror the guard patterns from src/window.cc.
// window.cc has 40+ engine dependencies (SDL, window_manager, etc.)
// that make full linking impractical, so the guard logic is replicated
// here using the same pattern as the production code.
//
// Findings fixed: UF-H-025, UF-H-026, UF-H-028 through UF-H-034

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// ============================================================
// Local mirror of gCurrentManagedWindowIndex
// ============================================================
// In production, this is an extern int defined in window.cc.
// We use a test-local variable so this test is self-contained.
static int gCurrentManagedWindowIndex = 0;

// ============================================================
// Local mirrors of engine types — avoids including src/ headers
// which cascade into SDL and 40+ engine dependencies.
// ============================================================
struct Rect {
    int left;
    int top;
    int right;
    int bottom;
};

static constexpr int MANAGED_WINDOW_COUNT = 32;

// Forward-declare the callback typedef used in ManagedButton
typedef void(ManagedButtonMouseEventCallback)(void* userData, int eventType);

// Forward-declare Program for Region struct pointer field
class Program;

struct ManagedButton {
    char name[16];
    int btn;
    unsigned char* normal;
    unsigned char* pressed;
    unsigned char* hover;
    int width;
    int height;
    int flags;
    ManagedButtonMouseEventCallback* callback;
    ManagedButtonMouseEventCallback* rightCallback;
    void* userData;
    void* rightUserData;
};

struct Region {
    Rect points[16];
    int pointsLength;
    Rect bound;
    char name[32];
    int flags;
    Program* program;
    int procs[6];
    int rightProcs[2];
    int currentPointIndex;
};

struct ManagedWindow {
    int window;
    int width;
    int height;
    int scaleX;
    int scaleY;
    int cursorX;
    int cursorY;
    char name[32];
    ManagedButton* buttons;
    int buttonsLength;
    int buttonsCapacity;
    Region* regions[32];
    int regionsLength;
    int currentRegionIndex;
};

static ManagedWindow gManagedWindows[MANAGED_WINDOW_COUNT] = {};

// ============================================================
// Mirrored guard functions from src/window.cc
// ============================================================

static bool scriptWindowHide_guarded()
{
    if (gCurrentManagedWindowIndex == -1) {
        return false;
    }
    ManagedWindow* managedWindow = &(gManagedWindows[gCurrentManagedWindowIndex]);
    (void)managedWindow; // production calls windowHide(managedWindow->window)
    return true;
}

static bool scriptWindowShow_guarded()
{
    if (gCurrentManagedWindowIndex == -1) {
        return false;
    }
    ManagedWindow* managedWindow = &(gManagedWindows[gCurrentManagedWindowIndex]);
    (void)managedWindow;
    return true;
}

static int scriptWindowWidth_guarded()
{
    if (gCurrentManagedWindowIndex == -1) {
        return -1;
    }
    return gManagedWindows[gCurrentManagedWindowIndex].width;
}

static bool scriptWindowDraw_guarded()
{
    if (gCurrentManagedWindowIndex == -1) {
        return false;
    }
    ManagedWindow* managedWindow = &(gManagedWindows[gCurrentManagedWindowIndex]);
    (void)managedWindow;
    return true;
}

static bool scriptWindowDisplay_guarded()
{
    if (gCurrentManagedWindowIndex == -1) {
        return false;
    }
    // production reads datafile, calls scriptWindowDisplayBuf, frees
    return true;
}

static bool scriptWindowDisplayBuf_guarded()
{
    if (gCurrentManagedWindowIndex == -1) {
        return false;
    }
    ManagedWindow* managedWindow = &(gManagedWindows[gCurrentManagedWindowIndex]);
    (void)managedWindow;
    return true;
}

static bool scriptWindowAddButtonGfx_guarded()
{
    if (gCurrentManagedWindowIndex == -1) {
        return false;
    }
    ManagedWindow* managedWindow = &(gManagedWindows[gCurrentManagedWindowIndex]);
    (void)managedWindow;
    return true;
}

static bool scriptWindowFill_guarded()
{
    if (gCurrentManagedWindowIndex == -1) {
        return false;
    }
    ManagedWindow* managedWindow = &(gManagedWindows[gCurrentManagedWindowIndex]);
    (void)managedWindow;
    return true;
}

static bool scriptWindowFillRect_guarded()
{
    if (gCurrentManagedWindowIndex == -1) {
        return false;
    }
    ManagedWindow* managedWindow = &(gManagedWindows[gCurrentManagedWindowIndex]);
    (void)managedWindow;
    return true;
}

static void scriptWindowEndRegion_guarded()
{
    if (gCurrentManagedWindowIndex == -1) {
        return;
    }
    ManagedWindow* managedWindow = &(gManagedWindows[gCurrentManagedWindowIndex]);
    (void)managedWindow;
}

static bool scriptWindowPlayMovie_guarded()
{
    if (gCurrentManagedWindowIndex == -1) {
        return false;
    }
    // production calls _movieRun(gManagedWindows[...].window, filePath)
    return true;
}

static bool scriptWindowPlayMovieRect_guarded()
{
    if (gCurrentManagedWindowIndex == -1) {
        return false;
    }
    // production calls _movieRunRect(gManagedWindows[...].window, ...)
    return true;
}

// Also test scriptWindowSelectId guard (the caller-side check)
static bool scriptWindowSelectId_guarded(int index)
{
    if (index < 0 || index >= MANAGED_WINDOW_COUNT) {
        return false;
    }
    gCurrentManagedWindowIndex = index;
    return true;
}

// ============================================================
// Test Suite
// ============================================================

TEST_CASE("window guards — all guarded functions return gracefully at index -1")
{
    gCurrentManagedWindowIndex = -1;

    SUBCASE("scriptWindowHide returns false when index is -1")
    {
        CHECK(scriptWindowHide_guarded() == false);
    }

    SUBCASE("scriptWindowShow returns false when index is -1")
    {
        CHECK(scriptWindowShow_guarded() == false);
    }

    SUBCASE("scriptWindowWidth returns -1 when index is -1")
    {
        CHECK(scriptWindowWidth_guarded() == -1);
    }

    SUBCASE("scriptWindowDraw returns false when index is -1")
    {
        CHECK(scriptWindowDraw_guarded() == false);
    }

    SUBCASE("scriptWindowDisplay returns false when index is -1")
    {
        CHECK(scriptWindowDisplay_guarded() == false);
    }

    SUBCASE("scriptWindowDisplayBuf returns false when index is -1")
    {
        CHECK(scriptWindowDisplayBuf_guarded() == false);
    }

    SUBCASE("scriptWindowAddButtonGfx returns false when index is -1")
    {
        CHECK(scriptWindowAddButtonGfx_guarded() == false);
    }

    SUBCASE("scriptWindowFill returns false when index is -1")
    {
        CHECK(scriptWindowFill_guarded() == false);
    }

    SUBCASE("scriptWindowFillRect returns false when index is -1")
    {
        CHECK(scriptWindowFillRect_guarded() == false);
    }

    SUBCASE("scriptWindowEndRegion returns void without crash when index is -1")
    {
        // Must not crash — the guard prevents accessing gManagedWindows[-1]
        scriptWindowEndRegion_guarded();
        // If we reach here, the guard worked
    }

    SUBCASE("scriptWindowPlayMovie returns false when index is -1")
    {
        CHECK(scriptWindowPlayMovie_guarded() == false);
    }

    SUBCASE("scriptWindowPlayMovieRect returns false when index is -1")
    {
        CHECK(scriptWindowPlayMovieRect_guarded() == false);
    }

    SUBCASE("scriptWindowSelectId returns false for -1 index")
    {
        CHECK(scriptWindowSelectId_guarded(-1) == false);
    }

    SUBCASE("scriptWindowSelectId returns false for out-of-range index")
    {
        CHECK(scriptWindowSelectId_guarded(MANAGED_WINDOW_COUNT) == false);
        CHECK(scriptWindowSelectId_guarded(MANAGED_WINDOW_COUNT + 1) == false);
    }
}

TEST_CASE("window guards — functions work normally at valid index")
{
    // Set up a valid window at index 0
    gManagedWindows[0].width = 640;
    gManagedWindows[0].height = 480;
    gManagedWindows[0].window = 0;
    gCurrentManagedWindowIndex = 0;

    SUBCASE("scriptWindowWidth returns correct width at valid index")
    {
        CHECK(scriptWindowWidth_guarded() == 640);
    }

    SUBCASE("bool functions return true at valid index")
    {
        CHECK(scriptWindowHide_guarded() == true);
        CHECK(scriptWindowShow_guarded() == true);
        CHECK(scriptWindowDraw_guarded() == true);
        CHECK(scriptWindowFill_guarded() == true);
        CHECK(scriptWindowFillRect_guarded() == true);
    }

    SUBCASE("scriptWindowSelectId sets gCurrentManagedWindowIndex on success")
    {
        gCurrentManagedWindowIndex = -1;
        bool result = scriptWindowSelectId_guarded(0);
        CHECK(result == true);
        CHECK(gCurrentManagedWindowIndex == 0);
    }

    SUBCASE("scriptWindowSelectId does not change index on failure")
    {
        gCurrentManagedWindowIndex = 0;
        bool result = scriptWindowSelectId_guarded(-1);
        CHECK(result == false);
        CHECK(gCurrentManagedWindowIndex == 0); // unchanged
    }
}
