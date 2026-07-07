
// Integration test for ListAsArray → sfall_lists_fill chain.
//
// F2-051: ListAsArray→sfall_lists_fill integration seam was completely
// untested. This test mirrors the full chain:
//   opcode → ListAsArray() → sfall_lists_fill() → array creation → verify.
//
// Mirrors production at:
//   sfall_arrays.cc:857-874  (ListAsArray)
//   sfall_lists.cc:94-142    (sfall_lists_fill)
//   sfall_arrays.cc:664-698  (CreateArray / CreateTempArray)
//
// This test uses LOCAL mirrors of production logic (does not link
// sfall_arrays.cc or sfall_lists.cc — 50+ engine dependencies each).
// Includes production headers (sfall_lists.h, obj_types.h) for type
// checking and the kObjectTypeToListType[] mapping. Requires:
//   target_include_directories(test_meta_integration PRIVATE "${CMAKE_SOURCE_DIR}/src")
//
// LIMITATIONS:
// - LIST_SPATIAL: Production iterates Script objects via
//   scriptGetFirstSpatialScript/scriptGetNextSpatialScript. Test treats
//   spatial as all-objects since script iteration requires the full
//   engine script system. SPATIAL test results are an approximation.
// - kObjectTypeToListType is file-static in sfall_lists.cc; a local
//   mirror is used. Production array changes require manual mirror update.
// - Object iteration (objectFindFirst/objectFindNext) is not available;
//   test uses a controlled object store instead.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

// =================================================================
// Production header includes for type checking.
// Requires: target_include_directories for "${CMAKE_SOURCE_DIR}/src".
// sfall_lists.h transitively includes obj_types.h via #include "obj_types.h".
// =================================================================
#include "sfall_lists.h"

using namespace fallout;

// =================================================================
// Mirror: kObjectTypeToListType from sfall_lists.cc:12-19 (file-static).
// Maps OBJ_TYPE_* → LIST_* for type-filtered list iteration.
// =================================================================
static constexpr int kObjectTypeToListType[] = {
    /* OBJ_TYPE_ITEM */    LIST_ITEMS,
    /* OBJ_TYPE_CRITTER */ LIST_CRITTERS,
    /* OBJ_TYPE_SCENERY */ LIST_SCENERY,
    /* OBJ_TYPE_WALL */    LIST_WALLS,
    /* OBJ_TYPE_TILE */    LIST_TILES,
    /* OBJ_TYPE_MISC */    LIST_MISC,
};
static constexpr int kObjectTypeToListTypeSize =
    sizeof(kObjectTypeToListType) / sizeof(kObjectTypeToListType[0]);

// Verify the mapping array covers all primary object types.
static_assert(kObjectTypeToListTypeSize == 6,
    "kObjectTypeToListType size mismatch — update from sfall_lists.cc");

// =================================================================
// Mirror types — simplified versions of production types.
// =================================================================

// Simulated object type from obj_types.h
struct TestObject {
    int pid; // proto ID for type classification (use PID_TYPE(pid) for type)
    int id;
    std::string name;
};

// =================================================================
// Mirror: sfall_lists_fill
// Fills objects vector based on list type. In production this iterates
// engine object lists; here we use a test-controlled object store.
// Uses the production kObjectTypeToListType[] mapping for type filtering.
// =================================================================

struct TestObjectStore {
    std::vector<TestObject> objects;
};

static void TestListsFill(int type, const TestObjectStore& store,
                          std::vector<TestObject>& outObjects)
{
    if (type == LIST_TILES) {
        // For unknown reason this list type is not implemented in Sfall.
        return;
    }

    outObjects.reserve(100);

    // SPATIAL LIMITATION: Production (sfall_lists.cc:103-114) iterates
    // spatial scripts via scriptGetFirstSpatialScript /
    // scriptGetNextSpatialScript across all 3 elevations. Test treats
    // spatial as all-objects since script iteration requires full engine.
    if (type == LIST_SPATIAL) {
        for (const auto& obj : store.objects) {
            outObjects.push_back(obj);
        }
    } else if (type == LIST_ALL) {
        // All objects — no type filtering
        for (const auto& obj : store.objects) {
            outObjects.push_back(obj);
        }
    } else {
        // Type-filtered iteration using production kObjectTypeToListType[]
        // mapping (mirrors sfall_lists.cc:131-138).
        for (const auto& obj : store.objects) {
            if (obj.pid >= 0
                && PID_TYPE(obj.pid) < kObjectTypeToListTypeSize
                && kObjectTypeToListType[PID_TYPE(obj.pid)] == type) {
                outObjects.push_back(obj);
            }
        }
    }
}
// =================================================================
// Mirror: ListAsArray
// Production: sfall_arrays.cc:857-874
// Creates a temp array and fills it from sfall_lists_fill.
// =================================================================

