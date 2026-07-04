// Unit tests for sfall_global_vars.cc — global variable store/fetch/lifecycle.
//
// Tests: sfall_gl_vars_init, sfall_gl_vars_store, sfall_gl_vars_fetch,
//        sfall_gl_vars_reset, sfall_gl_vars_exit

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "sfall_global_vars.h"

using namespace fallout;

TEST_CASE("sfall_gl_vars lifecycle")
{
    SUBCASE("init allocates state")
    {
        CHECK(sfall_gl_vars_init());

        // Verify we can store and fetch after init
        CHECK(sfall_gl_vars_store(42, 100));
        int val = 0;
        CHECK(sfall_gl_vars_fetch(42, val));
        CHECK(val == 100);

        sfall_gl_vars_exit();
    }

    SUBCASE("double init is safe")
    {
        CHECK(sfall_gl_vars_init());
        // Second init — state already allocated, overwrites pointer
        CHECK(sfall_gl_vars_init());
        sfall_gl_vars_exit();
    }

    SUBCASE("exit after exit is safe")
    {
        CHECK(sfall_gl_vars_init());
        sfall_gl_vars_exit();
        sfall_gl_vars_exit(); // double exit — no-op since state is nullptr
    }
}

TEST_CASE("sfall_gl_vars_store / sfall_gl_vars_fetch — int keys")
{
    CHECK(sfall_gl_vars_init());

    SUBCASE("store new key")
    {
        CHECK(sfall_gl_vars_store(1, 42));
        int val = 0;
        CHECK(sfall_gl_vars_fetch(1, val));
        CHECK(val == 42);
    }

    SUBCASE("overwrite existing key with non-zero")
    {
        sfall_gl_vars_store(1, 10);
        sfall_gl_vars_store(1, 20);
        int val = 0;
        CHECK(sfall_gl_vars_fetch(1, val));
        CHECK(val == 20);
    }

    SUBCASE("store value 0 erases key")
    {
        sfall_gl_vars_store(1, 42);
        sfall_gl_vars_store(1, 0); // erase
        int val = 0;
        CHECK_FALSE(sfall_gl_vars_fetch(1, val));
    }

    SUBCASE("fetch non-existent key returns false")
    {
        int val = 0;
        CHECK_FALSE(sfall_gl_vars_fetch(999, val));
    }

    SUBCASE("multiple keys")
    {
        sfall_gl_vars_store(1, 10);
        sfall_gl_vars_store(2, 20);
        sfall_gl_vars_store(3, 30);

        int val = 0;
        CHECK(sfall_gl_vars_fetch(1, val));
        CHECK(val == 10);
        CHECK(sfall_gl_vars_fetch(2, val));
        CHECK(val == 20);
        CHECK(sfall_gl_vars_fetch(3, val));
        CHECK(val == 30);
    }

    SUBCASE("negative values")
    {
        sfall_gl_vars_store(1, -1);
        int val = 0;
        CHECK(sfall_gl_vars_fetch(1, val));
        CHECK(val == -1);
    }

    SUBCASE("store value 0 on new key inserts with value 0")
    {
        // Store 0 on a new key: emplace() inserts (key, 0).
        // The erase-on-0 path only triggers when the key already exists.
        sfall_gl_vars_store(999, 0);
        int val = -1;
        CHECK(sfall_gl_vars_fetch(999, val));
        CHECK(val == 0);
    }

    sfall_gl_vars_exit();
}

TEST_CASE("sfall_gl_vars_store / sfall_gl_vars_fetch — string keys (8-char)")
{
    CHECK(sfall_gl_vars_init());

    SUBCASE("store and fetch with 8-char string key")
    {
        // "ABCDEFGH" = 8 bytes = 64-bit key
        CHECK(sfall_gl_vars_store("ABCDEFGH", 42));
        int val = 0;
        CHECK(sfall_gl_vars_fetch("ABCDEFGH", val));
        CHECK(val == 42);
    }

    SUBCASE("reject key != 8 chars")
    {
        CHECK_FALSE(sfall_gl_vars_store("ABC", 42));
        CHECK_FALSE(sfall_gl_vars_store("ABCDEFGHI", 42));  // 9 chars

        int val = 0;
        CHECK_FALSE(sfall_gl_vars_fetch("ABC", val));
        CHECK_FALSE(sfall_gl_vars_fetch("ABCDEFGHI", val));
    }

    SUBCASE("string with printable 8-char key")
    {
        // "ABCDEFGH" has strlen 8, valid for storage
        CHECK(sfall_gl_vars_store("ABCDEFGH", 42));
        int val = 0;
        CHECK(sfall_gl_vars_fetch("ABCDEFGH", val));
        CHECK(val == 42);
    }

    SUBCASE("overwrite string key")
    {
        sfall_gl_vars_store("ABCDEFGH", 10);
        sfall_gl_vars_store("ABCDEFGH", 20);
        int val = 0;
        CHECK(sfall_gl_vars_fetch("ABCDEFGH", val));
        CHECK(val == 20);
    }

    SUBCASE("erase string key with value 0")
    {
        sfall_gl_vars_store("ABCDEFGH", 42);
        sfall_gl_vars_store("ABCDEFGH", 0); // erase
        int val = 0;
        CHECK_FALSE(sfall_gl_vars_fetch("ABCDEFGH", val));
    }

    sfall_gl_vars_exit();
}

TEST_CASE("sfall_gl_vars_reset clears all keys")
{
    CHECK(sfall_gl_vars_init());

    sfall_gl_vars_store(1, 10);
    sfall_gl_vars_store(2, 20);
    sfall_gl_vars_store(3, 30);

    sfall_gl_vars_reset();

    int val = 0;
    CHECK_FALSE(sfall_gl_vars_fetch(1, val));
    CHECK_FALSE(sfall_gl_vars_fetch(2, val));
    CHECK_FALSE(sfall_gl_vars_fetch(3, val));

    sfall_gl_vars_exit();
}

TEST_CASE("sfall_gl_vars full cycle: init → store → reset → store → exit")
{
    CHECK(sfall_gl_vars_init());

    // First round
    sfall_gl_vars_store("GLOBA001", 100);
    int val = 0;
    CHECK(sfall_gl_vars_fetch("GLOBA001", val));
    CHECK(val == 100);

    // Reset
    sfall_gl_vars_reset();
    CHECK_FALSE(sfall_gl_vars_fetch("GLOBA001", val));

    // Second round after reset
    sfall_gl_vars_store("GLOBA002", 200);
    CHECK(sfall_gl_vars_fetch("GLOBA002", val));
    CHECK(val == 200);

    sfall_gl_vars_exit();
}
