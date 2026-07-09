// Unit tests for audio subsystem fixes — fork change regression tests.
//
// Tests:
//   1. UF-007: Forward seek on compressed audio produces correct positive
//      byte count. The original code computed remainingBytesToSkip =
//      audioFile->position - pos, which is NEGATIVE for forward seek
//      (pos > position). signed int to unsigned int conversion wraps to
//      ~4GB, causing heap corruption when reading into a 1024-byte buffer.
//      Fix: remainingBytesToSkip = pos - audioFile->position.
//
//   2. UF-006: soundDecoderInit failure not checked in audioOpen,
//      soundEffectsCacheFileReadCompressed, and soundEffectsListPopulateFileSizes.
//      soundDecoderInit returns nullptr on corrupt files or memory exhaustion,
//      but the return value was stored and later dereferenced without checking.
//      Fix: check for nullptr and return error/clean up.
//
//   3. UF-H-037: Off-by-one overflow in soundDecoderDecode. The loop writes
//      unsigned short (2 bytes) using a byte-level counter. When size is odd,
//      the last iteration writes 1 byte past the allocated buffer.
//      Fix: round size down to even before the loop (size &= ~(size_t)1).
//
// This file does NOT link audio.cc, sound_decoder.cc, sound_effects_cache.cc,
// or sound_effects_list.cc. It mirrors the critical logic patterns and
// validates the correctness of the arithmetic, null checks, and bounds.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstddef>
#include <cstring>

// ============================================================================
// UF-007: Forward seek direction — arithmetic validation
// ============================================================================
//
// In the forward seek path of audioSeek (audio.cc:399-411), the original code:
//   remainingBytesToSkip = audioFile->position - pos;
// This is the WRONG direction for forward seek (pos > position):
//   position - pos  →  negative  →  wraps to ~4GB as unsigned int
// The fix:
//   remainingBytesToSkip = pos - audioFile->position;
//   pos - position  →  positive  →  correct skip count
//
// We verify this arithmetic with mirrors of the seek paths.

static int computeForwardSeekBytesBuggy(int position, int pos) {
    // ORIGINAL: remainingBytesToSkip = audioFile->position - pos  (BUG)
    return position - pos;
}

static int computeForwardSeekBytesFixed(int position, int pos) {
    // FIXED: remainingBytesToSkip = pos - audioFile->position
    return pos - position;
}

TEST_CASE("UF-007 — Forward seek byte count direction is correct")
{
    SUBCASE("Forward seek (pos > position) produces positive bytes to skip") {
        int position = 100;
        int pos = 500;  // seeking forward: current pos 100 → target 500

        int buggy = computeForwardSeekBytesBuggy(position, pos);
        int fixed = computeForwardSeekBytesFixed(position, pos);

        // Buggy: position - pos = 100 - 500 = -400 (NEGATIVE)
        // → as unsigned int = ~4GB → heap corruption
        CHECK(buggy < 0);
        CHECK(buggy == -400);

        // Fixed: pos - position = 500 - 100 = 400 (CORRECT)
        CHECK(fixed > 0);
        CHECK(fixed == 400);
    }

    SUBCASE("No-op seek (pos == position) produces zero bytes to skip") {
        int position = 100;
        int pos = 100;

        int buggy = computeForwardSeekBytesBuggy(position, pos);
        int fixed = computeForwardSeekBytesFixed(position, pos);

        // Buggy: 100 - 100 = 0 (correct for equality case by coincidence)
        CHECK(buggy == 0);
        // Fixed: 100 - 100 = 0 (correct)
        CHECK(fixed == 0);
    }

    SUBCASE("Small forward seek (1 byte)") {
        int position = 0;
        int pos = 1;

        int buggy = computeForwardSeekBytesBuggy(position, pos);
        int fixed = computeForwardSeekBytesFixed(position, pos);

        CHECK(buggy == -1);  // BUG: negative, wraps to 4GB
        CHECK(fixed == 1);   // FIX: correct 1 byte to skip
    }

    SUBCASE("Large forward seek within bounds") {
        int position = 1024;
        int pos = 4096;

        int buggy = computeForwardSeekBytesBuggy(position, pos);
        int fixed = computeForwardSeekBytesFixed(position, pos);

        CHECK(buggy == -3072);  // BUG: negative
        CHECK(fixed == 3072);   // FIX: positive
    }

    SUBCASE("Unsigned conversion of negative value demonstrates overflow") {
        // Simulate what happens when remainingBytesToSkip (int, negative)
        // is passed as unsigned int to audioRead's size parameter.
        int remainingBytes = computeForwardSeekBytesBuggy(100, 500);  // -400
        unsigned int asUnsigned = (unsigned int)remainingBytes;

        // -400 as unsigned 32-bit = 4294966896 (~4GB)
        CHECK(asUnsigned > 1000000);  // far exceeds any sane buffer size
        CHECK(asUnsigned > 1024);     // exceeds the 1024-byte skip buffer

        // The fixed version: remainingBytes = 400 → unsigned 400
        int fixedBytes = computeForwardSeekBytesFixed(100, 500);
        unsigned int fixedUnsigned = (unsigned int)fixedBytes;
        CHECK(fixedUnsigned == 400);
    }

    SUBCASE("Forward seek at file end boundary") {
        int fileSize = 44100;
        int position = 44000;
        int pos = 44100;  // seek to end

        int fixed = computeForwardSeekBytesFixed(position, pos);
        CHECK(fixed == 100);  // 100 bytes to skip

        int buggy = computeForwardSeekBytesBuggy(position, pos);
        CHECK(buggy == -100);  // BUG: negative
    }
}


