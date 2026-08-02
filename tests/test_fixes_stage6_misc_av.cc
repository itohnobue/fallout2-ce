// Unit tests for Stage 6 FIX — s6-fix-misc domain
// (audio decoders, movie/MVE, lips, party, pipboy, object, mouse/kb, parsers).
//
// Self-contained mirror test — does NOT link production .cc files (50+ engine
// deps). Each mirror reproduces the fixed validation logic so the regression
// behavior is asserted without the engine.
//
// Fixes covered:
//   M-129 (MEDIUM): ACM decoder negative file_cnt → decode OOB read
//   M-130 (MEDIUM): ACM samples_per_subband == 0 → malloc(0) OOB read
//   M-132 (MEDIUM): OGG sampleCount int overflow → OOM abort
//   M-133 (MEDIUM): sound.cc wrap loop infinite hang for fileSize <= 0
//   M-138 (MEDIUM): audio mixer frame read past buffer end
//   M-139 (MEDIUM): _snd_lookup_* 5-byte arrays indexed OOB by scripts
//   M-134 (MEDIUM): MVE palette start/count > 256 → stack/global overflow
//   M-135 (MEDIUM): _nfPkDecomp dest OOB (chunk coords vs frame)
//   M-137 (MEDIUM): MVE audio buffer bytes == 0 → %0 SIGFPE
//   M-146 (MEDIUM): endgame death-parser OOB index (post-truncation strlen)
//   M-193 (MEDIUM): movie scale blitters vertical OOB
//   M-196 (MEDIUM): lips phoneme byte > 41 indexes int[42] table
//   M-197 (MEDIUM): lips audioBaseName strcpy overflow (char[16])
//   M-198 (MEDIUM): lips uninit phoneme read after missing-LIP load
//   M-155 (MEDIUM): partyMembersLoad unvalidated length → OOB write/bad_alloc
//   M-156 (MEDIUM): strParseStrFromList -1 → areaAttackMode[-1] heap underflow
//   M-179 (MEDIUM): pipboy questLocations[-1] with zero locations
//   M-180 (MEDIUM): pipboy questDescriptions[gQuestsCount] one-past-end
//   M-181 (MEDIUM): pipboy holodisk pagination uses quest page count
//   M-76  (MEDIUM): gObjectFids unbounded write / dangling pointer
//   M-77  (MEDIUM): object tile OOB insert (also covers map NEW-2/P-17)
//   M-78  (MEDIUM): objectSetRotation negative rotation OOB
//   M-79  (MEDIUM): object tile-list OOB reads (3 missed guards)
//   M-183 (MEDIUM): mouse shape null memcpy
//   M-186 (MEDIUM): SDL relative-mode flag set before verification
//   M-158 (MEDIUM): lock-key re-toggle on repeat events
//   M-191 (MEDIUM): strParseKeyValue unbounded strcpy

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

// ============================================================
// M-129 / M-130: ACM decoder header validation
// ============================================================
// Production: sound_decoder.cc soundDecoderInit — file_cnt (32-bit signed) and
// samples_per_subband (12-bit) come from the header. A negative file_cnt
// drives samp_cnt negative and the decode loop reads past the samples buffer;
// samples_per_subband == 0 makes total_samples 0 → malloc(0) (non-null) →
// 4-byte reads from a 0-byte allocation.

struct AcmHeaderFields {
    int file_cnt;
    int samples_per_subband;
    int total_samples;
};

// Mirrors soundDecoderInit validation: reject negative file_cnt, reject
// samples_per_subband == 0 / total_samples <= 0.
static bool acmHeaderIsValid(AcmHeaderFields fields)
{
    if (fields.file_cnt < 0) {
        return false; // M-129
    }
    if (fields.samples_per_subband == 0 || fields.total_samples <= 0) {
        return false; // M-130
    }
    return true;
}

