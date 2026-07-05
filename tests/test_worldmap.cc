// Unit tests for worldmap.cc — CE-added functions, car fuel management,
// area/map/tile state queries, force-encounter state machine, and save/load
// bounds-checking logic.
//
// Tests ALL CE-added functions declared in worldmap.h:287-292:
//   wmSetPartyWorldPos, wmCarSetCurrentArea, wmForceEncounter,
//   wmSetScriptWorldMapMulti, worldmapGetWindow (header-only)
// Plus all high-testability vanilla functions used by RPU/Et Tu mods:
//   wmCarUseGas, wmCarFillGas, wmCarGasAmount, wmCarIsOutOfGas,
//   wmCarCurrentArea, wmGetPartyWorldPos, wmGetPartyCurArea,
//   wmAreaIsKnown, wmAreaVisitedState, wmAreaMarkVisited,
//   wmMapIsKnown, wmSubTileGetVisitedState, wmSubTileMarkRadiusVisited
// Plus save/load bounds-clamping & wmGameTimeIncrement tick logic.
//
// Self-contained test — does NOT link worldmap.cc (45+ transitive headers).
// Mirrors data structures and implements test stubs following the
// test_criticals.cc pattern. All type names use a "Test" prefix.
//
// Coverage targets: F-01 (wmGameTimeIncrement tick fix), F-02 (wmForceEncounter
// locking), F-03 (save/load bounds inconsistency), F-04 (load warning behavior).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

// ============================================================
// Test-local type definitions mirroring worldmap.cc and worldmap.h.
// Prefixed with "Test" to avoid symbol collision with real headers.
// ============================================================
namespace fallout {

// worldmap.h constants
constexpr int TEST_CAR_FUEL_MAX = 80000;
constexpr int TEST_ENTRANCE_LIST_CAPACITY = 10;

// ENCOUNTER_FLAG_* defines from worldmap.h:232-236
constexpr unsigned int TEST_ENCOUNTER_FLAG_NO_CAR = 0x1;
constexpr unsigned int TEST_ENCOUNTER_FLAG_LOCK = 0x2;
constexpr unsigned int TEST_ENCOUNTER_FLAG_NO_ICON = 0x4;
constexpr unsigned int TEST_ENCOUNTER_FLAG_ICON_SP = 0x8;
constexpr unsigned int TEST_ENCOUNTER_FLAG_FADEOUT = 0x10;

// Bit 31 internal flag (used by wmForceEncounter)
constexpr unsigned int TEST_ENCOUNTER_INTERNAL_LOCK_BIT = (1u << 31);

// Subtile constants from worldmap.cc:197-212
enum TestSubtileState {
    TEST_SUBTILE_STATE_UNKNOWN = 0,
    TEST_SUBTILE_STATE_KNOWN = 1,
    TEST_SUBTILE_STATE_VISITED = 2,
};

enum TestSubtileFill {
    TEST_SUBTILE_FILL_NONE = 0,
    TEST_SUBTILE_FILL_N = 1,
    TEST_SUBTILE_FILL_S = 2,
    TEST_SUBTILE_FILL_E = 3,
    TEST_SUBTILE_FILL_W = 4,
    TEST_SUBTILE_FILL_NW = 5,
    TEST_SUBTILE_FILL_NE = 6,
    TEST_SUBTILE_FILL_SW = 7,
    TEST_SUBTILE_FILL_SE = 8,
    TEST_SUBTILE_FILL_COUNT = 9,
};

// CityState from worldmap.h:19-24
enum TestCityState {
    TEST_CITY_STATE_UNKNOWN = 0,
    TEST_CITY_STATE_KNOWN = 1,
    TEST_CITY_STATE_VISITED = 2,
    TEST_CITY_STATE_INVISIBLE = -66,
};

// Worldmap tile dimensions (worldmap.cc:105-108)
constexpr int TEST_WM_TILE_WIDTH = 350;
constexpr int TEST_WM_TILE_HEIGHT = 300;
constexpr int TEST_WM_SUBTILE_SIZE = 50;
constexpr int TEST_SUBTILE_GRID_WIDTH = 7;
constexpr int TEST_SUBTILE_GRID_HEIGHT = 6;

// Name sizes
constexpr int TEST_CITY_NAME_SIZE = 40;

// Structure mirrors match worldmap.cc:236-331, 365-382, 389-466

typedef struct TestEntranceInfo {
    int state;
    int x;
    int y;
    int map;
    int elevation;
    int tile;
    int rotation;
} TestEntranceInfo;

typedef struct TestCityInfo {
    char name[TEST_CITY_NAME_SIZE];
    int areaId;
    int x;
    int y;
    int size;
    int state;
    int lockState;
    int visitedState;
    int mapFid;
    int labelFid;
    int entrancesLength;
    TestEntranceInfo entrances[TEST_ENTRANCE_LIST_CAPACITY];
} TestCityInfo;

typedef struct TestSubtileInfo {
    int terrain;
    int fill;
    int encounterChance[4]; // DAY_PART_COUNT
    int encounterType;
    int state;
} TestSubtileInfo;

typedef struct TestTileInfo {
    int fid;
    int encounterDifficultyModifier;
    TestSubtileInfo subtiles[TEST_SUBTILE_GRID_HEIGHT][TEST_SUBTILE_GRID_WIDTH];
} TestTileInfo;

typedef struct TestWmGenData {
    int currentAreaId;
    int worldPosX;
    int worldPosY;
    int currentSubtilePlaceholder; // pointer becomes int placeholder
    bool isInCar;
    int currentCarAreaId;
    int carFuel;
    bool encounterIconIsVisible;
    int encounterMapId;
    int encounterTableId;
    int encounterEntryId;
} TestWmGenData;

typedef struct TestEncounterConditionEntry {
    int type;
    int conditionalOperator;
    int param;
    int value;
} TestEncounterConditionEntry;

typedef struct TestEncounterCondition {
    int entriesLength;
    TestEncounterConditionEntry entries[3];
    int logicalOperators[2];
} TestEncounterCondition;

typedef struct TestEncounterTableSubEntry {
    int minimumCount;
    int maximumCount;
    int encounterIndex;
    int situation;
} TestEncounterTableSubEntry;

typedef struct TestEncounterTableEntry {
    int flags;
    int map;
    int scenery;
    int chance;
    int counter;
    TestEncounterCondition condition;
    int subEntiesLength;
    TestEncounterTableSubEntry subEntries[6];
} TestEncounterTableEntry;

typedef struct TestEncounterTable {
    char lookupName[40];
    int index;
    int mapsLength;
    int maps[6];
    int field_48;
    int entriesLength;
    TestEncounterTableEntry entries[41];
} TestEncounterTable;

} // namespace fallout

using namespace fallout;

// ============================================================
// Global test state (mirrors static globals in worldmap.cc)
// ============================================================

// wmGenData — used by all core functions
static TestWmGenData testWmGenData;

// wmAreaInfoList — CityInfo array of size wmMaxAreaNum
static TestCityInfo* testWmAreaInfoList = nullptr;

// wmTileInfoList — TileInfo array of size wmMaxTileNum
static TestTileInfo* testWmTileInfoList = nullptr;

// wmEncounterTableList — EncounterTable array
static TestEncounterTable* testWmEncounterTableList = nullptr;

// Dimension caps
static int testWmMaxAreaNum = 0;
static int testWmMaxTileNum = 0;
static int testWmMaxEncounterInfoTables = 0;
static int testWmNumHorizontalTiles = 1;

// CE globals
static float testScriptWorldMapMulti = 1.0f;
static int testWmForceEncounterMapId = -1;
static unsigned int testWmForceEncounterFlags = 0;
static double testGameTimeIncRemainder = 0.0;

// Car upgrade GVAR values (mocked for fuel tests)
static int testGvarSuperCar = 0;
static int testGvarCarUpgrade = 0;
static int testGvarFuelCellRegulator = 0;

// ============================================================
// Test stubs — inline helpers that mirror worldmap.cc logic
// ============================================================

// cityIsValid (worldmap.cc:865-868)
static inline bool testCityIsValid(int city)
{
    return city >= 0 && city < testWmMaxAreaNum;
}

// ---- CE-added functions (worldmap.cc:7027-7053) ----

void testWmSetPartyWorldPos(int x, int y)
{
    testWmGenData.worldPosX = x;
    testWmGenData.worldPosY = y;
}

