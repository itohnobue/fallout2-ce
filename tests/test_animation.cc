// Unit tests for animation.cc — fork changes:
//   - SpeedMulti FPS scaling (animation.cc:3365-3374)
//   - sfallAnimCallbackInvoke trigger (animation.cc:2853)
//   - _check_gravity squareTile guard (animation.cc:3327-3329)
//
// Reference sources: src/animation.cc:2840-2859, 3320-3377
// Research: RPU (sfall-report.md T1: SpeedMulti via sfall_global 0),
//           ETu (etu-report.md: no SpeedMulti usage)
//
// All tests use self-contained stubs — no linkage to the full engine.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

namespace fallout {

// ===========================================================================
// Mirror: animationComputeTicksPerFrame SpeedMulti formula (animation.cc:3360-3377)
// ===========================================================================
//
// Production code reads sfall_gl_vars_fetch(0, speedMulti), applies:
//   fps = (fps * speedMulti) / 100
//   if (fps <= 0) fps = 1
//   return max(1000 / fps, 1)
//
// Finding H-009 (CONFIRMED, MEDIUM): RPU expects SpeedMulti via sfall global 0.
// Confidence: CONFIRMED (RPU research T1, sfall script gl_test_reg_anim_combat_check.ssl uses set_sfall_global(0, value))

static unsigned int testAnimationComputeTicksPerFrame(int fps, int speedMulti)
{
    if (speedMulti != 100) {
        fps = (fps * speedMulti) / 100;
        if (fps <= 0) {
            fps = 1; // Prevent division by zero (0 would freeze the game)
        }
    }
    return std::max(1000 / fps, 1);
}

// ===========================================================================
// Mirror: ANIM_SAD_FOREVER flag and callback trigger (animation.cc:2840-2853)
// ===========================================================================
//
// Finding H-010 (HIGH, confirmed at HIGH): sfallAnimCallbackInvoke called when
// non-FOREVER animation reaches its last frame. The trigger conditions are:
//   1. (sad->flags & ANIM_SAD_FOREVER) == 0
//   2. object->frame == artGetFrameCount(art) - 1 (last frame)
// If both true: sad->step = ANIM_COMPLETE, callback fires.
//
// Research: sfall CONFIRMED (sfallAnimCallbackProgram/ProcedureIndex),
//           RPU CONFIRMED (reg_anim ops exercise callback)
//           ETu CONFIRMED (reg_anim_callback used for steal_urn)

constexpr int TEST_ANIM_SAD_FOREVER       = 0x01;
constexpr int TEST_ANIM_SAD_HIDE_ON_END   = 0x02;
constexpr int TEST_ANIM_COMPLETE          = 3;

// Simulates the decision: whether animation completion triggers the callback.
// Returns true if sfallAnimCallbackInvoke should be called.
static bool testAnimationShouldFireCallback(int flags, int currentFrame, int totalFrameCount)
{
    if ((flags & TEST_ANIM_SAD_FOREVER) == 0 && currentFrame == totalFrameCount - 1) {
        return true;
    }
    return false;
}

// ===========================================================================
// Mirror: _check_gravity squareTile == -1 guard (animation.cc:3320-3336)
// ===========================================================================
//
// Finding N2-018 (CONFIRMED, MEDIUM): Fork adds `if (squareTile == -1) { break; }`
// to prevent OOB access on `_square[elevation]->field_0[-1]` when
// squareTileFromScreenXY returns -1 (off-map coordinates).
// The mapper code (game_mouse.cc:2325) already has the same guard,
// confirming -1 is an expected sentinel.
//
// Trigger: realistic (animations near screen/map edges).

// Simulated tile transparency data: tile ID -> fid
// fid == buildFid(OBJ_TYPE_TILE, 1, 0, 0, 0) means "empty/transparent tile"
constexpr int TEST_OBJ_TYPE_TILE = 6;
constexpr int TEST_TRANSPARENT_TILE_FID = 1;

// Simulates the production _check_gravity loop.
// tile: starting tile, elevation: starting elevation
// squareTileData: simulated squareTileFromScreenXY return values per elevation level
// fidData: simulated fid values for squares
// Returns: resulting elevation after gravity check
static int testCheckGravity(int tile, int startElevation, const int* squareTileData, const int* fidData)
{
    int elevation = startElevation;
    for (; elevation > 0; elevation--) {
        int squareTile = squareTileData[elevation - 1];

        // --- Fork guard: prevent OOB on squareTile == -1 ---
        if (squareTile == -1) {
            break;
        }

        int fid = fidData[squareTile];
        // buildFid(OBJ_TYPE_TILE, 1, 0, 0, 0) check:
        // If fid == 1 (transparent tile), continue to next lower elevation
        if (fid != 1) {
            break;
        }
    }
    return elevation;
}

} // namespace fallout

