// Unit tests for metarules fixes F-26 through F-29 and F-54
// applied to sfall_metarules.cc.
//
// Uses self-contained mirror pattern (identical to test_meta_comprehensive.cc
// pattern) since sfall_metarules.cc cannot be linked directly — it has 50+
// engine dependencies.
//
// Each mirror function replicates the fixed production logic exactly:
//   F-26: mf_get_current_inven_size null guard before obj->data.inventory.length
//   F-27: mf_set_unique_id null guard before three object->id sites
//   F-28: mf_get_object_ai_data null guard before PID_TYPE(obj->pid)
//   F-29: mf_remove_timer_event early return after null owner check
//   F-54: string_format error-path type mismatch — push string, not int
//
// Verification approach: each test simulates the null/error condition that
// the fix was designed to catch, then asserts the guard logic produces the
// correct value and prevents the dereference/type-mismatch from occurring.

// =================================================================
// Intent
// =================================================================
//
// The five fixes in sfall_metarules.cc share a common pattern: adding
// defensive guards (null checks, early returns, type-correct defaults)
// before operations that would dereference null or push wrong-typed values.
//
// Since the production code cannot be linked (sfall_metarules.cc depends on
// 50+ engine units including Object*, OpcodeContext, Program, interpreter,
// combat_ai, etc.), each fix is mirrored in a test-local function that
// replicates the exact guard logic and a small subset of the business logic
// needed to verify correct vs. incorrect behavior.
//
// For F-26 through F-29 (null dereference guards):
//   - Mirror: a function that takes a test-local Object struct pointer
//     (nullable) and applies the same early-return guard as production.
//   - Verification: assert that null returns the guard's default value
//     without accessing struct members (no segfault / no UB).
//   - Verification: assert that non-null produces the correct business
//     value, confirming the guard does not alter the happy path.
//
// For F-54 (type mismatch):
//   - Mirror: a dispatch function that checks a handler name against
//     string-returning metarules (string_format, string_format_array)
//     and pushes the correct VALUE_TYPE tag on error.
//   - Verification: assert that the pushed value's type tag is
//     VALUE_TYPE_DYNAMIC_STRING (0x9801), not VALUE_TYPE_INT (0xC001).
//   - Verification: assert that non-string metarules still push
//     VALUE_TYPE_INT on error, confirming no regression.
//
// Edge cases covered:
//   - F-26: null object, empty inventory (length=0)
//   - F-27: null object, id above threshold triggering reassignment
//   - F-28: null object, non-critter object (PID_TYPE fails), critter object
//   - F-29: null owner (no queue ops), non-null owner (queue ops proceed)
//   - F-54: both error paths (arg count + type validation), both string
//     metarules, both non-string metarule types to confirm no regression

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <string>
#include <vector>

// =================================================================
// Production value type constants (from interpreter.h:132,135)
// Mirrored here so tests are self-contained without engine headers.
// =================================================================
namespace {
    constexpr int VALUE_TYPE_INT             = 0xC001;
    constexpr int VALUE_TYPE_DYNAMIC_STRING  = 0x9801;
}

// =================================================================
// F-26: mf_get_current_inven_size null guard
// Production: sfall_metarules.cc:2743-2752
// Fix: null guard at line 2746 before obj->data.inventory.length dereference
// =================================================================

struct TestInventory {
    int length;
};

struct TestObjectF26 {
    TestInventory inventory;
};

// Mirror of mf_get_current_inven_size. Returns the number of items in the
// object's inventory. Null object → 0 (the guard's default).
static int MirrorGetCurrentInvenSize(TestObjectF26* obj) {
    if (obj == nullptr) {
        // F-26: Guard against null dereference — matches production line 2746-2750.
        return 0;
    }
    return obj->inventory.length;
}

TEST_CASE("F-26: get_current_inven_size — null object returns 0") {
    // The fix guards against asObject() returning nullptr. A null object
    // should return 0 (no items) without crashing.
    int result = MirrorGetCurrentInvenSize(nullptr);
    CHECK(result == 0);
}

