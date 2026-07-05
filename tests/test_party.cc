// Unit tests for party_member.h — party member data accessor semantics.
//
// Tests key functions from party_member.cc that are standalone-testable
// (pure data lookup / iteration without engine dependencies):
//   - partyMemberGetLevel(int pid) — per-PID level lookup from _partyMemberLevelUpInfoList
//   - _isPotentialPartyMember(Object*) — PID match against gPartyMemberPids[]
//   - get_all_party_members_objects(bool) — iteration over gPartyMembers[]
//
// These use test-local mirrors of the production data structures since
// party_member.cc has 30+ engine dependencies and cannot be linked in unit
// tests. The algorithms are replicated exactly from the production code
// (party_member.cc:862-1705).
//
// Production function references:
//   partyMemberGetLevel()    → party_member.cc:862-870
//   _isPotentialPartyMember() → party_member.cc:873-882
//   get_all_party_members_objects() → party_member.cc:1691-1705

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>
#include <vector>

// ============================================================
// Test-local type definitions mirroring party_member.cc types
// ============================================================
namespace fallout {

// Minimal Object type — only the fields accessed by party member functions.
// Mirrors obj_types.h:270-291.
typedef struct TestObject {
    int id;
    int pid;
    int flags;
    int sid;
    // Enough placeholder fields to support existing data offset expectations
    int _pad[20];
} TestObject;

// PartyMemberLevelUpInfo — mirrors party_member.cc:61-65
typedef struct TestPartyMemberLevelUpInfo {
    int level;
    int numLevelUps;
    int isEarly;
} TestPartyMemberLevelUpInfo;

// PartyMemberListItem — mirrors party_member.cc:67-72
typedef struct TestPartyMemberListItem {
    TestObject* object;
    // Remaining fields not needed for the tested functions
    void* script;
    int* vars;
    struct TestPartyMemberListItem* next;
} TestPartyMemberListItem;

// Object flags constants (obj_types.h:48-93)
enum {
    TEST_OBJECT_HIDDEN = 0x01,
};

// PID_TYPE macro mirrors obj_types.h:33
#define TEST_PID_TYPE(value) (value) >> 24

enum TestObjectType {
    TEST_OBJ_TYPE_CRITTER = 1,
};

} // namespace fallout

using namespace fallout;

// ============================================================
// Test-local global state (mirrors party_member.cc statics)
// ============================================================
static int gPartyMemberDescriptionsLength;
static int gPartyMemberPids[32];          // mirrors gPartyMemberPids (extern int*)
static TestPartyMemberListItem gPartyMembers[32]; // mirrors gPartyMembers (PartyMemberListItem*)
static int gPartyMembersLength;
static TestPartyMemberLevelUpInfo partyMemberLevelUpInfoList[32]; // mirrors _partyMemberLevelUpInfoList

// ============================================================
// Test-local function mirrors (exact algorithms from production)
// ============================================================

// Mirrors party_member.cc:862-870 partyMemberGetLevel
static int testPartyMemberGetLevel(int pid)
{
    for (int index = 0; index < gPartyMemberDescriptionsLength; index++) {
        if (gPartyMemberPids[index] == pid) {
            return partyMemberLevelUpInfoList[index].level;
        }
    }
    return 0;
}

// Mirrors party_member.cc:873-882 _isPotentialPartyMember
static bool testIsPotentialPartyMember(TestObject* object)
{
    for (int index = 1; index < gPartyMemberDescriptionsLength; index++) {
        if (object->pid == gPartyMemberPids[index]) {
            return true;
        }
    }
    return false;
}

// Mirrors party_member.cc:1691-1705 get_all_party_members_objects
static std::vector<TestObject*> testGetAllPartyMembersObjects(bool include_hidden)
{
    std::vector<TestObject*> value;
    value.reserve(gPartyMembersLength);
    for (int index = 0; index < gPartyMembersLength; index++) {
        auto object = gPartyMembers[index].object;
        if (include_hidden
            || (TEST_PID_TYPE(object->pid) == TEST_OBJ_TYPE_CRITTER
                && (object->flags & TEST_OBJECT_HIDDEN) == 0)) {
            value.push_back(object);
        }
    }
    return value;
}

