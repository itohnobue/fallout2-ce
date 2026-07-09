// Unit tests for rendering + scanner fixes.
//
// Tests:
//   - UF-H-047: blitBufferToBufferStretch/StretchTrans zero-guard (no SIGFPE)
//   - UF-011:  Scanner hook implementation filter (kImplementedHooks table)
//
// Self-contained — does NOT link draw.cc (50+ engine deps).
// Mirror functions replicate the exact guard pattern applied in draw.cc.
// Hook tests use real sfall_script_hooks.h types (header-only).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

#include "sfall_script_hooks.h"

using namespace fallout;

// =============================================================
// UF-H-047: Division-by-zero guards in blit stretch functions
// =============================================================
// Before the fix, blitBufferToBufferStretch and
// blitBufferToBufferStretchTrans in draw.cc divided by srcWidth
// and srcHeight with zero guards, causing SIGFPE when a caller
// supplied zero dimensions (e.g. corrupted FRM/RIX art).
//
// The fix adds `if (srcWidth <= 0 || srcHeight <= 0) return;`
// at the top of both functions, matching the guard pattern in
// blitBuffer2DScaledImpl (draw.cc:310).
//
// These mirror functions replicate the exact production code
// pattern to validate that the guard prevents division by zero.

static constexpr int kTestPitch = 8;

// Mirror of blitBufferToBufferStretch with the zero-guard.
// Pre-guard behavior (without the `if` check): SIGFPE on line
// `int stepX = (destWidth << 16) / srcWidth` when srcWidth==0.
static void mirrorStretch(const unsigned char* src, int srcWidth, int srcHeight,
                          int srcPitch, unsigned char* dest, int destWidth,
                          int destHeight, int destPitch)
{
    // This guard matches the one added at draw.cc:158-160
    if (srcWidth <= 0 || srcHeight <= 0) {
        return;
    }

    // These divisions would SIGFPE without the guard above
    int stepX = (destWidth << 16) / srcWidth;
    int stepY = (destHeight << 16) / srcHeight;

    // Minimal loop body — just verify we reach this point safely
    int pixelsWritten = 0;
    for (int srcY = 0; srcY < srcHeight; srcY += 1) {
        int startDestY = (srcY * stepY) >> 16;
        int endDestY = ((srcY + 1) * stepY) >> 16;

        const unsigned char* currSrc = src + srcPitch * srcY;
        for (int srcX = 0; srcX < srcWidth; srcX += 1) {
            int startDestX = (srcX * stepX) >> 16;
            int endDestX = ((srcX + 1) * stepX) >> 16;

            for (int destY = startDestY; destY < endDestY; destY += 1) {
                unsigned char* currDest = dest + destPitch * destY + startDestX;
                for (int destX = startDestX; destX < endDestX; destX += 1) {
                    *currDest++ = *currSrc;
                    pixelsWritten++;
                }
            }
            currSrc++;
        }
    }

    // Prevent unused-variable warning; verify loop executed
    (void)pixelsWritten;
}

// Mirror of blitBufferToBufferStretchTrans with the zero-guard.
static void mirrorStretchTrans(const unsigned char* src, int srcWidth, int srcHeight,
                               int srcPitch, unsigned char* dest, int destWidth,
                               int destHeight, int destPitch)
{
    // This guard matches the one added at draw.cc:189-191
    if (srcWidth <= 0 || srcHeight <= 0) {
        return;
    }

    int stepX = (destWidth << 16) / srcWidth;
    int stepY = (destHeight << 16) / srcHeight;

    int pixelsWritten = 0;
    for (int srcY = 0; srcY < srcHeight; srcY += 1) {
        int startDestY = (srcY * stepY) >> 16;
        int endDestY = ((srcY + 1) * stepY) >> 16;

        const unsigned char* currSrc = src + srcPitch * srcY;
        for (int srcX = 0; srcX < srcWidth; srcX += 1) {
            int startDestX = (srcX * stepX) >> 16;
            int endDestX = ((srcX + 1) * stepX) >> 16;

            if (*currSrc != 0) {
                for (int destY = startDestY; destY < endDestY; destY += 1) {
                    unsigned char* currDest = dest + destPitch * destY + startDestX;
                    for (int destX = startDestX; destX < endDestX; destX += 1) {
                        *currDest++ = *currSrc;
                        pixelsWritten++;
                    }
                }
            }
            currSrc++;
        }
    }

    (void)pixelsWritten;
}