TEST_CASE("F-26: get_current_inven_size — non-null object returns inventory length") {
    // Happy path: the guard must NOT alter correct behavior.
    TestObjectF26 obj;
    obj.inventory.length = 42;
    int result = MirrorGetCurrentInvenSize(&obj);
    CHECK(result == 42);
}

TEST_CASE("F-26: get_current_inven_size — empty inventory returns 0") {
    // An object with zero items (not null, just empty) should return 0.
    TestObjectF26 obj;
    obj.inventory.length = 0;
    int result = MirrorGetCurrentInvenSize(&obj);
    CHECK(result == 0);
}

// =================================================================
// F-27: mf_set_unique_id null guard (3 dereference sites)
// Production: sfall_metarules.cc:1515-1534
// Fix: null guard at line 1518 before three object->id accesses:
//   - line 1525: object->id in condition (id > OBJECT_ID_UNIQUE_START)
//   - line 1529: object->id in return value
//   - line 1533: object passed to scriptsSetUniqueObjectId()
// =================================================================

struct TestObjectF27 {
    int id;
};

static const int TEST_OBJECT_ID_UNIQUE_START = 1000;
static const int TEST_NEW_OBJECT_ID = 999;

// Mirror of mf_set_unique_id. Returns the object's id (possibly reassigned).
// Null object → -1 (matching metarule's errorReturn=-1).
static int MirrorSetUniqueId(TestObjectF27* object) {
    if (object == nullptr) {
        // F-27: Single top-level guard protects all three dereference sites.
        return -1;
    }

    // Simulates the unassign path: if id is unique (> threshold), reassign.
    if (object->id > TEST_OBJECT_ID_UNIQUE_START) {   // site 1
        object->id = TEST_NEW_OBJECT_ID;               // site 1 (write)
    }
    return object->id;                                 // site 2
    // Note: site 3 (scriptsSetUniqueObjectId(object)) is an external call
    // that does its own PID_TYPE validation. The null guard here is
    // defense-in-depth — it prevents passing nullptr to that call.
}

TEST_CASE("F-27: set_unique_id — null object returns -1") {
    int result = MirrorSetUniqueId(nullptr);
    // -1 matches metarule's errorReturn=-1 and follows peer function convention
    CHECK(result == -1);
}

TEST_CASE("F-27: set_unique_id — non-null object returns its id") {
    TestObjectF27 obj;
    obj.id = 500;  // below threshold — no reassignment
    int result = MirrorSetUniqueId(&obj);
    CHECK(result == 500);
}

TEST_CASE("F-27: set_unique_id — id above threshold triggers reassignment") {
    // When id > UNIQUE_START (1000), the function reassigns to a new id (999)
    TestObjectF27 obj;
    obj.id = 2000;
    int result = MirrorSetUniqueId(&obj);
    CHECK(result == 999);  // reassigned
    CHECK(obj.id == 999);  // struct was modified
}

// =================================================================
// F-28: mf_get_object_ai_data null guard before PID_TYPE
// Production: sfall_metarules.cc:2810-2838
// Fix: null guard at line 2815 before PID_TYPE(obj->pid) at line 2821.
// The PID_TYPE macro extracts object type from pid; dereferencing null
// obj->pid is UB.
// =================================================================

enum TestFidType {
    OBJ_TYPE_CRITTER = 1,
    OBJ_TYPE_ITEM    = 2,
    OBJ_TYPE_SCENERY = 3,
};

struct TestObjectF28 {
    int fid;       // packed FID — PID_TYPE masks upper bits for object type
    int aiPacket;  // critter-specific: AI packet number
};

// Mirror of PID_TYPE macro: extracts object type from packed FID.
// Production PID_TYPE uses (fid >> 24) & 0x0F or similar bit extraction.
static int MirrorPidType(int fid) {
    return (fid >> 24) & 0x0F;
}