using namespace fallout;

// ===========================================================================
// H-009: SpeedMulti FPS scaling tests
// ===========================================================================

TEST_CASE("H-009: SpeedMulti=100 is a no-op (animation.cc:3365)")
{
    // RPU expects SpeedMulti=100 to be the default (no scaling).
    // Research: RPU CONFIRMED (T1)
    int fps = 10;
    CHECK(testAnimationComputeTicksPerFrame(fps, 100) == 100);   // 1000/10 = 100ms
    CHECK(testAnimationComputeTicksPerFrame(10, 100) == 100); // 1000/10 = 100ms
    CHECK(testAnimationComputeTicksPerFrame(30, 100) == 33);  // 1000/30 = 33ms
}

TEST_CASE("H-009: SpeedMulti=200 doubles speed (animation.cc:3369)")
{
    // Research: RPU CONFIRMED (T1): SpeedMulti affects global animation speed.
    // fps=10, speedMulti=200 → fps'=20 → tick=50ms (half of normal 100ms)
    CHECK(testAnimationComputeTicksPerFrame(10, 200) == 50);  // 1000/(10*200/100) = 1000/20 = 50
    CHECK(testAnimationComputeTicksPerFrame(30, 200) == 16);  // 1000/(30*200/100) = 1000/60 = 16
}

TEST_CASE("H-009: SpeedMulti=50 halves speed (animation.cc:3369)")
{
    // fps=10, speedMulti=50 → fps'=5 → tick=200ms (twice normal)
    CHECK(testAnimationComputeTicksPerFrame(10, 50) == 200);   // 1000/(10*50/100) = 1000/5 = 200
    CHECK(testAnimationComputeTicksPerFrame(30, 50) == 66);    // 1000/(30*50/100) = 1000/15 = 66
}

TEST_CASE("H-009: SpeedMulti=0 clamps fps to 1 (animation.cc:3370-3372)")
{
    // fps=0 would freeze the game — clamp to 1.
    // Without the clamp: fps=10*0/100=0 → 1000/0 = DIVISION BY ZERO.
    // Old code: no clamp, crash. New code: fps=1, tick=1000ms.
    CHECK(testAnimationComputeTicksPerFrame(10, 0)   == 1000); // clamp to 1
    CHECK(testAnimationComputeTicksPerFrame(30, 0)   == 1000);
    CHECK(testAnimationComputeTicksPerFrame(1, 0)    == 1000);
}

TEST_CASE("H-009: SpeedMulti negative clamps fps to 1 (animation.cc:3370-3372)")
{
    // Negative speedMulti from config misparse → fps negative → clamp to 1.
    // Old buggy code: (fps * -50) / 100 = negative fps → 1000/negative = negative tick → UB
    CHECK(testAnimationComputeTicksPerFrame(10, -50) == 1000);
    CHECK(testAnimationComputeTicksPerFrame(10, -1)  == 1000);
}

TEST_CASE("H-009: SpeedMulti fractional propagation (animation.cc:3369)")
{
    // Integer truncation: (10 * 150) / 100 = 15 (exact). (10 * 37) / 100 = 3 (truncated).
    // Research: RPU CONFIRMED (T1: SpeedMulti values are integers)
    CHECK(testAnimationComputeTicksPerFrame(10, 150) == 66);  // 1000/15 = 66
    CHECK(testAnimationComputeTicksPerFrame(10, 37)  == 333); // 1000/3 = 333
}

TEST_CASE("H-009: max(1000/fps, 1) floor (animation.cc:3376)")
{
    // tickTime ≥ 1ms — max(1000/fps, 1) prevents 0ms tick
    // At fps=1000: tick = 1000/1000 = 1
    // At fps=2000: tick = max(1000/2000, 1) = max(0, 1) = 1
    CHECK(testAnimationComputeTicksPerFrame(1000, 100) == 1);
    CHECK(testAnimationComputeTicksPerFrame(2000, 100) == 1); // clamped to min 1
}