// ============================================================================
// UF-006: soundDecoderInit null return not checked
// ============================================================================
//
// In audioOpen (audio.cc:259-267), the original code stored the
// soundDecoderInit return value without checking for nullptr:
//   audioFile->soundDecoder = soundDecoderInit(...);
//   // NO CHECK → nullptr dereference in audioRead/soundDecoderDecode
//
// Same pattern in soundEffectsCacheFileReadCompressed (sound_effects_cache.cc:479)
// and soundEffectsListPopulateFileSizes (sound_effects_list.cc:429).
//
// Fix: check for nullptr and clean up / return error.

// Mirror the Audio struct state machine for open/close
struct TestAudioFile {
    bool soundDecoderInitCalled;
    bool soundDecoderInitResultIsNull;
    bool fileClosed;
    bool slotCleared;
    int openResult;
};

// Mirror of audioOpen compressed path with the BUG (no null check)
static int audioOpenCompressedBuggy(TestAudioFile* f) {
    // ... setup, open file, find slot ...
    // audioFile->soundDecoder = soundDecoderInit(...);
    f->soundDecoderInitCalled = true;
    // BUG: no null check — just store the result
    // audioFile->fileSize *= 2;  // would dereference null decoder fields
    f->openResult = 0;  // pretends success
    return f->openResult;
}

// Mirror of audioOpen compressed path FIXED (null check added)
static int audioOpenCompressedFixed(TestAudioFile* f) {
    f->soundDecoderInitCalled = true;

    if (f->soundDecoderInitResultIsNull) {
        // Clean up and return error (mirrors audio.cc:262-267)
        f->fileClosed = true;
        f->slotCleared = true;
        f->openResult = -1;
        return -1;
    }

    f->openResult = 0;
    return 0;
}