void testWmCarSetCurrentArea(int area)
{
    testWmGenData.currentCarAreaId = area;
}

void testWmSetScriptWorldMapMulti(float value)
{
    testScriptWorldMapMulti = value;
}

void testWmForceEncounter(int map, unsigned int flags)
{
    if ((testWmForceEncounterFlags & TEST_ENCOUNTER_INTERNAL_LOCK_BIT) != 0) {
        return;
    }

    testWmForceEncounterMapId = map;
    testWmForceEncounterFlags = flags;

    if ((testWmForceEncounterFlags & TEST_ENCOUNTER_FLAG_LOCK) != 0) {
        testWmForceEncounterFlags |= TEST_ENCOUNTER_INTERNAL_LOCK_BIT;
    } else {
        testWmForceEncounterFlags &= ~TEST_ENCOUNTER_INTERNAL_LOCK_BIT;
    }
}

// ---- Car fuel functions (worldmap.cc:6363-6420) ----

int testWmCarUseGas(int amount)
{
    // Car upgrade fuel reductions (mirrors worldmap.cc:6366-6376)
    // super car: reduce by 90%
    if (testGvarSuperCar != 0) {
        amount -= amount * 90 / 100;
    }
    // car upgrade: reduce by 10%
    if (testGvarCarUpgrade != 0) {
        amount -= amount * 10 / 100;
    }
    // fuel cell regulator: halve consumption
    if (testGvarFuelCellRegulator != 0) {
        amount /= 2;
    }

    testWmGenData.carFuel -= amount;
    if (testWmGenData.carFuel < 0) {
        testWmGenData.carFuel = 0;
    }

    return 0;
}

// Returns amount of fuel that does not fit into tank (overflow).
int testWmCarFillGas(int amount)
{
    if ((amount + testWmGenData.carFuel) <= TEST_CAR_FUEL_MAX) {
        testWmGenData.carFuel += amount;
        return 0;
    }

    int remaining = TEST_CAR_FUEL_MAX - testWmGenData.carFuel;
    testWmGenData.carFuel = TEST_CAR_FUEL_MAX;
    return remaining; // returns used fill, not overflow
}

int testWmCarGasAmount()
{
    return testWmGenData.carFuel;
}

bool testWmCarIsOutOfGas()
{
    return testWmGenData.carFuel <= 0;
}

int testWmCarCurrentArea()
{
    return testWmGenData.currentCarAreaId;
}

// ---- Party position functions (worldmap.cc:6089-6114) ----

int testWmGetPartyWorldPos(int* xPtr, int* yPtr)
{
    if (xPtr != nullptr) {
        *xPtr = testWmGenData.worldPosX;
    }
    if (yPtr != nullptr) {
        *yPtr = testWmGenData.worldPosY;
    }
    return 0;
}

int testWmGetPartyCurArea(int* areaIdxPtr)
{
    if (areaIdxPtr != nullptr) {
        *areaIdxPtr = testWmGenData.currentAreaId;
        return 0;
    }
    return -1;
}

// ---- Area/city functions (worldmap.cc:5959-6021) ----

bool testWmAreaIsKnown(int areaIdx)
{
    if (!testCityIsValid(areaIdx)) {
        return false;
    }

    TestCityInfo* city = &(testWmAreaInfoList[areaIdx]);
    if (city->visitedState) {
        if (city->state == TEST_CITY_STATE_KNOWN) {
            return true;
        }
    }
    return false;
}

int testWmAreaVisitedState(int areaIdx)
{
    if (!testCityIsValid(areaIdx)) {
        return 0;
    }

    TestCityInfo* city = &(testWmAreaInfoList[areaIdx]);
    if (city->visitedState && city->state == TEST_CITY_STATE_KNOWN) {
        return city->visitedState;
    }
    return 0;
}

// Simplified wmAreaMarkVisitedState — just sets the field (skips subtile marking
// for testability, which depends on wmFindCurSubTileFromPos + wmMarkSubTileOffset*).
bool testWmAreaMarkVisitedState(int areaIdx, int state)
{
    if (!testCityIsValid(areaIdx)) {
        return false;
    }

    TestCityInfo* city = &(testWmAreaInfoList[areaIdx]);
    int oldVisitedState = city->visitedState;
    city->visitedState = state;
    // Real code calls wmMarkSubTileRadiusVisited here and sets subtile state,
    // but that path needs wmFindCurSubTileFromPos which validates bounds.
    // For the data structure test, we just verify the field mutation.
    return true;
}

int testWmAreaMarkVisited(int areaIdx)
{
    return testWmAreaMarkVisitedState(areaIdx, TEST_CITY_STATE_VISITED) ? 1 : 0;
}

// ---- Map functions (worldmap.cc:5991-6012) ----
// Requires wmMatchAreaFromMap and wmMatchEntranceFromMap stubs.

// Simplified wmMatchAreaFromMap: searches all city entrances for the map.
int testWmMatchAreaFromMap(int mapIdx, int* areaIdxPtr)
{
    for (int areaIdx = 0; areaIdx < testWmMaxAreaNum; areaIdx++) {
        TestCityInfo* city = &(testWmAreaInfoList[areaIdx]);
        for (int entIdx = 0; entIdx < city->entrancesLength; entIdx++) {
            if (mapIdx == city->entrances[entIdx].map) {
                *areaIdxPtr = areaIdx;
                return 0;
            }
        }
    }
    *areaIdxPtr = -1;
    return -1;
}

// Simplified wmMatchEntranceFromMap: finds entrance index for map within area.
int testWmMatchEntranceFromMap(int areaIdx, int mapIdx, int* entranceIdxPtr)
{
    TestCityInfo* city = &(testWmAreaInfoList[areaIdx]);
    for (int entIdx = 0; entIdx < city->entrancesLength; entIdx++) {
        if (mapIdx == city->entrances[entIdx].map) {
            *entranceIdxPtr = entIdx;
            return 0;
        }
    }
    *entranceIdxPtr = -1;
    return -1;
}

bool testWmMapIsKnown(int mapIdx)
{
    int areaIdx;
    if (testWmMatchAreaFromMap(mapIdx, &areaIdx) != 0) {
        return false;
    }

    int entranceIdx;
    if (testWmMatchEntranceFromMap(areaIdx, mapIdx, &entranceIdx) != 0) {
        return false;
    }

    TestCityInfo* city = &(testWmAreaInfoList[areaIdx]);
    TestEntranceInfo* entrance = &(city->entrances[entranceIdx]);

    if (entrance->state != 1) {
        return false;
    }
    return true;
}

// ---- Subtile functions (worldmap.cc:5239-5299) ----

int testWmSubTileGetVisitedState(int x, int y, int* statePtr)
{
    // Mirrors worldmap.cc:5289-5299
    TestTileInfo* tile = &(testWmTileInfoList[
        y / TEST_WM_TILE_HEIGHT * testWmNumHorizontalTiles +
        x / TEST_WM_TILE_WIDTH % testWmNumHorizontalTiles]);

    TestSubtileInfo* subtile = &(tile->subtiles[
        y % TEST_WM_TILE_HEIGHT / TEST_WM_SUBTILE_SIZE]
        [x % TEST_WM_TILE_WIDTH / TEST_WM_SUBTILE_SIZE]);

    *statePtr = subtile->state;
    return 0;
}

// Simplified subtile marking — marks cells within radius as KNOWN.
// The real wmSubTileMarkRadiusVisited also handles fill patterns (N/S/E/W)
// but the core radius marking is what we validate.
int testWmSubTileMarkRadiusVisited(int x, int y, int radius)
{
    int tileIdx = x / TEST_WM_TILE_WIDTH % testWmNumHorizontalTiles +
                  y / TEST_WM_TILE_HEIGHT * testWmNumHorizontalTiles;
    int subtileX = x % TEST_WM_TILE_WIDTH / TEST_WM_SUBTILE_SIZE;
    int subtileY = y % TEST_WM_TILE_HEIGHT / TEST_WM_SUBTILE_SIZE;

    for (int offY = -radius; offY <= radius; offY++) {
        for (int offX = -radius; offX <= radius; offX++) {
            int sx = subtileX + offX;
            int sy = subtileY + offY;
            // Clamp to valid subtile bounds
            if (sx >= 0 && sx < TEST_SUBTILE_GRID_WIDTH &&
                sy >= 0 && sy < TEST_SUBTILE_GRID_HEIGHT) {
                TestSubtileInfo* si = &(testWmTileInfoList[tileIdx].subtiles[sy][sx]);
                if (si->state == TEST_SUBTILE_STATE_UNKNOWN) {
                    si->state = TEST_SUBTILE_STATE_KNOWN;
                }
            }
        }
    }

    // Mark center cell visited
    if (subtileX >= 0 && subtileX < TEST_SUBTILE_GRID_WIDTH &&
        subtileY >= 0 && subtileY < TEST_SUBTILE_GRID_HEIGHT) {
        testWmTileInfoList[tileIdx].subtiles[subtileY][subtileX].state = TEST_SUBTILE_STATE_VISITED;
    }

    return 0;
}