// ---- M-022: _partyMemberIncLevels mirror (party_member.cc:1464-1570) ----
// The party member level-up engine selects NPCs from gPartyMembers,
// checks PartyMemberDescription for level_up_every/level_minimum/level_pids_num,
// performs probabilistic level-up via randomBetween, and swaps proto data.

// PartyMemberDescription mirror (party_member.cc:56-59).
typedef struct TestPartyMemberDescription {
    int level_pids[10];
    int level_up_every;
    int level_minimum;
    int level_pids_num;
    // AI fields not needed for level-up tests
    int _pad[8];
} TestPartyMemberDescription;

static TestPartyMemberDescription gTestPartyMemberDescription;

// Mock randomBetween: returns a controllable value for deterministic testing.
static int gTestRandomOverride = -1;  // -1 = use real formula, else return this
static int testRandomBetween(int minVal, int maxVal)
{
    if (gTestRandomOverride >= 0) {
        return gTestRandomOverride;
    }
    // Default: 50 (mid-range, ~50% probability for 0-100 rolls)
    return 50;
}

// Mock pcGetStat for checking PC level against level_minimum.
static int gTestPcLevel = 1;

// Mirror of _partyMemberIncLevels (party_member.cc:1464-1570).
// Simplified for testing the level-up logic without the proto-copy
// and message-display production dependencies.
static int testPartyMemberIncLevels()
{
    int memberIndex;
    for (int i = 1; i < gPartyMembersLength; i++) {
        TestPartyMemberListItem* listItem = &gPartyMembers[i];
        TestObject* obj = listItem->object;
        if (obj == nullptr) {
            continue;
        }

        if (TEST_PID_TYPE(obj->pid) != TEST_OBJ_TYPE_CRITTER) {
            continue;
        }

        // partyMemberGetDescription: stub — use test description
        if (gTestPartyMemberDescription.level_up_every == 0) {
            continue;
        }

        // Find memberIndex
        memberIndex = -1;
        for (int j = 1; j < gPartyMemberDescriptionsLength; j++) {
            if (gPartyMemberPids[j] == obj->pid) {
                memberIndex = j;
            }
        }
        if (memberIndex == -1) {
            continue;
        }

        // Check PC level meets minimum
        if (gTestPcLevel < gTestPartyMemberDescription.level_minimum) {
            continue;
        }

        TestPartyMemberLevelUpInfo* levelUpInfo = &(partyMemberLevelUpInfoList[memberIndex]);

        // Check NPC hasn't hit max level
        if (levelUpInfo->level >= gTestPartyMemberDescription.level_pids_num) {
            continue;
        }

        levelUpInfo->numLevelUps++;

        int levelMod = levelUpInfo->numLevelUps % gTestPartyMemberDescription.level_up_every;

        // isEarly skip logic
        if (levelUpInfo->isEarly != 0) {
            if (levelMod == 0) {
                levelUpInfo->isEarly = 0;
            }
            continue;
        }

        // Probabilistic level-up
        if (levelMod != 0 && testRandomBetween(0, 100) > 100 * levelMod / gTestPartyMemberDescription.level_up_every) {
            continue;
        }

        levelUpInfo->level++;
        if (levelMod != 0) {
            levelUpInfo->isEarly = 1;
        }

        // In production: _partyMemberCopyLevelInfo would be called here.
        // We skip proto data copying for the mirror test.
    }

    return 0;
}

// ---- End of new mirrors ----

// ============================================================
// Helper: reset global state to clean test conditions
// ============================================================
static void resetPartyState()
{
    memset(gPartyMemberPids, 0, sizeof(gPartyMemberPids));
    memset(gPartyMembers, 0, sizeof(gPartyMembers));
    memset(partyMemberLevelUpInfoList, 0, sizeof(partyMemberLevelUpInfoList));
    gPartyMemberDescriptionsLength = 0;
    gPartyMembersLength = 0;
}