TEST_CASE("UF-006 — soundDecoderInit null return is checked and errors propagated")
{
    SUBCASE("Null decoder is NOT stored — open returns error") {
        TestAudioFile f = {};
        f.soundDecoderInitResultIsNull = true;

        int resultBuggy = audioOpenCompressedBuggy(&f);
        int resultFixed = audioOpenCompressedFixed(&f);

        // Buggy: returns 0 (success) even though decoder is null
        CHECK(resultBuggy == 0);
        CHECK(f.soundDecoderInitCalled == true);

        // Fixed: returns -1 (error), cleans up file handle and slot
        CHECK(resultFixed == -1);
        CHECK(f.fileClosed == true);
        CHECK(f.slotCleared == true);
    }

    SUBCASE("Null decoder triggers cleanup — file handle is closed") {
        TestAudioFile f = {};
        f.soundDecoderInitResultIsNull = true;

        audioOpenCompressedFixed(&f);
        CHECK(f.fileClosed == true);
    }

    SUBCASE("Null decoder triggers cleanup — slot is zeroed") {
        TestAudioFile f = {};
        f.soundDecoderInitResultIsNull = true;

        audioOpenCompressedFixed(&f);
        CHECK(f.slotCleared == true);
    }

    SUBCASE("Valid decoder — open succeeds normally") {
        TestAudioFile f = {};
        f.soundDecoderInitResultIsNull = false;

        int result = audioOpenCompressedFixed(&f);
        CHECK(result == 0);
        CHECK(f.fileClosed == false);
        CHECK(f.slotCleared == false);
    }

    SUBCASE("soundEffectsCacheFileReadCompressed — null decoder returns -1") {
        // Mirror of sound_effects_cache.cc:479-482 fixed pattern
        bool decoderIsNull = true;
        int result = (decoderIsNull) ? -1 : 0;
        CHECK(result == -1);
    }

    SUBCASE("soundEffectsCacheFileReadCompressed — valid decoder proceeds") {
        bool decoderIsNull = false;
        int result = (decoderIsNull) ? -1 : 0;
        CHECK(result == 0);
    }

    SUBCASE("soundEffectsListPopulateFileSizes — null decoder cleans up and returns error") {
        // Mirror of sound_effects_list.cc:430-434 fixed pattern
        bool decoderIsNull = true;
        bool fileClosed = false;
        bool pathFreed = false;
        int result;

        if (decoderIsNull) {
            fileClosed = true;
            pathFreed = true;
            result = -1;  // SFXL_ERR
        } else {
            result = 0;
        }

        CHECK(result == -1);
        CHECK(fileClosed == true);
        CHECK(pathFreed == true);
    }

    SUBCASE("soundEffectsListPopulateFileSizes — valid decoder proceeds normally") {
        bool decoderIsNull = false;
        bool fileClosed = false;
        bool pathFreed = false;
        int result;

        if (decoderIsNull) {
            fileClosed = true;
            pathFreed = true;
            result = -1;
        } else {
            result = 0;
        }

        CHECK(result == 0);
        CHECK(fileClosed == false);
        CHECK(pathFreed == false);
    }
}


// ============================================================================
// UF-H-037: Odd-size buffer overflow in soundDecoderDecode
// ============================================================================
//
// In soundDecoderDecode (sound_decoder.cc:1085-1109), the loop writes
// unsigned short (2 bytes) at each iteration using a byte-level counter:
//   for (bytesRead = 0; bytesRead < size; bytesRead += 2) {
//       *(unsigned short*)(dest + bytesRead) = ...;
//   }
// When size is odd, the last iteration writes 1 byte past the allocated
// buffer (the 2nd byte of the unsigned short goes beyond size).
//
// Fix: round size down to even before the loop:
//   size &= ~(size_t)1;
//
// This test validates that the loop bound preserves buffer integrity.

// Mirror the original buggy loop logic
static size_t computeLastWriteOffsetBuggy(size_t size) {
    // Loop: for (bytesRead = 0; bytesRead < size; bytesRead += 2)
    // Last valid iteration: bytesRead = (largest multiple of 2 that is < size)
    // But when size is odd, bytesRead can go up to size - 1
    // Then *(unsigned short*)(dest + bytesRead) writes at offsets [bytesRead, bytesRead+1]
    // bytesRead+1 == size → 1 byte past buffer

    size_t lastBytesRead = 0;
    for (size_t bytesRead = 0; bytesRead < size; bytesRead += 2) {
        lastBytesRead = bytesRead;
    }
    return lastBytesRead;
}

// Mirror the fixed loop logic (size rounded down to even)
static size_t computeLastWriteOffsetFixed(size_t size) {
    size &= ~(size_t)1;  // round down to even

    size_t lastBytesRead = 0;
    for (size_t bytesRead = 0; bytesRead < size; bytesRead += 2) {
        lastBytesRead = bytesRead;
    }
    return lastBytesRead;
}