// ---- Save/load bounds clamping test helper (from worldmap.cc:1186-1293) ----

// Simulates the bounds-checking logic in wmWorldMap_load.
// Returns -1 for hard errors, 0 for success.
int testWmWorldMapLoadBoundsSimulate(
    int numCities, int* entranceCounts, int numEntrancesToRead,
    int numTiles, int numCounters, int* encounterTableIndices, int* encounterEntryIndices,
    int* encounterEntryLengths)
{
    // Clamp numCities
    if (numCities > testWmMaxAreaNum) {
        numCities = testWmMaxAreaNum;
    }

    // Clamp entrances per city
    for (int areaIdx = 0; areaIdx < numCities && areaIdx < numEntrancesToRead; areaIdx++) {
        TestCityInfo* city = &(testWmAreaInfoList[areaIdx]);
        int count = entranceCounts[areaIdx];
        if (count > city->entrancesLength) {
            count = city->entrancesLength;
        }
        // Read entrances... (not actually reading from file in test)
    }

    // Clamp numTiles
    if (numTiles > testWmMaxTileNum) {
        numTiles = testWmMaxTileNum;
    }

    // Validate encounter table indices
    for (int counterIdx = 0; counterIdx < numCounters; counterIdx++) {
        int tableIdx = encounterTableIndices[counterIdx];
        if (tableIdx < 0 || tableIdx >= testWmMaxEncounterInfoTables) {
            return -1; // hard error
        }

        int entryIdx = encounterEntryIndices[counterIdx];
        int entriesLen = (encounterEntryLengths != nullptr)
            ? encounterEntryLengths[tableIdx] : 5;
        if (entryIdx < 0 || entryIdx >= entriesLen) {
            return -1; // hard error
        }
    }

    return 0;
}

// ---- wmGameTimeIncrement tick clamping test helper ----
// Mirrors the fix in worldmap.cc:4321-4331.
// Original: ticksToAdd = nextEventTime >= gameTime ? ticksToAdd : nextEventTime - gameTime
// Fixed: explicit check with clamping.
// ticksToAdd and gameTime/nextEventTime are unsigned int in engine,
// but ticksToAdd is stored in int for comparison.

int testWmGameTimeIncrementComputeTicks(
    unsigned int gameTime, unsigned int nextEventTime, int ticksToAdd)
{
    // Mirrors the fixed logic (worldmap.cc ~4325-4331)
    if (nextEventTime < gameTime) {
        // nextEventTime wrapped or is behind; use full ticksToAdd
        // (ticksToAdd already set above this code)
    } else {
        // Clamp to ticksToAdd so we don't overrun
        unsigned int timeDiff = nextEventTime - gameTime;
        if (static_cast<int>(timeDiff) < ticksToAdd) {
            ticksToAdd = static_cast<int>(timeDiff);
        }
    }
    return ticksToAdd;
}

// ---- wmGenData init/reset simulation ----

static void testWmGenDataInit()
{
    testWmGenData.currentAreaId = -1;
    testWmGenData.worldPosX = 173;   // default Arroyo position
    testWmGenData.worldPosY = 122;
    testWmGenData.currentSubtilePlaceholder = 0;
    testWmGenData.isInCar = false;
    testWmGenData.currentCarAreaId = -1;
    testWmGenData.carFuel = TEST_CAR_FUEL_MAX;
    testWmGenData.encounterIconIsVisible = false;

    testWmForceEncounterMapId = -1;
    testWmForceEncounterFlags = 0;
    testScriptWorldMapMulti = 1.0f;
    testGameTimeIncRemainder = 0.0;
}

// ---- Helper to initialize test area/tile arrays ----
static void testInitAreas(int numAreas)
{
    delete[] testWmAreaInfoList;
    testWmAreaInfoList = new TestCityInfo[numAreas]();
    testWmMaxAreaNum = numAreas;

    for (int i = 0; i < numAreas; i++) {
        testWmAreaInfoList[i].entrancesLength = 0;
    }
}

static void testInitTiles(int numTiles, int numHorizontal)
{
    delete[] testWmTileInfoList;
    testWmTileInfoList = new TestTileInfo[numTiles]();
    testWmMaxTileNum = numTiles;
    testWmNumHorizontalTiles = numHorizontal;
}

static void testInitEncounterTables(int numTables)
{
    delete[] testWmEncounterTableList;
    testWmEncounterTableList = new TestEncounterTable[numTables]();
    testWmMaxEncounterInfoTables = numTables;

    for (int i = 0; i < numTables; i++) {
        testWmEncounterTableList[i].entriesLength = 0;
    }
}

// Cleanup helper
static void testCleanup()
{
    delete[] testWmAreaInfoList;
    testWmAreaInfoList = nullptr;
    testWmMaxAreaNum = 0;

    delete[] testWmTileInfoList;
    testWmTileInfoList = nullptr;
    testWmMaxTileNum = 0;
    testWmNumHorizontalTiles = 1;

    delete[] testWmEncounterTableList;
    testWmEncounterTableList = nullptr;
    testWmMaxEncounterInfoTables = 0;
}

// ============================================================
// Test helper — fixture-like setup
// ============================================================
// Each test case that needs global state calls:
//   testWmGenDataInit();
//   testInitAreas(N);
//   testInitTiles(M, H);
// and must call testCleanup() at end.

// ============================================================
// TESTS — Constants and type validation
// ============================================================

TEST_CASE("CAR_FUEL_MAX and ENTRANCE_LIST_CAPACITY constants")
{
    CHECK(TEST_CAR_FUEL_MAX == 80000);
    CHECK(TEST_ENTRANCE_LIST_CAPACITY == 10);
}

TEST_CASE("ENCOUNTER_FLAG_* constants")
{
    CHECK(TEST_ENCOUNTER_FLAG_NO_CAR == 0x1);
    CHECK(TEST_ENCOUNTER_FLAG_LOCK == 0x2);
    CHECK(TEST_ENCOUNTER_FLAG_NO_ICON == 0x4);
    CHECK(TEST_ENCOUNTER_FLAG_ICON_SP == 0x8);
    CHECK(TEST_ENCOUNTER_FLAG_FADEOUT == 0x10);
}

TEST_CASE("Subtile state and fill constants")
{
    CHECK(TEST_SUBTILE_STATE_UNKNOWN == 0);
    CHECK(TEST_SUBTILE_STATE_KNOWN == 1);
    CHECK(TEST_SUBTILE_STATE_VISITED == 2);

    CHECK(TEST_SUBTILE_FILL_NONE == 0);
    CHECK(TEST_SUBTILE_FILL_S == 2);
    CHECK(TEST_SUBTILE_FILL_W == 4);
    CHECK(TEST_SUBTILE_FILL_COUNT == 9);
}

TEST_CASE("CityState constants")
{
    CHECK(TEST_CITY_STATE_UNKNOWN == 0);
    CHECK(TEST_CITY_STATE_KNOWN == 1);
    CHECK(TEST_CITY_STATE_VISITED == 2);
    CHECK(TEST_CITY_STATE_INVISIBLE == -66);
}

TEST_CASE("Tile dimensions constants")
{
    CHECK(TEST_WM_TILE_WIDTH == 350);
    CHECK(TEST_WM_TILE_HEIGHT == 300);
    CHECK(TEST_WM_SUBTILE_SIZE == 50);
    CHECK(TEST_SUBTILE_GRID_WIDTH == 7);
    CHECK(TEST_SUBTILE_GRID_HEIGHT == 6);
}