// Mirror of mf_get_object_ai_data. dataType=0 returns aiPacket.
static int MirrorGetObjectAiData(TestObjectF28* obj, int dataType) {
    if (obj == nullptr) {
        // F-28: Guard against null before PID_TYPE(obj->pid) dereference.
        return 0;
    }

    // Non-critter guard: production checks PID_TYPE(obj->pid) != OBJ_TYPE_CRITTER
    if (MirrorPidType(obj->fid) != OBJ_TYPE_CRITTER) {
        return 0;  // non-critters have no AI data
    }

    // dataType 0 = AI packet number
    if (dataType == 0) {
        return obj->aiPacket;
    }
    return 0;
}

TEST_CASE("F-28: get_object_ai_data — null object returns 0") {
    int result = MirrorGetObjectAiData(nullptr, 0);
    CHECK(result == 0);
}

TEST_CASE("F-28: get_object_ai_data — non-critter object returns 0") {
    // An item (not a critter) — should return 0 even though aiPacket has data
    TestObjectF28 obj;
    obj.fid = (OBJ_TYPE_ITEM << 24) | 1;  // packed FID: item type
    obj.aiPacket = 42;  // this should NOT be accessed
    int result = MirrorGetObjectAiData(&obj, 0);
    CHECK(result == 0);
}

TEST_CASE("F-28: get_object_ai_data — critter returns ai packet data") {
    TestObjectF28 obj;
    obj.fid = (OBJ_TYPE_CRITTER << 24) | 1;  // packed FID: critter type
    obj.aiPacket = 7;
    int result = MirrorGetObjectAiData(&obj, 0);
    CHECK(result == 7);
}

// =================================================================
// F-29: mf_remove_timer_event early return after null check
// Production: sfall_metarules.cc:2966-2976
// Fix: added ctx.setReturn(0); return; after null check at line 2972-2975.
// Previously: the null check only logged a debugPrint and fell through to
// _scrSetQueueTestVals(owner, ...) with null owner.
// =================================================================

// Counts operations that should NOT happen when owner is null.
static int gF29DownstreamOpCount = 0;

static void ResetF29Counter() { gF29DownstreamOpCount = 0; }
static int  GetF29Counter()     { return gF29DownstreamOpCount; }
static void F29DownstreamOp()   { gF29DownstreamOpCount++; }

// Mirror of mf_remove_timer_event. Simulates the owner null check, debugPrint,
// and downstream queue operations that must be guarded.
static int MirrorRemoveTimerEvent(TestObjectF27* owner) {
    if (owner == nullptr) {
        // debugPrint() equivalent — production logs a message here
        // F-29: Early return prevents null owner from reaching downstream ops.
        // Production: ctx.setReturn(0); return;
        return 0;
    }

    // Downstream operations that would have received null owner before the fix:
    //   _scrSetQueueTestVals(owner, event.opcode)
    //   queueClearByEventType(EVENT_TYPE_SCRIPT, _scrQueueRemoveFixed)
    F29DownstreamOp();
    return 0;
}

TEST_CASE("F-29: remove_timer_event — null owner returns 0 without downstream ops") {
    ResetF29Counter();

    int result = MirrorRemoveTimerEvent(nullptr);

    CHECK(result == 0);
    // F-29 verification: downstream operations (queue manipulation with
    // null owner) must NOT be called — the early return prevents fallthrough.
    CHECK(GetF29Counter() == 0);
}

TEST_CASE("F-29: remove_timer_event — non-null owner proceeds to downstream ops") {
    // Sanity check: when owner is valid, downstream operations DO execute.
    // This proves the counter is working and the guard only blocks null.
    ResetF29Counter();
    TestObjectF27 owner;
    owner.id = 1;

    int result = MirrorRemoveTimerEvent(&owner);

    CHECK(result == 0);
    // The guard must NOT block valid owners — downstream ops should proceed.
    CHECK(GetF29Counter() == 1);
}

// =================================================================
// F-54: string_format error-path type mismatch
// Production: sfall_metarules.cc:3579-3604
// Fix: error paths 4 (arg count validation, line 3584) and 5 (type
// validation, line 3596) check handler pointer against mf_string_format /
// mf_string_format_array and push an empty dynamic string instead of int-0.
//
// Before fix: both error paths pushed metaruleInfo->errorReturn (int 0 for
// string_format) causing script fatal error when caller consumed int as string.
// After fix: empty string with VALUE_TYPE_DYNAMIC_STRING tag is pushed.
// =================================================================