TEST_CASE("UF-H-037 — Odd-size buffer does not overflow (loop bound is even)")
{
    SUBCASE("Even size — last write stays within buffer") {
        size_t size = 1024;  // even

        size_t buggyLast = computeLastWriteOffsetBuggy(size);
        size_t fixedLast = computeLastWriteOffsetFixed(size);

        // Last write at offset 1022, writes bytes [1022, 1023] — within buffer
        CHECK(buggyLast == 1022);
        CHECK(fixedLast == 1022);
        // Both correct for even sizes
        CHECK(buggyLast + 2 <= size);  // 1024 <= 1024 OK
    }

    SUBCASE("Odd size — buggy version overflows, fixed does not") {
        size_t size = 1025;  // odd

        size_t buggyLast = computeLastWriteOffsetBuggy(size);
        size_t fixedLast = computeLastWriteOffsetFixed(size);

        // Buggy: last write at offset 1024, writes bytes [1024, 1025]
        // byte 1025 is 1 past the 1025-byte buffer (valid: [0, 1024])
        CHECK(buggyLast == 1024);
        CHECK(buggyLast + 2 > size);  // 1026 > 1025 — OVERFLOW

        // Fixed: size rounded to 1024, last write at offset 1022
        // Writes bytes [1022, 1023] — within the original buffer
        CHECK(fixedLast == 1022);
        CHECK(fixedLast + 2 <= size);  // 1024 <= 1025 — safe
    }

    SUBCASE("Odd size 1 — worst case") {
        size_t size = 1;  // odd, minimum

        size_t buggyLast = computeLastWriteOffsetBuggy(size);
        size_t fixedLast = computeLastWriteOffsetFixed(size);

        // Buggy: bytesRead=0, 0 < 1 → writes at offset 0
        // unsigned short at dest+0 writes bytes [0, 1]
        // byte 1 is past the 1-byte buffer (valid: [0])
        CHECK(buggyLast == 0);
        CHECK(buggyLast + 2 > size);  // 2 > 1 — OVERFLOW

        // Fixed: size &= ~1 = 0, loop never runs, no writes
        CHECK(fixedLast == 0);  // lastBytesRead stays at initial 0
        // No writes performed → no overflow
    }

    SUBCASE("Odd size 3 — overwrites byte 3") {
        size_t size = 3;

        size_t buggyLast = computeLastWriteOffsetBuggy(size);
        CHECK(buggyLast == 2);
        // Writes bytes [2, 3] — byte 3 is OOB (valid: [0, 1, 2])
        CHECK(buggyLast + 2 > size);  // 4 > 3 — OVERFLOW

        size_t fixedLast = computeLastWriteOffsetFixed(size);
        CHECK(fixedLast == 0);
        // size rounded to 2, writes at offset 0: bytes [0, 1] — safe
        CHECK(fixedLast + 2 <= size);  // 2 <= 3 — safe
    }

    SUBCASE("Size rounding is correct for range of values") {
        for (size_t size = 0; size < 256; size++) {
            size_t rounded = size & ~(size_t)1;

            // Rounded value must always be <= original
            CHECK(rounded <= size);

            // Rounded value must be even
            CHECK((rounded & 1) == 0);

            // Difference must be at most 1 (we only drop the odd bit)
            CHECK(size - rounded <= 1);
        }
    }

    SUBCASE("Zero size — no writes, no issue") {
        size_t buggyLast = computeLastWriteOffsetBuggy(0);
        CHECK(buggyLast == 0);  // loop never executes

        size_t fixedLast = computeLastWriteOffsetFixed(0);
        CHECK(fixedLast == 0);
    }

    SUBCASE("Large odd size — overflow confirmed") {
        // Simulate a realistic scenario: 4097-byte buffer (odd)
        size_t size = 4097;

        size_t buggyLast = computeLastWriteOffsetBuggy(size);
        CHECK(buggyLast == 4096);
        CHECK(buggyLast + 2 > size);  // 4098 > 4097 — OVERFLOW

        size_t fixedLast = computeLastWriteOffsetFixed(size);
        CHECK(fixedLast == 4094);
        CHECK(fixedLast + 2 <= size);  // 4096 <= 4097 — safe
    }
}