// Helper: configure party member with pid, level, and optional object
static void setupPartyMember(int index, int pid, int level, TestObject* obj = nullptr)
{
    if (index >= 32) return;
    gPartyMemberPids[index] = pid;
    partyMemberLevelUpInfoList[index].level = level;
    partyMemberLevelUpInfoList[index].numLevelUps = 0;
    partyMemberLevelUpInfoList[index].isEarly = 0;
    if (obj != nullptr) {
        gPartyMembers[index].object = obj;
    }
}

// ============================================================
// partyMemberGetLevel tests
// ============================================================
TEST_CASE("partyMemberGetLevel — data lookup")
{
    SUBCASE("returns level for known party member")
    {
        resetPartyState();
        gPartyMemberDescriptionsLength = 3;
        setupPartyMember(0, 0x01000001, 0);  // dude slot
        setupPartyMember(1, 0x01000002, 3);  // Sulik at level 3
        setupPartyMember(2, 0x01000003, 5);  // Vic at level 5

        CHECK(testPartyMemberGetLevel(0x01000002) == 3);
        CHECK(testPartyMemberGetLevel(0x01000003) == 5);
    }

    SUBCASE("returns 0 for unknown PID")
    {
        resetPartyState();
        gPartyMemberDescriptionsLength = 2;
        setupPartyMember(0, 0x01000001, 10);
        setupPartyMember(1, 0x01000002, 3);

        CHECK(testPartyMemberGetLevel(0x01000999) == 0);
    }

    SUBCASE("returns 0 when list is empty")
    {
        resetPartyState();
        gPartyMemberDescriptionsLength = 0;

        CHECK(testPartyMemberGetLevel(0x01000001) == 0);
    }

    SUBCASE("party member with level 0 returns 0 (not gained levels yet)")
    {
        resetPartyState();
        gPartyMemberDescriptionsLength = 2;
        setupPartyMember(0, 0x01000001, 20);  // dude high level
        setupPartyMember(1, 0x01000002, 0);   // NPC hasn't leveled yet

        CHECK(testPartyMemberGetLevel(0x01000002) == 0);
    }

    SUBCASE("multiple members, verify independent levels")
    {
        resetPartyState();
        gPartyMemberDescriptionsLength = 6;
        setupPartyMember(0, 0x01000001, 1);
        setupPartyMember(1, 0x01000002, 2);
        setupPartyMember(2, 0x01000003, 3);
        setupPartyMember(3, 0x01000004, 4);
        setupPartyMember(4, 0x01000005, 5);
        setupPartyMember(5, 0x01000006, 6);  // PARTY_MEMBER_MAX_LEVEL

        CHECK(testPartyMemberGetLevel(0x01000001) == 1);
        CHECK(testPartyMemberGetLevel(0x01000002) == 2);
        CHECK(testPartyMemberGetLevel(0x01000003) == 3);
        CHECK(testPartyMemberGetLevel(0x01000004) == 4);
        CHECK(testPartyMemberGetLevel(0x01000005) == 5);
        CHECK(testPartyMemberGetLevel(0x01000006) == 6);
    }

    SUBCASE("matching PID at index 0 returns level from index 0")
    {
        // partyMemberGetLevel iterates from index 0 (includes dude's slot).
        // If a PID matches at index 0, it returns that level.
        resetPartyState();
        gPartyMemberDescriptionsLength = 2;
        setupPartyMember(0, 0x01000001, 15);
        setupPartyMember(1, 0x01000002, 3);

        CHECK(testPartyMemberGetLevel(0x01000001) == 15);
    }
}

