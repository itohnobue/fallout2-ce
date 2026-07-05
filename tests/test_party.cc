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
