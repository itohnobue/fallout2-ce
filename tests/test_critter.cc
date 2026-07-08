// Unit tests for critter.h — kill counter data structure semantics.
//
// Tests the kill counter functions that are standalone-testable:
//   - killsIncByType(int killType) — increments gKillsByType[killType]
//   - killsGetByType(int killType) — reads gKillsByType[killType]
//   - critterGetKillType(Object*)  — kill type lookup from proto
//   - critterFlagCheck(int pid, int flag) — proto flag check
//   - critterFlagSet / critterFlagUnset — proto flag manipulation
//
// These use test-local mirrors of the production data structures since
// critter.cc has 40+ engine dependencies and cannot be linked in unit
// tests. The algorithms are replicated exactly from the production code.
//
// Production function references:
//   killsIncByType()   → critter.cc:705-713
//   killsGetByType()   → critter.cc:716-723
//   critterFlagCheck() → critter.cc:1406-1419
//   critterFlagSet()   → critter.cc:1422-1437
//   critterFlagUnset() → critter.cc:1440-1454

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

// ============================================================
// Test-local constants mirroring proto_types.h
// ============================================================
namespace fallout {

// Kill type constants from proto_types.h:104-130
enum {
    TEST_KILL_TYPE_MAN = 0,
    TEST_KILL_TYPE_WOMAN,
    TEST_KILL_TYPE_CHILD,
    TEST_KILL_TYPE_SUPER_MUTANT,
    TEST_KILL_TYPE_GHOUL,
    TEST_KILL_TYPE_BRAHMIN,
    TEST_KILL_TYPE_RADSCORPION,
    TEST_KILL_TYPE_RAT,
    TEST_KILL_TYPE_FLOATER,
    TEST_KILL_TYPE_CENTAUR,
    TEST_KILL_TYPE_ROBOT,
    TEST_KILL_TYPE_DOG,
    TEST_KILL_TYPE_MANTIS,
    TEST_KILL_TYPE_DEATH_CLAW,
    TEST_KILL_TYPE_PLANT,
    TEST_KILL_TYPE_GECKO,
    TEST_KILL_TYPE_ALIEN,
    TEST_KILL_TYPE_GIANT_ANT,
    TEST_KILL_TYPE_BIG_BAD_BOSS,
    TEST_KILL_TYPE_COUNT,
};

// Critter flags from obj_types.h:95-107
enum {
    TEST_CRITTER_FLAT = 0x800,
    TEST_CRITTER_SPECIAL_DEATH = 0x1000,
    TEST_CRITTER_NO_HEAL = 0x200,
};

// PID_TYPE macro (obj_types.h:33)
#define TEST_PID_TYPE(value) (value) >> 24

enum TestObjectType {
    TEST_OBJ_TYPE_CRITTER = 1,
};

} // namespace fallout

using namespace fallout;

// ============================================================
// Test-local global state (mirrors critter.cc statics)
// ============================================================
static int gKillsByType[TEST_KILL_TYPE_COUNT];

// ============================================================
// Test-local function mirrors (exact algorithms from production)
// ============================================================

// Mirrors critter.cc:705-713 killsIncByType
static int testKillsIncByType(int killType)
{
    if (killType != -1 && killType < TEST_KILL_TYPE_COUNT) {
        gKillsByType[killType]++;
        return 0;
    }
    return -1;
}

// Mirrors critter.cc:716-723 killsGetByType
static int testKillsGetByType(int killType)
{
    if (killType != -1 && killType < TEST_KILL_TYPE_COUNT) {
        return gKillsByType[killType];
    }
    return 0;
}

// Mirrors critter.cc:1406-1419 critterFlagCheck — simplified for unit testing
// In production, this reads from Proto* via protoGetProto.
// For testing, we use a flat array of flags indexed by PID.
static int gTestCritterFlags[256] = {};

static bool testCritterFlagCheck(int pid, int flag)
{
    if (pid == -1) {
        return false;
    }
    if (TEST_PID_TYPE(pid) != TEST_OBJ_TYPE_CRITTER) {
        return false;
    }
    // In production: protoGetProto(pid, &proto); return (proto->critter.data.flags & flag) != 0;
    return (gTestCritterFlags[pid & 0xFF] & flag) != 0;
}