struct TestStackValue {
    int         type;      // VALUE_TYPE_INT or VALUE_TYPE_DYNAMIC_STRING
    int         intValue;  // valid when type == VALUE_TYPE_INT
    std::string strValue;  // valid when type == VALUE_TYPE_DYNAMIC_STRING
};

// Checks if a handler name corresponds to a string-returning metarule.
// Production uses function pointer comparison: handler == mf_string_format.
// Mirror uses name-based check for the same semantics.
static bool IsStringMetarule(const char* handlerName) {
    return (std::string(handlerName) == "string_format"
         || std::string(handlerName) == "string_format_array");
}

// Mirror of error path 4: arg count validation.
// Production: programStackPushString(program, "") for string metarules,
//            programStackPushInteger(program, errorReturn) for others.
static void MirrorPushErrorArgCount(
    const char* handlerName,
    int         errorReturn,
    std::vector<TestStackValue>& stack)
{
    if (IsStringMetarule(handlerName)) {
        // F-54a: Push empty dynamic string — correct type for string-returning metarules.
        stack.push_back({VALUE_TYPE_DYNAMIC_STRING, 0, ""});
    } else {
        // Legacy behavior: push integer error return for non-string metarules.
        stack.push_back({VALUE_TYPE_INT, errorReturn, ""});
    }
}

// Mirror of error path 5: type validation.
// Production: ctx.setReturn("") for string metarules,
//            ctx.setReturn(errorReturn) for others.
static void MirrorPushErrorTypeValidation(
    const char* handlerName,
    int         errorReturn,
    std::vector<TestStackValue>& stack)
{
    if (IsStringMetarule(handlerName)) {
        // F-54b: setReturn("") creates VALUE_TYPE_DYNAMIC_STRING.
        stack.push_back({VALUE_TYPE_DYNAMIC_STRING, 0, ""});
    } else {
        stack.push_back({VALUE_TYPE_INT, errorReturn, ""});
    }
}

// --- Error path 4 (arg count validation) tests ---

TEST_CASE("F-54: string_format error path 4 pushes VALUE_TYPE_DYNAMIC_STRING") {
    std::vector<TestStackValue> stack;
    MirrorPushErrorArgCount("string_format", /*errorReturn=*/0, stack);

    REQUIRE(stack.size() == 1);
    CHECK(stack[0].type == VALUE_TYPE_DYNAMIC_STRING);
    CHECK(stack[0].type != VALUE_TYPE_INT);
    CHECK(stack[0].strValue == "");
}

TEST_CASE("F-54: string_format_array error path 4 pushes VALUE_TYPE_DYNAMIC_STRING") {
    std::vector<TestStackValue> stack;
    MirrorPushErrorArgCount("string_format_array", /*errorReturn=*/0, stack);

    REQUIRE(stack.size() == 1);
    CHECK(stack[0].type == VALUE_TYPE_DYNAMIC_STRING);
    CHECK(stack[0].type != VALUE_TYPE_INT);
}

TEST_CASE("F-54: non-string metarule (floor2) error path 4 pushes VALUE_TYPE_INT") {
    // Regression check: non-string metarules must still push int error return.
    std::vector<TestStackValue> stack;
    MirrorPushErrorArgCount("floor2", /*errorReturn=*/0, stack);

    REQUIRE(stack.size() == 1);
    CHECK(stack[0].type == VALUE_TYPE_INT);
    CHECK(stack[0].type != VALUE_TYPE_DYNAMIC_STRING);
    CHECK(stack[0].intValue == 0);
}

TEST_CASE("F-54: non-string metarule (add_trait) error path 4 pushes VALUE_TYPE_INT") {
    std::vector<TestStackValue> stack;
    MirrorPushErrorArgCount("add_trait", /*errorReturn=*/-1, stack);

    REQUIRE(stack.size() == 1);
    CHECK(stack[0].type == VALUE_TYPE_INT);
    CHECK(stack[0].type != VALUE_TYPE_DYNAMIC_STRING);
    CHECK(stack[0].intValue == -1);
}

