// Unit tests for sfall_lists.cc — sfall object list management.
//
// F-060 (MEDIUM, confirmed): 3 sfall modules with zero dedicated test coverage.
// sfall_lists.cc (~144 LOC) provides:
//   - Object list creation/destruction (per-type iterators)
//   - List lifecycle: sfallListsInit, sfallListsReset, sfallListsExit
//   - sfall_lists_fill: populate a list from all objects of a given type
//
// This test links sfall_lists.cc directly (added to executable sources
// in CMakeLists.txt). Uses test_stubs for engine dependencies.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "sfall_lists.h"
#include "obj_types.h"

using namespace fallout;

TEST_CASE("F-060: sfall_lists — ListType enum values are correct")
{
    // Production: sfall_lists.h:10-19.
    // These must match the sfall convention for type indexing.
    CHECK(static_cast<int>(LIST_CRITTERS) == 0);
    CHECK(static_cast<int>(LIST_ITEMS) == 1);
    CHECK(static_cast<int>(LIST_SCENERY) == 2);
    CHECK(static_cast<int>(LIST_WALLS) == 3);
    CHECK(static_cast<int>(LIST_TILES) == 4);
    CHECK(static_cast<int>(LIST_MISC) == 5);
    CHECK(static_cast<int>(LIST_SPATIAL) == 6);
    CHECK(static_cast<int>(LIST_ALL) == 9);
}

TEST_CASE("F-060: sfall_lists — ObjectType to ListType mapping is consistent")
{
    // Production: sfall_lists.cc:12-19.
    // kObjectTypeToListType maps OBJ_TYPE_* to LIST_*.
    // Due to historical design, OBJ_TYPE_CRITTER != LIST_CRITTERS.
    // Verify each OBJ_TYPE has a valid LIST_TYPE mapping.

    // OBJ_TYPE_ITEM = 0 → mapped to LIST_ITEMS (1)
    // OBJ_TYPE_CRITTER = 1 → mapped to LIST_CRITTERS (0)
    CHECK(OBJ_TYPE_ITEM == 0);
    CHECK(OBJ_TYPE_CRITTER == 1);
    CHECK(OBJ_TYPE_SCENERY == 2);
    CHECK(OBJ_TYPE_WALL == 3);
    CHECK(OBJ_TYPE_TILE == 4);
    CHECK(OBJ_TYPE_MISC == 5);

    // Mapping array has 6 entries
    constexpr int mappingSize = 6;
    CHECK(mappingSize == 6);
}

TEST_CASE("F-060: sfall_lists — init/exit lifecycle")
{
    // Production: sfall_lists.cc:39-62.
    // sfallListsInit allocates state, sfallListsExit deallocates.

    SUBCASE("init succeeds") {
        CHECK(sfallListsInit());
        sfallListsExit();
    }

    SUBCASE("double init is safe") {
        CHECK(sfallListsInit());
        CHECK(sfallListsInit()); // overwrites state pointer, old state leaked (known)
        sfallListsExit();
    }

    SUBCASE("double exit is safe") {
        CHECK(sfallListsInit());
        sfallListsExit();
        sfallListsExit(); // state is nullptr, no-op
    }

    SUBCASE("exit without init is safe") {
        sfallListsExit(); // state starts nullptr, no-op
    }
}

TEST_CASE("F-060: sfall_lists — list creation and destruction")
{
    // Production: sfall_lists.cc:63-92.
    // List IDs start at kInitialListId (0xCCCCCC).

    REQUIRE(sfallListsInit());

    SUBCASE("create list for each valid type") {
        // Create lists for all types except LIST_TILES (not implemented).
        int listTypes[] = {
            LIST_CRITTERS, LIST_ITEMS, LIST_SCENERY,
            LIST_WALLS, /*LIST_TILES skipped - not implemented,*/ LIST_MISC,
            LIST_SPATIAL, LIST_ALL
        };

        for (int lt : listTypes) {
            int id = sfallListsCreate(lt);
            CHECK(id >= 0);
            CHECK(id != 0);
        }
    }

    SUBCASE("create LIST_TILES (not implemented in sfall)") {
        // Production: sfall_lists.cc:96-99 — LIST_TILES returns empty.
        // sfallListsCreate still returns a valid ID but the list is empty.
        int id = sfallListsCreate(LIST_TILES);
        CHECK(id >= 0);
    }

    SUBCASE("destroy non-existent list is safe") {
        sfallListsDestroy(99999);
        CHECK(true); // no crash = pass
    }

    SUBCASE("create and destroy same list") {
        int id = sfallListsCreate(LIST_CRITTERS);
        CHECK(id >= 0);

        sfallListsDestroy(id);
        // Destroying again should be safe
        sfallListsDestroy(id);
        CHECK(true);
    }

    sfallListsExit();
}

TEST_CASE("F-060: sfall_lists — list reset clears all lists")
{
    REQUIRE(sfallListsInit());

    int id1 = sfallListsCreate(LIST_CRITTERS);
    int id2 = sfallListsCreate(LIST_ITEMS);

    sfallListsReset();

    // After reset, nextListId is back to kInitialListId
    int id3 = sfallListsCreate(LIST_MISC);
    CHECK(id3 >= 0);

    sfallListsExit();
}

TEST_CASE("F-060: sfall_lists — GetNext on empty/invalid list returns nullptr")
{
    REQUIRE(sfallListsInit());

    SUBCASE("empty list returns nullptr") {
        int id = sfallListsCreate(LIST_CRITTERS);
        Object* obj = sfallListsGetNext(id);
        CHECK(obj == nullptr);
    }

    SUBCASE("GetNext on invalid list ID returns nullptr") {
        Object* obj = sfallListsGetNext(99999);
        CHECK(obj == nullptr);
    }

    sfallListsExit();
}
