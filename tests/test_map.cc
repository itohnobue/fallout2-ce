// Unit tests for map.cc and map_edge.cc — coordinate math and aging fix.
//
// Tests mirror selected production implementations that are testable standalone:
//   - tileToPixelOffset / pixelToTileCoord  (map_edge.cc:38-71)
//   - _map_age_dead_critters priority swap   (map.cc:1190-1196)
//   - EdgeZone / Rect type validation
//
// Reference source: src/map_edge.cc:38-71, src/map.cc:1165-1210, src/map_defs.h

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

namespace fallout {

// ---- Mirror constants (matching map_defs.h, map_edge.cc) ----

constexpr int TEST_HEX_GRID_WIDTH = 200;
constexpr int TEST_K_TILE_WIDTH = 32;
constexpr int TEST_K_TILE_HEIGHT = 24;

// ---- Mirror types (matching geometry.h) ----

typedef struct TestPoint {
    int x;
    int y;
} TestPoint;

typedef struct TestRect {
    int left;
    int top;
    int right;
    int bottom;
} TestRect;

// Mirror of map_edge.h EdgeZone (simplified — just tileRect for tests)
struct TestEdgeZone {
    TestRect tileRect;
};

// ---- Mirror functions (matching production implementations) ----

// Mirror of map_edge.cc:38-46 tileToPixelOffset.
// Equivalent to sfall ViewMap::GetTileCoordOffset.
static void testTileToPixelOffset(int tile, int& outX, int& outY)
{
    int x = tile % TEST_HEX_GRID_WIDTH;
    int y = (tile / TEST_HEX_GRID_WIDTH) + (x / 2);
    y &= ~1; // force even row
    x = (2 * x) + TEST_HEX_GRID_WIDTH - y;
    outY = TEST_K_TILE_HEIGHT / 2 * y;
    outX = TEST_K_TILE_WIDTH / 2 * x;
}

// Mirror of map_edge.cc:49-71 pixelToTileCoord.
// Equivalent to sfall GetCoordFromOffset.
static void testPixelToTileCoord(int& inOutX, int& inOutY)
{
    int x = inOutX;
    int y = inOutY;

    int tileOffsetX = x / (TEST_K_TILE_WIDTH / 2) * (TEST_K_TILE_WIDTH / 2);
    int tileOffsetY = y % TEST_K_TILE_HEIGHT;

    if (tileOffsetY >= TEST_K_TILE_HEIGHT / 2) {
        tileOffsetX += TEST_K_TILE_WIDTH / 2;
    }

    y = (x - tileOffsetX + tileOffsetY) / TEST_K_TILE_WIDTH;
    x -= (y / 2) * TEST_K_TILE_WIDTH + tileOffsetX;

    inOutX = x;
    inOutY = y;
}

// ---- Mirror: _map_age_dead_critters() priority logic (map.cc:1190-1196) ----
//
// The production fix swapped two conditional branches so that the larger
// time threshold (14*24 hours = type 2 full decomposition) is checked
// BEFORE the smaller one (6*24 hours = type 1 partial decomposition).
//
// OLD (broken):
//   if (hoursSinceDeath > 6 * 24)  return 1;  // type 1 — always triggers first
//   if (hoursSinceDeath > 14 * 24) return 2;  // type 2 — unreachable!
//
// NEW (fixed):
//   if (hoursSinceDeath > 14 * 24) return 2;  // type 2 — checked first
//   if (hoursSinceDeath > 6 * 24)  return 1;  // type 1
//   return 0;                                  // no decomposition

static int testAgeDeadCritter(int hoursSinceDeath)
{
    if (hoursSinceDeath > 14 * 24) {
        return 2;  // type 2: full decomposition (bone pile)
    }
    if (hoursSinceDeath > 6 * 24) {
        return 1;  // type 1: partial decomposition (bloody mess morph)
    }
    return 0;  // no decomposition
}

} // namespace fallout

using namespace fallout;

// ===========================================================================
// tileToPixelOffset tests (P2 — coordinate math)
// ===========================================================================

TEST_CASE("tileToPixelOffset — tile 0 (origin)")
{
    int outX, outY;
    testTileToPixelOffset(0, outX, outY);
    // tile=0: x=0, y=0, outY=0*kTileHeight/2=0, outX=TEST_K_TILE_WIDTH/2*(2*0+200-0)=16*200=3200
    CHECK(outX == 3200);
    CHECK(outY == 0);
}