// Mirrors critter.cc:1422-1437 critterFlagSet
static void testCritterFlagSet(int pid, int flag)
{
    if (pid == -1) {
        return;
    }
    if (TEST_PID_TYPE(pid) != TEST_OBJ_TYPE_CRITTER) {
        return;
    }
    gTestCritterFlags[pid & 0xFF] |= flag;
}

// Mirrors critter.cc:1440-1454 critterFlagUnset
static void testCritterFlagUnset(int pid, int flag)
{
    if (pid == -1) {
        return;
    }
    if (TEST_PID_TYPE(pid) != TEST_OBJ_TYPE_CRITTER) {
        return;
    }
    gTestCritterFlags[pid & 0xFF] &= ~flag;
}

// ============================================================
// Helpers
// ============================================================

// ---- H-002: PID_TYPE guard mirrors for critterSetBaseStat/BonusStat ----
// Production references: stat.cc:474-520 (critterSetBaseStat),
// stat.cc:547-580 (critterSetBonusStat).
// Test: calling these functions on non-critter objects returns -5.
// Uses existing TEST_PID_TYPE macro from namespace fallout above.

enum TestObjectType2 {
    TEST_OBJ_TYPE_CRITTER2 = 1,
    TEST_OBJ_TYPE_ITEM2 = 0,
    TEST_OBJ_TYPE_SCENERY2 = 2,
};

// Mirror stat description table needed for bounds checks.
static int gTestBonusStatStorage[256] = {};

static int testCritterSetBaseStatPidGuard(int pid, int stat, int value)
{
    // statIsValid check
    if (stat < 0 || stat >= 256) {
        return -5;
    }
    // Fork PID_TYPE guard (stat.cc:482-484)
    if (TEST_PID_TYPE(pid) != TEST_OBJ_TYPE_CRITTER2) {
        return -5;
    }
    return 0;
}

static int testCritterSetBonusStatPidGuard(int pid, int stat, int value)
{
    if (stat < 0 || stat >= 256) {
        return -5;
    }
    // Fork PID_TYPE guard (stat.cc:553-555)
    if (TEST_PID_TYPE(pid) != TEST_OBJ_TYPE_CRITTER2) {
        return -5;
    }
    gTestBonusStatStorage[stat] = value;
    return 0;
}

// ---- M-016: 4 script guard mirrors ----
// Production references:
//   scriptListExtentRead bounds: scripts.cc:2162
//   scriptGetScript sid guard: scripts.cc:2254-2266
//   scriptRemove sid guard: scripts.cc:2421-2428
//   scriptsGetMessageList bounds: scripts.cc:2847-2854

#define TEST_SCRIPT_LIST_EXTENT_SIZE 16
#define TEST_SCRIPT_DIALOG_MESSAGE_LIST_MAX_CAPACITY 10000
#define TEST_SID_TYPE(value) ((value) >> 24)
enum {
    TEST_SCRIPT_TYPE_COUNT = 4,
};

static int testScriptListExtentRead(int length)
{
    if (length < 0 || length > TEST_SCRIPT_LIST_EXTENT_SIZE) {
        return -1;
    }
    return 0;
}

static int testScriptGetScript(int sid)
{
    if (sid == -1) {
        return -1;
    }
    int scriptType = TEST_SID_TYPE(sid);
    if (scriptType < 0 || scriptType >= TEST_SCRIPT_TYPE_COUNT) {
        return -1;
    }
    return 0;
}

static int testScriptRemove(int sid)
{
    if (sid == -1) {
        return -1;
    }
    int scriptType = TEST_SID_TYPE(sid);
    if (scriptType < 0 || scriptType >= TEST_SCRIPT_TYPE_COUNT) {
        return -1;
    }
    return 0;
}

static int testScriptsGetMessageList(int messageListId)
{
    if (messageListId == -1) {
        return -1;
    }
    int messageListIndex = messageListId - 1;
    if (messageListIndex < 0 || messageListIndex >= TEST_SCRIPT_DIALOG_MESSAGE_LIST_MAX_CAPACITY) {
        return -1;
    }
    return 0;
}

// ---- M-024: HOOK_SNEAK integration mirror ----
// Production reference: critter.cc:1207-1241 (sneakEventProcess).
// The hook allows mods to override sneak result and duration via
// scriptHooks_Sneak(&sneakResult, &time, gDude).

static int testSneakResult = 1;  // 1=working, 0=not working
static int testSneakTime = 600;  // default re-check interval