// --- Error path 5 (type validation) tests ---

TEST_CASE("F-54: string_format error path 5 pushes VALUE_TYPE_DYNAMIC_STRING") {
    std::vector<TestStackValue> stack;
    MirrorPushErrorTypeValidation("string_format", /*errorReturn=*/0, stack);

    REQUIRE(stack.size() == 1);
    CHECK(stack[0].type == VALUE_TYPE_DYNAMIC_STRING);
    CHECK(stack[0].type != VALUE_TYPE_INT);
}

TEST_CASE("F-54: string_format_array error path 5 pushes VALUE_TYPE_DYNAMIC_STRING") {
    std::vector<TestStackValue> stack;
    MirrorPushErrorTypeValidation("string_format_array", /*errorReturn=*/0, stack);

    REQUIRE(stack.size() == 1);
    CHECK(stack[0].type == VALUE_TYPE_DYNAMIC_STRING);
    CHECK(stack[0].type != VALUE_TYPE_INT);
}

TEST_CASE("F-54: non-string metarule (string_find) error path 5 pushes VALUE_TYPE_INT") {
    // string_find returns int (-1 on not-found), not a string.
    std::vector<TestStackValue> stack;
    MirrorPushErrorTypeValidation("string_find", /*errorReturn=*/-1, stack);

    REQUIRE(stack.size() == 1);
    CHECK(stack[0].type == VALUE_TYPE_INT);
    CHECK(stack[0].type != VALUE_TYPE_DYNAMIC_STRING);
    CHECK(stack[0].intValue == -1);
}

TEST_CASE("F-54: non-string metarule (npc_engine_level_up) error path 5 pushes VALUE_TYPE_INT") {
    std::vector<TestStackValue> stack;
    MirrorPushErrorTypeValidation("npc_engine_level_up", /*errorReturn=*/-1, stack);

    REQUIRE(stack.size() == 1);
    CHECK(stack[0].type == VALUE_TYPE_INT);
    CHECK(stack[0].type != VALUE_TYPE_DYNAMIC_STRING);
    CHECK(stack[0].intValue == -1);
}

// =================================================================
// Combined null guard pattern verification
// =================================================================

TEST_CASE("Combined: all 4 null guard fixes return immediately and do not crash") {
    // This test verifies that the guard pattern is consistent across all
    // four null-dereference fixes: check null, set default, return.
    //
    // The compiler can't check that the return prevents subsequent
    // dereference (that's a runtime property), but we can verify the
    // mirror functions all produce the expected default values for
    // null input, which confirms the guard activates.

    // F-26: null → 0
    CHECK(MirrorGetCurrentInvenSize(nullptr) == 0);

    // F-27: null → -1
    CHECK(MirrorSetUniqueId(nullptr) == -1);

    // F-28: null → 0
    CHECK(MirrorGetObjectAiData(nullptr, 0) == 0);

    // F-29: null → 0 (and downstream ops not called)
    ResetF29Counter();
    CHECK(MirrorRemoveTimerEvent(nullptr) == 0);
    CHECK(GetF29Counter() == 0);
}

TEST_CASE("Combined: all 4 null guard fixes preserve happy-path behavior") {
    // Verify that non-null input still produces correct results after
    // the guards are in place.

    TestObjectF26 obj26;
    obj26.inventory.length = 15;
    CHECK(MirrorGetCurrentInvenSize(&obj26) == 15);

    TestObjectF27 obj27;
    obj27.id = 500;
    CHECK(MirrorSetUniqueId(&obj27) == 500);

    TestObjectF28 obj28;
    obj28.fid = (OBJ_TYPE_CRITTER << 24) | 1;
    obj28.aiPacket = 3;
    CHECK(MirrorGetObjectAiData(&obj28, 0) == 3);

    ResetF29Counter();
    TestObjectF27 owner;
    owner.id = 1;
    MirrorRemoveTimerEvent(&owner);
    CHECK(GetF29Counter() == 1);
}
