// Unit tests for automap.cc / automap.h — pipboy automap capacity for RPU
// (remaining-work item 4, P2): AUTOMAP_MAP_COUNT raised from 160 to 173 so
// RPU's 173 maps all get AUTOMAP.DB entries (maps 160-172 were silently
// excluded: EPA sublevels, SF Sheng's, Slaver Camp, safehouses, etc.).
//
// Self-contained test — does NOT link automap.cc (35+ engine dependencies:
// art, game, object, svga, window_manager...). Mirrors the automap index
// guard, display-map availability list, header dataSize derivation, and the
// pipboy mapCount clamp, while including automap.h for the REAL constants so
// static_assert cross-checks pin the production values at compile time.
//
// Regression pins:
//   (a) AUTOMAP_MAP_COUNT == 173 (compile-time static_assert + runtime CHECK)
//   (b) derived AUTOMAP.DB header dataSize for 173 entries:
//       5 + AUTOMAP_MAP_COUNT * ELEVATION_COUNT * sizeof(int) == 2081
//   (c) automapMapIndexIsValid(172) == true, automapMapIndexIsValid(173) == false
//   (d) pipboy clamp std::min(wmMapMaxCount(), AUTOMAP_MAP_COUNT) == 173 for a
//       173-map world, and stays bounded when wmMapMaxCount() > 173 (guards
//       retained for mods beyond RPU)
//   (e) _displayMapList zero-init tail: new indices 160-172 default to 0
//       ("available" — displayed in list), matching vanilla maps 3+ default.
//   (f) AUTOMAP_DB_VERSION == 2 — the on-disk format version bumped when the
//       count was raised, so stale version-1 AUTOMAP.DB.SAV files restored
//       from pre-change saves are detected and regenerated.
//   (g) version-check + regenerate decision mirror: old version → regenerate
//       (automapEnsureCurrent → automapCreate); matching version → use.
//   (h) header-size mismatch detection mirror: dataSize below the 2081-byte
//       header is rejected.
//
// This mirrors the automap.cc:73-81 / :310-321 / :1293-1298 / :1176-1208 /
// :1294-1313 / pipboy.cc:1814 / :1942 logic. The compile-time static_asserts
// against automap.h constants fail if the count is ever changed without
// updating this test.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cstring>

#include "automap.h"
#include "map_defs.h"

// ============================================================
// Compile-time cross-checks against the real production constants
// ============================================================

// (a) AUTOMAP_MAP_COUNT must be pinned at RPU's exact map count (173).
static_assert(AUTOMAP_MAP_COUNT == 173,
    "AUTOMAP_MAP_COUNT must match RPU's map count (maps.txt [Map 000]..[Map 172])");

// automap.cc:33 AUTOMAP_OFFSET_COUNT is derived from the count.
constexpr int TEST_AUTOMAP_OFFSET_COUNT = AUTOMAP_MAP_COUNT * ELEVATION_COUNT;
static_assert(TEST_AUTOMAP_OFFSET_COUNT == 173 * 3,
    "AUTOMAP_OFFSET_COUNT must be count * ELEVATION_COUNT");

// (b) automap.cc:1211 automapCreate dataSize — on-disk header size:
// version (1 byte) + dataSize (int, 4 bytes) + offsets[] ints.
// Old hardcoded value for 160 entries was 5 + 160*3*4 = 1925.
constexpr int TEST_AUTOMAP_HEADER_DATA_SIZE = 5 + TEST_AUTOMAP_OFFSET_COUNT * sizeof(int);
static_assert(TEST_AUTOMAP_HEADER_DATA_SIZE == 2081,
    "derived header dataSize for 173 entries must be 5 + 173*3*4 = 2081");

// Structural cross-check: the derived size equals 5 bytes + the byte size of
// the offsets array written by automapSaveHeader (automap.cc:1127) and read
// by automapLoadHeader (automap.cc:1154).
typedef struct TestAutomapHeader {
    unsigned char version;
    int dataSize;
    int offsets[AUTOMAP_MAP_COUNT][ELEVATION_COUNT];
} TestAutomapHeader;
static_assert(sizeof(TestAutomapHeader::offsets) == AUTOMAP_MAP_COUNT * ELEVATION_COUNT * sizeof(int),
    "offsets array byte size must match the on-disk offset table size");