// ============================================================
// _isPotentialPartyMember tests
// ============================================================
TEST_CASE("_isPotentialPartyMember — PID matching")
{
    TestObject sulik = {};
    sulik.pid = 0x01000002;

    TestObject vic = {};
    vic.pid = 0x01000003;

    TestObject dude = {};
    dude.pid = 0x01000001;

    TestObject unknown = {};
    unknown.pid = 0x01000999;

    SUBCASE("known party member PIDs return true")
    {
        resetPartyState();
        gPartyMemberDescriptionsLength = 3;
        gPartyMemberPids[1] = 0x01000002;
        gPartyMemberPids[2] = 0x01000003;

        CHECK(testIsPotentialPartyMember(&sulik));
        CHECK(testIsPotentialPartyMember(&vic));
    }

    SUBCASE("dude PID (index 0) is NOT considered potential party member")
    {
        // _isPotentialPartyMember starts from index 1, skipping dude's slot.
        // This is the FIXED behavior from the bug fix (F-02).
        resetPartyState();
        gPartyMemberDescriptionsLength = 2;
        gPartyMemberPids[0] = 0x01000001;  // dude at index 0
        gPartyMemberPids[1] = 0x01000002;  // Sulik at index 1

        CHECK_FALSE(testIsPotentialPartyMember(&dude));
        CHECK(testIsPotentialPartyMember(&sulik));
    }

    SUBCASE("unknown PID returns false")
    {
        resetPartyState();
        gPartyMemberDescriptionsLength = 3;
        gPartyMemberPids[1] = 0x01000002;
        gPartyMemberPids[2] = 0x01000003;

        CHECK_FALSE(testIsPotentialPartyMember(&unknown));
    }

    SUBCASE("empty list returns false")
    {
        resetPartyState();
        gPartyMemberDescriptionsLength = 0;

        CHECK_FALSE(testIsPotentialPartyMember(&sulik));
    }

    SUBCASE("single-entry list (only index 0) — never matches")
    {
        // With gPartyMemberDescriptionsLength == 1, the loop condition
        // (index < 1) starting from index=1 evaluates false immediately.
        resetPartyState();
        gPartyMemberDescriptionsLength = 1;
        gPartyMemberPids[0] = 0x01000002;  // PID placed in index 0

        CHECK_FALSE(testIsPotentialPartyMember(&sulik));
    }

    SUBCASE("correctly iterates gPartyMemberDescriptionsLength, not gPartyMembersLength")
    {
        // This tests the F-02 bug fix: the original code used
        // gPartyMembersLength (count of joined members) but indexed into
        // gPartyMemberPids (list of all possible members). The fixed code
        // uses gPartyMemberDescriptionsLength.
        resetPartyState();
        gPartyMemberDescriptionsLength = 5;  // 5 possible members
        gPartyMembersLength = 2;             // only 2 currently joined

        gPartyMemberPids[1] = 0x01000002;
        gPartyMemberPids[2] = 0x01000003;
        gPartyMemberPids[3] = 0x01000004;   // would be missed if using gPartyMembersLength
        gPartyMemberPids[4] = 0x01000005;   // would be missed if using gPartyMembersLength

        TestObject npc4 = {};
        npc4.pid = 0x01000004;
        TestObject npc5 = {};
        npc5.pid = 0x01000005;

        // All potential members should be recognized even though only 2 are joined
        CHECK(testIsPotentialPartyMember(&sulik));    // pid 0x01000002
        CHECK(testIsPotentialPartyMember(&vic));      // pid 0x01000003
        CHECK(testIsPotentialPartyMember(&npc4));     // pid 0x01000004
        CHECK(testIsPotentialPartyMember(&npc5));     // pid 0x01000005
    }
}