struct TestArrayValue {
    int intVal;
    std::string strVal;
    bool isString;
};

struct TestArrayList {
    std::vector<TestArrayValue> elements;
    int flags;
};

struct TestArrayStore {
    unsigned int nextId = 1;
    std::map<unsigned int, TestArrayList> arrays;

    unsigned int createTempArray(int len, unsigned int flags) {
        unsigned int id = nextId++;
        TestArrayList arr;
        arr.flags = flags;
        arr.elements.resize(len > 0 ? len : 0);
        arrays[id] = arr;
        return id;
    }

    TestArrayList* getArray(unsigned int id) {
        auto it = arrays.find(id);
        return (it != arrays.end()) ? &it->second : nullptr;
    }
};

static unsigned int TestListAsArray(int type, const TestObjectStore& store,
                                     TestArrayStore& arrayStore)
{
    std::vector<TestObject> objects;
    TestListsFill(type, store, objects);

    int count = static_cast<int>(objects.size());
    unsigned int arrayId = arrayStore.createTempArray(count, 0);
    auto* arr = arrayStore.getArray(arrayId);

    if (arr != nullptr) {
        for (int i = 0; i < count && i < (int)arr->elements.size(); i++) {
            arr->elements[i].intVal = objects[i].id;
            arr->elements[i].isString = false;
        }
    }

    return arrayId;
}

// =================================================================
// F2-051 Tests
// =================================================================

// Test object store with mixed types.
// Uses Fallout-style PIDs where PID_TYPE(pid) = pid >> 24 gives object type:
//   0x00 = OBJ_TYPE_ITEM    → kObjectTypeToListType[0] = LIST_ITEMS
//   0x01 = OBJ_TYPE_CRITTER → kObjectTypeToListType[1] = LIST_CRITTERS
//   0x02 = OBJ_TYPE_SCENERY → kObjectTypeToListType[2] = LIST_SCENERY
static TestObjectStore MakeMixedStore() {
    TestObjectStore store;
    // Items: PID_TYPE = 0 (OBJ_TYPE_ITEM)
    store.objects.push_back({0x00000100, 100, "Item1"});
    store.objects.push_back({0x00000101, 101, "Item2"});
    // Critters: PID_TYPE = 1 (OBJ_TYPE_CRITTER)
    store.objects.push_back({0x01000100, 201, "Critter1"});
    store.objects.push_back({0x01000101, 202, "Critter2"});
    // Scenery: PID_TYPE = 2 (OBJ_TYPE_SCENERY)
    store.objects.push_back({0x02000100, 301, "Scenery1"});
    store.objects.push_back({0x00000102, 102, "Item3"});
    store.objects.push_back({0x01000102, 203, "Critter3"});
    return store;
}

TEST_CASE("F2-051: ListAsArray — LIST_ALL returns all objects")
{
    TestObjectStore store = MakeMixedStore();
    TestArrayStore arrayStore;

    unsigned int arrayId = TestListAsArray(LIST_ALL, store, arrayStore);
    CHECK_FALSE(arrayId == 0);

    TestArrayList* arr = arrayStore.getArray(arrayId);
    REQUIRE(arr != nullptr);
    CHECK(arr->elements.size() == 7); // all 7 objects

    // First element is first object (id=100)
    CHECK(arr->elements[0].intVal == 100);
    // Last element is last object (id=203)
    CHECK(arr->elements[6].intVal == 203);
}

