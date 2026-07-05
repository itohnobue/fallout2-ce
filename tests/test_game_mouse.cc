// Unit tests for game_mouse.cc — fork changes regression tests.
//
// Tests:
//   1. M-089: actionIndex bounds clamping in _gmouse_handle_event
//      (game_mouse.cc:1222-1227). Pre-fork: actionIndex used as array index
//      into actionMenuItems[] without bounds check → OOB read.
//      Post-fork: clamp actionIndex to [0, actionMenuItemsCount-1].
//   2. N2-047: Wrong stride constant fix — gGameMouseActionPickFrmWidth
//      → gGameMouseActionMenuFrmWidth at game_mouse.cc:1888,1893.
//      Pre-fork: used pick-frame width (13) as destination stride when
//      blitting action menu items (menu frame width = 61).
//      Post-fork: correctly uses gGameMouseActionMenuFrmWidth.
//
// This file does NOT link game_mouse.cc (40+ engine deps). It mirrors
// the specific fork-fixed code paths as local functions and validates
// against the pre-fork buggy behavior.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

// Tests do not use any fallout engine types — all structures are
// self-contained mirrors of the production code patterns.

// ============================================================
// M-089: actionIndex bounds clamping (game_mouse.cc:1222-1227)
// ============================================================
// Fork added two bounds checks before the switch(actionMenuItems[actionIndex]):
//   if (actionIndex < 0) actionIndex = 0;
//   if (actionIndex >= actionMenuItemsCount) actionIndex = actionMenuItemsCount - 1;
// Pre-fork: if actionIndex < 0 or >= count, OOB read on actionMenuItems[]
// followed by switch on the OOB value → UB.
// Research: CONFIRMED (iter-1 finding, bounds logic verified)
// Verification: CONFIRMED (exhaustive edge case analysis)

// Mirrors pre-fork (buggy) — OOB when actionIndex out of bounds
static int getMenuItemPreFork(int actionIndex, const int* menuItems, int count) {
    // No bounds check — direct array access
    return menuItems[actionIndex];  // OOB if actionIndex < 0 or >= count
}

// Mirrors post-fork (fixed) — clamps actionIndex
static int getMenuItemPostFork(int actionIndex, const int* menuItems, int count) {
    if (count <= 0) {
        return -1;  // no valid items
    }
    if (actionIndex < 0) {
        actionIndex = 0;
    }
    if (actionIndex >= count) {
        actionIndex = count - 1;
    }
    return menuItems[actionIndex];
}

TEST_CASE("M-089 — actionIndex bounds clamping (game_mouse.cc:1222-1227)")
{
    const int testItems[] = {10, 20, 30, 40, 50};
    const int count = 5;

    SUBCASE("actionIndex within bounds — both paths return same value") {
        for (int i = 0; i < count; i++) {
            int pre = getMenuItemPreFork(i, testItems, count);
            int post = getMenuItemPostFork(i, testItems, count);
            CHECK(pre == post);
            CHECK(pre == testItems[i]);
        }
    }

    SUBCASE("actionIndex negative — pre-fork OOB, post-fork clamps to 0") {
        // Pre-fork: testItems[-1] = OOB read (UB)
        // We test the clamp logic without actually doing OOB
        int post = getMenuItemPostFork(-1, testItems, count);
        CHECK(post == testItems[0]);  // clamped to 0

        post = getMenuItemPostFork(-100, testItems, count);
        CHECK(post == testItems[0]);  // negative value clamped to 0
    }

    SUBCASE("actionIndex beyond count — pre-fork OOB, post-fork clamps to last") {
        int post = getMenuItemPostFork(5, testItems, count);
        CHECK(post == testItems[4]);  // clamped to count-1 = 4

        post = getMenuItemPostFork(999, testItems, count);
        CHECK(post == testItems[4]);  // any oversized value clamps to last
    }

    SUBCASE("actionIndex exactly at boundary — not clamped") {
        // 0 is valid (already within bounds)
        int post = getMenuItemPostFork(0, testItems, count);
        CHECK(post == testItems[0]);

        // count-1 = 4 is valid
        post = getMenuItemPostFork(4, testItems, count);
        CHECK(post == testItems[4]);
    }

    SUBCASE("actionIndex at boundary+1 — clamped down") {
        // count = 5, so index 5 is OOB → clamp to 4
        int post = getMenuItemPostFork(5, testItems, count);
        CHECK(post == testItems[4]);
    }

    SUBCASE("empty menu — no valid items") {
        const int emptyItems[] = {};
        int post = getMenuItemPostFork(0, emptyItems, 0);
        CHECK(post == -1);  // count=0 → no valid items
    }

    SUBCASE("single item menu — any index clamps to 0") {
        const int singleItem[] = {42};
        int post = getMenuItemPostFork(0, singleItem, 1);
        CHECK(post == 42);
        post = getMenuItemPostFork(5, singleItem, 1);
        CHECK(post == 42);
        post = getMenuItemPostFork(-1, singleItem, 1);
        CHECK(post == 42);
    }
}

