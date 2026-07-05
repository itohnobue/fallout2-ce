// Unit tests for sound.cc — fork changes regression tests.
//
// Tests:
//   1. N2-003: _preloadBuffers return value discarded in rewind path.
//      _soundRewind at sound.cc:671 calls _preloadBuffers(sound) as void
//      expression and unconditionally sets gSoundLastError = SOUND_NO_ERROR
//      at line 682. soundPlay at line 760 also discards the return.
//      Contrast: soundOpen at line 646 correctly propagates:
//        return _preloadBuffers(sound);
//      This asymmetry means read failures during rewind are silently swallowed.
//
// This file does NOT link sound.cc. It mirrors the error propagation
// patterns and validates the silent-failure asymmetry.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

// Sound error codes (mirrored from sound.h constant patterns)
#define SOUND_NO_ERROR   0
#define SOUND_NO_SOUND  -5
#define SOUND_UNKNOWN_ERROR -6

// Flags used in test (mirrored from sound constants)
#define SOUND_FLAG_LOOPING   0x01
#define SOUND_FLAG_0x100     0x100
#define SOUND_FLAG_0x80      0x80
#define SOUND_FLAG_0x200     0x200

// Simulated Sound struct with minimal fields for error propagation testing
struct TestSound {
    int soundFlags;
    int statusFlags;
    bool ioReadCalled;
    int ioReadReturn;
    int bytesReadClamped;
    bool preloadBuffersCalled;
    int preloadBuffersResult;
};

// Mirrors the _preloadBuffers behavior at sound.cc:543-597
// Returns error code on failure, SOUND_NO_ERROR on success
static int preloadBuffers(TestSound* sound) {
    sound->preloadBuffersCalled = true;

    int bytesRead = sound->ioReadReturn;
    sound->ioReadCalled = true;

    if (bytesRead < 0) {
        bytesRead = 0;  // fork fix: clamp negative read
    }
    sound->bytesReadClamped = bytesRead;

    // Simulate _soundSetData returning error
    if (sound->preloadBuffersResult != SOUND_NO_ERROR) {
        return sound->preloadBuffersResult;
    }

    return SOUND_NO_ERROR;
}

// Mirrors soundOpen at sound.cc:646 — CORRECT error propagation
static int soundOpenCorrect(TestSound* sound) {
    return preloadBuffers(sound);
    // Caller gets gSoundLastError = preloadBuffers result
}

// Mirrors _soundRewind at sound.cc:650-684 — BUGGY: discards return
static int soundRewindBuggy(TestSound* sound) {
    // Lines 654-670: sound guard checks
    preloadBuffers(sound);  // line 671: return value DISCARDED

    // Line 676-682: unconditionally sets success
    // gSoundLastError = SOUND_NO_ERROR;  // BUG: overwrites actual error
    return SOUND_NO_ERROR;  // always returns success
}

// Mirrors _soundRewind FIXED: propagates preloadBuffers error
static int soundRewindFixed(TestSound* sound) {
    int result = preloadBuffers(sound);  // capture return value
    if (result != SOUND_NO_ERROR) {
        return result;  // propagate error
    }
    return SOUND_NO_ERROR;
}

// Mirrors soundPlay at sound.cc:758-760 — BUGGY: calls _soundRewind, discards return
static int soundPlayBuggy(TestSound* sound) {
    if (sound->statusFlags & 0x01) {  // SOUND_STATUS_DONE
        soundRewindBuggy(sound);  // line 760: return value DISCARDED
    }
    return SOUND_NO_ERROR;  // always returns success
}

// Mirrors soundPlay FIXED: propagates _soundRewind error
static int soundPlayFixed(TestSound* sound) {
    if (sound->statusFlags & 0x01) {  // SOUND_STATUS_DONE
        int result = soundRewindFixed(sound);
        if (result != SOUND_NO_ERROR) {
            return result;
        }
    }
    return SOUND_NO_ERROR;
}