TEST_CASE("F2-051: ListAsArray — LIST_ITEMS filters to OBJ_TYPE_ITEM (type 0) only")
{
    TestObjectStore store = MakeMixedStore();
    TestArrayStore arrayStore;

    unsigned int arrayId = TestListAsArray(LIST_ITEMS, store, arrayStore);
    TestArrayList* arr = arrayStore.getArray(arrayId);
    REQUIRE(arr != nullptr);

    // 3 items with PID_TYPE=0: ids 100, 101, 102
    CHECK(arr->elements.size() == 3);

    // Verify all entries have item ids
    bool allItems = true;
    for (const auto& elem : arr->elements) {
        if (elem.intVal < 100 || elem.intVal > 102) {
            allItems = false;
        }
    }
    CHECK(allItems);
}

TEST_CASE("F2-051: ListAsArray — LIST_CRITTERS filters to OBJ_TYPE_CRITTER (type 1) only")
{
    TestObjectStore store = MakeMixedStore();
    TestArrayStore arrayStore;

    unsigned int arrayId = TestListAsArray(LIST_CRITTERS, store, arrayStore);
    TestArrayList* arr = arrayStore.getArray(arrayId);
    REQUIRE(arr != nullptr);

    // 3 critters with PID_TYPE=1: ids 201, 202, 203
    CHECK(arr->elements.size() == 3);
}

TEST_CASE("F2-051: ListAsArray — LIST_SCENERY filters to OBJ_TYPE_SCENERY (type 2) only")
{
    TestObjectStore store = MakeMixedStore();
    TestArrayStore arrayStore;

    unsigned int arrayId = TestListAsArray(LIST_SCENERY, store, arrayStore);
    TestArrayList* arr = arrayStore.getArray(arrayId);
    REQUIRE(arr != nullptr);

    // 1 scenery with PID_TYPE=2: id 301
    CHECK(arr->elements.size() == 1);
    CHECK(arr->elements[0].intVal == 301);
}

TEST_CASE("F2-051: ListAsArray — LIST_TILES returns empty (not implemented)")
{
    TestObjectStore store = MakeMixedStore();
    TestArrayStore arrayStore;

    unsigned int arrayId = TestListAsArray(LIST_TILES, store, arrayStore);
    TestArrayList* arr = arrayStore.getArray(arrayId);
    REQUIRE(arr != nullptr);

    // LIST_TILES is not implemented in sfall — always returns empty
    CHECK(arr->elements.empty());
}

TEST_CASE("F2-051: ListAsArray — empty store produces empty array")
{
    TestObjectStore emptyStore;
    TestArrayStore arrayStore;

    unsigned int arrayId = TestListAsArray(LIST_ALL, emptyStore, arrayStore);
    TestArrayList* arr = arrayStore.getArray(arrayId);
    REQUIRE(arr != nullptr);
    CHECK(arr->elements.empty());
}

TEST_CASE("F2-051: ListAsArray — LIST_SPATIAL returns all objects (approximation)")
{
    // SPATIAL LIMITATION: Production iterates spatial scripts only.
    // Test treats SPATIAL as all-objects approximation since full script
    // iteration requires the engine script system.
    TestObjectStore store = MakeMixedStore();
    TestArrayStore arrayStore;

    unsigned int arrayId = TestListAsArray(LIST_SPATIAL, store, arrayStore);
    TestArrayList* arr = arrayStore.getArray(arrayId);
    REQUIRE(arr != nullptr);
    CHECK(arr->elements.size() == 7); // all objects (approximation)
}

TEST_CASE("F2-051: ListAsArray — array IDs are unique")
{
    TestObjectStore store = MakeMixedStore();
    TestArrayStore arrayStore;

    unsigned int id1 = TestListAsArray(LIST_ALL, store, arrayStore);
    unsigned int id2 = TestListAsArray(LIST_ITEMS, store, arrayStore);
    unsigned int id3 = TestListAsArray(LIST_CRITTERS, store, arrayStore);

    // Each call produces a unique array ID
    CHECK(id1 != id2);
    CHECK(id2 != id3);
    CHECK(id1 != id3);

    // All three arrays exist
    CHECK(arrayStore.getArray(id1) != nullptr);
    CHECK(arrayStore.getArray(id2) != nullptr);
    CHECK(arrayStore.getArray(id3) != nullptr);
}