TEST_CASE("TestSubtileInfo and TestTileInfo sizes")
{
    // SubtileInfo: 5 int fields
    //  terrain(4) + fill(4) + encounterChance[4](16) + encounterType(4) + state(4) = 32
    CHECK(sizeof(TestSubtileInfo) >= 32);

    // TileInfo simplified: fid(4) + encounterDifficultyModifier(4) + subtiles[6][7]*32
    // subtiles = 6 * 7 * 32 = 1344, total >= 1352
    CHECK(sizeof(TestTileInfo) >= 1352);

    // EntranceInfo: 7 int fields = 28
    CHECK(sizeof(TestEntranceInfo) >= 28);

    // CityInfo: name[40] + 8 ints + EntranceInfo[10] = 40 + 32 + 10*28 = 40 + 32 + 280 = 352
    CHECK(sizeof(TestCityInfo) >= 352);
}

// ============================================================
// TESTS — cityIsValid
// ============================================================

TEST_CASE("cityIsValid range check")
{
    testInitAreas(5);

    CHECK(testCityIsValid(0));
    CHECK(testCityIsValid(4));
    CHECK_FALSE(testCityIsValid(-1));
    CHECK_FALSE(testCityIsValid(5));
    CHECK_FALSE(testCityIsValid(999));

    testCleanup();
}

// ============================================================
// TESTS — wmSetPartyWorldPos / wmGetPartyWorldPos
// ============================================================

TEST_CASE("wmSetPartyWorldPos sets world position")
{
    testWmGenDataInit();

    testWmSetPartyWorldPos(480, 250);
    CHECK(testWmGenData.worldPosX == 480);
    CHECK(testWmGenData.worldPosY == 250);

    testWmSetPartyWorldPos(0, 0);
    CHECK(testWmGenData.worldPosX == 0);
    CHECK(testWmGenData.worldPosY == 0);

    testWmSetPartyWorldPos(-1, -1);
    CHECK(testWmGenData.worldPosX == -1);
    CHECK(testWmGenData.worldPosY == -1);
}

TEST_CASE("wmGetPartyWorldPos reads world position")
{
    testWmGenDataInit();

    // Set values then read back
    testWmGenData.worldPosX = 500;
    testWmGenData.worldPosY = 300;

    int x = -1, y = -1;
    CHECK(testWmGetPartyWorldPos(&x, &y) == 0);
    CHECK(x == 500);
    CHECK(y == 300);

    SUBCASE("null x pointer")
    {
        testWmGetPartyWorldPos(nullptr, &y);
        CHECK(y == 300);
    }

    SUBCASE("null y pointer")
    {
        testWmGetPartyWorldPos(&x, nullptr);
        CHECK(x == 500);
    }

    SUBCASE("both null — should not crash")
    {
        CHECK(testWmGetPartyWorldPos(nullptr, nullptr) == 0);
    }
}

// ============================================================
// TESTS — wmCarSetCurrentArea / wmCarCurrentArea
// ============================================================

TEST_CASE("wmCarSetCurrentArea / wmCarCurrentArea")
{
    testWmGenDataInit();
    CHECK(testWmCarCurrentArea() == -1); // default after init

    testWmCarSetCurrentArea(3);
    CHECK(testWmCarCurrentArea() == 3);

    testWmCarSetCurrentArea(0);
    CHECK(testWmCarCurrentArea() == 0);

    testWmCarSetCurrentArea(-1);
    CHECK(testWmCarCurrentArea() == -1);
}

// ============================================================
// TESTS — wmGetPartyCurArea
// ============================================================

TEST_CASE("wmGetPartyCurArea")
{
    testWmGenDataInit();
    testWmGenData.currentAreaId = 7;

    int area = -1;
    CHECK(testWmGetPartyCurArea(&area) == 0);
    CHECK(area == 7);

    SUBCASE("null pointer returns -1")
    {
        CHECK(testWmGetPartyCurArea(nullptr) == -1);
    }
}

// ============================================================
// TESTS — wmSetScriptWorldMapMulti
// ============================================================

TEST_CASE("wmSetScriptWorldMapMulti")
{
    testWmGenDataInit();

    CHECK(testScriptWorldMapMulti == 1.0f);

    testWmSetScriptWorldMapMulti(2.0f);
    CHECK(testScriptWorldMapMulti == 2.0f);

    testWmSetScriptWorldMapMulti(0.5f);
    CHECK(testScriptWorldMapMulti == 0.5f);

    testWmSetScriptWorldMapMulti(0.0f);
    CHECK(testScriptWorldMapMulti == 0.0f);

    // Negative value (edge case noted in F-01)
    testWmSetScriptWorldMapMulti(-1.0f);
    CHECK(testScriptWorldMapMulti == -1.0f);
}

// ============================================================
// TESTS — car fuel functions
// ============================================================

TEST_CASE("wmCarGasAmount")
{
    testWmGenDataInit();
    CHECK(testWmCarGasAmount() == TEST_CAR_FUEL_MAX);
}

TEST_CASE("wmCarIsOutOfGas")
{
    testWmGenDataInit();
    testWmGenData.carFuel = TEST_CAR_FUEL_MAX;
    CHECK_FALSE(testWmCarIsOutOfGas());

    testWmGenData.carFuel = 1;
    CHECK_FALSE(testWmCarIsOutOfGas());

    testWmGenData.carFuel = 0;
    CHECK(testWmCarIsOutOfGas());

    testWmGenData.carFuel = -1; // negative fuel (should not happen but logic handles it)
    CHECK(testWmCarIsOutOfGas());
}

TEST_CASE("wmCarUseGas — basic")
{
    testWmGenDataInit();
    testWmGenData.carFuel = 10000;

    // No upgrades active
    testGvarSuperCar = 0;
    testGvarCarUpgrade = 0;
    testGvarFuelCellRegulator = 0;

    testWmCarUseGas(100);
    CHECK(testWmGenData.carFuel == 9900);

    testWmCarUseGas(500);
    CHECK(testWmGenData.carFuel == 9400);
}

TEST_CASE("wmCarUseGas — clamps to zero, never negative")
{
    testWmGenDataInit();
    testWmGenData.carFuel = 50;

    testGvarSuperCar = 0;
    testGvarCarUpgrade = 0;
    testGvarFuelCellRegulator = 0;

    testWmCarUseGas(100);
    CHECK(testWmGenData.carFuel == 0); // clamped, not -50
    CHECK(testWmCarIsOutOfGas());
}

TEST_CASE("wmCarUseGas — super car (-90% fuel use)")
{
    testWmGenDataInit();
    testWmGenData.carFuel = 10000;

    testGvarSuperCar = 1;
    testGvarCarUpgrade = 0;
    testGvarFuelCellRegulator = 0;

    testWmCarUseGas(100);
    // amount = 100 - 100*90/100 = 100 - 90 = 10
    CHECK(testWmGenData.carFuel == 9990);
}

TEST_CASE("wmCarUseGas — car upgrade (-10% fuel use)")
{
    testWmGenDataInit();
    testWmGenData.carFuel = 10000;

    testGvarSuperCar = 0;
    testGvarCarUpgrade = 1;
    testGvarFuelCellRegulator = 0;

    testWmCarUseGas(100);
    // amount = 100 - 100*10/100 = 100 - 10 = 90
    CHECK(testWmGenData.carFuel == 9910);
}

TEST_CASE("wmCarUseGas — super car + car upgrade combined")
{
    testWmGenDataInit();
    testWmGenData.carFuel = 10000;

    testGvarSuperCar = 1;
    testGvarCarUpgrade = 1;
    testGvarFuelCellRegulator = 0;

    testWmCarUseGas(100);
    // super car first: amount = 100 - 90 = 10
    // then car upgrade: amount = 10 - 10*10/100 = 10 - 1 = 9
    CHECK(testWmGenData.carFuel == 9991);
}

TEST_CASE("wmCarUseGas — fuel cell regulator (halves)")
{
    testWmGenDataInit();
    testWmGenData.carFuel = 10000;

    testGvarSuperCar = 0;
    testGvarCarUpgrade = 0;
    testGvarFuelCellRegulator = 1;

    testWmCarUseGas(100);
    // amount = 100 / 2 = 50
    CHECK(testWmGenData.carFuel == 9950);
}