static_assert(TEST_AUTOMAP_HEADER_DATA_SIZE == 5 + static_cast<int>(sizeof(TestAutomapHeader::offsets)),
    "derived dataSize must match the on-disk header layout written/read by automapSaveHeader/automapLoadHeader");

// (c) Boundary of the automap index range, derived from the real constant.
constexpr int TEST_AUTOMAP_LAST_VALID_MAP = AUTOMAP_MAP_COUNT - 1;
static_assert(TEST_AUTOMAP_LAST_VALID_MAP == 172,
    "last valid automap map index must be 172");

// (f) The on-disk AUTOMAP.DB format version must be pinned at 2: version 1
// was the vanilla 160-map layout (1925-byte header); raising the count to 173
// changed the serialized header (2081 bytes), so the version was bumped so
// stale version-1 DBs restored from pre-change saves are detected and
// regenerated instead of misread (automap.cc automapLoadHeader /
// automapEnsureCurrent / automapCreate).
static_assert(AUTOMAP_DB_VERSION == 2,
    "AUTOMAP_DB_VERSION must be 2 for the raised 173-entry layout");

// ============================================================
// Test-local mirrors of automap.cc / pipboy.cc logic
// ============================================================

namespace fallout {

// _displayMapList initializer mirror (automap.cc:91-252): first three entries
// are explicitly -1 ("not available"); the remaining entries — including the
// 13 new indices 160-172 — are zero-initialized to 0 ("available"). This
// mirrors how vanilla maps 3+ default to available; automapSetDisplayMap
// marks availability on visit / from maps.txt "automap" config entries.
static int testDisplayMapList[AUTOMAP_MAP_COUNT] = {
    -1,
    -1,
    -1,
};

} // namespace fallout

using namespace fallout;

// automap.cc:78-81 automapMapIndexIsValid
bool testAutomapMapIndexIsValid(int map)
{
    return map >= 0 && map < AUTOMAP_MAP_COUNT;
}

// automap.cc:310-321 _automapDisplayMap
int testAutomapDisplayMap(int map)
{
    if (!testAutomapMapIndexIsValid(map)) {
        return -1;
    }
    return testDisplayMapList[map];
}

// automap.cc:1293-1298 automapSetDisplayMap
void testAutomapSetDisplayMap(int map, bool available)
{
    if (map >= 0 && map < AUTOMAP_MAP_COUNT) {
        testDisplayMapList[map] = available ? 0 : -1;
    }
}

// pipboy.cc:1814 / 1942 — _PrintAMelevList / _PrintAMList clamp.
int testPipboyMapCount(int wmMapMaxCount)
{
    return std::min(wmMapMaxCount, AUTOMAP_MAP_COUNT);
}

// automap.cc:1176-1208 automapLoadHeader — the on-disk format version gate.
// A stale version (e.g. 1 from a pre-change save's AUTOMAP.DB.SAV) is
// rejected before dataSize/offsets are read; the load paths then regenerate
// the DB (automapEnsureCurrent → automapCreate).
bool testAutomapHeaderVersionIsCurrent(unsigned char version)
{
    return version == AUTOMAP_DB_VERSION;
}

// automap.cc:1176-1208 automapLoadHeader — header-size mismatch detection.
// A current-format DB always has dataSize >= the header size (a fresh DB is
// exactly the header; saved entries only add bytes). Anything smaller means
// a stale (version-1, 1925-byte header) or truncated file, and is rejected.
bool testAutomapHeaderSizeIsValid(int dataSize)
{
    return dataSize >= TEST_AUTOMAP_HEADER_DATA_SIZE;
}

// automap.cc:1294-1313 automapEnsureCurrent — the regenerate decision:
// matching version → use as-is; old/unknown version → regenerate.
enum class TestAutomapDbAction : int {
    kUse,
    kRegenerate,
};

TestAutomapDbAction testAutomapEnsureCurrentDecision(int version)
{
    return version == AUTOMAP_DB_VERSION ? TestAutomapDbAction::kUse : TestAutomapDbAction::kRegenerate;
}

// ============================================================
// TESTS
// ============================================================

TEST_CASE("automap capacity: AUTOMAP_MAP_COUNT matches RPU's 173 maps")
{
    // (a) Runtime pin in addition to the compile-time static_assert.
    CHECK(AUTOMAP_MAP_COUNT == 173);
    CHECK(TEST_AUTOMAP_LAST_VALID_MAP == 172);
}