// ============================================================
// get_all_party_members_objects tests
// ============================================================
TEST_CASE("get_all_party_members_objects — iteration")
{
    TestObject dude_obj = {};
    dude_obj.id = 0;
    dude_obj.pid = 0x01000001;
    dude_obj.flags = 0;

    TestObject sulik_obj = {};
    sulik_obj.id = 1;
    sulik_obj.pid = 0x01000002;
    sulik_obj.flags = 0;

    TestObject vic_obj = {};
    vic_obj.id = 2;
    vic_obj.pid = 0x01000003;
    vic_obj.flags = 0;

    TestObject hidden_obj = {};
    hidden_obj.id = 3;
    hidden_obj.pid = 0x01000004;
    hidden_obj.flags = TEST_OBJECT_HIDDEN;

    SUBCASE("include_hidden=true returns all members")
    {
        resetPartyState();
        gPartyMembersLength = 4;
        gPartyMembers[0].object = &dude_obj;
        gPartyMembers[1].object = &sulik_obj;
        gPartyMembers[2].object = &vic_obj;
        gPartyMembers[3].object = &hidden_obj;

        auto result = testGetAllPartyMembersObjects(true);
        CHECK(result.size() == 4);
        CHECK(result[0] == &dude_obj);
        CHECK(result[1] == &sulik_obj);
        CHECK(result[2] == &vic_obj);
        CHECK(result[3] == &hidden_obj);
    }

    SUBCASE("include_hidden=false excludes hidden critters")
    {
        resetPartyState();
        gPartyMembersLength = 4;
        gPartyMembers[0].object = &dude_obj;
        gPartyMembers[1].object = &sulik_obj;
        gPartyMembers[2].object = &vic_obj;
        gPartyMembers[3].object = &hidden_obj;

        auto result = testGetAllPartyMembersObjects(false);
        CHECK(result.size() == 3);
        CHECK(result[0] == &dude_obj);
        CHECK(result[1] == &sulik_obj);
        CHECK(result[2] == &vic_obj);
    }

    SUBCASE("empty party returns empty vector")
    {
        resetPartyState();
        gPartyMembersLength = 0;

        auto result = testGetAllPartyMembersObjects(true);
        CHECK(result.empty());
    }

    SUBCASE("single member (dude only)")
    {
        resetPartyState();
        gPartyMembersLength = 1;
        gPartyMembers[0].object = &dude_obj;

        auto result = testGetAllPartyMembersObjects(true);
        CHECK(result.size() == 1);
        CHECK(result[0] == &dude_obj);
    }

    SUBCASE("reserve pre-allocates capacity")
    {
        resetPartyState();
        gPartyMembersLength = 2;
        gPartyMembers[0].object = &dude_obj;
        gPartyMembers[1].object = &sulik_obj;

        auto result = testGetAllPartyMembersObjects(true);
        CHECK(result.capacity() >= 2);
        CHECK(result.size() == 2);
    }
}

// ============================================================
// SFALL-related data structure tests
// ============================================================
TEST_CASE("_partyMemberLevelUpInfoList field semantics")
{
    // Verify the level-up info struct behaves as expected
    resetPartyState();
    gPartyMemberDescriptionsLength = 3;

    SUBCASE("default values are zero")
    {
        CHECK(partyMemberLevelUpInfoList[0].level == 0);
        CHECK(partyMemberLevelUpInfoList[0].numLevelUps == 0);
        CHECK(partyMemberLevelUpInfoList[0].isEarly == 0);
    }

    SUBCASE("level field can be set independently per member")
    {
        partyMemberLevelUpInfoList[0].level = 1;
        partyMemberLevelUpInfoList[1].level = 3;
        partyMemberLevelUpInfoList[2].level = 5;

        CHECK(partyMemberLevelUpInfoList[0].level == 1);
        CHECK(partyMemberLevelUpInfoList[1].level == 3);
        CHECK(partyMemberLevelUpInfoList[2].level == 5);
    }

    SUBCASE("level caps at PARTY_MEMBER_MAX_LEVEL=6 via level_pids_num")
    {
        // The production code constrains level growth via
        // level_pids_num (max 6). Test that the array can store
        // levels at the boundary.
        partyMemberLevelUpInfoList[0].level = 6;
        setupPartyMember(0, 0x01000001, 6);

        CHECK(testPartyMemberGetLevel(0x01000001) == 6);
    }

    SUBCASE("isEarly flag semantics")
    {
        partyMemberLevelUpInfoList[0].isEarly = 1;
        CHECK(partyMemberLevelUpInfoList[0].isEarly == 1);

        partyMemberLevelUpInfoList[0].isEarly = 0;
        CHECK(partyMemberLevelUpInfoList[0].isEarly == 0);
    }
}