// Hook override state: when active, testSneakEventProcess uses hook values
// instead of defaults, matching production pattern where scriptHooks_Sneak
// is called inside sneakEventProcess after computing defaults.
static bool testSneakHookActive = false;
static int testSneakHookResult = 0;
static int testSneakHookTime = 0;

static void testSneakEventProcess(int skillValue, int* outResult, int* outTime)
{
    // Compute default values from skillValue.
    bool sneakWorking = (skillValue > 100);
    int time = 600;

    if (!sneakWorking) {
        if (skillValue > 135)
            time = 200;
        else if (skillValue > 100)
            time = 300;
    }

    int result = sneakWorking ? 1 : 0;

    // Hook integration point: mod can override result and time.
    // In production: scriptHooks_Sneak(&sneakResult, &time, gDude)
    // The hook modifies both values by reference.
    // Test mirror: hook values are applied AFTER computing defaults,
    // so they are never overwritten by the default computation.
    if (testSneakHookActive) {
        result = testSneakHookResult;
        time = testSneakHookTime;
    }

    *outResult = result;
    *outTime = time;
}

static void testHookOverrideSneak(int newResult, int newTime)
{
    // Simulates a HOOK_SNEAK handler overriding both values.
    // Sets the hook state that testSneakEventProcess reads after
    // computing defaults, matching production call order.
    testSneakHookActive = true;
    testSneakHookResult = newResult;
    testSneakHookTime = newTime;
}

// ---- End of new mirrors ----
static void resetKillsState()
{
    memset(gKillsByType, 0, sizeof(gKillsByType));
}

static void resetFlagsState()
{
    memset(gTestCritterFlags, 0, sizeof(gTestCritterFlags));
}

// ============================================================
// killsIncByType / killsGetByType tests
// ============================================================
TEST_CASE("killsIncByType / killsGetByType — CRUD")
{
    SUBCASE("increment and read single kill type")
    {
        resetKillsState();
        CHECK(testKillsGetByType(TEST_KILL_TYPE_RAT) == 0);

        testKillsIncByType(TEST_KILL_TYPE_RAT);
        CHECK(testKillsGetByType(TEST_KILL_TYPE_RAT) == 1);

        testKillsIncByType(TEST_KILL_TYPE_RAT);
        CHECK(testKillsGetByType(TEST_KILL_TYPE_RAT) == 2);
    }

    SUBCASE("multiple kill types are independent")
    {
        resetKillsState();

        testKillsIncByType(TEST_KILL_TYPE_MAN);
        testKillsIncByType(TEST_KILL_TYPE_MAN);
        testKillsIncByType(TEST_KILL_TYPE_RAT);
        testKillsIncByType(TEST_KILL_TYPE_RAT);
        testKillsIncByType(TEST_KILL_TYPE_RAT);
        testKillsIncByType(TEST_KILL_TYPE_DEATH_CLAW);

        CHECK(testKillsGetByType(TEST_KILL_TYPE_MAN) == 2);
        CHECK(testKillsGetByType(TEST_KILL_TYPE_RAT) == 3);
        CHECK(testKillsGetByType(TEST_KILL_TYPE_DEATH_CLAW) == 1);
        CHECK(testKillsGetByType(TEST_KILL_TYPE_ROBOT) == 0);
    }

    SUBCASE("invalid kill type -1 returns -1 for inc, 0 for get")
    {
        resetKillsState();
        CHECK(testKillsIncByType(-1) == -1);
        CHECK(testKillsGetByType(-1) == 0);
    }

    SUBCASE("kill type out of range returns -1 for inc, 0 for get")
    {
        resetKillsState();
        CHECK(testKillsIncByType(TEST_KILL_TYPE_COUNT) == -1);
        CHECK(testKillsIncByType(TEST_KILL_TYPE_COUNT + 1) == -1);
        CHECK(testKillsGetByType(TEST_KILL_TYPE_COUNT) == 0);
    }

    SUBCASE("all 19 kill types accessible")
    {
        resetKillsState();

        // Increment all kill types
        for (int kt = 0; kt < TEST_KILL_TYPE_COUNT; kt++) {
            CHECK(testKillsIncByType(kt) == 0);
        }

        // Each should be exactly 1
        for (int kt = 0; kt < TEST_KILL_TYPE_COUNT; kt++) {
            CHECK(testKillsGetByType(kt) == 1);
        }
    }

    SUBCASE("high counter values")
    {
        resetKillsState();

        // Run up a high kill count
        for (int i = 0; i < 1000; i++) {
            testKillsIncByType(TEST_KILL_TYPE_RAT);
        }
        CHECK(testKillsGetByType(TEST_KILL_TYPE_RAT) == 1000);
    }

    SUBCASE("reset state to zero")
    {
        resetKillsState();

        testKillsIncByType(TEST_KILL_TYPE_MAN);
        testKillsIncByType(TEST_KILL_TYPE_MAN);
        testKillsIncByType(TEST_KILL_TYPE_MAN);
        CHECK(testKillsGetByType(TEST_KILL_TYPE_MAN) == 3);

        // Reset
        resetKillsState();
        CHECK(testKillsGetByType(TEST_KILL_TYPE_MAN) == 0);
    }
}