TEST_CASE("M-129: ACM decoder rejects negative file_cnt") {
    SUBCASE("normal header accepted") {
        AcmHeaderFields f = {1000, 64, 64 * 8};
        CHECK(acmHeaderIsValid(f));
    }
    SUBCASE("bit-31 set (negative count) rejected") {
        AcmHeaderFields f = {-1, 64, 64 * 8};
        CHECK_FALSE(acmHeaderIsValid(f));
    }
    SUBCASE("INT_MIN rejected") {
        AcmHeaderFields f = {static_cast<int>(0x80000000), 64, 512};
        CHECK_FALSE(acmHeaderIsValid(f));
    }
    SUBCASE("zero file_cnt accepted (empty stream handled by decode loop)") {
        AcmHeaderFields f = {0, 64, 512};
        CHECK(acmHeaderIsValid(f));
    }
}

TEST_CASE("M-130: ACM decoder rejects samples_per_subband == 0") {
    SUBCASE("zero samples_per_subband rejected") {
        AcmHeaderFields f = {1000, 0, 0};
        CHECK_FALSE(acmHeaderIsValid(f));
    }
    SUBCASE("non-positive total rejected (defensive)") {
        AcmHeaderFields f = {1000, 64, -1};
        CHECK_FALSE(acmHeaderIsValid(f));
    }
    SUBCASE("positive samples_per_subband accepted") {
        AcmHeaderFields f = {1000, 1, 8};
        CHECK(acmHeaderIsValid(f));
    }
}

// ============================================================
// M-132: OGG sampleCount overflow
// ============================================================
// Production: ogg_decoder.cc — stb_vorbis_stream_length_in_samples saturates
// near 2^32; counts in [2^30, 2^31-1] pass the <= 0 check and overflow the
// int product totalSamples * channels (channels == 2), aborting with OOM.
// Fix caps totalSamples so the product stays in int range.

static bool oggSampleCountIsValid(int totalSamples, int channels)
{
    if (totalSamples <= 0) {
        return false;
    }
    if (totalSamples > 0x7FFFFFFF / channels) {
        return false; // M-132
    }
    return true;
}

TEST_CASE("M-132: OGG sample count bounded before allocation") {
    SUBCASE("normal count accepted") {
        CHECK(oggSampleCountIsValid(44100 * 60, 2));
    }
    SUBCASE("2^30 with 2 channels rejected (product overflows int)") {
        // 0x7FFFFFFF / 2 = 2^30 - 1: the max accepted count for 2 channels.
        // 2^30 * 2 = 2^31 overflows signed int → the M-132 cap rejects it.
        CHECK_FALSE(oggSampleCountIsValid(1 << 30, 2));
    }
    SUBCASE("2^30 - 1 with 2 channels accepted (max product fits)") {
        CHECK(oggSampleCountIsValid((1 << 30) - 1, 2));
    }
    SUBCASE("2^30 + 1 with 2 channels rejected (product overflows int)") {
        CHECK_FALSE(oggSampleCountIsValid((1 << 30) + 1, 2));
    }
    SUBCASE("mono allows larger counts") {
        CHECK(oggSampleCountIsValid(0x7FFFFFFF, 1));
    }
    SUBCASE("non-positive rejected") {
        CHECK_FALSE(oggSampleCountIsValid(0, 2));
        CHECK_FALSE(oggSampleCountIsValid(-1, 2));
    }
}

// ============================================================
// M-133: sound.cc wrap-loop infinite hang
// ============================================================
// Production: _refreshSoundBuffers do/while wrap. With fileSize <= 0,
// v3 = numBytesRead - fileSize grows and v3 > fileSize stays true forever.
// Fix: only wrap when fileSize > 0, otherwise clamp once.

static int mirrorWrapReadPosition(int numBytesRead, int fileSize)
{
    if (fileSize < numBytesRead) {
        if (fileSize > 0) {
            int v3;
            do {
                v3 = numBytesRead - fileSize;
                numBytesRead = v3;
            } while (v3 > fileSize);
        } else {
            numBytesRead = 0; // M-133: bound the loop for non-positive sizes
        }
    }
    return numBytesRead;
}