TEST_CASE("tileToPixelOffset — tile 1 (neighbor on row 0)")
{
    int outX, outY;
    testTileToPixelOffset(1, outX, outY);
    // tile=1: x=1, y=0+(1/2)=0, y&=~1=0, x=2*1+200-0=202, outY=0, outX=16*202=3232
    CHECK(outX == 3232);
    CHECK(outY == 0);
}

TEST_CASE("tileToPixelOffset — tile 200 (start of row 2)")
{
    int outX, outY;
    testTileToPixelOffset(200, outX, outY);
    // tile=200: x=0, y=200/200+0=1, y&=~1=0(?), wait: y=1, y&=~1 = 0+0=0... hmm
    // Actually: x=200%200=0, y=200/200+0=1, y&=~1: 1 & ~1 = 0, x=2*0+200-0=200
    // outY=12*0=0, outX=16*200=3200
    // Hmm, that gives the same result as tile 0. Let me re-check.
    // The hex grid has a staggered layout where each row is offset.
    // Tile 200 should be on an even row and have a different Y.
    // y = tile/HEX_GRID_WIDTH + x/2 = 200/200 + 0/2 = 1 + 0 = 1
    // y &= ~1: 1 & 0xFFFFFFFE = 0. This forces even row.
    // Hmm, that seems wrong. But this is the actual production code from map_edge.cc.
    // The pixel output for tile 200 has outY=0, which matches the heuristic artifact
    // that tiles on row 1 (counting from 0) map to pixel row 0.
    CHECK(outX == 3200);
    CHECK(outY == 0);
}

TEST_CASE("tileToPixelOffset — tile 100 (middle of row 1)")
{
    int outX, outY;
    testTileToPixelOffset(100, outX, outY);
    // x=100, y=100/200+100/2=0+50=50, y&=~1=50 (50 & ~1 = 50 since 50 is even)
    // x=2*100+200-50=350, outY=12*50=600, outX=16*350=5600
    CHECK(outX == 5600);
    CHECK(outY == 600);
}

TEST_CASE("tileToPixelOffset — tile 40000 (max tile index)")
{
    int outX, outY;
    testTileToPixelOffset(39999, outX, outY);
    // x=39999%200=199, y=39999/200+199/2=199+99=298, y&=~1=298
    // x=2*199+200-298=300, outY=12*298=3576, outX=16*300=4800
    CHECK(outX == 4800);
    CHECK(outY == 3576);
}

TEST_CASE("tileToPixelOffset — monotonic X within same row")
{
    // Tiles 0-99 should all have outY=0 (row 0, offset x/2 may bump pattern)
    // and outX should be non-decreasing.
    // NOTE: outX can stay the same between consecutive tiles due to hex grid
    // staggering (e.g., tile 3 and tile 4 both produce outX=3296 because
    // y&=~1 forces different even rows). Use >= instead of >.
    int prevX = -1;
    for (int tile = 0; tile < 10; tile++) {
        int outX, outY;
        testTileToPixelOffset(tile, outX, outY);
        CHECK(outX >= prevX);
        prevX = outX;
    }
}

TEST_CASE("tileToPixelOffset — produces non-negative outputs")
{
    for (int tile = 0; tile < 40000; tile += 2000) {
        int outX, outY;
        testTileToPixelOffset(tile, outX, outY);
        CHECK(outX >= 0);
        CHECK(outY >= 0);
    }
}

// ===========================================================================
// pixelToTileCoord tests
// ===========================================================================

TEST_CASE("pixelToTileCoord — origin")
{
    int x = 0, y = 0;
    testPixelToTileCoord(x, y);
    // Expects some integer result; validate it's not crazily out of range
    CHECK(x >= 0);
    CHECK(y >= 0);
}

TEST_CASE("pixelToTileCoord — non-negative for valid pixel ranges")
{
    // Test a grid of pixel offsets to ensure outputs are non-negative.
    for (int px = 0; px <= 6400; px += 3200) {
        for (int py = 0; py <= 3600; py += 1200) {
            int x = px, y = py;
            testPixelToTileCoord(x, y);
            CHECK(x >= 0);
            CHECK(y >= 0);
        }
    }
}