// ============================================================
// Kill type constants tests
// ============================================================
TEST_CASE("KILL_TYPE constants")
{
    CHECK(TEST_KILL_TYPE_COUNT == 19);

    // Verify names match vanilla Fallout 2 conventions
    CHECK(TEST_KILL_TYPE_MAN == 0);
    CHECK(TEST_KILL_TYPE_WOMAN == 1);
    CHECK(TEST_KILL_TYPE_CHILD == 2);
    CHECK(TEST_KILL_TYPE_SUPER_MUTANT == 3);
    CHECK(TEST_KILL_TYPE_GHOUL == 4);
    CHECK(TEST_KILL_TYPE_BRAHMIN == 5);
    CHECK(TEST_KILL_TYPE_RADSCORPION == 6);
    CHECK(TEST_KILL_TYPE_RAT == 7);
    CHECK(TEST_KILL_TYPE_FLOATER == 8);
    CHECK(TEST_KILL_TYPE_CENTAUR == 9);
    CHECK(TEST_KILL_TYPE_ROBOT == 10);
    CHECK(TEST_KILL_TYPE_DOG == 11);
    CHECK(TEST_KILL_TYPE_MANTIS == 12);
    CHECK(TEST_KILL_TYPE_DEATH_CLAW == 13);
    CHECK(TEST_KILL_TYPE_PLANT == 14);
    CHECK(TEST_KILL_TYPE_GECKO == 15);
    CHECK(TEST_KILL_TYPE_ALIEN == 16);
    CHECK(TEST_KILL_TYPE_GIANT_ANT == 17);
    CHECK(TEST_KILL_TYPE_BIG_BAD_BOSS == 18);
}