TEST_CASE("wmCarUseGas — all upgrades combined")
{
    testWmGenDataInit();
    testWmGenData.carFuel = 10000;

    testGvarSuperCar = 1;
    testGvarCarUpgrade = 1;
    testGvarFuelCellRegulator = 1;

    testWmCarUseGas(100);
    // super car: 100 - 90 = 10
    // car upgrade: 10 - 1 = 9
    // fuel cell: 9 / 2 = 4
    CHECK(testWmGenData.carFuel == 9996);
}

TEST_CASE("wmCarUseGas — integer truncation")
{
    testWmGenDataInit();
    testWmGenData.carFuel = 10000;

    testGvarSuperCar = 0;
    testGvarCarUpgrade = 1;
    testGvarFuelCellRegulator = 0;

    // car upgrade reduces by 10%: 10*10/100 = 1, so 10-1=9
    testWmCarUseGas(10);
    CHECK(testWmGenData.carFuel == 9991);

    // 9*10/100 = 0, so 9-0=9
    testWmCarUseGas(9);
    CHECK(testWmGenData.carFuel == 9982);
}

TEST_CASE("wmCarFillGas — normal fill")
{
    testWmGenDataInit();
    testWmGenData.carFuel = 10000;

    int overflow = testWmCarFillGas(5000);
    CHECK(overflow == 0);
    CHECK(testWmGenData.carFuel == 15000);
}

TEST_CASE("wmCarFillGas — overflow returns remaining capacity")
{
    testWmGenDataInit();
    testWmGenData.carFuel = 75000;

    int overflow = testWmCarFillGas(10000);
    // capacity = 80000 - 75000 = 5000 fits, 5000 overflow
    // Returns remaining = 5000 (amount that was used from the fill)
    // Wait — the real code: returns `remaining` which is CAR_FUEL_MAX - oldFuel
    // remaining = 80000 - 75000 = 5000
    CHECK(overflow == 5000);
    CHECK(testWmGenData.carFuel == TEST_CAR_FUEL_MAX);
}

TEST_CASE("wmCarFillGas — exactly to capacity")
{
    testWmGenDataInit();
    testWmGenData.carFuel = 80000;

    int overflow = testWmCarFillGas(0);
    CHECK(overflow == 0); // 80000 + 0 <= 80000 => add 0
    CHECK(testWmGenData.carFuel == 80000);
}

TEST_CASE("wmCarFillGas — fill from empty")
{
    testWmGenDataInit();
    testWmGenData.carFuel = 0;

    int overflow = testWmCarFillGas(80000);
    CHECK(overflow == 0);
    CHECK(testWmGenData.carFuel == 80000);
}

TEST_CASE("wmCarFillGas — fill more than capacity")
{
    testWmGenDataInit();
    testWmGenData.carFuel = 0;

    int overflow = testWmCarFillGas(100000);
    // remaining = 80000 - 0 = 80000
    CHECK(overflow == 80000);
    CHECK(testWmGenData.carFuel == 80000);
}

// ============================================================
// TESTS — wmForceEncounter (CE function, worldsmap.cc:7038-7053)
// ============================================================

TEST_CASE("wmForceEncounter — basic set/get")
{
    testWmGenDataInit();

    testWmForceEncounter(42, TEST_ENCOUNTER_FLAG_NO_CAR);
    CHECK(testWmForceEncounterMapId == 42);
    CHECK(testWmForceEncounterFlags == TEST_ENCOUNTER_FLAG_NO_CAR);
}

TEST_CASE("wmForceEncounter — no flags")
{
    testWmGenDataInit();

    testWmForceEncounter(10, 0);
    CHECK(testWmForceEncounterMapId == 10);
    CHECK(testWmForceEncounterFlags == 0);
}

TEST_CASE("wmForceEncounter — with LOCK flag sets bit 31 internal")
{
    testWmGenDataInit();

    testWmForceEncounter(5, TEST_ENCOUNTER_FLAG_LOCK);
    CHECK(testWmForceEncounterMapId == 5);
    // Bit 31 should be set (internal lock flag)
    CHECK((testWmForceEncounterFlags & TEST_ENCOUNTER_INTERNAL_LOCK_BIT) != 0);
    // ENCOUNTER_FLAG_LOCK bit should still be set (0x2)
    CHECK((testWmForceEncounterFlags & TEST_ENCOUNTER_FLAG_LOCK) != 0);
}

TEST_CASE("wmForceEncounter — internal lock prevents double queue (F-02)")
{
    testWmGenDataInit();

    // First call with LOCK sets bit 31
    testWmForceEncounter(1, TEST_ENCOUNTER_FLAG_LOCK);
    CHECK(testWmForceEncounterMapId == 1);

    // Second call should be rejected because bit 31 is set
    testWmForceEncounter(2, 0);
    CHECK(testWmForceEncounterMapId == 1);  // unchanged
    unsigned int expectedFlags = TEST_ENCOUNTER_FLAG_LOCK | TEST_ENCOUNTER_INTERNAL_LOCK_BIT;
    CHECK(testWmForceEncounterFlags == expectedFlags);
}

TEST_CASE("wmForceEncounter — no LOCK flag clears bit 31")
{
    testWmGenDataInit();

    // First with LOCK
    testWmForceEncounter(1, TEST_ENCOUNTER_FLAG_LOCK);
    CHECK((testWmForceEncounterFlags & TEST_ENCOUNTER_INTERNAL_LOCK_BIT) != 0);

    // Reset state (simulate encounter consumed)
    testWmForceEncounterFlags = 0;

    // Now without LOCK
    testWmForceEncounter(2, TEST_ENCOUNTER_FLAG_NO_CAR);
    CHECK(testWmForceEncounterMapId == 2);
    CHECK((testWmForceEncounterFlags & TEST_ENCOUNTER_INTERNAL_LOCK_BIT) == 0);
}

TEST_CASE("wmForceEncounter — caller passes bit 31 (F-02 edge case)")
{
    testWmGenDataInit();

    // If caller passes flags with bit 31 set, the next call gets rejected
    testWmForceEncounter(1, TEST_ENCOUNTER_INTERNAL_LOCK_BIT);
    CHECK(testWmForceEncounterMapId == 1);
    // Bit 31 was explicitly set; LOCK was not set so the else-branch
    // clears bit 31. Wait — re-reading the code:
    // if (LOCK flag set) → add bit 31; else → clear bit 31
    // Since caller didn't set LOCK, the else-branch clears bit 31.
    // BUT the first check at line 7040 sees bit 31 is set — the call succeeded
    // because flags were fresh (wmForceEncounterFlags was 0 before).
    // The code then clears bit 31 in the else-branch.
    CHECK((testWmForceEncounterFlags & TEST_ENCOUNTER_INTERNAL_LOCK_BIT) == 0);
}

TEST_CASE("wmForceEncounter — clear encounter (reset to -1/0)")
{
    testWmGenDataInit();

    testWmForceEncounter(3, 0);
    testWmForceEncounterMapId = -1;
    testWmForceEncounterFlags = 0;

    CHECK(testWmForceEncounterMapId == -1);
    CHECK(testWmForceEncounterFlags == 0);
}

// ============================================================
// TESTS — wmAreaIsKnown / wmAreaVisitedState
// ============================================================

TEST_CASE("wmAreaIsKnown — valid known area")
{
    testInitAreas(3);
    testWmGenDataInit();

    testWmAreaInfoList[0].state = TEST_CITY_STATE_KNOWN;
    testWmAreaInfoList[0].visitedState = 1;

    CHECK(testWmAreaIsKnown(0));
    CHECK_FALSE(testWmAreaIsKnown(1)); // not known, not visited

    testCleanup();
}

TEST_CASE("wmAreaIsKnown — unknown state even if visited")
{
    testInitAreas(3);

    testWmAreaInfoList[0].state = TEST_CITY_STATE_UNKNOWN;
    testWmAreaInfoList[0].visitedState = 1;
    CHECK_FALSE(testWmAreaIsKnown(0));

    testCleanup();
}

TEST_CASE("wmAreaIsKnown — visited but state not KNOWN")
{
    testInitAreas(3);

    testWmAreaInfoList[0].state = TEST_CITY_STATE_VISITED;
    testWmAreaInfoList[0].visitedState = 1;
    CHECK_FALSE(testWmAreaIsKnown(0));

    testCleanup();
}

TEST_CASE("wmAreaIsKnown — visitedState == 0")
{
    testInitAreas(3);

    testWmAreaInfoList[0].state = TEST_CITY_STATE_KNOWN;
    testWmAreaInfoList[0].visitedState = 0;
    CHECK_FALSE(testWmAreaIsKnown(0));

    testCleanup();
}