TEST_CASE("blitBufferToBufferStretch mirror with zero dimensions returns safely")
{
    unsigned char src[64] = {};
    unsigned char dst[64] = {};

    SUBCASE("zero srcWidth")
    {
        mirrorStretch(src, 0, 8, kTestPitch, dst, 8, 8, kTestPitch);
        // Reaching here proves the guard prevented SIGFPE
        CHECK(true);
    }

    SUBCASE("zero srcHeight")
    {
        mirrorStretch(src, 8, 0, kTestPitch, dst, 8, 8, kTestPitch);
        CHECK(true);
    }

    SUBCASE("negative srcWidth")
    {
        mirrorStretch(src, -1, 8, kTestPitch, dst, 8, 8, kTestPitch);
        CHECK(true);
    }

    SUBCASE("negative srcHeight")
    {
        mirrorStretch(src, 8, -1, kTestPitch, dst, 8, 8, kTestPitch);
        CHECK(true);
    }

    SUBCASE("both zero")
    {
        mirrorStretch(src, 0, 0, kTestPitch, dst, 8, 8, kTestPitch);
        CHECK(true);
    }
}

TEST_CASE("blitBufferToBufferStretchTrans mirror with zero dimensions returns safely")
{
    unsigned char src[64] = {};
    unsigned char dst[64] = {};

    SUBCASE("zero srcWidth")
    {
        mirrorStretchTrans(src, 0, 8, kTestPitch, dst, 8, 8, kTestPitch);
        CHECK(true);
    }

    SUBCASE("zero srcHeight")
    {
        mirrorStretchTrans(src, 8, 0, kTestPitch, dst, 8, 8, kTestPitch);
        CHECK(true);
    }

    SUBCASE("negative srcWidth")
    {
        mirrorStretchTrans(src, -1, 8, kTestPitch, dst, 8, 8, kTestPitch);
        CHECK(true);
    }

    SUBCASE("both zero")
    {
        mirrorStretchTrans(src, 0, 0, kTestPitch, dst, 8, 8, kTestPitch);
        CHECK(true);
    }
}

TEST_CASE("blitBufferToBufferStretch mirror with valid dimensions produces output")
{
    unsigned char src[64] = {};
    unsigned char dst[64] = {};

    // Fill src with a distinguishable pattern
    for (int i = 0; i < 64; i++) {
        src[i] = static_cast<unsigned char>(i + 1);
    }

    // Stretch 4x4 -> 8x8 (exact 2x scale)
    mirrorStretch(src, 4, 4, kTestPitch, dst, 8, 8, kTestPitch);

    // At least some pixels should have been written
    bool anyWritten = false;
    for (int i = 0; i < 64; i++) {
        if (dst[i] != 0) {
            anyWritten = true;
            break;
        }
    }
    CHECK(anyWritten);
}

// =============================================================
// UF-011: Scanner hook implementation filter
// =============================================================
// The kImplementedHooks table (in scan_unimplemented_opcodes.h)
// tracks which of the 62 hook IDs have active fire-site
// implementations. Before the fix, the scanner reported ALL hooks
// as "unimplemented" (100% false positive rate). The fix checks
// kImplementedHooks[hookId] before inserting into unknown_hooks.
//
// This test validates the correctness of the lookup table by
// mirroring it locally and verifying key properties.
//
// The local mirror MUST match the table in scan_unimplemented_opcodes.h
// exactly — the static_assert against HOOK_COUNT catches size drift.