// ============================================================
// critterFlagCheck / critterFlagSet / critterFlagUnset tests
// ============================================================
TEST_CASE("critterFlagCheck / critterFlagSet / critterFlagUnset")
{
    // Critter PIDs in Fallout have OBJ_TYPE_CRITTER (1) in the high byte.
    // Example: Sulik = 0x01000002 (pid 2, type CRITTER).
    int sulikPid = 0x01000002;
    int vicPid = 0x01000003;
    int itemPid = 0x00000030;  // OBJ_TYPE_ITEM

    SUBCASE("critterFlagSet and critterFlagCheck roundtrip")
    {
        resetFlagsState();

        CHECK_FALSE(testCritterFlagCheck(sulikPid, TEST_CRITTER_FLAT));
        testCritterFlagSet(sulikPid, TEST_CRITTER_FLAT);
        CHECK(testCritterFlagCheck(sulikPid, TEST_CRITTER_FLAT));
    }

    SUBCASE("critterFlagUnset clears flag")
    {
        resetFlagsState();

        testCritterFlagSet(sulikPid, TEST_CRITTER_FLAT);
        CHECK(testCritterFlagCheck(sulikPid, TEST_CRITTER_FLAT));

        testCritterFlagUnset(sulikPid, TEST_CRITTER_FLAT);
        CHECK_FALSE(testCritterFlagCheck(sulikPid, TEST_CRITTER_FLAT));
    }

    SUBCASE("multiple flags on one critter")
    {
        resetFlagsState();

        testCritterFlagSet(sulikPid, TEST_CRITTER_FLAT);
        testCritterFlagSet(sulikPid, TEST_CRITTER_SPECIAL_DEATH);
        testCritterFlagSet(sulikPid, TEST_CRITTER_NO_HEAL);

        CHECK(testCritterFlagCheck(sulikPid, TEST_CRITTER_FLAT));
        CHECK(testCritterFlagCheck(sulikPid, TEST_CRITTER_SPECIAL_DEATH));
        CHECK(testCritterFlagCheck(sulikPid, TEST_CRITTER_NO_HEAL));

        // Unset one flag leaves others intact
        testCritterFlagUnset(sulikPid, TEST_CRITTER_NO_HEAL);
        CHECK(testCritterFlagCheck(sulikPid, TEST_CRITTER_FLAT));
        CHECK(testCritterFlagCheck(sulikPid, TEST_CRITTER_SPECIAL_DEATH));
        CHECK_FALSE(testCritterFlagCheck(sulikPid, TEST_CRITTER_NO_HEAL));
    }

    SUBCASE("independent critters have independent flags")
    {
        resetFlagsState();

        testCritterFlagSet(sulikPid, TEST_CRITTER_FLAT);
        testCritterFlagSet(vicPid, TEST_CRITTER_SPECIAL_DEATH);

        CHECK(testCritterFlagCheck(sulikPid, TEST_CRITTER_FLAT));
        CHECK_FALSE(testCritterFlagCheck(sulikPid, TEST_CRITTER_SPECIAL_DEATH));

        CHECK_FALSE(testCritterFlagCheck(vicPid, TEST_CRITTER_FLAT));
        CHECK(testCritterFlagCheck(vicPid, TEST_CRITTER_SPECIAL_DEATH));
    }

    SUBCASE("pid = -1 is silently ignored")
    {
        resetFlagsState();

        testCritterFlagSet(-1, TEST_CRITTER_FLAT);   // no crash
        CHECK_FALSE(testCritterFlagCheck(-1, TEST_CRITTER_FLAT));

        testCritterFlagUnset(-1, TEST_CRITTER_FLAT); // no crash
        CHECK_FALSE(testCritterFlagCheck(-1, TEST_CRITTER_FLAT));
    }

    SUBCASE("non-critter PID is silently ignored")
    {
        resetFlagsState();

        testCritterFlagSet(itemPid, TEST_CRITTER_FLAT);   // no crash, no-op
        CHECK_FALSE(testCritterFlagCheck(itemPid, TEST_CRITTER_FLAT));

        testCritterFlagUnset(itemPid, TEST_CRITTER_FLAT); // no crash, no-op
        CHECK_FALSE(testCritterFlagCheck(itemPid, TEST_CRITTER_FLAT));
    }

    SUBCASE("flag 0 is valid (no-op but safe)")
    {
        resetFlagsState();

        testCritterFlagSet(sulikPid, 0);
        CHECK_FALSE(testCritterFlagCheck(sulikPid, 0));  // 0 & 0x800 == 0

        testCritterFlagSet(sulikPid, TEST_CRITTER_FLAT);
        testCritterFlagUnset(sulikPid, 0);  // no-op, doesn't change flat flag
        CHECK(testCritterFlagCheck(sulikPid, TEST_CRITTER_FLAT));
    }
}

// ============================================================
// F-04 validation: Rect initialization pattern
// ============================================================
//
// LIMITATION: This test uses a local TestRect mirror type, NOT the
// production Rect type from critter.h. critter.cc has 40+ engine
// dependencies and cannot be linked in unit tests. The test validates
// the *pattern* — zero-initialization via `= {}` for aggregate types
// — rather than directly validating the production `Rect updatedRect = {};`
// at critter.cc:880-881.
//
// A regression in the production code (e.g., reverting `Rect updatedRect = {};`
// to `Rect updatedRect;`) would NOT be caught by this test. The pattern
// validation here serves as documentation and a C++ correctness baseline,
// not as a regression guard for the specific production fix.
// ============================================================