TEST_CASE("automap header dataSize derivation")
{
    // (b) automapCreate (automap.cc:1211) now derives the header size instead
    // of the hardcoded 1925 for 160 entries.
    CHECK(TEST_AUTOMAP_HEADER_DATA_SIZE == 2081);
    CHECK(TEST_AUTOMAP_HEADER_DATA_SIZE == 5 + TEST_AUTOMAP_OFFSET_COUNT * static_cast<int>(sizeof(int)));

    // The saved header (automapSaveHeader automap.cc:1127) writes exactly
    // version + dataSize + offsets bytes, so fileTell() after a save would
    // equal the derived dataSize for a freshly created DB.
    int serializedBytes = 1 + 4 + AUTOMAP_MAP_COUNT * ELEVATION_COUNT * 4;
    CHECK(serializedBytes == TEST_AUTOMAP_HEADER_DATA_SIZE);
}

TEST_CASE("automapMapIndexIsValid boundary (172 valid, 173 rejected)")
{
    // (c) With the count raised, RPU's last map index 172 is valid and 173
    // (one past the end) is rejected — mirrors automap.cc:78-81.
    CHECK(testAutomapMapIndexIsValid(0));
    CHECK(testAutomapMapIndexIsValid(160));  // previously excluded
    CHECK(testAutomapMapIndexIsValid(171));  // previously excluded
    CHECK(testAutomapMapIndexIsValid(172));  // last valid map
    CHECK_FALSE(testAutomapMapIndexIsValid(173));
    CHECK_FALSE(testAutomapMapIndexIsValid(-1));
    CHECK_FALSE(testAutomapMapIndexIsValid(1000));
}

TEST_CASE("pipboy clamp for a 173-map world")
{
    // (d) RPU ships exactly 173 maps; wmMapMaxCount() returns 173, so the
    // clamp std::min(wmMapMaxCount(), AUTOMAP_MAP_COUNT) yields 173 — the
    // full RPU map set is iterable (maps 160-172 no longer cut off).
    CHECK(testPipboyMapCount(173) == 173);
    CHECK(testPipboyMapCount(0) == 0);

    // Guard retention: a mod with MORE than 173 maps is still clamped to 173
    // so automapHeader->offsets / _displayMapList are never indexed OOB.
    CHECK(testPipboyMapCount(174) == 173);
    CHECK(testPipboyMapCount(200) == 173);
    CHECK(testPipboyMapCount(1000) == 173);
}

TEST_CASE("_displayMapList zero-init tail for new indices 160-172")
{
    // (e) The 13 new indices fall in the zero-initialized tail of the array,
    // defaulting to 0 = "available" (displayed in the pipboy automap list),
    // matching how vanilla maps 3+ default. automapSetDisplayMap marks
    // availability on visit / from maps.txt automap config entries.
    for (int map = 160; map <= 172; map++) {
        CHECK(testAutomapDisplayMap(map) == 0);
    }

    // First three entries remain -1 ("not available") per the initializer.
    CHECK(testAutomapDisplayMap(0) == -1);
    CHECK(testAutomapDisplayMap(1) == -1);
    CHECK(testAutomapDisplayMap(2) == -1);
    // A vanilla defaulted map (index 3) is available.
    CHECK(testAutomapDisplayMap(3) == 0);

    // Out-of-range indices return -1 (safe-skip for mods > 173 maps).
    CHECK(testAutomapDisplayMap(173) == -1);
    CHECK(testAutomapDisplayMap(-1) == -1);
}

TEST_CASE("automapSetDisplayMap marks availability within the raised range")
{
    // automap.cc:1293-1298 — with the raised count, maps 160-172 are settable.
    testAutomapSetDisplayMap(172, false);
    CHECK(testAutomapDisplayMap(172) == -1);
    testAutomapSetDisplayMap(172, true);
    CHECK(testAutomapDisplayMap(172) == 0);

    testAutomapSetDisplayMap(160, false);
    CHECK(testAutomapDisplayMap(160) == -1);
    testAutomapSetDisplayMap(160, true);
    CHECK(testAutomapDisplayMap(160) == 0);

    // Out-of-range maps are ignored, not written.
    testAutomapSetDisplayMap(173, false);
    testAutomapSetDisplayMap(-1, false);
    CHECK(testAutomapDisplayMap(173) == -1);  // still rejected by the guard

    // Reset the shared mirror state for other test cases.
    testAutomapSetDisplayMap(172, true);
    testAutomapSetDisplayMap(160, true);
}