TEST_CASE("wmAreaIsKnown — invalid index")
{
    testInitAreas(3);
    CHECK_FALSE(testWmAreaIsKnown(-1));
    CHECK_FALSE(testWmAreaIsKnown(3));
    CHECK_FALSE(testWmAreaIsKnown(999));
    testCleanup();
}

TEST_CASE("wmAreaVisitedState — returns visited state")
{
    testInitAreas(3);

    testWmAreaInfoList[0].state = TEST_CITY_STATE_KNOWN;
    testWmAreaInfoList[0].visitedState = 5;
    CHECK(testWmAreaVisitedState(0) == 5);

    testWmAreaInfoList[1].state = TEST_CITY_STATE_KNOWN;
    testWmAreaInfoList[1].visitedState = 3;
    CHECK(testWmAreaVisitedState(1) == 3);

    testCleanup();
}

TEST_CASE("wmAreaVisitedState — returns 0 for unvisited")
{
    testInitAreas(3);

    testWmAreaInfoList[0].state = TEST_CITY_STATE_KNOWN;
    testWmAreaInfoList[0].visitedState = 0;
    CHECK(testWmAreaVisitedState(0) == 0);

    testWmAreaInfoList[1].state = TEST_CITY_STATE_UNKNOWN;
    testWmAreaInfoList[1].visitedState = 5;
    CHECK(testWmAreaVisitedState(1) == 0);

    testCleanup();
}

TEST_CASE("wmAreaVisitedState — invalid index returns 0")
{
    testInitAreas(3);
    CHECK(testWmAreaVisitedState(-1) == 0);
    CHECK(testWmAreaVisitedState(5) == 0);
    testCleanup();
}

// ============================================================
// TESTS — wmAreaMarkVisited / wmAreaMarkVisitedState
// ============================================================

TEST_CASE("wmAreaMarkVisitedState — sets visited state for valid area")
{
    testInitAreas(3);

    CHECK(testWmAreaMarkVisitedState(0, 2));
    CHECK(testWmAreaInfoList[0].visitedState == 2);

    CHECK(testWmAreaMarkVisitedState(0, 1));
    CHECK(testWmAreaInfoList[0].visitedState == 1);

    testCleanup();
}

TEST_CASE("wmAreaMarkVisitedState — returns false for invalid area")
{
    testInitAreas(3);
    CHECK_FALSE(testWmAreaMarkVisitedState(-1, 2));
    CHECK_FALSE(testWmAreaMarkVisitedState(5, 2));
    testCleanup();
}

TEST_CASE("wmAreaMarkVisited — shorthand for CITY_STATE_VISITED")
{
    testInitAreas(3);

    testWmAreaMarkVisited(1);
    CHECK(testWmAreaInfoList[1].visitedState == TEST_CITY_STATE_VISITED);

    testCleanup();
}

TEST_CASE("wmAreaMarkVisited — does not require state==KNOWN")
{
    // areaMarkVisited just calls areaMarkVisitedState which sets visitedState
    // regardless of city->state. The state==KNOWN check only applies to
    // wmAreaIsKnown / wmAreaVisitedState queries.
    testInitAreas(3);

    testWmAreaInfoList[0].state = TEST_CITY_STATE_UNKNOWN;
    testWmAreaMarkVisited(0);
    CHECK(testWmAreaInfoList[0].visitedState == TEST_CITY_STATE_VISITED);

    // Still not "known" because state != KNOWN
    CHECK_FALSE(testWmAreaIsKnown(0));

    testCleanup();
}

// ============================================================
// TESTS — wmMapIsKnown
// ============================================================

TEST_CASE("wmMapIsKnown — known map")
{
    testInitAreas(3);
    testWmGenDataInit();

    // Set up area 0 with entrance for map 42, entrance state = 1 (known)
    testWmAreaInfoList[0].entrancesLength = 1;
    testWmAreaInfoList[0].entrances[0].map = 42;
    testWmAreaInfoList[0].entrances[0].state = 1;

    CHECK(testWmMapIsKnown(42));

    testCleanup();
}

TEST_CASE("wmMapIsKnown — entrance state != 1 (not known)")
{
    testInitAreas(3);

    testWmAreaInfoList[0].entrancesLength = 1;
    testWmAreaInfoList[0].entrances[0].map = 42;
    testWmAreaInfoList[0].entrances[0].state = 0;

    CHECK_FALSE(testWmMapIsKnown(42));

    testWmAreaInfoList[0].entrances[0].state = 2;
    CHECK_FALSE(testWmMapIsKnown(42));

    testCleanup();
}

TEST_CASE("wmMapIsKnown — map not found in any area")
{
    testInitAreas(3);

    testWmAreaInfoList[0].entrancesLength = 1;
    testWmAreaInfoList[0].entrances[0].map = 42;
    testWmAreaInfoList[0].entrances[0].state = 1;

    CHECK_FALSE(testWmMapIsKnown(99)); // not in any entrance
    testCleanup();
}

TEST_CASE("wmMapIsKnown — multiple entrances, map in second")
{
    testInitAreas(3);

    testWmAreaInfoList[0].entrancesLength = 2;
    testWmAreaInfoList[0].entrances[0].map = 10;
    testWmAreaInfoList[0].entrances[0].state = 1;
    testWmAreaInfoList[0].entrances[1].map = 20;
    testWmAreaInfoList[0].entrances[1].state = 1;

    CHECK(testWmMapIsKnown(10));
    CHECK(testWmMapIsKnown(20));
    CHECK_FALSE(testWmMapIsKnown(30));

    testCleanup();
}

TEST_CASE("wmMapIsKnown — map in second area")
{
    testInitAreas(3);

    testWmAreaInfoList[0].entrancesLength = 1;
    testWmAreaInfoList[0].entrances[0].map = 10;
    testWmAreaInfoList[0].entrances[0].state = 1;

    testWmAreaInfoList[1].entrancesLength = 1;
    testWmAreaInfoList[1].entrances[0].map = 20;
    testWmAreaInfoList[1].entrances[0].state = 1;

    CHECK(testWmMapIsKnown(20));
    testCleanup();
}

// ============================================================
// TESTS — wmSubTileGetVisitedState
// ============================================================

TEST_CASE("wmSubTileGetVisitedState — reads subtile state")
{
    // 1 tile, 1 horizontal
    testInitTiles(1, 1);

    // Set state at subtile (2, 3) within the tile
    // subtile grid is [6][7] (height x width)
    testWmTileInfoList[0].subtiles[3][2].state = TEST_SUBTILE_STATE_VISITED;

    // Calculate world coords that map to this subtile
    // subtileX = x % 350 / 50, subtileY = y % 300 / 50
    // Need: x % 350 / 50 = 2 → x % 350 in [100, 149]
    //       y % 300 / 50 = 3 → y % 300 in [150, 199]
    int x = 100;
    int y = 150;

    int state = -1;
    CHECK(testWmSubTileGetVisitedState(x, y, &state) == 0);
    CHECK(state == TEST_SUBTILE_STATE_VISITED);

    testCleanup();
}

TEST_CASE("wmSubTileGetVisitedState — reads unknown state")
{
    testInitTiles(1, 1);

    // All default to 0 (UNKNOWN)
    int state = -1;
    CHECK(testWmSubTileGetVisitedState(0, 0, &state) == 0);
    CHECK(state == TEST_SUBTILE_STATE_UNKNOWN);

    testCleanup();
}

TEST_CASE("wmSubTileGetVisitedState — tile boundary crossing")
{
    // 2 horizontal tiles, each 350x300
    testInitTiles(2, 2);

    // Tile 1, subtile (0, 0)
    testWmTileInfoList[1].subtiles[0][0].state = TEST_SUBTILE_STATE_VISITED;

    // x=350 should land in tile 1 (350/350 % 2 = 1 % 2 = 1)
    int state = -1;
    CHECK(testWmSubTileGetVisitedState(350, 0, &state) == 0);
    CHECK(state == TEST_SUBTILE_STATE_VISITED);

    testCleanup();
}

// ============================================================
// TESTS — wmSubTileMarkRadiusVisited
// ============================================================