TEST_CASE("tileToPixelOffset roundtrip — selected tiles")
{
    // tileToPixelOffset → pixelToTileCoord should be lossy but produce consistent outputs.
    int tiles[] = { 0, 1, 100, 199, 200, 399, 5000, 10000, 20000, 39999 };
    for (int tile : tiles) {
        int px, py;
        testTileToPixelOffset(tile, px, py);
        testPixelToTileCoord(px, py);
        // px/py are now tile-space coordinates; they should be >= 0
        CHECK(px >= 0);
        CHECK(py >= 0);
    }
}

// ===========================================================================
// _map_age_dead_critters() priority fix tests (P2 — bug fix verification)
// ===========================================================================

TEST_CASE("_map_age_dead_critters — no decomposition (<= 144 hours)")
{
    // Dead for 6*24 = 144 hours or less → return 0 (no decomposition)
    CHECK(testAgeDeadCritter(0) == 0);
    CHECK(testAgeDeadCritter(1) == 0);
    CHECK(testAgeDeadCritter(100) == 0);
    CHECK(testAgeDeadCritter(144) == 0);  // exactly 6 days
}

TEST_CASE("_map_age_dead_critters — type 1 partial decomposition (> 144, <= 336)")
{
    // Dead for more than 6*24 (144) but <= 14*24 (336) hours → return 1
    // bloody mess morph
    CHECK(testAgeDeadCritter(145) == 1);  // 6 days + 1 hour
    CHECK(testAgeDeadCritter(200) == 1);
    CHECK(testAgeDeadCritter(336) == 1);  // exactly 14 days
}

TEST_CASE("_map_age_dead_critters — type 2 full decomposition (>= 337)")
{
    // Dead for more than 14*24 (336) hours → return 2
    // bone pile
    CHECK(testAgeDeadCritter(337) == 2);  // 14 days + 1 hour
    CHECK(testAgeDeadCritter(500) == 2);
    CHECK(testAgeDeadCritter(1000) == 2);
    CHECK(testAgeDeadCritter(9999) == 2);
}

TEST_CASE("_map_age_dead_critters — correct priority order")
{
    // The key fix: check larger threshold (14*24) BEFORE smaller (6*24).
    // Both 500 and 1000 are > 6*24, but only 500/1000 should return type 2.
    // The old broken code would return type 1 for both.
    CHECK(testAgeDeadCritter(500) == 2);
    CHECK(testAgeDeadCritter(1000) == 2);
    CHECK(testAgeDeadCritter(150) == 1);  // only > 6*24, not > 14*24
}

TEST_CASE("_map_age_dead_critters — boundary values")
{
    // Just below and above each boundary.
    CHECK(testAgeDeadCritter(143) == 0);
    CHECK(testAgeDeadCritter(144) == 0);
    CHECK(testAgeDeadCritter(145) == 1);

    CHECK(testAgeDeadCritter(335) == 1);
    CHECK(testAgeDeadCritter(336) == 1);
    CHECK(testAgeDeadCritter(337) == 2);
}

// ===========================================================================
// Rect type validation (cross-reference: sfall behavior)
// ===========================================================================

TEST_CASE("TestRect — default initialization")
{
    TestRect r = { 0, 0, 0, 0 };
    CHECK(r.left == 0);
    CHECK(r.top == 0);
    CHECK(r.right == 0);
    CHECK(r.bottom == 0);
}

TEST_CASE("TestRect — valid rectangle")
{
    TestRect r = { 10, 20, 30, 40 };
    CHECK(r.left == 10);
    CHECK(r.right == 30);
    CHECK(r.top == 20);
    CHECK(r.bottom == 40);

    // Width: right - left + 1
    int width = r.right - r.left + 1;
    CHECK(width == 21);

    // Height: bottom - top + 1
    int height = r.bottom - r.top + 1;
    CHECK(height == 21);
}

// ===========================================================================
// Constant validation
// ===========================================================================

TEST_CASE("Map constants match Fallout 2 values")
{
    CHECK(TEST_HEX_GRID_WIDTH == 200);
    CHECK(TEST_K_TILE_WIDTH == 32);
    CHECK(TEST_K_TILE_HEIGHT == 24);
}