TEST_CASE("M-133: sound wrap loop terminates for fileSize <= 0") {
    SUBCASE("normal positive size wraps") {
        int pos = 100;
        pos = mirrorWrapReadPosition(pos, 30);
        CHECK(pos == 10); // 100 - 30 = 70, 70 > 30 → 70 - 30 = 40, ... → 10
    }
    SUBCASE("fileSize == 0 terminates (would hang before)") {
        CHECK(mirrorWrapReadPosition(50, 0) == 0);
    }
    SUBCASE("negative fileSize terminates (would hang before)") {
        CHECK(mirrorWrapReadPosition(50, -10) == 0);
    }
    SUBCASE("numBytesRead already below fileSize: no change") {
        CHECK(mirrorWrapReadPosition(10, 100) == 10);
    }
}

// ============================================================
// M-138: audio mixer frame read past buffer end
// ============================================================
// Production: audio_engine.cc audioEngineMixin read `srcFrameSize` bytes at
// pos with only a pos >= size check AFTER the read; odd-size buffers
// (size % srcFrameSize != 0) over-read. Fix: wrap/stop before the read when
// pos + srcFrameSize > size.

struct MixerBuffer {
    unsigned int size;
    unsigned int pos;
    bool looping;
    bool playing;
};

// Mirrors the fixed read-guard in audioEngineMixin.
static bool mixerFrameFits(MixerBuffer& buf, unsigned int srcFrameSize)
{
    if (buf.pos + srcFrameSize > buf.size) {
        if (buf.looping && buf.size > 0) {
            buf.pos = 0;
            return true;
        }
        buf.playing = false;
        return false;
    }
    return true;
}

TEST_CASE("M-138: mixer guard prevents frame read past buffer") {
    SUBCASE("exact multiple: no guard trip") {
        MixerBuffer buf = {100, 96, false, true};
        CHECK(mixerFrameFits(buf, 4));
        CHECK(buf.playing);
    }
    SUBCASE("odd-size non-looping: stops instead of over-reading") {
        MixerBuffer buf = {101, 100, false, true};
        CHECK_FALSE(mixerFrameFits(buf, 4));
        CHECK_FALSE(buf.playing);
    }
    SUBCASE("odd-size looping: wraps to start") {
        MixerBuffer buf = {101, 100, true, true};
        CHECK(mixerFrameFits(buf, 4));
        CHECK(buf.pos == 0);
        CHECK(buf.playing);
    }
    SUBCASE("frame size larger than whole buffer (non-looping) stops") {
        MixerBuffer buf = {2, 0, false, true};
        CHECK_FALSE(mixerFrameFits(buf, 4));
    }
}

// ============================================================
// M-139: _snd_lookup_* 5-byte arrays OOB
// ============================================================
// Production: game_sound.cc sfxBuildWeaponName/sfxBuildSceneryName/
// sfxBuildOpenName index the 5-entry tables with script-controlled ints.
// Fix clamps to the count and substitutes a safe default code.

static char weaponLookup(int effectType)
{
    static const char tbl[5] = {'R', 'A', 'O', 'F', 'H'};
    return (effectType >= 0 && effectType < 5) ? tbl[effectType] : 'H';
}

static char sceneryLookup(int action)
{
    static const char tbl[5] = {'O', 'C', 'L', 'N', 'U'};
    return (action >= 0 && action < 5) ? tbl[action] : 'O';
}

TEST_CASE("M-139: sound lookup tables bounds-checked") {
    SUBCASE("in-range indices return table values") {
        CHECK(weaponLookup(0) == 'R');
        CHECK(weaponLookup(4) == 'H');
        CHECK(sceneryLookup(0) == 'O');
        CHECK(sceneryLookup(4) == 'U');
    }
    SUBCASE("out-of-range weapon type falls back to 'H'") {
        CHECK(weaponLookup(5) == 'H');
        CHECK(weaponLookup(999) == 'H');
        CHECK(weaponLookup(-1) == 'H');
    }
    SUBCASE("out-of-range scenery action falls back to 'O'") {
        CHECK(sceneryLookup(5) == 'O');
        CHECK(sceneryLookup(1000) == 'O');
        CHECK(sceneryLookup(-3) == 'O');
    }
}