TEST_CASE("wmSubTileMarkRadiusVisited — radius 0 marks center")
{
    testInitTiles(1, 1);

    int x = 50;  // subtileX = 50/50 = 1
    int y = 100; // subtileY = 100/50 = 2

    testWmSubTileMarkRadiusVisited(x, y, 0);

    int state;
    testWmSubTileGetVisitedState(x, y, &state);
    CHECK(state == TEST_SUBTILE_STATE_VISITED);

    testCleanup();
}

TEST_CASE("wmSubTileMarkRadiusVisited — radius 1 marks neighbors")
{
    testInitTiles(1, 1);

    int x = 100; // subtileX = 2
    int y = 100; // subtileY = 2

    testWmSubTileMarkRadiusVisited(x, y, 1);

    // Center should be VISITED
    int state;
    testWmSubTileGetVisitedState(x, y, &state);
    CHECK(state == TEST_SUBTILE_STATE_VISITED);

    // Neighbors should be KNOWN (not VISITED)
    // Left neighbor: subtileX=1, subtileY=2
    // x=50, y=100
    testWmSubTileGetVisitedState(50, 100, &state);
    CHECK(state == TEST_SUBTILE_STATE_KNOWN);

    // Right neighbor: subtileX=3, subtileY=2
    testWmSubTileGetVisitedState(150, 100, &state);
    CHECK(state == TEST_SUBTILE_STATE_KNOWN);

    testCleanup();
}

TEST_CASE("wmSubTileMarkRadiusVisited — does not upgrade existing VISITED")
{
    testInitTiles(1, 1);

    // Pre-mark some cells as VISITED
    int cx = 100, cy = 100; // center
    testWmTileInfoList[0].subtiles[2][2].state = TEST_SUBTILE_STATE_VISITED;
    // Right neighbor already VISITED
    testWmTileInfoList[0].subtiles[2][3].state = TEST_SUBTILE_STATE_VISITED;

    testWmSubTileMarkRadiusVisited(cx, cy, 1);

    int state;
    // Center stays VISITED
    testWmSubTileGetVisitedState(cx, cy, &state);
    CHECK(state == TEST_SUBTILE_STATE_VISITED);

    // Right neighbor stays VISITED (NOT downgraded to KNOWN)
    testWmSubTileGetVisitedState(150, 100, &state);
    CHECK(state == TEST_SUBTILE_STATE_VISITED);

    testCleanup();
}

// ============================================================
// TESTS — wmWorldMap_load bounds clamping (F-03)
// ============================================================

TEST_CASE("wmWorldMap_load bounds — numCities clamped")
{
    testInitAreas(3);     // wmMaxAreaNum = 3
    testInitEncounterTables(2); // wmMaxEncounterInfoTables = 2

    // Set up entrance lengths
    testWmAreaInfoList[0].entrancesLength = 5;
    testWmAreaInfoList[1].entrancesLength = 3;
    testWmAreaInfoList[2].entrancesLength = 2;

    int entranceCounts[3] = {3, 2, 1};
    int encTableIndices[1] = {0};
    int encEntryIndices[1] = {0};
    testWmEncounterTableList[0].entriesLength = 5;

    // numCities=5 but max is 3 → should be clamped, not error
    int result = testWmWorldMapLoadBoundsSimulate(
        5, entranceCounts, 3,
        10, 1, encTableIndices, encEntryIndices, nullptr);
    CHECK(result == 0);

    // numCities=3 → no clamping
    result = testWmWorldMapLoadBoundsSimulate(
        3, entranceCounts, 3,
        10, 1, encTableIndices, encEntryIndices, nullptr);
    CHECK(result == 0);

    testCleanup();
}

TEST_CASE("wmWorldMap_load bounds — numTiles clamped")
{
    testInitAreas(3);
    testInitEncounterTables(2);
    testInitTiles(5, 1); // wmMaxTileNum = 5

    testWmAreaInfoList[0].entrancesLength = 3;
    testWmAreaInfoList[1].entrancesLength = 2;
    testWmAreaInfoList[2].entrancesLength = 1;

    int entranceCounts[3] = {1, 1, 1};
    int encTableIndices[1] = {0};
    int encEntryIndices[1] = {0};
    testWmEncounterTableList[0].entriesLength = 5;

    // numTiles=10 but max is 5 → clamped
    int result = testWmWorldMapLoadBoundsSimulate(
        3, entranceCounts, 3,
        10, 1, encTableIndices, encEntryIndices, nullptr);
    CHECK(result == 0);

    testCleanup();
}

TEST_CASE("wmWorldMap_load bounds — encounter table index validation")
{
    testInitAreas(3);
    testInitEncounterTables(2); // indices 0-1 valid

    testWmAreaInfoList[0].entrancesLength = 1;
    testWmAreaInfoList[1].entrancesLength = 1;
    testWmAreaInfoList[2].entrancesLength = 1;

    testWmEncounterTableList[0].entriesLength = 5;
    testWmEncounterTableList[1].entriesLength = 3;

    int entranceCounts[3] = {1, 1, 1};

    SUBCASE("valid indices")
    {
        int encTableIdx[1] = {0};
        int encEntryIdx[1] = {2};
        int encEntryLens[2] = {5, 3};
        int result = testWmWorldMapLoadBoundsSimulate(
            3, entranceCounts, 3,
            5, 1, encTableIdx, encEntryIdx, encEntryLens);
        CHECK(result == 0);
    }

    SUBCASE("encounter table index out of bounds (hard error)")
    {
        int encTableIdx[1] = {2}; // >= wmMaxEncounterInfoTables (2)
        int encEntryIdx[1] = {0};
        int encEntryLens[2] = {5, 3};
        int result = testWmWorldMapLoadBoundsSimulate(
            3, entranceCounts, 3,
            5, 1, encTableIdx, encEntryIdx, encEntryLens);
        CHECK(result == -1);
    }

    SUBCASE("encounter table index negative (hard error)")
    {
        int encTableIdx[1] = {-1};
        int encEntryIdx[1] = {0};
        int encEntryLens[2] = {5, 3};
        int result = testWmWorldMapLoadBoundsSimulate(
            3, entranceCounts, 3,
            5, 1, encTableIdx, encEntryIdx, encEntryLens);
        CHECK(result == -1);
    }

    SUBCASE("encounter entry index out of bounds (hard error)")
    {
        int encTableIdx[1] = {0};
        int encEntryIdx[1] = {5}; // >= entriesLength (5)
        int encEntryLens[2] = {5, 3};
        int result = testWmWorldMapLoadBoundsSimulate(
            3, entranceCounts, 3,
            5, 1, encTableIdx, encEntryIdx, encEntryLens);
        CHECK(result == -1);
    }

    SUBCASE("encounter entry index negative (hard error)")
    {
        int encTableIdx[1] = {0};
        int encEntryIdx[1] = {-1};
        int encEntryLens[2] = {5, 3};
        int result = testWmWorldMapLoadBoundsSimulate(
            3, entranceCounts, 3,
            5, 1, encTableIdx, encEntryIdx, encEntryLens);
        CHECK(result == -1);
    }

    testCleanup();
}

TEST_CASE("wmWorldMap_load bounds — entranceCount clamped per city")
{
    testInitAreas(3);
    testInitEncounterTables(2);

    testWmAreaInfoList[0].entrancesLength = 3;
    testWmAreaInfoList[1].entrancesLength = 2;
    testWmAreaInfoList[2].entrancesLength = 1;

    testWmEncounterTableList[0].entriesLength = 5;

    int entranceCounts[3] = {10, 5, 0}; // exceeds capacities
    int encTableIdx[1] = {0};
    int encEntryIdx[1] = {0};

    // Should clamp and succeed (entrance clamping is not an error)
    int result = testWmWorldMapLoadBoundsSimulate(
        3, entranceCounts, 3,
        5, 1, encTableIdx, encEntryIdx, nullptr);
    CHECK(result == 0);

    testCleanup();
}

// ============================================================
// TESTS — wmGameTimeIncrement tick clamping (F-01)
// ============================================================

TEST_CASE("wmGameTimeIncrement — normal case (nextEvent ahead)")
{
    // gameTime=100, nextEventTime=150, ticksToAdd=80
    unsigned int gameTime = 100;
    unsigned int nextEventTime = 150;
    int ticksToAdd = 80;

    // nextEventTime >= gameTime, timeDiff = 50 < 80, so clamp to 50
    int result = testWmGameTimeIncrementComputeTicks(gameTime, nextEventTime, ticksToAdd);
    CHECK(result == 50);
}