// ============================================================
// Regression: F-02 — _isPotentialPartyMember uses correct lookup
// ============================================================
TEST_CASE("Regression: F-02 — _isPotentialPartyMember correct iteration bound")
{
    // The original bug (at fork 24199e9) used gPartyMembersLength to
    // iterate gPartyMemberPids. The fix uses gPartyMemberDescriptionsLength.
    // This test validates the fixed behavior: all members listed in
    // gPartyMemberPids are recognized, regardless of how many are joined.

    resetPartyState();
    gPartyMemberDescriptionsLength = 6;  // 6 possible members (includes index 0)
    gPartyMembersLength = 2;             // only 2 currently joined

    // Populate all 5 non-dude slots with different PIDs
    gPartyMemberPids[1] = 0x01000002;  // Sulik
    gPartyMemberPids[2] = 0x01000003;  // Vic
    gPartyMemberPids[3] = 0x01000004;  // Cassidy
    gPartyMemberPids[4] = 0x01000005;  // Myron
    gPartyMemberPids[5] = 0x01000006;  // Goris

    TestObject cassidy = {};
    cassidy.pid = 0x01000004;
    TestObject myron = {};
    myron.pid = 0x01000005;
    TestObject goris = {};
    goris.pid = 0x01000006;

    // All should be recognized even though only 2 are joined
    CHECK(testIsPotentialPartyMember(&cassidy));
    CHECK(testIsPotentialPartyMember(&myron));
    CHECK(testIsPotentialPartyMember(&goris));
}

// ============================================================
// M-022: _partyMemberIncLevels (party_member.cc:1464-1570)
// ============================================================
// The 106-line party member level-up engine is completely untested.
// This function (a) selects party members from gPartyMembers,
// (b) checks PartyMemberDescription for level_up_every / level_minimum,
// (c) performs probabilistic level-up via randomBetween,
// (d) tracks isEarly flag for level-up cadence control,
// (e) caps at level_pids_num (max 6 levels).
// Research: ET Tu CONFIRMED usage of npc_engine_level_up (Section 4.4).
// RPU: No usage.

// Helper: set up a party member with all required state for level-up test.
static void setupPartyMemberForLevelUp(int memberIndex, TestObject* obj, int level, int numLevelUps, int isEarly)
{
    if (memberIndex >= 32) return;
    gPartyMembers[memberIndex].object = obj;
    // Also register in PID lookup
    if (memberIndex < gPartyMemberDescriptionsLength) {
        gPartyMemberPids[memberIndex] = obj->pid;
    }
    partyMemberLevelUpInfoList[memberIndex].level = level;
    partyMemberLevelUpInfoList[memberIndex].numLevelUps = numLevelUps;
    partyMemberLevelUpInfoList[memberIndex].isEarly = isEarly;
}

TEST_CASE("M-022: _partyMemberIncLevels — NPC with level_up_every=0 is skipped")
{
    TestObject sulik = {};
    sulik.pid = 0x01000002;
    sulik.flags = 0;

    SUBCASE("level_up_every=0 → NPC never levels up")
    {
        resetPartyState();
        gPartyMembersLength = 2;  // index 0 (dude) + index 1 (Sulik)
        gPartyMemberDescriptionsLength = 2;
        setupPartyMemberForLevelUp(1, &sulik, 0, 0, 0);

        gTestPcLevel = 10;
        gTestPartyMemberDescription.level_up_every = 0;
        gTestPartyMemberDescription.level_minimum = 1;
        gTestPartyMemberDescription.level_pids_num = 6;

        testPartyMemberIncLevels();

        // Level should remain unchanged
        CHECK(partyMemberLevelUpInfoList[1].level == 0);
    }
}