TEST_CASE("N2-003 — _preloadBuffers return discarded in _soundRewind (sound.cc:671)")
{
    SUBCASE("soundOpen correctly propagates preloadBuffers error") {
        TestSound sound = {};
        sound.ioReadReturn = 1024;
        sound.preloadBuffersResult = SOUND_UNKNOWN_ERROR;

        int result = soundOpenCorrect(&sound);
        CHECK(sound.preloadBuffersCalled == true);
        CHECK(result == SOUND_UNKNOWN_ERROR);  // error propagated to caller
    }

    SUBCASE("_soundRewind (buggy) discards preloadBuffers error — always returns success") {
        TestSound sound = {};
        sound.ioReadReturn = 1024;
        sound.preloadBuffersResult = SOUND_UNKNOWN_ERROR;

        int result = soundRewindBuggy(&sound);
        CHECK(sound.preloadBuffersCalled == true);
        CHECK(result == SOUND_NO_ERROR);  // BUG: error swallowed, always returns success
    }

    SUBCASE("_soundRewind (fixed) propagates preloadBuffers error") {
        TestSound sound = {};
        sound.ioReadReturn = 1024;
        sound.preloadBuffersResult = SOUND_UNKNOWN_ERROR;

        int result = soundRewindFixed(&sound);
        CHECK(sound.preloadBuffersCalled == true);
        CHECK(result == SOUND_UNKNOWN_ERROR);  // FIX: error propagated
    }

    SUBCASE("_soundRewind (buggy) always succeeds even on preload failure") {
        TestSound sound = {};
        sound.ioReadReturn = -1;  // I/O read error
        sound.preloadBuffersResult = SOUND_NO_SOUND;

        int result = soundRewindBuggy(&sound);
        CHECK(sound.ioReadCalled == true);
        CHECK(sound.bytesReadClamped == 0);  // fork clamps negative to 0
        CHECK(result == SOUND_NO_ERROR);  // BUG: error silently ignored
    }

    SUBCASE("soundPlay (buggy) discards soundRewind error on SOUND_STATUS_DONE") {
        TestSound sound = {};
        sound.statusFlags = 0x01;  // SOUND_STATUS_DONE
        sound.ioReadReturn = -1;
        sound.preloadBuffersResult = SOUND_NO_SOUND;

        int result = soundPlayBuggy(&sound);
        CHECK(sound.preloadBuffersCalled == true);
        CHECK(result == SOUND_NO_ERROR);  // BUG: error not propagated up
    }

    SUBCASE("soundPlay (fixed) propagates soundRewind error on SOUND_STATUS_DONE") {
        TestSound sound = {};
        sound.statusFlags = 0x01;  // SOUND_STATUS_DONE
        sound.ioReadReturn = -1;
        sound.preloadBuffersResult = SOUND_NO_SOUND;

        int result = soundPlayFixed(&sound);
        CHECK(sound.preloadBuffersCalled == true);
        CHECK(result == SOUND_NO_SOUND);  // FIX: error propagated
    }

    SUBCASE("soundPlay does NOT call rewind when not SOUND_STATUS_DONE") {
        TestSound sound = {};
        sound.statusFlags = 0x00;  // not done
        sound.preloadBuffersResult = SOUND_UNKNOWN_ERROR;

        int resultBuggy = soundPlayBuggy(&sound);
        CHECK(sound.preloadBuffersCalled == false);  // no rewind needed
        CHECK(resultBuggy == SOUND_NO_ERROR);

        sound = {};
        sound.statusFlags = 0x00;
        sound.preloadBuffersResult = SOUND_UNKNOWN_ERROR;

        int resultFixed = soundPlayFixed(&sound);
        CHECK(sound.preloadBuffersCalled == false);  // no rewind needed
        CHECK(resultFixed == SOUND_NO_ERROR);
    }

    SUBCASE("asymmetry verification: soundOpen vs _soundRewind error paths") {
        // soundOpen at sound.cc:646: `return _preloadBuffers(sound);`
        // _soundRewind at sound.cc:671: `_preloadBuffers(sound);` (void)
        // This is the core asymmetry: one propagates, one discards.

        TestSound sOpen, sRewind;
        sOpen.preloadBuffersResult = SOUND_NO_SOUND;
        sRewind.preloadBuffersResult = SOUND_NO_SOUND;

        int openResult = soundOpenCorrect(&sOpen);
        int rewindResult = soundRewindBuggy(&sRewind);

        CHECK(openResult == SOUND_NO_SOUND);     // error correctly propagated
        CHECK(rewindResult == SOUND_NO_ERROR);   // BUG: error NOT propagated
        CHECK(openResult != rewindResult);       // asymmetric handling confirmed
    }

    SUBCASE("_soundRewind (buggy) with negative read — bytes clamped but error not propagated") {
        TestSound sound = {};
        sound.ioReadReturn = -5;  // OS-level I/O error
        sound.preloadBuffersResult = SOUND_NO_SOUND;

        int result = soundRewindBuggy(&sound);
        // Fork fix at sound.cc:574-576 clamps negative read to 0
        CHECK(sound.bytesReadClamped == 0);
        // But the error from _preloadBuffers is discarded
        CHECK(result == SOUND_NO_ERROR);
    }

    SUBCASE("_soundRewind success path — both versions return NO_ERROR") {
        TestSound sound = {};
        sound.ioReadReturn = 4096;
        sound.preloadBuffersResult = SOUND_NO_ERROR;

        int buggyResult = soundRewindBuggy(&sound);
        CHECK(buggyResult == SOUND_NO_ERROR);

        sound = {};
        sound.ioReadReturn = 4096;
        sound.preloadBuffersResult = SOUND_NO_ERROR;

        int fixedResult = soundRewindFixed(&sound);
        CHECK(fixedResult == SOUND_NO_ERROR);

        // When no error, both paths produce same result
        CHECK(buggyResult == fixedResult);
    }
}