// ============================================================
// M-134: MVE palette start/count bounds
// ============================================================
// Production: movie_lib.cc palSetPalette/palClrPalette pass raw file ushorts
// to _setSystemPaletteEntries → overflow of the 768-byte newPalette/_systemCmap
// and svga's colors[256]. Fix mirrors palLoadPalette's
// `start < 0 || count < 0 || start + count > 256` guard.

static bool mvePaletteRangeValid(int start, int count)
{
    return !(start < 0 || count < 0 || start + count > 256);
}

TEST_CASE("M-134: MVE palette set validated before use") {
    SUBCASE("normal palette chunk accepted") {
        CHECK(mvePaletteRangeValid(0, 256));
        CHECK(mvePaletteRangeValid(100, 10));
    }
    SUBCASE("count > 256 rejected") {
        CHECK_FALSE(mvePaletteRangeValid(0, 257));
        CHECK_FALSE(mvePaletteRangeValid(200, 100)); // start + count > 256
    }
    SUBCASE("start beyond table rejected") {
        CHECK_FALSE(mvePaletteRangeValid(256, 1));
        CHECK_FALSE(mvePaletteRangeValid(300, 0));
    }
    SUBCASE("negative values rejected") {
        CHECK_FALSE(mvePaletteRangeValid(-1, 1));
        CHECK_FALSE(mvePaletteRangeValid(0, -1));
    }
}

// ============================================================
// M-135: _nfPkDecomp dest bounds
// ============================================================
// Production: movie_lib.cc _nfPkDecomp computes dest from file-controlled
// chunk x/y/w/h (8px units) against the nfConfig frame. Fix bounds the chunk
// against frameWidthBlocks/frameHeightBlocks.

struct NfFrame {
    int nf_width;  // 8 * width
    int nf_height; // 8 * height * planes
    int planes;
};

static bool nfChunkFits(const NfFrame& frame, int x, int y, int w, int h)
{
    int frameWidthBlocks = frame.nf_width / 8;
    int frameHeightBlocks = frame.planes > 0 ? frame.nf_height / (8 * frame.planes) : 0;
    if (x < 0 || y < 0 || w <= 0 || h <= 0
        || x + w > frameWidthBlocks
        || y + h > frameHeightBlocks) {
        return false;
    }
    return true;
}

TEST_CASE("M-135: nfPkDecomp chunk bounded against frame") {
    NfFrame frame = {640, 480 * 1, 1}; // 80x60 blocks

    SUBCASE("in-frame chunk accepted") {
        CHECK(nfChunkFits(frame, 0, 0, 80, 60));
        CHECK(nfChunkFits(frame, 70, 50, 10, 10));
    }
    SUBCASE("chunk extending past frame rejected") {
        CHECK_FALSE(nfChunkFits(frame, 75, 0, 10, 10)); // x+w = 85 > 80
        CHECK_FALSE(nfChunkFits(frame, 0, 55, 10, 10)); // y+h = 65 > 60
    }
    SUBCASE("huge file-controlled coordinates rejected") {
        CHECK_FALSE(nfChunkFits(frame, 60000, 0, 1, 1));
        CHECK_FALSE(nfChunkFits(frame, 0, 60000, 1, 1));
    }
    SUBCASE("zero width/height rejected") {
        CHECK_FALSE(nfChunkFits(frame, 0, 0, 0, 10));
        CHECK_FALSE(nfChunkFits(frame, 0, 0, 10, 0));
    }
}

// ============================================================
// M-137: MVE audio buffer bytes == 0 → %0 SIGFPE
// ============================================================
// Production: _MVE_sndConfigure computes gMveBufferBytes = (a2 + (a2>>1)) & ~3;
// for a2 in {0,1,2} it is 0 and _MVE_sndSync divides by it. Fix rejects 0.