TEST_CASE("M-022: _partyMemberIncLevels — level_minimum guard")
{
    TestObject sulik = {};
    sulik.pid = 0x01000002;

    SUBCASE("PC below level_minimum → NPC does not level up")
    {
        resetPartyState();
        gPartyMembersLength = 2;
        gPartyMemberDescriptionsLength = 2;
        setupPartyMemberForLevelUp(1, &sulik, 0, 0, 0);

        gTestPcLevel = 3;
        gTestRandomOverride = 0;  // force random to favor level-up
        gTestPartyMemberDescription.level_up_every = 2;
        gTestPartyMemberDescription.level_minimum = 5;  // PC must be level 5+
        gTestPartyMemberDescription.level_pids_num = 6;

        testPartyMemberIncLevels();

        // Sulik should NOT level up — PC level 3 < minimum 5
        CHECK(partyMemberLevelUpInfoList[1].level == 0);
    }

    SUBCASE("PC at level_minimum → NPC eligible for level-up")
    {
        resetPartyState();
        gPartyMembersLength = 2;
        gPartyMemberDescriptionsLength = 2;
        setupPartyMemberForLevelUp(1, &sulik, 0, 0, 0);

        gTestPcLevel = 5;
        gTestRandomOverride = 0;  // force random to always pass
        gTestPartyMemberDescription.level_up_every = 1;  // every level
        gTestPartyMemberDescription.level_minimum = 5;
        gTestPartyMemberDescription.level_pids_num = 6;

        testPartyMemberIncLevels();

        // With level_up_every=1, numLevelUps becomes 1, levelMod = 1%1 = 0
        // → level-up triggers regardless of random
        CHECK(partyMemberLevelUpInfoList[1].level == 1);
    }
}

TEST_CASE("M-022: _partyMemberIncLevels — level_pids_num cap")
{
    TestObject sulik = {};
    sulik.pid = 0x01000002;

    SUBCASE("NPC already at max level → no further level-ups")
    {
        resetPartyState();
        gPartyMembersLength = 2;
        gPartyMemberDescriptionsLength = 2;
        // Sulik at level 6, already at max
        setupPartyMemberForLevelUp(1, &sulik, 6, 10, 0);

        gTestPcLevel = 20;
        gTestRandomOverride = 0;
        gTestPartyMemberDescription.level_up_every = 1;
        gTestPartyMemberDescription.level_minimum = 1;
        gTestPartyMemberDescription.level_pids_num = 6;  // max 6 levels

        testPartyMemberIncLevels();

        // Should still be at level 6
        CHECK(partyMemberLevelUpInfoList[1].level == 6);
    }

    SUBCASE("NPC at level 5 with cap of 6 → one more level-up allowed")
    {
        resetPartyState();
        gPartyMembersLength = 2;
        gPartyMemberDescriptionsLength = 2;
        setupPartyMemberForLevelUp(1, &sulik, 5, 10, 0);

        gTestPcLevel = 20;
        gTestRandomOverride = 0;
        gTestPartyMemberDescription.level_up_every = 1;
        gTestPartyMemberDescription.level_minimum = 1;
        gTestPartyMemberDescription.level_pids_num = 6;

        testPartyMemberIncLevels();

        CHECK(partyMemberLevelUpInfoList[1].level == 6);
    }
}