TEST_CASE("automap header size stability: struct layout matches on-disk format")
{
    // A freshly created AUTOMAP.DB header written by automapSaveHeader has
    // size 5 + AUTOMAP_OFFSET_COUNT*4 for ANY count; the derived dataSize in
    // automapCreate must equal the byte size of the in-memory struct's
    // serialized form so automapSaveCurrent's append (automap.cc:948:
    // dataSize += entryDataSize + 5) stays consistent.
    TestAutomapHeader header;
    header.version = AUTOMAP_DB_VERSION;
    header.dataSize = TEST_AUTOMAP_HEADER_DATA_SIZE;
    std::memset(header.offsets, 0, sizeof(header.offsets));

    int onDiskSize = 1 + 4 + AUTOMAP_MAP_COUNT * ELEVATION_COUNT * 4;
    CHECK(header.dataSize == onDiskSize);

    // The offsets table covers exactly the maps.txt range [0, 172].
    CHECK((int)(sizeof(header.offsets) / sizeof(header.offsets[0][0])) == 519);
}

TEST_CASE("automap DB format version pinned at 2 for the 173-entry layout")
{
    // (f) The on-disk AUTOMAP.DB format version. Version 1 was the vanilla
    // 160-map layout (header 1925 bytes); the count raise to 173 changed the
    // serialized header (2081 bytes), so the version byte was bumped. A
    // pre-change save restores a version-1 AUTOMAP.DB.SAV over the fresh v2
    // DB (loadsave.cc _SlotMap2Game); the version gate detects it so the
    // automap load paths regenerate instead of misreading the stale layout.
    CHECK(AUTOMAP_DB_VERSION == 2);
    CHECK(AUTOMAP_DB_VERSION != 1); // must differ from the vanilla layout

    // The header the new version gates is the raised 173-entry header.
    CHECK(TEST_AUTOMAP_HEADER_DATA_SIZE == 2081);
}

TEST_CASE("automap DB version-check + regenerate decision")
{
    // (g) Mirrors automapLoadHeader's version gate + automapEnsureCurrent's
    // decision: a version-1 DB (from a pre-change save's AUTOMAP.DB.SAV)
    // fails the version check → the load paths regenerate a fresh v2 DB; a
    // matching version-2 DB is used as-is.
    CHECK(testAutomapHeaderVersionIsCurrent(AUTOMAP_DB_VERSION));
    CHECK_FALSE(testAutomapHeaderVersionIsCurrent(1)); // pre-change save layout

    CHECK(testAutomapEnsureCurrentDecision(AUTOMAP_DB_VERSION) == TestAutomapDbAction::kUse);
    CHECK(testAutomapEnsureCurrentDecision(1) == TestAutomapDbAction::kRegenerate);
    CHECK(testAutomapEnsureCurrentDecision(0) == TestAutomapDbAction::kRegenerate);
    CHECK(testAutomapEnsureCurrentDecision(3) == TestAutomapDbAction::kRegenerate);
}

TEST_CASE("automap header-size mismatch detection")
{
    // (h) Mirrors automapLoadHeader's dataSize sanity check: a current-format
    // DB always has dataSize >= the header size (fresh DB = 2081; saved
    // entries only add bytes). The stale version-1 header is 1925 bytes →
    // rejected; truncated/corrupt headers below the header size are rejected
    // too, so garbage offsets are never trusted.
    CHECK(testAutomapHeaderSizeIsValid(TEST_AUTOMAP_HEADER_DATA_SIZE));       // 2081 — fresh v2 DB
    CHECK(testAutomapHeaderSizeIsValid(TEST_AUTOMAP_HEADER_DATA_SIZE + 100)); // 2181 — with entries
    CHECK_FALSE(testAutomapHeaderSizeIsValid(1925)); // stale version-1 header size
    CHECK_FALSE(testAutomapHeaderSizeIsValid(0));
    CHECK_FALSE(testAutomapHeaderSizeIsValid(-1));
}