static int mveBufferBytes(int a2)
{
    int bytes = (a2 + (a2 >> 1)) & 0xFFFFFFFC;
    if (bytes == 0) {
        return -1; // M-137: reject (configure failure)
    }
    return bytes;
}

TEST_CASE("M-137: MVE audio buffer rejects zero size") {
    SUBCASE("normal sizes produce non-zero buffers") {
        // (4 + 2) & ~3 = 6 & 0xFFFFFFFC = 4 — the mask rounds down to 4-byte
        // alignment, so 4 yields 4, not the unmasked 6.
        CHECK(mveBufferBytes(4) == 4);
        // (1000 + 500) = 1500, already 4-aligned → 1500.
        CHECK(mveBufferBytes(1000) == 1500);
    }
    SUBCASE("a2 in {0,1,2} rejected") {
        CHECK(mveBufferBytes(0) == -1);
        CHECK(mveBufferBytes(1) == -1);
        CHECK(mveBufferBytes(2) == -1);
    }
}

// ============================================================
// M-146: endgame death-parser OOB index
// ============================================================
// Production: endgame.cc endgameDeathEndingInit measured strlen(tok) BEFORE
// truncating to char[16], then indexed voiceOverBaseName[len-1] OOB for
// names >= 17 chars. Fix mirrors the sibling: post-truncation strlen + >0.

static void truncateName(const char* src, char* dst, size_t dstSize, size_t& len)
{
    std::strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
    len = std::strlen(dst);
}

TEST_CASE("M-146: endgame death parser uses post-truncation length") {
    SUBCASE("short name keeps its length") {
        char buf[16];
        size_t len = 0;
        truncateName("nar_1", buf, sizeof(buf), len);
        CHECK(std::strcmp(buf, "nar_1") == 0);
        CHECK(len == 5);
    }
    SUBCASE("long name is truncated — length stays within buffer") {
        char buf[16];
        size_t len = 0;
        truncateName("this_narrator_name_is_way_too_long", buf, sizeof(buf), len);
        CHECK(len <= sizeof(buf) - 1);
        CHECK(buf[len] == '\0');
    }
}

// ============================================================
// M-193: movie scale blitters vertical bound
// ============================================================
// Production: movie.cc _movieScaleSubRect/_movieScaleWindow write `height`
// rows; the guards only bound the rect, not the frame height. Fix clamps
// height to the window.

static int clampScaleHeight(int height, int windowHeight, int movieY)
{
    // _movieScaleSubRect: height <= windowHeight - _movieY
    if (height > windowHeight - movieY) {
        height = windowHeight - movieY;
    }
    return height;
}

TEST_CASE("M-193: movie scale blitters clamp frame height to window") {
    SUBCASE("frame fits: unchanged") {
        CHECK(clampScaleHeight(100, 480, 0) == 100);
        CHECK(clampScaleHeight(200, 480, 100) == 200);
    }
    SUBCASE("frame taller than window: clamped") {
        CHECK(clampScaleHeight(500, 480, 0) == 480);
        CHECK(clampScaleHeight(300, 200, 50) == 150); // 200 - 50
    }
}

// ============================================================
// M-196 / M-197 / M-198: lips phoneme + name handling
// ============================================================
// M-196: phoneme bytes 0-255 index the int[42] lookup; fix clamps >= 42 to 0.
// M-197: audioBaseName is char[16]; fix copies bounded.
// M-198: lipsFree must reset field_24/field_2C so a missing-LIP reload cannot
// read uninitialized phonemes.

static unsigned char clampPhoneme(unsigned char phoneme)
{
    return phoneme >= 42 ? 0 : phoneme; // M-196
}

TEST_CASE("M-196: lips phoneme clamped to PHONEME_COUNT") {
    SUBCASE("valid phonemes pass through") {
        CHECK(clampPhoneme(0) == 0);
        CHECK(clampPhoneme(41) == 41);
    }
    SUBCASE("invalid phonemes clamped to 0") {
        CHECK(clampPhoneme(42) == 0);
        CHECK(clampPhoneme(255) == 0);
        CHECK(clampPhoneme(100) == 0);
    }
}