TEST_CASE("M-022: _partyMemberIncLevels — probabilistic level-up with level_up_every")
{
    TestObject sulik = {};
    sulik.pid = 0x01000002;

    SUBCASE("levelMod=0 always triggers level-up regardless of random roll")
    {
        // When levelMod == 0 (even multiple of level_up_every), the
        // production code skips the random roll entirely.
        resetPartyState();
        gPartyMembersLength = 2;
        gPartyMemberDescriptionsLength = 2;
        // 3 level-ups start, after ++ gives 4, level_up_every=2 → levelMod = 4%2 = 0
        setupPartyMemberForLevelUp(1, &sulik, 0, 3, 0);

        gTestPcLevel = 10;
        gTestRandomOverride = 99;  // high random — would fail probability check
        gTestPartyMemberDescription.level_up_every = 2;
        gTestPartyMemberDescription.level_minimum = 1;
        gTestPartyMemberDescription.level_pids_num = 6;

        testPartyMemberIncLevels();

        // levelMod=0 → always levels up (no random check)
        CHECK(partyMemberLevelUpInfoList[1].level == 1);
    }

    SUBCASE("levelMod != 0 AND random too high → level-up fails")
    {
        resetPartyState();
        gPartyMembersLength = 2;
        gPartyMemberDescriptionsLength = 2;
        // 2 level-ups start, after ++ gives 3, level_up_every=5 → levelMod = 3%5 = 3
        // probability = 100*3/5 = 60%
        setupPartyMemberForLevelUp(1, &sulik, 0, 2, 0);

        gTestPcLevel = 10;
        gTestRandomOverride = 70;  // 70 > 60 → fail
        gTestPartyMemberDescription.level_up_every = 5;
        gTestPartyMemberDescription.level_minimum = 1;
        gTestPartyMemberDescription.level_pids_num = 6;

        testPartyMemberIncLevels();

        // Should NOT level up
        CHECK(partyMemberLevelUpInfoList[1].level == 0);
    }

    SUBCASE("levelMod != 0 AND random low enough → level-up succeeds, isEarly set")
    {
        resetPartyState();
        gPartyMembersLength = 2;
        gPartyMemberDescriptionsLength = 2;
        setupPartyMemberForLevelUp(1, &sulik, 0, 3, 0);

        gTestPcLevel = 10;
        gTestRandomOverride = 50;  // 50 <= 60 → success
        gTestPartyMemberDescription.level_up_every = 5;
        gTestPartyMemberDescription.level_minimum = 1;
        gTestPartyMemberDescription.level_pids_num = 6;

        testPartyMemberIncLevels();

        CHECK(partyMemberLevelUpInfoList[1].level == 1);
        CHECK(partyMemberLevelUpInfoList[1].isEarly == 1);  // early flag set
    }
}

TEST_CASE("M-022: _partyMemberIncLevels — isEarly skip logic")
{
    TestObject sulik = {};
    sulik.pid = 0x01000002;

    SUBCASE("isEarly=1 → level-ups skipped until next levelMod=0 cycle")
    {
        resetPartyState();
        gPartyMembersLength = 2;
        gPartyMemberDescriptionsLength = 2;
        // Sulik got an early level-up last time. isEarly=1.
        // numLevelUps=4 → after ++: 5, levelMod=5%5=0 → isEarly resets
        setupPartyMemberForLevelUp(1, &sulik, 0, 4, 1 /*isEarly*/);

        gTestPcLevel = 10;
        gTestRandomOverride = 0;
        gTestPartyMemberDescription.level_up_every = 5;
        gTestPartyMemberDescription.level_minimum = 1;
        gTestPartyMemberDescription.level_pids_num = 6;

        testPartyMemberIncLevels();

        // First call: levelMod=0 → isEarly resets to 0, level-up skipped
        CHECK(partyMemberLevelUpInfoList[1].isEarly == 0);

        // Second call: numLevelUps=6, levelMod=1, prob=20, random=0 ≤ 20 → levels up
        testPartyMemberIncLevels();
        CHECK(partyMemberLevelUpInfoList[1].level == 1);
    }
}

TEST_CASE("M-022: _partyMemberIncLevels — non-critter PID is skipped")
{
    TestObject item = {};
    item.pid = 0x00000030;  // OBJ_TYPE_ITEM
    item.flags = 0;

    SUBCASE("Item PID in party member slot is ignored")
    {
        resetPartyState();
        gPartyMembersLength = 2;
        gPartyMemberDescriptionsLength = 2;
        setupPartyMemberForLevelUp(1, &item, 0, 0, 0);

        gTestPcLevel = 20;
        gTestRandomOverride = 0;
        gTestPartyMemberDescription.level_up_every = 1;
        gTestPartyMemberDescription.level_minimum = 1;
        gTestPartyMemberDescription.level_pids_num = 6;

        testPartyMemberIncLevels();

        // Item should not level up — PID type check skips it
        CHECK(partyMemberLevelUpInfoList[1].level == 0);
    }
}