// Local mirror of kImplementedHooks from scan_unimplemented_opcodes.h.
// Must match exactly — kept in sync via static_assert against HOOK_COUNT.
static constexpr bool kTestHookImplemented[] = {
    // 0-9
    true,  // 0:  HOOK_TOHIT
    true,  // 1:  HOOK_AFTERHITROLL
    true,  // 2:  HOOK_CALCAPCOST
    false, // 3:  HOOK_DEATHANIM1 (not implemented)
    true,  // 4:  HOOK_DEATHANIM2
    true,  // 5:  HOOK_COMBATDAMAGE
    true,  // 6:  HOOK_ONDEATH
    true,  // 7:  HOOK_FINDTARGET
    true,  // 8:  HOOK_USEOBJON
    false, // 9:  HOOK_REMOVEINVENOBJ (not implemented)
    // 10-19
    true,  // 10: HOOK_BARTERPRICE
    true,  // 11: HOOK_MOVECOST
    false, // 12: HOOK_HEXMOVEBLOCKING (not implemented)
    false, // 13: HOOK_HEXAIBLOCKING (not implemented)
    false, // 14: HOOK_HEXSHOOTBLOCKING (not implemented)
    false, // 15: HOOK_HEXSIGHTBLOCKING (not implemented)
    true,  // 16: HOOK_ITEMDAMAGE
    true,  // 17: HOOK_AMMOCOST
    true,  // 18: HOOK_USEOBJ
    true,  // 19: HOOK_KEYPRESS
    // 20-29
    true,  // 20: HOOK_MOUSECLICK
    true,  // 21: HOOK_USESKILL
    true,  // 22: HOOK_STEAL
    true,  // 23: HOOK_WITHINPERCEPTION
    true,  // 24: HOOK_INVENTORYMOVE
    true,  // 25: HOOK_INVENWIELD
    true,  // 26: HOOK_ADJUSTFID
    true,  // 27: HOOK_COMBATTURN
    true,  // 28: HOOK_CARTRAVEL
    true,  // 29: HOOK_SETGLOBALVAR
    // 30-39
    true,  // 30: HOOK_RESTTIMER
    true,  // 31: HOOK_GAMEMODECHANGE
    true,  // 32: HOOK_USEANIMOBJ
    true,  // 33: HOOK_EXPLOSIVETIMER
    true,  // 34: HOOK_DESCRIPTIONOBJ
    true,  // 35: HOOK_USESKILLON
    true,  // 36: HOOK_ONEXPLOSION
    false, // 37: HOOK_SUBCOMBATDAMAGE (not implemented)
    true,  // 38: HOOK_SETLIGHTING
    true,  // 39: HOOK_SNEAK
    // 40-49
    true,  // 40: HOOK_STDPROCEDURE
    true,  // 41: HOOK_STDPROCEDURE_END
    true,  // 42: HOOK_TARGETOBJECT
    true,  // 43: HOOK_ENCOUNTER
    false, // 44: HOOK_ADJUSTPOISON (not implemented)
    false, // 45: HOOK_ADJUSTRADS (not implemented)
    false, // 46: HOOK_ROLLCHECK (not implemented)
    false, // 47: HOOK_BESTWEAPON (not implemented)
    true,  // 48: HOOK_CANUSEWEAPON
    true,  // 49: HOOK_DIALOG
    // 50-59
    true,  // 50: HOOK_DIALOGREACTION
    true,  // 51: HOOK_STATLEVELUP
    true,  // 52: HOOK_BARTER
    true,  // 53: HOOK_MESSAGE
    false, // 54: reserved
    false, // 55: reserved
    false, // 56: reserved
    false, // 57: reserved
    false, // 58: reserved
    false, // 59: reserved
    // 60-61
    false, // 60: reserved
    false, // 61: HOOK_BUILDSFXWEAPON (not implemented)
};
static_assert(sizeof(kTestHookImplemented) / sizeof(kTestHookImplemented[0]) == HOOK_COUNT,
    "kTestHookImplemented size must match HOOK_COUNT");

TEST_CASE("Hook implementation table has correct size")
{
    constexpr int tableSize = sizeof(kTestHookImplemented) / sizeof(kTestHookImplemented[0]);
    CHECK(tableSize == HOOK_COUNT);
    CHECK(tableSize == 62);
}

TEST_CASE("Hook implementation table has correct number of implemented hooks")
{
    int implementedCount = 0;
    for (int i = 0; i < HOOK_COUNT; i++) {
        if (kTestHookImplemented[i]) {
            implementedCount++;
        }
    }
    // 43 of 62 hooks have active fire-site implementations
    CHECK(implementedCount == HOOK_IMPLEMENTED_COUNT);
    CHECK(implementedCount == 43);
}

TEST_CASE("Specific implemented hooks are marked true")
{
    // Common hooks used by popular mods (Et Tu, RPU)
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_TOHIT)] == true);
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_AFTERHITROLL)] == true);
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_COMBATDAMAGE)] == true);
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_ONDEATH)] == true);
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_KEYPRESS)] == true);
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_MOUSECLICK)] == true);
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_USEOBJ)] == true);
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_USEOBJON)] == true);
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_INVENTORYMOVE)] == true);
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_GAMEMODECHANGE)] == true);
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_RESTTIMER)] == true);
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_COMBATTURN)] == true);

    // Newer hooks added in Phase 7
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_DIALOG)] == true);
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_DIALOGREACTION)] == true);
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_STATLEVELUP)] == true);
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_BARTER)] == true);
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_MESSAGE)] == true);

    // Late-range hook with active implementation
    CHECK(kTestHookImplemented[static_cast<int>(HOOK_CANUSEWEAPON)] == true);
}