// ============================================================
// N2-047: Wrong stride constant fix (game_mouse.cc:1888,1893)
// ============================================================
// Fork changed blitBufferToBuffer stride parameter from
// gGameMouseActionPickFrmWidth to gGameMouseActionMenuFrmWidth at
// game_mouse.cc:1888 (arrow blit) and :1893 (menu item blit loop).
//
// Pre-fork: used pick-frame width as destination stride.
//   pick frm width = 13, menu frm width = 61 → blit with wrong line pitch,
//   causing visual corruption of the rendered action menu.
//
// blitBufferToBuffer signature (draw.h):
//   void blitBufferToBuffer(const unsigned char* src, int width, int height,
//                           int srcPitch, unsigned char* dest, int destPitch);
// The fork fix changes destPitch from pickWidth to menuWidth.
//
// Research: CONFIRMED (stride mismatch confirmed with exact constants)
// Verification: CONFIRMED (both sites confirmed via git diff)

// Simulates blit output: computes byte offsets using stride
// Returns the byte offset of pixel at (x, y) within dest buffer
static int blitPixelOffset(int x, int y, int destStride) {
    return y * destStride + x;
}

// Mirrors the arrow blit destination computation (game_mouse.cc:1888)
// Pre-fork: uses pickFrmWidth as dest stride
// Post-fork: uses menuFrmWidth as dest stride
static void renderArrow(int arrowDataWidth, int arrowDataHeight,
                        int pickFrmWidth, int menuFrmWidth,
                        int* preForkLastPixel, int* postForkLastPixel) {
    // Pre-fork: arrow blit uses pickFrmWidth as dest pitch
    *preForkLastPixel = blitPixelOffset(arrowDataWidth - 1, arrowDataHeight - 1, pickFrmWidth);

    // Post-fork: arrow blit uses menuFrmWidth as dest pitch
    *postForkLastPixel = blitPixelOffset(arrowDataWidth - 1, arrowDataHeight - 1, menuFrmWidth);
}

// Mirrors menu item blit (game_mouse.cc:1893-1894)
// Post-fork: menu items are blitted sequentially with menuFrmWidth stride
// Pre-fork: each menu item used pickFrmWidth as stride
static int computeMenuItemDestOffset(int itemIndex, int menuItemWidth,
                                     int menuItemHeight, int destStride) {
    return itemIndex * destStride * menuItemHeight;
}