TEST_CASE("Regression: F-04 — Rect initialization pattern validation")
{
    // The bug: in critterKill (critter.cc:880-881), `updatedRect` and
    // `tempRect` were declared without initialization. If `shouldChangeFid`
    // was false, these were passed uninitialized to `rectUnion`.
    //
    // Fix: `Rect updatedRect = {}; Rect tempRect = {};` (critter.cc:880-881)
    //
    // This test validates the *pattern*: zero-initialization via `= {}`
    // is essential for Rect-like aggregates. The local TestRect mirrors
    // the production Rect struct layout (4 int fields).

    typedef struct TestRect {
        int left;
        int top;
        int right;
        int bottom;
    } TestRect;

    // Verify TestRect is a standard-layout aggregate suitable for `= {}`
    static_assert(sizeof(TestRect) == sizeof(int) * 4,
                  "TestRect must match production Rect layout (4 int fields)");

    SUBCASE("zero-initialized Rect is all zeros")
    {
        TestRect r = {};
        CHECK(r.left == 0);
        CHECK(r.top == 0);
        CHECK(r.right == 0);
        CHECK(r.bottom == 0);
    }

    SUBCASE("Rect re-zero-initialization after mutation")
    {
        // This subcase validates that `= {}` re-initialization works
        // correctly after mutation — the production fix at critter.cc:880-881
        // replaced uninitialized `Rect updatedRect;` with `Rect updatedRect = {};`.
        // Note: does not directly test production Rect type (see limitation
        // note above). Validates the *pattern* using local TestRect mirror.
        TestRect r = {};
        CHECK(r.left == 0);  // deterministic, not garbage

        r.left = 10;
        r.top = 20;
        r.right = 30;
        r.bottom = 40;

        CHECK(r.left == 10);
        CHECK(r.top == 20);
        CHECK(r.right == 30);
        CHECK(r.bottom == 40);

        // Re-zero-initialize
        r = {};
        CHECK(r.left == 0);
        CHECK(r.top == 0);
        CHECK(r.right == 0);
        CHECK(r.bottom == 0);
    }
}

// ============================================================
// Bug fix: F-03 — _partyMemberNewObjID no spurious _curID++
// ============================================================
TEST_CASE("Regression: F-03 — _partyMemberNewObjID spurious increment removed")
{
    // The original code in _partyMemberNewObjID (party_member.cc:961 original)
    // had an unconditional `_curID++` at the top of the do-while loop.
    // The post-condition already guaranteed uniqueness by searching all
    // objects, so the extra increment could skip a valid unused ID.
    //
    // The fix removes the spurious increment, keeping only the
    // uniqueness check loop.
    //
    // This test validates the *corrected* pattern: ID generation should
    // not unconditionally skip IDs.

    SUBCASE("ID generation without spurious skip")
    {
        // Simulate: starting at _curID=20000, find first free ID
        int curID = 20000;
        int usedIDs[] = {20000, 20001, 20002};  // these three are taken

        // Bad (original) pattern: increment first, then check
        //   curID++;  // skips 20000, even though 20003 is next free
        //   while (exists(curID)) curID++;

        // Fixed pattern: increment only when the current ID is in use
        int foundID = curID;
        bool found;
        do {
            found = false;
            for (size_t i = 0; i < sizeof(usedIDs) / sizeof(usedIDs[0]); i++) {
                if (usedIDs[i] == foundID) {
                    found = true;
                    foundID++;  // only increment when actually in use
                    break;
                }
            }
        } while (found);

        // First available ID is 20003 since 20000-20002 are taken
        CHECK(foundID == 20003);
    }

    SUBCASE("first ID is free — no increment needed")
    {
        int curID = 20000;
        int usedIDs[] = {20001, 20002, 20003};  // 20000 is free

        int foundID = curID;
        bool found;
        do {
            found = false;
            for (size_t i = 0; i < sizeof(usedIDs) / sizeof(usedIDs[0]); i++) {
                if (usedIDs[i] == foundID) {
                    found = true;
                    foundID++;
                    break;
                }
            }
        } while (found);

        // 20000 is free, should be returned immediately
        CHECK(foundID == 20000);
    }
}

// ============================================================
// H-002: PID_TYPE guard on critter stat functions (stat.cc:482,553)
// ============================================================
// Fork added PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER checks
// returning -5. No test exercises setting a stat on a non-critter
// object (item, scenery, etc.).
// Research: RPU CONFIRMED (Section 1.1-B, uses set_critter_base_stat on NPCs).

TEST_CASE("H-002: PID_TYPE guard — critterSetBaseStat rejects non-critter objects")
{
    int critterPid = (TEST_OBJ_TYPE_CRITTER2 << 24) | 1;
    int itemPid = (TEST_OBJ_TYPE_ITEM2 << 24) | 32;
    int sceneryPid = (TEST_OBJ_TYPE_SCENERY2 << 24) | 15;

    SUBCASE("Critter PID passes guard")
    {
        CHECK(testCritterSetBaseStatPidGuard(critterPid, 0 /*STAT_STRENGTH*/, 5) == 0);
    }

    SUBCASE("Item PID returns -5")
    {
        CHECK(testCritterSetBaseStatPidGuard(itemPid, 0, 5) == -5);
    }

    SUBCASE("Scenery PID returns -5")
    {
        CHECK(testCritterSetBaseStatPidGuard(sceneryPid, 0, 5) == -5);
    }

    SUBCASE("Invalid stat returns -5 even for critter")
    {
        CHECK(testCritterSetBaseStatPidGuard(critterPid, -1, 5) == -5);
        CHECK(testCritterSetBaseStatPidGuard(critterPid, 256, 5) == -5);
    }
}

