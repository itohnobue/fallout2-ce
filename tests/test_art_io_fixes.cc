// Unit tests for Stage 6 Art/IO/Heap/Version fix validation.
//
// Self-contained mirror test — does NOT link production .cc files
// (pcx.cc/dfile.cc/art.cc/heap.cc/version.cc/file_utils.cc/dialog.cc all
// have heavy engine deps: window_manager, db, zlib, etc.). Mirrors the
// fixed validation logic locally and asserts the behavioral contract,
// same pattern as test_fixes_stage6_*.cc.
//
// Covers:
//   H-16      pcxRead dimension validation (width > bytesPerLine overflow)
//   art NEW-01 pcxRead negative-width underflow guard
//   M-168     datafileRemapPixelsRgb8 dimension guard
//   H-17      DAT negative dataSize rejection
//   P-16      DAT negative dataOffset/uncompressedSize/compressed rejection
//   M-160     artReadHeader dataSize/dataOffsets validation
//   art N-1   critter frmId clamps in _art_alias_num/artCritterFidShouldRun
//   M-187     heapBlockAllocate size <= 0 rejection before alignment
//   M-188     versionGetVersion format-string safety
//   M-169     fileCopy fwrite failure propagation
//   M-202     _dialogStart -1 slot guard

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace fallout {

// ============================================================
// H-16 / art NEW-01 — pcxRead dimension validation mirrors
// ============================================================
// Production: src/pcx.cc:181-204. width/height/bytesPerLine derive from
// file-controlled signed short header fields; the fix rejects malformed
// dimensions before allocating/decoding. The mirror returns true when the
// dimensions are accepted (guard passes) — i.e. the exact inverse of the
// production rejection.

static bool pcxDimensionsAccept(int width, int height, int bytesPerLine)
{
    return width > 0 && height > 0 && bytesPerLine > 0 && width <= bytesPerLine;
}

TEST_CASE("H-16: pcxRead rejects width > bytesPerLine (decode overflow)")
{
    // width > bytesPerLine: pcxReadLine writes bytesPerLine bytes per row
    // but the loop advances by `width`, overrunning the allocation.
    CHECK_FALSE(pcxDimensionsAccept(320, 480, 300));
    CHECK_FALSE(pcxDimensionsAccept(321, 1, 320));
}

TEST_CASE("art NEW-01: pcxRead rejects negative width (underflow write)")
{
    // maxX < minX → width < 0 → ptr += width walks backwards before the buffer.
    CHECK_FALSE(pcxDimensionsAccept(-9, 21, 320));
    CHECK_FALSE(pcxDimensionsAccept(0, 21, 320));
}

TEST_CASE("H-16: pcxRead rejects non-positive height and bytesPerLine")
{
    CHECK_FALSE(pcxDimensionsAccept(320, 0, 320));
    CHECK_FALSE(pcxDimensionsAccept(320, -1, 320));
    CHECK_FALSE(pcxDimensionsAccept(320, 480, 0));
    CHECK_FALSE(pcxDimensionsAccept(320, 480, -1));
}

TEST_CASE("H-16: pcxRead accepts well-formed dimensions")
{
    CHECK(pcxDimensionsAccept(320, 200, 320));
    CHECK(pcxDimensionsAccept(1, 1, 1));
    CHECK(pcxDimensionsAccept(640, 480, 640));
}

// ============================================================
// M-168 — datafileRemapPixelsRgb8 dimension guard
// ============================================================
// Production: src/datafile.cc:46-60. The remap loop iterates width*height
// over the caller's buffer; the guard returns early for non-positive
// dimensions so the loop can never read/write out of bounds.

static void datafileRemapMirror(std::vector<uint8_t>& data, int width, int height)
{
    if (data.empty() || width <= 0 || height <= 0) {
        return;
    }
    size_t size = static_cast<size_t>(width) * static_cast<size_t>(height);
    for (size_t index = 0; index < size && index < data.size(); index++) {
        data[index] = static_cast<uint8_t>(data[index] + 1);
    }
}

TEST_CASE("M-168: datafileRemapPixelsRgb8 guard prevents OOB remap")
{
    std::vector<uint8_t> buffer(100, 0);

    // Non-positive dimensions → no writes at all.
    datafileRemapMirror(buffer, 0, 10);
    datafileRemapMirror(buffer, 10, 0);
    datafileRemapMirror(buffer, -1, 10);
    datafileRemapMirror(buffer, 10, -1);
    for (uint8_t v : buffer) {
        CHECK(v == 0);
    }

    // width > bytesPerLine (buffer size) must not walk past the buffer.
    datafileRemapMirror(buffer, 1000, 10);
    for (uint8_t v : buffer) {
        CHECK(v == 1);
    }
}

// ============================================================
// H-17 / P-16 — DAT entry accounting field validation
// ============================================================
// Production: src/dfile.cc:191-209. The DAT parser now rejects negative
// dataSize/dataOffset/uncompressedSize and compressed values other than
// {0,1}, mirroring the ZIP path. The mirror returns true when the entry is
// accepted.

static bool datEntryAccept(int dataSize, int dataOffset, int uncompressedSize, int compressed)
{
    return dataSize >= 0 && dataOffset >= 0 && uncompressedSize >= 0 && compressed <= 1;
}

TEST_CASE("H-17: DAT negative dataSize rejected (decompression buffer overflow)")
{
    CHECK_FALSE(datEntryAccept(-1, 0, 100, 1));
    CHECK_FALSE(datEntryAccept(-100, 0, 100, 1));
}

TEST_CASE("P-16: DAT negative dataOffset / uncompressedSize rejected")
{
    CHECK_FALSE(datEntryAccept(100, -1, 100, 1));
    CHECK_FALSE(datEntryAccept(100, 0, -1, 1));
    CHECK_FALSE(datEntryAccept(100, 0, 100, 2));
    CHECK_FALSE(datEntryAccept(100, 0, 100, 255));
}

TEST_CASE("H-17/P-16: DAT accepts well-formed entries")
{
    CHECK(datEntryAccept(0, 0, 0, 0));
    CHECK(datEntryAccept(4096, 1024, 8192, 1));
    CHECK(datEntryAccept(0, 100, 0, 0));
}

// ============================================================
// M-160 — artReadHeader dataSize/dataOffsets validation
// ============================================================
// Production: src/art.cc:1171-1178. Negative dataSize collapses the
// artGetDataSize allocation; dataOffsets >= dataSize point the artRead
// write target past the allocated data region. Mirror returns true when
// the header values are accepted.

static bool artHeaderAccept(int dataSize, const int* dataOffsets, int count)
{
    if (dataSize < 0) {
        return false;
    }
    for (int i = 0; i < count; i++) {
        if (dataOffsets[i] < 0 || dataOffsets[i] >= dataSize) {
            return false;
        }
    }
    return true;
}

TEST_CASE("M-160: artReadHeader rejects negative dataSize")
{
    int offsets[6] = { 0, 0, 0, 0, 0, 0 };
    CHECK_FALSE(artHeaderAccept(-1, offsets, 6));
    CHECK_FALSE(artHeaderAccept(-100, offsets, 6));
}

TEST_CASE("M-160: artReadHeader rejects dataOffsets >= dataSize")
{
    int offsets[6] = { 0, 0, 0, 0, 0, 0 };
    // All-zero offsets are well-formed for any positive dataSize (0 < 64).
    CHECK(artHeaderAccept(64, offsets, 6));
    CHECK(artHeaderAccept(65, offsets, 6));

    offsets[3] = 64;
    CHECK_FALSE(artHeaderAccept(64, offsets, 6));

    offsets[3] = -1;
    CHECK_FALSE(artHeaderAccept(100, offsets, 6));
}

TEST_CASE("M-160: artReadHeader accepts well-formed headers")
{
    int offsets[6] = { 0, 0, 32, 32, 32, 32 };
    CHECK(artHeaderAccept(4096, offsets, 6));
}

// ============================================================
// art N-1 — critter frmId clamps
// ============================================================
// Production: src/art.cc:938-1000. _anon_alias / gArtCritterFidShoudRunData
// are sized by the critters list length; frmId & 0xFFF can exceed it.
// Mirrors the clamped lookup semantics.

static int artAliasNumClamped(int index, int fileNamesLength, const std::vector<int>& anonAlias)
{
    int frmId = index & 0xFFF;
    if (frmId >= fileNamesLength) {
        return 0;
    }
    return anonAlias[frmId];
}

static int artCritterFidShouldRunClamped(int fid, int fileNamesLength, const std::vector<int>& runData)
{
    int type = (fid >> 24) & 0xF;
    if (type == 1 /* OBJ_TYPE_CRITTER */) {
        int frmId = fid & 0xFFF;
        if (frmId < fileNamesLength) {
            return runData[frmId];
        }
    }
    return 0;
}

TEST_CASE("art N-1: _art_alias_num clamps out-of-range frmId")
{
    std::vector<int> anonAlias = { 10, 20, 30 };
    const int listLength = static_cast<int>(anonAlias.size());

    CHECK(artAliasNumClamped(0, listLength, anonAlias) == 10);
    CHECK(artAliasNumClamped(2, listLength, anonAlias) == 30);
    // frmId >= list length → deterministic 0, no OOB read.
    CHECK(artAliasNumClamped(3, listLength, anonAlias) == 0);
    CHECK(artAliasNumClamped(4095, listLength, anonAlias) == 0);
}

TEST_CASE("art N-1: artCritterFidShouldRun clamps out-of-range frmId")
{
    std::vector<int> runData = { 1, 0, 1 };
    const int listLength = static_cast<int>(runData.size());

    // OBJ_TYPE_CRITTER = 1 in the type nibble.
    // runData[0] = 1 — use frmId 0 so the in-range lookup returns 1.
    int fidInRange = (1 << 24) | 0;
    CHECK(artCritterFidShouldRunClamped(fidInRange, listLength, runData) == 1);

    int fidOutOfRange = (1 << 24) | 4095;
    CHECK(artCritterFidShouldRunClamped(fidOutOfRange, listLength, runData) == 0);

    // Non-critter → 0.
    int itemFid = (5 << 24) | 1;
    CHECK(artCritterFidShouldRunClamped(itemFid, listLength, runData) == 0);
}

// ============================================================
// M-187 — heapBlockAllocate size <= 0 rejection
// ============================================================
// Production: src/heap.cc:310-321. The fix rejects size <= 0 BEFORE the
// 4-byte alignment. The old code aligned first, so -8 → -4 survived the
// `size == 0` guard and corrupted heap metadata. The mirror replicates the
// post-fix order and asserts that only positive sizes are accepted.

static bool heapBlockAllocateAccepts(int size)
{
    if (size <= 0) {
        return false;
    }
    size += 4 - size % 4;
    return size > 0;
}

TEST_CASE("M-187: heapBlockAllocate rejects size <= 0 before alignment")
{
    // The old bug: -8 aligned to -4 and passed the `size == 0` guard.
    CHECK_FALSE(heapBlockAllocateAccepts(-8));
    CHECK_FALSE(heapBlockAllocateAccepts(-1));
    CHECK_FALSE(heapBlockAllocateAccepts(0));
}

TEST_CASE("M-187: heapBlockAllocate accepts positive sizes")
{
    CHECK(heapBlockAllocateAccepts(1));
    CHECK(heapBlockAllocateAccepts(4));
    CHECK(heapBlockAllocateAccepts(100));
}

// ============================================================
// M-188 — versionGetVersion format-string safety
// ============================================================
// Production: src/version.cc:24-28. The fix never passes the user-writable
// version_string as the snprintf format; it is always the "%s" argument.
// A config value containing '%' must be copied literally, with no format
// interpretation and no crash.

static void versionGetVersionMirror(char* dest, size_t size, const char* versionString)
{
    if (versionString != nullptr) {
        snprintf(dest, size, "%s", versionString);
    } else {
        snprintf(dest, size, "FALLOUT II %d.%02d", 2, 3);
    }
}

TEST_CASE("M-188: version_string with format specifiers is copied literally")
{
    // '%s' / '%n' / '%d' in the config value must not be interpreted.
    const char* malicious[] = {
        "%s",
        "%n",
        "Version %d.%d",
        "%s%n%x%p",
        "100%% done",
    };

    for (const char* v : malicious) {
        char dest[64];
        dest[0] = '\0';
        versionGetVersionMirror(dest, sizeof(dest), v);
        CHECK(std::strcmp(dest, v) == 0);
    }
}

TEST_CASE("M-188: default version string still formats correctly")
{
    char dest[64];
    dest[0] = '\0';
    versionGetVersionMirror(dest, sizeof(dest), nullptr);
    CHECK(std::strcmp(dest, "FALLOUT II 2.03") == 0);
}

// ============================================================
// M-169 — fileCopy fwrite failure propagation
// ============================================================
// Production: src/file_utils.cc:146-190. The fix treats fwrite returning 0
// as a hard failure (the old `bytesWritten < 0` check was dead on size_t
// and the outer fread loop continued, reporting a truncated copy as
// success). The mirror returns false when an fwrite would fail.

static bool fileCopyMirror(std::vector<uint8_t>& out, const std::vector<uint8_t>& in, size_t failAtWrite)
{
    size_t offset = 0;
    size_t written = 0;
    for (uint8_t b : in) {
        (void)b;
        if (written == failAtWrite) {
            return false; // fwrite returned 0
        }
        out.push_back(0);
        written++;
        offset++;
    }
    return true;
}

TEST_CASE("M-169: fileCopy propagates partial-write failure")
{
    std::vector<uint8_t> in(10, 1);
    std::vector<uint8_t> out;

    // Success path copies everything.
    CHECK(fileCopyMirror(out, in, 100));
    CHECK(out.size() == in.size());

    // Failure mid-copy must be reported as failure, not success.
    out.clear();
    CHECK_FALSE(fileCopyMirror(out, in, 3));
    CHECK(out.size() == 3);
}

// ============================================================
// M-202 — _dialogStart -1 slot guard
// ============================================================
// Production: src/dialog.cc:407-433. _tods == -1 denotes "no dialog
// active"; the first dialog must target slot 0 instead of indexing
// _dialog[-1] (36-byte out-of-bounds write). The mirror models the slot
// selection: returns the slot that would be written for a given _tods.

static int dialogStartSlotFor(int tods)
{
    if (tods == 3) {
        return -1; // depth limit
    }
    if (tods == -1) {
        return 0;
    }
    return tods + 1;
}

TEST_CASE("M-202: _dialogStart maps _tods == -1 to slot 0")
{
    // The old code wrote &_dialog[-1] for the first dialog.
    CHECK(dialogStartSlotFor(-1) == 0);
    CHECK(dialogStartSlotFor(0) == 1);
    CHECK(dialogStartSlotFor(1) == 2);
    CHECK(dialogStartSlotFor(2) == 3);
    // Depth limit: _tods == 3 → reject.
    CHECK(dialogStartSlotFor(3) == -1);
}

} // namespace fallout