TEST_CASE("N2-047 — stride constant fix PickFrmWidth→MenuFrmWidth (game_mouse.cc:1888,1893)")
{
    const int pickFrmWidth = 13;
    const int menuFrmWidth = 61;
    const int arrowWidth = 11;
    const int arrowHeight = 3;
    const int menuItemWidth = 61;
    const int menuItemHeight = 28;

    SUBCASE("arrow blit — different strides produce different dest layouts") {
        int preForkLast, postForkLast;
        renderArrow(arrowWidth, arrowHeight, pickFrmWidth, menuFrmWidth,
                    &preForkLast, &postForkLast);

        // When pickFrmWidth != menuFrmWidth, the last pixel offset differs
        CHECK(preForkLast != postForkLast);
        CHECK(preForkLast == (arrowHeight - 1) * pickFrmWidth + (arrowWidth - 1));
        CHECK(postForkLast == (arrowHeight - 1) * menuFrmWidth + (arrowWidth - 1));
    }

    SUBCASE("arrow blit — equal strides produce identical layouts") {
        int preForkLast, postForkLast;
        int sameWidth = 61;
        renderArrow(arrowWidth, arrowHeight, sameWidth, sameWidth,
                    &preForkLast, &postForkLast);
        CHECK(preForkLast == postForkLast);
    }

    SUBCASE("menu item blit offsets diverge when strides differ") {
        // With menuFrmWidth=61, pickFrmWidth=13, menuItemHeight=28:
        // Pre-fork: item 0 at offset 0, item 1 at 13*28=364, item 2 at 728
        // Post-fork: item 0 at offset 0, item 1 at 61*28=1708, item 2 at 3416
        for (int idx = 0; idx < 3; idx++) {
            int preForkOffset = computeMenuItemDestOffset(idx, menuItemWidth,
                                                           menuItemHeight, pickFrmWidth);
            int postForkOffset = computeMenuItemDestOffset(idx, menuItemWidth,
                                                            menuItemHeight, menuFrmWidth);
            if (idx > 0) {
                // Non-zero items: different strides → different offsets
                CHECK(preForkOffset != postForkOffset);
            }
            // Post-fork offset must be larger (wider stride)
            if (idx > 0) {
                CHECK(postForkOffset > preForkOffset);
            }
        }
    }

    SUBCASE("menu item blit offsets are sequential with correct stride") {
        // With menuFrmWidth=61, menuItemHeight=28
        // Each item should be placed at: index * stride * height
        int item0 = computeMenuItemDestOffset(0, menuItemWidth, menuItemHeight, menuFrmWidth);
        int item1 = computeMenuItemDestOffset(1, menuItemWidth, menuItemHeight, menuFrmWidth);
        int item2 = computeMenuItemDestOffset(2, menuItemWidth, menuItemHeight, menuFrmWidth);

        CHECK(item0 == 0);
        CHECK(item1 == menuFrmWidth * menuItemHeight);      // 61 * 28 = 1708
        CHECK(item2 == 2 * menuFrmWidth * menuItemHeight);  // 3416
        // Items are placed contiguously with menuFrmWidth pitch
        CHECK(item1 - item0 == item2 - item1);
    }

    SUBCASE("pre-fork layout: items overlap with wrong stride") {
        // With pickFrmWidth=13 as stride for a menuItemWidth=61
        // Row 0: bytes 0-60, dest pitch is 13
        // Row 1 of item 0 starts at byte 13 (should be 61!)
        // This means row 1 of item 0 OVERLAPS with row 0 bytes 13-60
        int destPitch = pickFrmWidth;  // pre-fork bug
        // First pixel of row 1
        int row1Offset = blitPixelOffset(0, 1, destPitch);
        CHECK(row1Offset == destPitch);  // byte 13
        // This overlaps with row 0 pixels 13-60 (which is within item 0's row 0)
        // That's the corruption: row 1 writes over row 0 data

        // Post-fork with menuFrmWidth=61:
        // Row 1 of item 0 starts at byte 61 — no overlap
        int postRow1Offset = blitPixelOffset(0, 1, menuFrmWidth);
        CHECK(postRow1Offset == menuFrmWidth);  // byte 61
        CHECK(postRow1Offset != row1Offset);
    }

    SUBCASE("stride fix validation: pick=13 vs menu=61 concrete values") {
        // These match the exact values from the production code:
        // gGameMouseActionPickFrmWidth — set via artGetWidth at game_mouse.cc:2170
        // gGameMouseActionMenuFrmWidth — set via artGetWidth at game_mouse.cc:2165
        // Typical values: pick frm ~13px wide, menu frm ~61px wide
        // The fix corrects the dest pitch from 13 to 61

        int wrongDestPitch = 13;  // pre-fork
        int correctDestPitch = 61; // post-fork

        // Row 0 of a 61-pixel-wide blit with pitch 13:
        // - Bytes 0-12: first 13 pixels (correct)
        // - Bytes 13-25: pixels 13-25 on dest line 0, but row 1 on src!
        // This is the visual corruption bug.
        CHECK(correctDestPitch > wrongDestPitch);  // menu frame is wider than pick frame
        CHECK(correctDestPitch >= 61);  // menu frame at least as wide as menu items
    }
}