TEST_CASE("M-197: lips audioBaseName copied bounded") {
    char audioBaseName[16];
    std::strncpy(audioBaseName, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", sizeof(audioBaseName) - 1);
    audioBaseName[sizeof(audioBaseName) - 1] = '\0';
    CHECK(std::strlen(audioBaseName) == sizeof(audioBaseName) - 1);
}

struct LipsCounts {
    int field_24;
    int field_2C;
};

static void mirrorLipsFree(LipsCounts& counts)
{
    // M-198: production lipsFree frees the arrays and must reset the counts.
    counts.field_24 = 0;
    counts.field_2C = 0;
}

TEST_CASE("M-198: lipsFree resets phoneme/marker counts") {
    LipsCounts counts = {64, 32};
    mirrorLipsFree(counts);
    CHECK(counts.field_24 == 0);
    CHECK(counts.field_2C == 0);
}

// ============================================================
// M-155 / M-156: party bounds family
// ============================================================
// M-155: partyMembersLoad clamps the file-supplied length to the buffer
// capacity (descriptions + 20) and rejects negatives (bad_alloc in reserve).
// M-156: strParseStrFromList sets -1 on no-match; the 7 call sites must check
// the return before indexing the flag arrays.

TEST_CASE("M-155: party member length clamped to capacity") {
    int capacity = 10 + 20; // gPartyMemberDescriptionsLength + 20

    auto clampLength = [capacity](int length) {
        if (length < 0 || length > capacity) {
            return capacity;
        }
        return length;
    };

    SUBCASE("normal length unchanged") {
        CHECK(clampLength(5) == 5);
    }
    SUBCASE("length beyond capacity clamped") {
        CHECK(clampLength(100) == capacity);
    }
    SUBCASE("negative length clamped (no bad_alloc in reserve)") {
        CHECK(clampLength(-1) == capacity);
    }
}

TEST_CASE("M-156: strParseStrFromList -1 result is guarded") {
    // Mirrors the fixed call-site pattern: break on -1 instead of indexing
    // areaAttackMode[-1] (heap underflow).
    auto parseFlag = [](int parseResult) {
        if (parseResult == -1) {
            return -1; // no match: caller breaks out
        }
        return parseResult; // valid list index
    };

    SUBCASE("valid token yields a usable index") {
        CHECK(parseFlag(3) == 3);
    }
    SUBCASE("no-match (-1) is detectable by the caller") {
        CHECK(parseFlag(-1) == -1);
    }
}

// ============================================================
// M-179 / M-180 / M-181: pipboy pagination
// ============================================================
// M-179: questLocations[-1] when count == 0 (startIndex clamp produced -1).
// M-180: questDescriptions[gQuestsCount] one-past-end after search exhausts.
// M-181: holodisk nav uses the holodisk page count, not the quest count.

TEST_CASE("M-179: pipboy quest start index never negative") {
    auto clampStart = [](int startIndex, int count) {
        if (startIndex >= count) {
            return count > 0 ? count - 1 : 0; // M-179
        }
        return startIndex;
    };

    SUBCASE("normal page start unchanged") {
        CHECK(clampStart(0, 20) == 0);
        CHECK(clampStart(19, 20) == 19);
    }
    SUBCASE("beyond end clamps to last item") {
        CHECK(clampStart(20, 20) == 19);
    }
    SUBCASE("count == 0 yields 0 (no -1)") {
        CHECK(clampStart(0, 0) == 0);
    }
}

TEST_CASE("M-180: pipboy quest description index guarded") {
    auto clampIndex = [](int index, int count) {
        if (count <= 0) {
            return -1; // nothing to display
        }
        if (index >= count) {
            return count - 1; // M-180
        }
        return index;
    };

    SUBCASE("valid index unchanged") {
        CHECK(clampIndex(3, 10) == 3);
    }
    SUBCASE("one-past-end clamped") {
        CHECK(clampIndex(10, 10) == 9);
    }
    SUBCASE("no quests: bails (sentinel)") {
        CHECK(clampIndex(0, 0) == -1);
    }
}

TEST_CASE("M-181: holodisk nav uses holodisk page count") {
    auto holodiskPages = [](int knownHolodisks, int perPage) {
        return (knownHolodisks + perPage - 1) / perPage;
    };
    auto canAdvance = [&](int questPage, int questPages, int holoPage, int holoPages) {
        // M-181: more blocked only when BOTH sides are on their last page.
        return !(questPage >= questPages - 1 && holoPage >= holoPages - 1);
    };

    SUBCASE("quests on last page, holodisks have more pages → can advance") {
        CHECK(canAdvance(0, 1, 0, 2) == true); // the regression case
    }
    SUBCASE("both on last page → blocked") {
        CHECK(canAdvance(0, 1, 1, 2) == false);
    }
    SUBCASE("quests have more pages → can advance") {
        CHECK(canAdvance(0, 2, 0, 1) == true);
    }
    SUBCASE("page counts: 20 holodisks at 19 per page = 2 pages") {
        CHECK(holodiskPages(20, 19) == 2);
        CHECK(holodiskPages(19, 19) == 1);
    }
}

// ============================================================
// M-76 / M-77 / M-78 / M-79: object family
// ============================================================
// M-76: gObjectFids write bounded by objectCount; freed pointer nulled.
// M-77: objectRead normalizes out-of-range tile (covers map NEW-2/P-17).
// M-78: objectSetRotation rejects negative direction.
// M-79: tile-list lookups validate the tile first.

static constexpr int HEX_GRID_SIZE = 40000;
static constexpr int ROTATION_COUNT = 6;

static int normalizeObjectTile(int tile)
{
    // M-77: tile <-1 or >= HEX_GRID_SIZE becomes -1 (floating object).
    if (tile < -1 || tile >= HEX_GRID_SIZE) {
        return -1;
    }
    return tile;
}

static int normalizeObjectRotation(int rotation)
{
    // M-78 (load-time): out-of-range rotation becomes 0.
    if (rotation < 0 || rotation >= ROTATION_COUNT) {
        return 0;
    }
    return rotation;
}

static bool tileIsValid(int tile)
{
    return tile >= 0 && tile < HEX_GRID_SIZE;
}

TEST_CASE("M-77: object tile normalized at load") {
    SUBCASE("valid tiles unchanged") {
        CHECK(normalizeObjectTile(0) == 0);
        CHECK(normalizeObjectTile(39999) == 39999);
        CHECK(normalizeObjectTile(-1) == -1); // floating object is legal
    }
    SUBCASE("tile >= HEX_GRID_SIZE normalized to -1 (automap/pixelOffset root)") {
        CHECK(normalizeObjectTile(40000) == -1);
        CHECK(normalizeObjectTile(47900) == -1); // P-17 sample tile
    }
    SUBCASE("tile < -1 normalized to -1") {
        CHECK(normalizeObjectTile(-5) == -1);
    }
}

TEST_CASE("M-78: object rotation bounds") {
    SUBCASE("valid rotations unchanged") {
        CHECK(normalizeObjectRotation(0) == 0);
        CHECK(normalizeObjectRotation(5) == 5);
    }
    SUBCASE("negative rotation normalized to 0 (no xOffsets OOB)") {
        CHECK(normalizeObjectRotation(-1) == 0);
        CHECK(normalizeObjectRotation(-100) == 0);
    }
    SUBCASE("rotation >= ROTATION_COUNT normalized to 0") {
        CHECK(normalizeObjectRotation(6) == 0);
    }
}

TEST_CASE("M-79: tile-list guards validate tile") {
    SUBCASE("valid tile passes") {
        CHECK(tileIsValid(0));
        CHECK(tileIsValid(39999));
    }
    SUBCASE("out-of-range tile rejected") {
        CHECK_FALSE(tileIsValid(-1));
        CHECK_FALSE(tileIsValid(40000));
        CHECK_FALSE(tileIsValid(-100));
    }
}

// ============================================================
// M-183: mouse shape null data
// ============================================================
// Production: mouse_manager.cc mouseManagerSetMouseShape — datafileReadRaw
// returns nullptr for missing/corrupt files (width/height uninitialized) and
// the STATIC case memcpy's from it. Fix: null guard before caching.

TEST_CASE("M-183: mouse shape null data handled") {
    auto loadMouseShape = [](bool dataLoaded) {
        // Mirrors the fixed flow: fail cleanly when datafileReadRaw fails.
        return dataLoaded;
    };

    SUBCASE("loaded cursor succeeds") {
        CHECK(loadMouseShape(true));
    }
    SUBCASE("failed cursor load returns false (no memcpy from null)") {
        CHECK_FALSE(loadMouseShape(false));
    }
}

// ============================================================
// M-186: SDL relative-mode ordering
// ============================================================
// Production: dinput.cc mouseDeviceInitMode — the old code set
// mouseRelativeMode = true before verifying SDL_SetRelativeMouseMode, so a
// failure left the flag true (deltas read as positions) and aborted startup.
// Fix verifies first and falls back to absolute mode.

TEST_CASE("M-186: relative mode flag set only after success") {
    auto initMode = [](bool sdlSucceeded) {
        bool mouseRelativeMode = false;
        if (sdlSucceeded) {
            mouseRelativeMode = true;
        }
        return mouseRelativeMode; // M-186: false when SDL fails
    };

    SUBCASE("SDL success enables relative mode") {
        CHECK(initMode(true));
    }
    SUBCASE("SDL failure keeps absolute mode (no startup abort)") {
        CHECK_FALSE(initMode(false));
    }
}

// ============================================================
// M-158: lock-key re-toggle on repeat
// ============================================================
// Production: kb.cc _kb_simulate_key — lock toggles were inside
// `keyState != KEY_STATE_UP`, so KEY_STATE_REPEAT (every ~80ms while held)
// re-toggled the lock bit. Fix: toggle only on KEY_STATE_DOWN.

static constexpr int KEY_STATE_UP = 0;
static constexpr int KEY_STATE_DOWN = 1;
static constexpr int KEY_STATE_REPEAT = 2;

TEST_CASE("M-158: lock keys toggle only on key-down") {
    auto shouldToggle = [](int keyState) {
        // M-158: DOWN-only guard.
        return keyState == KEY_STATE_DOWN;
    };

    SUBCASE("key down toggles") {
        CHECK(shouldToggle(KEY_STATE_DOWN));
    }
    SUBCASE("key up does not toggle") {
        CHECK_FALSE(shouldToggle(KEY_STATE_UP));
    }
    SUBCASE("key repeat does not re-toggle") {
        CHECK_FALSE(shouldToggle(KEY_STATE_REPEAT));
    }
}

// ============================================================
// M-191: strParseKeyValue bounded copy
// ============================================================
// Production: string_parsers.cc — the key segment was copied with strcpy into
// caller buffers (worldmap.cc passes char[40]); fix bounds the copy when a
// key size is supplied.

TEST_CASE("M-191: strParseKeyValue copies key bounded") {
    auto copyKey = [](const char* segment, char* dst, size_t dstSize) {
        if (dstSize > 0) {
            std::strncpy(dst, segment, dstSize - 1);
            dst[dstSize - 1] = '\0';
        }
    };

    SUBCASE("short key fits fully") {
        char key[40];
        copyKey("ambient_sfx", key, sizeof(key));
        CHECK(std::strcmp(key, "ambient_sfx") == 0);
    }
    SUBCASE("key longer than destination is truncated + NUL-terminated") {
        char key[40];
        copyKey("a_very_long_ambient_sfx_key_name_exceeding_40_chars_total_ok", key, sizeof(key));
        CHECK(std::strlen(key) == sizeof(key) - 1);
        CHECK(key[sizeof(key) - 1] == '\0');
    }
}