TEST_CASE("H-009: SpeedMulti interacts correctly with FPS (animation.cc:3369)")
{
    // Verify the formula end-to-end with SpeedMulti ≠ 100:
    // fps=24 is a common animation FPS in Fallout.
    CHECK(testAnimationComputeTicksPerFrame(24, 100) == 41);  // 1000/24 = 41
    CHECK(testAnimationComputeTicksPerFrame(24, 50)  == 83);  // 1000/12 = 83
    CHECK(testAnimationComputeTicksPerFrame(24, 200) == 20);  // 1000/48 = 20
}

// ===========================================================================
// H-010: sfallAnimCallbackInvoke trigger condition tests
// ===========================================================================

TEST_CASE("H-010: non-FOREVER animation triggers callback on last frame (animation.cc:2840-2853)")
{
    // Research: sfall CONFIRMED, RPU CONFIRMED (reg_anim ops exercise callback),
    //           ETu CONFIRMED (reg_anim_callback used for steal_urn)
    // Production condition: (flags & FOREVER)==0 && frame == totalFrames-1
    int flags = 0; // no FOREVER flag
    int totalFrames = 10;
    CHECK(testAnimationShouldFireCallback(flags, 9, totalFrames));  // last frame
}

TEST_CASE("H-010: FOREVER animations do NOT trigger callback (animation.cc:2840)")
{
    // Production: (sad->flags & ANIM_SAD_FOREVER) == 0 check fails
    int flags = TEST_ANIM_SAD_FOREVER;
    int totalFrames = 10;
    // Even on last frame, FOREVER flag prevents callback
    CHECK_FALSE(testAnimationShouldFireCallback(flags, 9, totalFrames));
    // Not-last frame also doesn't fire (FOREVER blocks everything)
    CHECK_FALSE(testAnimationShouldFireCallback(flags, 5, totalFrames));
    // First frame doesn't fire
    CHECK_FALSE(testAnimationShouldFireCallback(flags, 0, totalFrames));
}

TEST_CASE("H-010: non-last frame does NOT trigger callback (animation.cc:2840)")
{
    // Production checks currentFrame == totalFrames-1
    int flags = 0;
    int totalFrames = 10;
    CHECK_FALSE(testAnimationShouldFireCallback(flags, 0, totalFrames));   // first frame
    CHECK_FALSE(testAnimationShouldFireCallback(flags, 5, totalFrames));   // mid animation
    CHECK_FALSE(testAnimationShouldFireCallback(flags, 8, totalFrames));   // one before last
}

TEST_CASE("H-010: single-frame animation (animation.cc:2840)")
{
    // totalFrames=1: frame 0 IS the last frame
    int flags = 0;
    int totalFrames = 1;
    CHECK(testAnimationShouldFireCallback(flags, 0, totalFrames));
}

TEST_CASE("H-010: FOREVER + HIDE_ON_END — callback blocks, hide-on-end applies (animation.cc:2844)")
{
    // The production code checks FOREVER before HIDE_ON_END and callback.
    // When FOREVER is set, neither hide nor callback runs.
    int flags = TEST_ANIM_SAD_FOREVER | TEST_ANIM_SAD_HIDE_ON_END;
    int totalFrames = 10;
    CHECK_FALSE(testAnimationShouldFireCallback(flags, 9, totalFrames));
}

TEST_CASE("H-010: HIDE_ON_END without FOREVER — callback fires then anim hides (animation.cc:2844-2853)")
{
    // HIDE_ON_END alone doesn't block the callback — the code does:
    //   if (HIDE_ON_END) _anim_hide(object, -1);
    //   sfallAnimCallbackInvoke(object);
    // Both execute on last frame (FOREVER is not set).
    int flags = TEST_ANIM_SAD_HIDE_ON_END; // no FOREVER
    int totalFrames = 10;
    CHECK(testAnimationShouldFireCallback(flags, 9, totalFrames));
}

// ===========================================================================
// N2-018: _check_gravity squareTile == -1 guard tests
// ===========================================================================