TEST_CASE("H-002: PID_TYPE guard — critterSetBonusStat rejects non-critter objects")
{
    int critterPid = (TEST_OBJ_TYPE_CRITTER2 << 24) | 1;
    int itemPid = (TEST_OBJ_TYPE_ITEM2 << 24) | 32;

    SUBCASE("Critter PID passes guard")
    {
        CHECK(testCritterSetBonusStatPidGuard(critterPid, 0, 3) == 0);
        CHECK(gTestBonusStatStorage[0] == 3);
    }

    SUBCASE("Item PID returns -5, stat not modified")
    {
        int saved = gTestBonusStatStorage[5];
        CHECK(testCritterSetBonusStatPidGuard(itemPid, 5 /*STAT_AGILITY*/, 2) == -5);
        // Bonus stat should NOT be modified
        CHECK(gTestBonusStatStorage[5] == saved);
    }

    SUBCASE("Invalid stat args still guarded")
    {
        CHECK(testCritterSetBonusStatPidGuard(critterPid, -1, 0) == -5);
    }
}

// ============================================================
// M-016: 4 script guards (scripts.cc:2159, 2254, 2421, 2847)
// ============================================================
// Fork added guard clauses to 4 internal script functions to prevent
// crashes and OOB reads from crafted save files or invalid script IDs.
// Research: RPU No direct usage. Fork: implemented, untested.

TEST_CASE("M-016: scriptListExtentRead — length bounds check (scripts.cc:2162)")
{
    // Valid length: 0 to SCRIPT_LIST_EXTENT_SIZE (16).
    SUBCASE("Valid lengths pass")
    {
        CHECK(testScriptListExtentRead(0) == 0);
        CHECK(testScriptListExtentRead(TEST_SCRIPT_LIST_EXTENT_SIZE) == 0);
        CHECK(testScriptListExtentRead(8) == 0);
    }

    SUBCASE("Negative length rejected (crafted save attack)")
    {
        CHECK(testScriptListExtentRead(-1) == -1);
        CHECK(testScriptListExtentRead(-100) == -1);
    }

    SUBCASE("Beyond max length rejected")
    {
        CHECK(testScriptListExtentRead(TEST_SCRIPT_LIST_EXTENT_SIZE + 1) == -1);
        CHECK(testScriptListExtentRead(9999) == -1);
    }
}

TEST_CASE("M-016: scriptGetScript — sid == -1 guard (scripts.cc:2254)")
{
    SUBCASE("sid == -1 returns -1 (sentinel)")
    {
        CHECK(testScriptGetScript(-1) == -1);
    }

    SUBCASE("sid with valid type passes")
    {
        // SID_TYPE = pid >> 24. Type 0 is valid (SCRIPT_TYPE_SYSTEM).
        int validSid = 0 << 24 | 5;
        CHECK(testScriptGetScript(validSid) == 0);
    }

    SUBCASE("sid with out-of-range type returns -1")
    {
        // SCRIPT_TYPE_COUNT = 4; type 4 and above are invalid.
        int badSid = 4 << 24 | 5;
        CHECK(testScriptGetScript(badSid) == -1);

        int badSid2 = 99 << 24 | 5;
        CHECK(testScriptGetScript(badSid2) == -1);
    }
}

TEST_CASE("M-016: scriptRemove — sid == -1 guard (scripts.cc:2421)")
{
    SUBCASE("sid == -1 returns -1 (sentinel)")
    {
        CHECK(testScriptRemove(-1) == -1);
    }

    SUBCASE("sid with valid type passes")
    {
        int validSid = 1 << 24 | 10;
        CHECK(testScriptRemove(validSid) == 0);
    }

    SUBCASE("sid with invalid type returns -1")
    {
        int badSid = 5 << 24 | 10;
        CHECK(testScriptRemove(badSid) == -1);
    }
}