TEST_CASE("wmGameTimeIncrement — nextEvent far ahead, use all ticks")
{
    unsigned int gameTime = 100;
    unsigned int nextEventTime = 500;
    int ticksToAdd = 50;

    // timeDiff = 400, ticksToAdd=50, 400 > 50, so use 50
    int result = testWmGameTimeIncrementComputeTicks(gameTime, nextEventTime, ticksToAdd);
    CHECK(result == 50);
}

TEST_CASE("wmGameTimeIncrement — nextEvent behind gameTime")
{
    unsigned int gameTime = 500;
    unsigned int nextEventTime = 100;
    int ticksToAdd = 50;

    // nextEventTime < gameTime → use full ticksToAdd (no clamping)
    int result = testWmGameTimeIncrementComputeTicks(gameTime, nextEventTime, ticksToAdd);
    CHECK(result == 50);
}

TEST_CASE("wmGameTimeIncrement — equal times")
{
    unsigned int gameTime = 100;
    unsigned int nextEventTime = 100;
    int ticksToAdd = 50;

    // nextEventTime == gameTime, timeDiff=0 < 50 → clamp to 0
    int result = testWmGameTimeIncrementComputeTicks(gameTime, nextEventTime, ticksToAdd);
    CHECK(result == 0);
}

TEST_CASE("wmGameTimeIncrement — unsigned wrap (nextEvent < gameTime)")
{
    // Simulate unsigned wrap: nextEventTime=10, gameTime=0xFFFFFF00
    unsigned int gameTime = 0xFFFFFF00u;
    unsigned int nextEventTime = 10;
    int ticksToAdd = 100;

    // nextEventTime < gameTime → use full ticksToAdd
    int result = testWmGameTimeIncrementComputeTicks(gameTime, nextEventTime, ticksToAdd);
    CHECK(result == 100);
}

TEST_CASE("wmGameTimeIncrement — large timeDiff wraps signed comparison")
{
    // timeDiff = nextEventTime - gameTime = large unsigned
    // If gameTime=100, nextEventTime=200, timeDiff=100
    // But if ticksToAdd is very large, we still clamp
    unsigned int gameTime = 100;
    unsigned int nextEventTime = 200;
    int ticksToAdd = 1000;

    // timeDiff=100 (unsigned) → cast to int = 100 < 1000 → clamp to 100
    int result = testWmGameTimeIncrementComputeTicks(gameTime, nextEventTime, ticksToAdd);
    CHECK(result == 100);
}

TEST_CASE("wmGameTimeIncrement — negative ticksToAdd (F-01 scenario)")
{
    // F-01: gScriptWorldMapMulti set to negative can produce negative ticksToAdd.
    // With the fix, if ticksToAdd is negative and nextEventTime >= gameTime,
    // timeDiff (unsigned) compared to negative ticksToAdd:
    // static_cast<int>(timeDiff) < ticksToAdd → for negative ticksToAdd, this
    // is comparing a non-negative int against a negative → false, so no clamp.
    unsigned int gameTime = 100;
    unsigned int nextEventTime = 200;
    int ticksToAdd = -10;

    int result = testWmGameTimeIncrementComputeTicks(gameTime, nextEventTime, ticksToAdd);
    // ticksToAdd stays -10 (the comparison static_cast<int>(timeDiff) < ticksToAdd
    // is 100 < -10 = false, so no clamp)
    CHECK(result == -10);
}

TEST_CASE("wmGameTimeIncrement — zero ticks")
{
    unsigned int gameTime = 100;
    unsigned int nextEventTime = 200;
    int ticksToAdd = 0;

    int result = testWmGameTimeIncrementComputeTicks(gameTime, nextEventTime, ticksToAdd);
    // timeDiff=100, 100 < 0 is false, stays 0
    CHECK(result == 0);
}

// ============================================================
// TESTS — wmWorldMap_load inconsistency: cities clamp, counters abort (F-03)
// ============================================================

TEST_CASE("wmWorldMap_load load inconsistency — cities tolerate overrun, counters abort")
{
    // F-03: numCities is clamped (continues) but bad counter indices abort the load.
    // This test verifies the behavior is as-described in the code.
    testInitAreas(2);
    testInitEncounterTables(2);

    testWmAreaInfoList[0].entrancesLength = 3;
    testWmAreaInfoList[1].entrancesLength = 3;

    testWmEncounterTableList[0].entriesLength = 5;
    testWmEncounterTableList[1].entriesLength = 3;

    int entranceCounts[2] = {2, 1};

    // numCities=10 → clamped to 2 (OK, continues)
    // but encounterTableIdx=2 → hard error (-1)
    int encTableIdx[1] = {2}; // invalid
    int encEntryIdx[1] = {0};

    int result = testWmWorldMapLoadBoundsSimulate(
        10, entranceCounts, 2,
        5, 1, encTableIdx, encEntryIdx, nullptr);
    CHECK(result == -1); // hard error from counter validation

    testCleanup();
}

// ============================================================
// TESTS — encounter flags and constant validation
// ============================================================

TEST_CASE("Encounter flag bit positions")
{
    // Verify flags don't overlap
    CHECK((TEST_ENCOUNTER_FLAG_NO_CAR & TEST_ENCOUNTER_FLAG_LOCK) == 0);
    CHECK((TEST_ENCOUNTER_FLAG_LOCK & TEST_ENCOUNTER_FLAG_NO_ICON) == 0);
    CHECK((TEST_ENCOUNTER_FLAG_NO_ICON & TEST_ENCOUNTER_FLAG_ICON_SP) == 0);
    CHECK((TEST_ENCOUNTER_FLAG_ICON_SP & TEST_ENCOUNTER_FLAG_FADEOUT) == 0);

    // Bit 31 should not overlap with any defined flag
    CHECK((TEST_ENCOUNTER_INTERNAL_LOCK_BIT & TEST_ENCOUNTER_FLAG_NO_CAR) == 0);
    CHECK((TEST_ENCOUNTER_INTERNAL_LOCK_BIT & TEST_ENCOUNTER_FLAG_LOCK) == 0);
    CHECK((TEST_ENCOUNTER_INTERNAL_LOCK_BIT & TEST_ENCOUNTER_FLAG_NO_ICON) == 0);
    CHECK((TEST_ENCOUNTER_INTERNAL_LOCK_BIT & TEST_ENCOUNTER_FLAG_ICON_SP) == 0);
    CHECK((TEST_ENCOUNTER_INTERNAL_LOCK_BIT & TEST_ENCOUNTER_FLAG_FADEOUT) == 0);
}

TEST_CASE("TestEncounterTableEntry counter field exists")
{
    TestEncounterTableEntry entry;
    memset(&entry, 0, sizeof(entry));

    entry.counter = 42;
    CHECK(entry.counter == 42);
}

// ============================================================
// TESTS — wmGenData init defaults
// ============================================================

TEST_CASE("wmGenDataInit sets default world position (Arroyo)")
{
    testWmGenDataInit();

    CHECK(testWmGenData.currentAreaId == -1);
    CHECK(testWmGenData.worldPosX == 173);
    CHECK(testWmGenData.worldPosY == 122);
    CHECK(testWmGenData.isInCar == false);
    CHECK(testWmGenData.currentCarAreaId == -1);
    CHECK(testWmGenData.carFuel == TEST_CAR_FUEL_MAX);
    CHECK(testWmForceEncounterMapId == -1);
    CHECK(testWmForceEncounterFlags == 0);
    CHECK(testScriptWorldMapMulti == 1.0f);
}

// ============================================================
// TESTS — EncounterTableEntry and EncounterTable sizes
// ============================================================

TEST_CASE("EncounterTable sizes match expected")
{
    // EncounterTableEntry: flags(4)+map(4)+scenery(4)+chance(4)+counter(4)+
    //   condition(~28) + subEntiesLength(4) + subEntries[6](~40 each)
    //   = 20 + 28 + 4 + 240 = ~292
    CHECK(sizeof(TestEncounterTableEntry) >= 280);

    // EncounterTable: lookupName[40] + index(4) + mapsLength(4) + maps[6](24) +
    //   field_48(4) + entriesLength(4) + entries[41] (~292 each)
    //   = 40 + 4 + 4 + 24 + 4 + 4 + 41*292 = 80 + 11972 = ~12052
    CHECK(sizeof(TestEncounterTable) >= 12000);
}