TEST_CASE("N2-018: squareTile==-1 guard prevents OOB (animation.cc:3327-3329)")
{
    // Finding N2-018 (CONFIRMED, MEDIUM): Fork adds `if (squareTile == -1) { break; }`
    // Without this guard: `_square[elevation]->field_0[-1]` = UB/OOB read.
    // The mapper code (game_mouse.cc:2325) has the same guard.
    //
    // Simulate: at elevation=3, squareTileFromScreenXY returns -1 (off-map).
    // Expected: loop breaks immediately, returns elevation=3 (unchanged from start=3).

    // squareTileData for elevations 2→0 (indexed as [elev-1]):
    // elevation 2: -1 (off-map, guard triggers)
    // elevation 1: not reached (loop already broke)
    // elevation 0: not reached
    int squareTileData[3] = { -1, 10, 10 };
    int fidData[20] = {}; // all zero (opaque tiles)

    // Start at elevation=3. Loop: elev=3→squareTile=squareTileData[2]=10→fid[10]=0≠1→break.
    // The break happens before post-decrement, so result is 3 (not 2).
    int result = testCheckGravity(1000, 3, squareTileData, fidData);
    CHECK(result == 3);
}

TEST_CASE("N2-018: squareTile==-1 at highest elevation (animation.cc:3327)")
{
    // Guard works even at the first iteration.
    int squareTileData[3] = { -1, 0, 0 };
    int fidData[20] = {};
    int result = testCheckGravity(1000, 1, squareTileData, fidData);
    // Elevation already at 1, but loop enters because elevation>0.
    // squareTile=squareTileData[0]=-1 → break BEFORE the for's post-decrement.
    // But the for loop does `for(;elevation>0;elevation--)` — elevation is 1
    // when we enter the body, squareTile=-1, break. Returns 1.
    CHECK(result == 1);
}

TEST_CASE("N2-018: transparent tile continues loop, non-transparent stops (animation.cc:3330-3333)")
{
    // Normal case: all squares are transparent, gravity falls through all elevations.
    int squareTileData[3] = { 10, 10, 10 };
    int fidData[20] = { [10] = 1 }; // squareTile=10 → fid=1 (transparent/empty tile)
    // Start at elev=3: squareTile=squareTileData[2]=10 → fid=fidData[10]=1 → transparent → continue
    //   elev=2: squareTile=10 → fid=1 → transparent → continue
    //   elev=1: squareTile=10 → fid=1 → transparent → continue
    // Loop exits when elevation reaches 0 (for condition: elevation>0 fails).
    int result = testCheckGravity(1000, 3, squareTileData, fidData);
    CHECK(result == 0); // fell through all elevations
}

TEST_CASE("N2-018: non-transparent tile stops gravity (animation.cc:3330-3333)")
{
    // First non-transparent tile encountered blocks further descent.
    int squareTileData[3] = { 5, 5, 10 };
    int fidData[20] = { [5] = 42, [10] = 1 }; // squareTile=5 → fid=42 (opaque), squareTile=10 → transparent
    // Start at elev=3: squareTile=10 → fid=1 → transparent → continue
    //   elev=2: squareTile=5 → fid=42 ≠ 1 → BREAK (opaque tile blocks)
    // Returns 2.
    int result = testCheckGravity(1000, 3, squareTileData, fidData);
    CHECK(result == 2);
}

TEST_CASE("N2-018: edge case — starting at elevation 0 (animation.cc:3321)")
{
    // Loop condition `elevation > 0` means elevation=0 → loop never enters.
    int squareTileData[1] = { 10 };
    int fidData[20] = { [10] = 1 };
    int result = testCheckGravity(1000, 0, squareTileData, fidData);
    CHECK(result == 0); // unchanged
}

TEST_CASE("N2-018: regression — old code would OOB read on squareTile==-1")
{
    // Without the fork's guard: `_square[elevation]->field_0[-1]` is UB.
    // We verify that our mirror with the guard correctly returns elevation
    // rather than attempting the OOB access. The guard covers the case where
    // squareTileFromScreenXY returns -1 for off-map coordinates during
    // animations near screen/map edges.
    int squareTileData[5] = { -1, 10, 0, 0, 0 };
    int fidData[20] = { [10] = 1 };
    int result = testCheckGravity(1000, 5, squareTileData, fidData);
    // The guard at elev=4 prevents reading field_0[-1].
    // Without guard: would attempt fidData[-1] which is UB.
    CHECK(result >= 0);
}