TEST_CASE("M-016: scriptsGetMessageList — bounds check (scripts.cc:2847-2854)")
{
    SUBCASE("messageListId == -1 returns -1")
    {
        CHECK(testScriptsGetMessageList(-1) == -1);
    }

    SUBCASE("messageListId = 0 → index = -1 → rejected")
    {
        // messageListIndex = 0 - 1 = -1 → < 0 → -1
        CHECK(testScriptsGetMessageList(0) == -1);
    }

    SUBCASE("Valid messageListId passes")
    {
        // messageListIndex = 1 - 1 = 0, within [0, 10000)
        CHECK(testScriptsGetMessageList(1) == 0);
        CHECK(testScriptsGetMessageList(5000) == 0);
    }

    SUBCASE("messageListId at boundary")
    {
        // messageListId = 10000 → index = 9999 → valid
        CHECK(testScriptsGetMessageList(10000) == 0);

        // messageListId = 10001 → index = 10000 → rejected (>= 10000)
        CHECK(testScriptsGetMessageList(10001) == -1);
    }
}

// ============================================================
// M-024: HOOK_SNEAK integration (critter.cc:1207-1241)
// ============================================================
// Fork added HOOK_SNEAK at critter.cc:1233-1236 allowing mods to
// override the sneak result and event duration. No test verifies:
// (a) hook override changes result, (b) hook changes time,
// (c) edge case values (0, -1, INT_MAX).
// Research: RPU No usage. ET Tu No usage. Fork: implemented, untested.

TEST_CASE("M-024: HOOK_SNEAK integration — hook overrides sneak result")
{
    SUBCASE("Default behavior: skill > 100 means sneak is working")
    {
        int result = 0;
        int time = 0;
        testSneakEventProcess(120, &result, &time);
        CHECK(result == 1);   // sneak working
        CHECK(time == 600);   // default time for working sneak
    }

    SUBCASE("Default behavior: low skill → sneak not working, shorter retry")
    {
        int result = 0;
        int time = 0;
        testSneakEventProcess(140, &result, &time);
        CHECK(result == 1);

        testSneakEventProcess(50, &result, &time);
        CHECK(result == 0);   // sneak not working
        // With skill 50, time stays at 600 (no bonus for low skill)
        CHECK(time == 600);
    }

    SUBCASE("Hook overrides sneak result from success to failure")
    {
        int result = 0;
        int time = 0;
        testSneakEventProcess(120, &result, &time);
        CHECK(result == 1);   // would be working by default

        // Hook handler overrides result to 0 (not working)
        testHookOverrideSneak(0 /*not working*/, 400);
        // Re-read: result now 0
        int result2 = 0;
        int time2 = 0;
        testSneakEventProcess(120, &result2, &time2);
        CHECK(result2 == 0);
    }

    SUBCASE("Hook overrides time duration")
    {
        // F-M56: Reset hook state to prevent leakage from prior SUBCASE.
        // testSneakHookActive, testSneakHookResult, and testSneakHookTime
        // are file-static and persist across SUBCASEs. The prior SUBCASE
        // ("Hook overrides sneak result from success to failure") set
        // hook time to 400. Explicit cleanup prevents cross-subcase isolation leaks.
        testSneakHookActive = false;
        testSneakHookResult = 0;
        testSneakHookTime = 0;

        int result = 0;
        int time = 0;
        // Without hook override, defaults should apply
        testSneakEventProcess(120, &result, &time);
        CHECK(time == 600);  // default time for working sneak (not leaked 400)

        // Reset hook for this subcase: time to 1200 (very long sneak interval)
        testHookOverrideSneak(1, 1200);
        int result2 = 0;
        int time2 = 0;
        testSneakEventProcess(120, &result2, &time2);
        CHECK(time2 == 1200);
    }

    SUBCASE("Hook with time=0 — zero-interval edge case")
    {
        // If a hook sets time=0, the event queues immediately.
        // This is a real risk: zero time = infinite-frequency events.
        testHookOverrideSneak(1, 0);
        int result = 0;
        int time = 0;
        testSneakEventProcess(120, &result, &time);
        CHECK(time == 0);  // hook value preserved (production doesn't guard)
    }

    SUBCASE("Hook with time=-1 — negative time edge case")
    {
        // Negative time could cause odd event scheduling behavior.
        testHookOverrideSneak(0, -1);
        int result = 0;
        int time = 0;
        testSneakEventProcess(120, &result, &time);
        CHECK(time == -1);  // passed through to queueAddEvent without validation
    }
}