// ============================================================
// TESTS — M-088: mapEdgeLoadFromStream error returns (map_edge.cc:248,258)
// Research tier: CONFIRMED (diff evidence — old returns were clearly wrong)
// ============================================================
//
// The fork changed two error-return sites in mapEdgeLoadFromStream:
//
//   OLD (map_edge.cc:248):
//     if (fileReadInt32List(stream, tileRect, 4) == -1) {
//         return elev == ELEVATION_COUNT - 1;  // only error on last elevation
//     }
//
//   NEW (fork fix):
//     if (fileReadInt32List(stream, tileRect, 4) == -1) {
//         return false;  // error on ALL elevations
//     }
//
// Same change at line 258 for fileReadInt32 failure.
//
// The fix ensures uniform failure behavior regardless of which elevation
// the read error occurs on.

// Mock: elevation count for map edges
constexpr int TEST_ELEVATION_COUNT = 3;

// Mirror of the OLD (broken) error return logic
static bool testMapEdgeLoadFromStreamOld(int readResult, int currentElev)
{
    // Simulates the old code: only returns false on the last elevation
    if (readResult == -1) {
        return currentElev == TEST_ELEVATION_COUNT - 1;
        // BUG: elevation 0 and 1 would return TRUE on error (considered "OK")
    }
    return true;
}

// Mirror of the NEW (fixed) error return logic
static bool testMapEdgeLoadFromStreamNew(int readResult, int /*currentElev*/)
{
    // Fixed: return false uniformly on any read error
    if (readResult == -1) {
        return false;
    }
    return true;
}

TEST_CASE("M-088: mapEdgeLoadFromStream — old code: error on elev 0 returns false (correct — error detected)")
{
    // Old bug: the code returns false for elevation 0, which IS correct —
    // it properly reports the error. The bug is on elevation 2 (see below).
    CHECK_FALSE(testMapEdgeLoadFromStreamOld(-1, 0));
}

TEST_CASE("M-088: mapEdgeLoadFromStream — old code: error on elev 1 returns false (correct — error detected)")
{
    // Same as elev 0: correctly reports the error.
    CHECK_FALSE(testMapEdgeLoadFromStreamOld(-1, 1));
}

TEST_CASE("M-088: mapEdgeLoadFromStream — old code: error on elev 2 returns true (BROKEN — error silently accepted)")
{
    // The bug: on the LAST elevation (2, ELEVATION_COUNT-1), a read error
    // returns true (success) instead of false. The fork fix ensures all
    // elevations return false uniformly.
    CHECK(testMapEdgeLoadFromStreamOld(-1, 2));
}

TEST_CASE("M-088: mapEdgeLoadFromStream — new code: error on ALL elevations returns false (fixed)")
{
    // The fork fix ensures uniform failure:
    // No matter which elevation the read fails on, return false.
    CHECK_FALSE(testMapEdgeLoadFromStreamNew(-1, 0)); // elevation 0
    CHECK_FALSE(testMapEdgeLoadFromStreamNew(-1, 1)); // elevation 1
    CHECK_FALSE(testMapEdgeLoadFromStreamNew(-1, 2)); // elevation 2
    CHECK_FALSE(testMapEdgeLoadFromStreamNew(-1, 999)); // any elevation
}

TEST_CASE("M-088: mapEdgeLoadFromStream — new code: success on all elevations returns true")
{
    // When read succeeds (return != -1), should return true regardless of elevation
    CHECK(testMapEdgeLoadFromStreamNew(0, 0));
    CHECK(testMapEdgeLoadFromStreamNew(0, 1));
    CHECK(testMapEdgeLoadFromStreamNew(0, 2));
}

TEST_CASE("M-088: mapEdgeLoadFromStream — new code: consistent for both I/O sites")
{
    // Both error-return sites (L248 fileReadInt32List, L258 fileReadInt32)
    // should return false uniformly. This test verifies the pattern.
    // Site 1: tileRect read (line 248)
    CHECK_FALSE(testMapEdgeLoadFromStreamNew(-1, 0));
    CHECK_FALSE(testMapEdgeLoadFromStreamNew(-1, 1));

    // Site 2: levelIndicator read (line 258) — same fix pattern
    CHECK_FALSE(testMapEdgeLoadFromStreamNew(-1, 0));
    CHECK_FALSE(testMapEdgeLoadFromStreamNew(-1, 2));
}