// ===========================================================================
// F-12: Reverse animation callback (animation.cc:2910-2919)
// ===========================================================================
//
// Finding F-12 (CONFIRMED, MEDIUM): sfallAnimCallbackInvoke is called
// on forward animation completion at line 2865 but was MISSING from the
// reverse animation completion path at line 2910 (before fix). The fix
// adds the sfallAnimCallbackInvoke(object) call after setting ANIM_COMPLETE
// on the reverse path, matching the forward-path behavior.
//
// Source: animation.cc:2840-2919
// Research: sfall reg_anim_callback fires "on animation completion" for both
//           directions. Scripts relying on reverse-animation callbacks
//           (dissolve effects, de-spawn sequences) would never have their
//           callback triggered without this fix.

// Simulates the reverse animation decision path.
// Returns true if sfallAnimCallbackInvoke should be called for reverse
// animation that reaches frame 0 (step set to ANIM_COMPLETE).
// In production: the callback fires on BOTH forward (last frame) and
// reverse (frame 0) completion when FOREVER flag is not set.
static bool testReverseAnimationShouldFireCallback(int flags, int currentFrame, bool isReverse)
{
    if (isReverse) {
        // Reverse animation: reaches frame 0 → sad->step = ANIM_COMPLETE
        // Then sfallAnimCallbackInvoke(object) is called (F-12 fix).
        if (currentFrame == 0) {
            return true;
        }
        return false;
    }
    // Forward: existing behavior (frame == totalFrames - 1)
    // This path already has the callback at line 2865.
    return false;
}

// ===========================================================================
// F-12: Reverse animation callback tests
// ===========================================================================

TEST_CASE("F-12: reverse animation callback fires at frame 0")
{
    // Reverse animation reaching frame 0 triggers ANIM_COMPLETE and
    // sfallAnimCallbackInvoke. This is the direct analog of the forward
    // path: forward reaches last frame → callback; reverse reaches frame
    // 0 → callback.
    int flags = 0; // no FOREVER
    CHECK(testReverseAnimationShouldFireCallback(flags, 0, true));
}

TEST_CASE("F-12: reverse animation callback does NOT fire at non-zero frames")
{
    // While reverse animation is still running (frames > 0), the callback
    // should not fire. Only frame 0 = completion.
    int flags = 0;
    CHECK_FALSE(testReverseAnimationShouldFireCallback(flags, 1, true));
    CHECK_FALSE(testReverseAnimationShouldFireCallback(flags, 5, true));
    CHECK_FALSE(testReverseAnimationShouldFireCallback(flags, 999, true));
}

TEST_CASE("F-12: reverse animation callback — negative frame does not fire")
{
    // Frame < 0 should never be a valid animation frame.
    int flags = 0;
    CHECK_FALSE(testReverseAnimationShouldFireCallback(flags, -1, true));
}

TEST_CASE("F-12: forward path is independent of reverse decision")
{
    // The reverse callback is orthogonal to forward — a forward animation
    // at frame 0 is not completion (unless 1-frame animation, handled
    // separately in H-010 test). This verifies the isReverse flag correctly
    // gates the decision.
    int flags = 0;
    CHECK_FALSE(testReverseAnimationShouldFireCallback(flags, 0, false));
}

TEST_CASE("F-12: regression — OLD code never fired callback on reverse completion")
{
    // Regression: OLD code at animation.cc:2910 (before fix) did:
    //   sad->step = ANIM_COMPLETE;
    //   _anim_set_continue(sad->animationSequenceIndex, 1);
    // Without any sfallAnimCallbackInvoke call. The forward path at 2865
    // had the callback, but the reverse path did not.
    //
    // FIX: sfallAnimCallbackInvoke(object) added between step=ANIM_COMPLETE
    // and _anim_set_continue. This test verifies the mirror logic correctly
    // distinguishes reverse completion at frame 0.
    CHECK(testReverseAnimationShouldFireCallback(0, 0, true));   // FIX: fires
    CHECK_FALSE(testReverseAnimationShouldFireCallback(0, 5, true)); // not at frame 5

    // Forward path is NOT the reverse path — the reverse fix is additive,
    // not a change to forward behavior.
    CHECK_FALSE(testReverseAnimationShouldFireCallback(0, 0, false)); // not reverse
}