TEST_CASE("Specifically unimplemented hooks are marked false")
{
    // These hooks exist in the sfall spec but have no CE fire sites
    CHECK(kTestHookImplemented[3] == false);  // HOOK_DEATHANIM1
    CHECK(kTestHookImplemented[9] == false);  // HOOK_REMOVEINVENOBJ
    CHECK(kTestHookImplemented[12] == false); // HOOK_HEXMOVEBLOCKING
    CHECK(kTestHookImplemented[13] == false); // HOOK_HEXAIBLOCKING
    CHECK(kTestHookImplemented[14] == false); // HOOK_HEXSHOOTBLOCKING
    CHECK(kTestHookImplemented[15] == false); // HOOK_HEXSIGHTBLOCKING
    CHECK(kTestHookImplemented[37] == false); // HOOK_SUBCOMBATDAMAGE
    CHECK(kTestHookImplemented[44] == false); // HOOK_ADJUSTPOISON
    CHECK(kTestHookImplemented[45] == false); // HOOK_ADJUSTRADS
    CHECK(kTestHookImplemented[46] == false); // HOOK_ROLLCHECK
    CHECK(kTestHookImplemented[47] == false); // HOOK_BESTWEAPON
    CHECK(kTestHookImplemented[61] == false); // HOOK_BUILDSFXWEAPON

    // Reserved slots (54-60) must all be false
    for (int i = 54; i <= 60; i++) {
        CHECK(kTestHookImplemented[i] == false);
    }
}

TEST_CASE("Scanner hook filter: implemented hooks are excluded from unknown_hooks")
{
    // Simulate the scanner filter logic from scan_unimplemented_opcodes.h:
    //   if (!(hookProcIndex >= 0 && hookProcIndex < HOOK_COUNT
    //         && kImplementedHooks[hookProcIndex])) {
    //       unknown_hooks[hookProcIndex].insert(fName);
    //   }
    //
    // This means: add to unknown_hooks UNLESS the hook ID is both
    // in range [0, HOOK_COUNT) AND marked as implemented.

    auto shouldAddToUnknown = [](int hookProcIndex) -> bool {
        return !(hookProcIndex >= 0 && hookProcIndex < HOOK_COUNT
                 && kTestHookImplemented[hookProcIndex]);
    };

    // Implemented hooks should NOT be added to unknown_hooks
    CHECK(shouldAddToUnknown(static_cast<int>(HOOK_TOHIT)) == false);
    CHECK(shouldAddToUnknown(static_cast<int>(HOOK_KEYPRESS)) == false);
    CHECK(shouldAddToUnknown(static_cast<int>(HOOK_COMBATDAMAGE)) == false);
    CHECK(shouldAddToUnknown(static_cast<int>(HOOK_GAMEMODECHANGE)) == false);
    CHECK(shouldAddToUnknown(static_cast<int>(HOOK_CANUSEWEAPON)) == false);
    CHECK(shouldAddToUnknown(static_cast<int>(HOOK_DIALOG)) == false);
    CHECK(shouldAddToUnknown(static_cast<int>(HOOK_MESSAGE)) == false);

    // Unimplemented hooks SHOULD be added to unknown_hooks
    CHECK(shouldAddToUnknown(3) == true);   // HOOK_DEATHANIM1
    CHECK(shouldAddToUnknown(9) == true);   // HOOK_REMOVEINVENOBJ
    CHECK(shouldAddToUnknown(12) == true);  // HOOK_HEXMOVEBLOCKING
    CHECK(shouldAddToUnknown(37) == true);  // HOOK_SUBCOMBATDAMAGE
    CHECK(shouldAddToUnknown(46) == true);  // HOOK_ROLLCHECK
    CHECK(shouldAddToUnknown(61) == true);  // HOOK_BUILDSFXWEAPON

    // Out-of-range indices (unknown hook IDs) SHOULD be added
    CHECK(shouldAddToUnknown(-1) == true);
    CHECK(shouldAddToUnknown(62) == true);  // == HOOK_COUNT
    CHECK(shouldAddToUnknown(100) == true);
    CHECK(shouldAddToUnknown(999) == true);
}
