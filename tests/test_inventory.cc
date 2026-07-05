// Unit tests for inventory.cc core logic patterns.
//
// Since inventory.cc is 6,227 LOC with 60+ external engine dependencies
// (proto, art, combat, animation, etc.), it cannot be practically linked
// as test_sources. Instead, this test validates the core logic patterns by:
//
// 1. Mirroring the Object/Inventory/InventoryItem data structures
// 2. Providing lightweight stubs for the minimal external deps
// 3. Re-implementing the key functions verbatim from inventory.cc
// 4. Testing them against the discovery report's findings
//
// This follows the same approach as test_criticals.cc (self-contained,
// no linkage to the production translation unit).
//
// P1: AP cost functions (inventoryGetInvenApCost, set/reset, QuickPockets)
// P2: Equipment accessors (critterGetItem1/2, critterGetArmor)
// P4: Object search (getCarriedByPid, findById/ByType/ByIndex)
// P5: Strip/Restore (critterStripEquipped, critterRestoreEquipped)
//
// Fork changes tested:
//   - kQuickPocketsApCostReduction mutable static int (line 565)
//   - inventorySetQuickPocketsApCostReduction setter (lines 1068-1071)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cstring>
#include <cassert>

// ============================================================================
// Section 1: Test-local type definitions mirroring the production types
// from obj_types.h, proto_types.h, and inventory.h
// ============================================================================

namespace fallout {

// ----- Object type constants (mirror of obj_types.h:17-30) -----
enum {
    TEST_OBJ_TYPE_ITEM,
    TEST_OBJ_TYPE_CRITTER,
    TEST_OBJ_TYPE_SCENERY,
    TEST_OBJ_TYPE_WALL,
    TEST_OBJ_TYPE_TILE,
    TEST_OBJ_TYPE_MISC,
    TEST_OBJ_TYPE_INTERFACE,
    TEST_OBJ_TYPE_INVENTORY,
    TEST_OBJ_TYPE_HEAD,
    TEST_OBJ_TYPE_BACKGROUND,
    TEST_OBJ_TYPE_SKILLDEX,
    TEST_OBJ_TYPE_COUNT,
};

// ----- Object flags (mirror of obj_types.h:48-93) -----
enum TestObjectFlags : unsigned int {
    TEST_OBJECT_HIDDEN      = 0x01,
    TEST_OBJECT_NO_SAVE     = 0x04,
    TEST_OBJECT_FLAT        = 0x08,
    TEST_OBJECT_NO_BLOCK    = 0x10,
    TEST_OBJECT_LIGHTING    = 0x20,
    TEST_OBJECT_NO_REMOVE   = 0x400,
    TEST_OBJECT_MULTIHEX    = 0x800,
    TEST_OBJECT_NO_HIGHLIGHT = 0x1000,
    TEST_OBJECT_QUEUED      = 0x2000,
    TEST_OBJECT_TRANS_RED   = 0x4000,
    TEST_OBJECT_TRANS_NONE  = 0x8000,
    TEST_OBJECT_TRANS_WALL  = 0x10000,
    TEST_OBJECT_TRANS_GLASS = 0x20000,
    TEST_OBJECT_TRANS_STEAM = 0x40000,
    TEST_OBJECT_TRANS_ENERGY = 0x80000,
    TEST_OBJECT_IN_LEFT_HAND  = 0x1000000,
    TEST_OBJECT_IN_RIGHT_HAND = 0x2000000,
    TEST_OBJECT_WORN          = 0x4000000,
    TEST_OBJECT_WALL_TRANS_END = 0x10000000,
    TEST_OBJECT_LIGHT_THRU    = 0x20000000,
    TEST_OBJECT_SEEN          = 0x40000000,
    TEST_OBJECT_SHOOT_THRU    = 0x80000000,
};

// ----- Item type constants (mirror of proto_types.h:25-29) -----
enum TestItemType {
    TEST_ITEM_TYPE_ARMOR,
    TEST_ITEM_TYPE_CONTAINER,
    TEST_ITEM_TYPE_DRUG,
    TEST_ITEM_TYPE_WEAPON,
    TEST_ITEM_TYPE_AMMO,
};

// ----- Perk constants -----
enum {
    TEST_PERK_QUICK_POCKETS,
};

// ----- Hand enum (mirror of inventory.h:16-22) -----
typedef enum TestHand {
    TEST_HAND_LEFT,
    TEST_HAND_RIGHT,
    TEST_HAND_COUNT,
} TestHand;

// ----- Data structures (mirror of obj_types.h:164-291) -----
typedef struct TestObject TestObject;

typedef struct TestInventoryItem {
    TestObject* item;
    int quantity;
    TestInventoryItem() : item(nullptr), quantity(0) {}
} TestInventoryItem;

typedef struct TestInventory {
    int length;
    int capacity;
    TestInventoryItem* items;
    TestInventory() : length(0), capacity(0), items(nullptr) {}
} TestInventory;

// Minimal CritterEquipped mirror (from inventory.h:40-45)
struct TestCritterEquipped {
    TestObject* leftHand = nullptr;
    TestObject* rightHand = nullptr;
    TestObject* armor = nullptr;
    int weight = 0;
};

// Minimal Object mirror — matches layout of obj_types.h:270-291
// for the fields that inventory functions access.
typedef struct TestObject {
    int id;
    int tile;
    int x;
    int y;
    int sx;
    int sy;
    int frame;
    int rotation;
    int fid;
    int flags;
    int elevation;
    TestInventory inventory; // data.inventory (offset match)
    int pid;
    int cid;
    int lightDistance;
    int lightIntensity;
    int outline;
    int sid;
    TestObject* owner;
} TestObject;

// Helper: check if an object has a specific inventory flag.
// Wraps the bitwise & so doctest can decompose the comparison.
static bool testHasItemFlag(int flags, unsigned int flagMask)
{
    return (flags & flagMask) != 0;
}

// ============================================================================
// Section 2: Test helper functions — object/inventory allocation
// ============================================================================

// Simple allocator for test objects; uses static pool to avoid malloc.
static constexpr int kMaxTestObjects = 50;
static TestObject gTestObjectPool[kMaxTestObjects];
static int gTestObjectPoolIndex = 0;

static TestObject* testObjectAlloc(int pid, int flags = 0, int id = 0)
{
    assert(gTestObjectPoolIndex < kMaxTestObjects);
    TestObject* obj = &gTestObjectPool[gTestObjectPoolIndex++];
    std::memset(obj, 0, sizeof(TestObject));
    obj->pid = pid;
    obj->flags = flags;
    obj->id = id;
    return obj;
}

static void testObjectResetPool()
{
    std::memset(gTestObjectPool, 0, sizeof(gTestObjectPool));
    gTestObjectPoolIndex = 0;
}

// Simple inventory item allocator.
static constexpr int kMaxTestItems = 100;
static TestInventoryItem gTestItemPool[kMaxTestItems];
static int gTestItemPoolIndex = 0;

static TestInventoryItem* testItemAlloc(TestObject* item, int quantity = 1)
{
    assert(gTestItemPoolIndex < kMaxTestItems);
    TestInventoryItem* invItem = &gTestItemPool[gTestItemPoolIndex++];
    invItem->item = item;
    invItem->quantity = quantity;
    return invItem;
}

static void testItemResetPool()
{
    std::memset(gTestItemPool, 0, sizeof(gTestItemPool));
    gTestItemPoolIndex = 0;
}

// Set up an object's inventory from a list of (item, quantity) pairs.
static void testInventorySetup(TestObject* obj, TestInventoryItem** items, int count)
{
    obj->inventory.items = (items && count > 0) ? items[0] : nullptr;
    obj->inventory.length = count;
    obj->inventory.capacity = count;
}

// Helper: create a simple item with given pid and flags.
static TestObject* testMakeItem(int pid, int flags = 0, int id = 1)
{
    static int sNextId = 1;
    return testObjectAlloc(pid, flags, id > 0 ? id : sNextId++);
}

// Helper: create equipped item with quantity
static TestObject* testMakeEquippedItem(int pid, int flags)
{
    static int sNextId = 100;
    return testObjectAlloc(pid, flags, sNextId++);
}

// Reset all pools before each test case (handled in TEST_CASE setup).
static void testResetAllPools()
{
    testObjectResetPool();
    testItemResetPool();
}

// ============================================================================
// Section 3: Stub functions — minimal mocks for external dependencies
// ============================================================================

// ---- Globals used by inventory functions ----
static TestObject* gDude = nullptr;
static TestObject* gInventoryDude = nullptr;      // _inven_dude
static int gInventoryPid = 0x1000000;              // _inven_pid
static TestObject* gInventoryLeftHandItem = nullptr;
static TestObject* gInventoryRightHandItem = nullptr;
static TestObject* gInventoryArmor = nullptr;

static constexpr int kDefaultInventoryApCost = 4;
static int kQuickPocketsApCostReduction = 2;
static int gInventoryApCost = kDefaultInventoryApCost;

// ---- Stub: perkGetRank ----
// In tests, we control the return value via a global to simulate
// different Quick Pockets ranks.
static int gStubPerkQuickPocketsRank = 0;

static int testPerkGetRank(TestObject* /*critter*/, int perk)
{
    if (perk == TEST_PERK_QUICK_POCKETS) {
        return gStubPerkQuickPocketsRank;
    }
    return 0;
}

// ---- Stub: itemGetType ----
// Returns the item type based on a simple PID-based lookup.
// In production, this reads from proto data; for tests we map
// known PIDs to types via a table.
static int testItemGetType(TestObject* item)
{
    if (item == nullptr) return -1;

    // Simple mapping: PID 0-99 = armor, 100-199 = weapon, 200-299 = ammo,
    // 300-399 = drug, 400-499 = container
    int pid = item->pid;
    if (pid >= 0 && pid < 100) return TEST_ITEM_TYPE_ARMOR;
    if (pid >= 100 && pid < 200) return TEST_ITEM_TYPE_WEAPON;
    if (pid >= 200 && pid < 300) return TEST_ITEM_TYPE_AMMO;
    if (pid >= 300 && pid < 400) return TEST_ITEM_TYPE_DRUG;
    if (pid >= 400 && pid < 500) return TEST_ITEM_TYPE_CONTAINER;
    return -1;
}

// ---- Stub: itemGetWeight ----
static int gStubItemWeight = 5; // default weight for test items

static int testItemGetWeight(TestObject* /*item*/)
{
    return gStubItemWeight;
}

// ---- Stub: itemRemove ----
// Removes an item from the critter's inventory by shifting remaining items
// left. Returns 0 on success, -1 if not found.
static int testItemRemove(TestObject* critter, TestObject* item, int /*quantity*/)
{
    if (critter == nullptr || item == nullptr) return -1;

    TestInventory* inv = &critter->inventory;
    for (int i = 0; i < inv->length; i++) {
        if (inv->items[i].item == item) {
            // Shift remaining items left
            for (int j = i; j < inv->length - 1; j++) {
                inv->items[j] = inv->items[j + 1];
            }
            inv->length--;
            return 0;
        }
    }
    return -1;
}

// ---- Stub: itemAdd ----
// Adds an item to the end of the critter's inventory.
// Returns 0 on success, -1 on failure (capacity exceeded).
static int testItemAdd(TestObject* critter, TestObject* item, int quantity)
{
    if (critter == nullptr || item == nullptr) return -1;

    TestInventory* inv = &critter->inventory;
    if (inv->length >= inv->capacity) return -1;

    inv->items[inv->length].item = item;
    inv->items[inv->length].quantity = quantity;
    inv->length++;
    return 0;
}

// ============================================================================
// Section 4: Target functions — verbatim reimplementations from inventory.cc
// ============================================================================

// ---------------------------------------------------------------------------
// P1: AP cost functions (inventory.cc:1042-1071)
// ---------------------------------------------------------------------------

// inventory.cc:1042 — inventoryResetDude
void testInventoryResetDude()
{
    gInventoryDude = gDude;
    gInventoryPid = 0x1000000;
}

// inventory.cc:1048 — inventoryGetInvenApCost
int testInventoryGetInvenApCost()
{
    int quickPockets = 0;
    if (gDude != nullptr) {
        quickPockets = testPerkGetRank(gDude, TEST_PERK_QUICK_POCKETS);
    }

    return std::max(gInventoryApCost - kQuickPocketsApCostReduction * quickPockets, 0);
}

// inventory.cc:1058 — inventorySetInvenApCost
void testInventorySetInvenApCost(int cost)
{
    gInventoryApCost = cost;
}

// inventory.cc:1063 — inventoryResetInvenApCost
void testInventoryResetInvenApCost()
{
    gInventoryApCost = kDefaultInventoryApCost;
}

// inventory.cc:1068 — inventorySetQuickPocketsApCostReduction
void testInventorySetQuickPocketsApCostReduction(int reduction)
{
    kQuickPocketsApCostReduction = reduction;
}

// inventory.cc:1074 — inventorySetDude
void testInventorySetDude(TestObject* obj, int pid)
{
    gInventoryDude = obj;
    gInventoryPid = pid;
}

// ---------------------------------------------------------------------------
// P2: Equipment accessors (inventory.cc:2774-2837)
// ---------------------------------------------------------------------------

// inventory.cc:2774 — critterGetItem2
TestObject* testCritterGetItem2(TestObject* critter)
{
    if (gInventoryRightHandItem != nullptr && critter == gInventoryDude) {
        return gInventoryRightHandItem;
    }

    TestInventory* inventory = &(critter->inventory);
    for (int i = 0; i < inventory->length; i++) {
        TestObject* item = inventory->items[i].item;
        if (item->flags & TEST_OBJECT_IN_RIGHT_HAND) {
            return item;
        }
    }

    return nullptr;
}

// inventory.cc:2796 — critterGetItem1
TestObject* testCritterGetItem1(TestObject* critter)
{
    if (gInventoryLeftHandItem != nullptr && critter == gInventoryDude) {
        return gInventoryLeftHandItem;
    }

    TestInventory* inventory = &(critter->inventory);
    for (int i = 0; i < inventory->length; i++) {
        TestObject* item = inventory->items[i].item;
        if (item->flags & TEST_OBJECT_IN_LEFT_HAND) {
            return item;
        }
    }

    return nullptr;
}

// inventory.cc:2818 — critterGetArmor
TestObject* testCritterGetArmor(TestObject* critter)
{
    if (gInventoryArmor != nullptr && critter == gInventoryDude) {
        return gInventoryArmor;
    }

    TestInventory* inventory = &(critter->inventory);
    for (int i = 0; i < inventory->length; i++) {
        TestObject* item = inventory->items[i].item;
        if (item->flags & TEST_OBJECT_WORN) {
            return item;
        }
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// P4: Object search functions (inventory.cc:2941-2976, 3297-3366)
// ---------------------------------------------------------------------------

// inventory.cc:2941 — objectGetCarriedObjectByPid
TestObject* testObjectGetCarriedObjectByPid(TestObject* obj, int pid)
{
    TestInventory* inventory = &(obj->inventory);

    for (int index = 0; index < inventory->length; index++) {
        TestInventoryItem* inventoryItem = &(inventory->items[index]);
        if (inventoryItem->item->pid == pid) {
            return inventoryItem->item;
        }

        TestObject* found = testObjectGetCarriedObjectByPid(inventoryItem->item, pid);
        if (found != nullptr) {
            return found;
        }
    }

    return nullptr;
}

// inventory.cc:2961 — objectGetCarriedQuantityByPid
int testObjectGetCarriedQuantityByPid(TestObject* object, int pid)
{
    int quantity = 0;

    TestInventory* inventory = &(object->inventory);
    for (int index = 0; index < inventory->length; index++) {
        TestInventoryItem* inventoryItem = &(inventory->items[index]);
        if (inventoryItem->item->pid == pid) {
            quantity += inventoryItem->quantity;
        }

        quantity += testObjectGetCarriedQuantityByPid(inventoryItem->item, pid);
    }

    return quantity;
}

// inventory.cc:3297 — inventoryFindByType
TestObject* testInventoryFindByType(TestObject* obj, int itemType, int* indexPtr)
{
    int dummy = -1;
    if (indexPtr == nullptr) {
        indexPtr = &dummy;
    }

    *indexPtr += 1;

    TestInventory* inventory = &(obj->inventory);

    if (*indexPtr >= inventory->length) {
        return nullptr;
    }

    while (itemType != -1 && testItemGetType(inventory->items[*indexPtr].item) != itemType) {
        *indexPtr += 1;

        if (*indexPtr >= inventory->length) {
            return nullptr;
        }
    }

    return inventory->items[*indexPtr].item;
}

// inventory.cc:3327 — inventoryFindById
TestObject* testInventoryFindById(TestObject* obj, int id)
{
    if (obj->id == id) {
        return obj;
    }

    TestInventory* inventory = &(obj->inventory);
    for (int index = 0; index < inventory->length; index++) {
        TestInventoryItem* inventoryItem = &(inventory->items[index]);
        TestObject* item = inventoryItem->item;
        if (item->id == id) {
            return item;
        }

        if (testItemGetType(item) == TEST_ITEM_TYPE_CONTAINER) {
            item = testInventoryFindById(item, id);
            if (item != nullptr) {
                return item;
            }
        }
    }

    return nullptr;
}

// inventory.cc:3355 — inventoryItemByIndex
TestObject* testInventoryItemByIndex(TestObject* obj, int index)
{
    TestInventory* inventory = &(obj->inventory);

    if (index < 0 || index >= inventory->length) {
        return nullptr;
    }

    return inventory->items[index].item;
}

// ---------------------------------------------------------------------------
// P5: Strip / Restore functions (inventory.cc:2841-2889)
// ---------------------------------------------------------------------------

// inventory.cc:2841 — critterStripEquipped
TestCritterEquipped testCritterStripEquipped(TestObject* critter)
{
    TestCritterEquipped equipped;
    TestInventory* inv = &critter->inventory;
    for (int i = 0; i < inv->length; i++) {
        TestObject* item = inv->items[i].item;
        if ((item->flags & TEST_OBJECT_IN_LEFT_HAND) != 0) {
            if ((item->flags & TEST_OBJECT_IN_RIGHT_HAND) != 0) {
                equipped.rightHand = item;
            }
            equipped.leftHand = item;
        } else if ((item->flags & TEST_OBJECT_IN_RIGHT_HAND) != 0) {
            equipped.rightHand = item;
        } else if ((item->flags & TEST_OBJECT_WORN) != 0) {
            equipped.armor = item;
        }
    }
    if (equipped.leftHand != nullptr) {
        equipped.weight += testItemGetWeight(equipped.leftHand);
        testItemRemove(critter, equipped.leftHand, 1);
    }
    if (equipped.rightHand != nullptr && equipped.rightHand != equipped.leftHand) {
        equipped.weight += testItemGetWeight(equipped.rightHand);
        testItemRemove(critter, equipped.rightHand, 1);
    }
    if (equipped.armor != nullptr) {
        equipped.weight += testItemGetWeight(equipped.armor);
        testItemRemove(critter, equipped.armor, 1);
    }
    return equipped;
}

// inventory.cc:2873 — critterRestoreEquipped
void testCritterRestoreEquipped(TestObject* critter, TestCritterEquipped& equipped)
{
    if (equipped.leftHand != nullptr) {
        equipped.leftHand->flags |= TEST_OBJECT_IN_LEFT_HAND;
        if (equipped.leftHand == equipped.rightHand) equipped.leftHand->flags |= TEST_OBJECT_IN_RIGHT_HAND;
        testItemAdd(critter, equipped.leftHand, 1);
    }
    if (equipped.rightHand != nullptr && equipped.rightHand != equipped.leftHand) {
        equipped.rightHand->flags |= TEST_OBJECT_IN_RIGHT_HAND;
        testItemAdd(critter, equipped.rightHand, 1);
    }
    if (equipped.armor != nullptr) {
        equipped.armor->flags |= TEST_OBJECT_WORN;
        testItemAdd(critter, equipped.armor, 1);
    }
    equipped = {};
}

} // namespace fallout

using namespace fallout;

// ============================================================================
// Test Cases
// ============================================================================

// Macros for clean flag notation in tests
static constexpr unsigned int LH = TEST_OBJECT_IN_LEFT_HAND;
static constexpr unsigned int RH = TEST_OBJECT_IN_RIGHT_HAND;
static constexpr unsigned int WO = TEST_OBJECT_WORN;

// ============================================================================
// P1: AP Cost Tests
// ============================================================================

TEST_CASE("inventoryGetInvenApCost — base formula")
{
    testResetAllPools();
    // Reset globals to defaults
    gInventoryApCost = kDefaultInventoryApCost;
    kQuickPocketsApCostReduction = 2;
    gStubPerkQuickPocketsRank = 0;
    gDude = nullptr;

    SUBCASE("default cost with no dude")
    {
        // no dude → quickPockets = 0 → cost = 4 - 2*0 = 4
        CHECK(testInventoryGetInvenApCost() == 4);
    }

    SUBCASE("default cost with dude, no Quick Pockets ranks")
    {
        TestObject* dude = testObjectAlloc(0);
        gDude = dude;
        gStubPerkQuickPocketsRank = 0;
        CHECK(testInventoryGetInvenApCost() == 4);
    }

    SUBCASE("2 ranks of Quick Pockets → cost = 0")
    {
        TestObject* dude = testObjectAlloc(0);
        gDude = dude;
        gStubPerkQuickPocketsRank = 2;
        // 4 - 2*2 = 0
        CHECK(testInventoryGetInvenApCost() == 0);
    }

    SUBCASE("3 ranks → clamped to 0 (not negative)")
    {
        TestObject* dude = testObjectAlloc(0);
        gDude = dude;
        gStubPerkQuickPocketsRank = 3;
        // 4 - 2*3 = -2 → clamped to 0
        CHECK(testInventoryGetInvenApCost() == 0);
    }

    SUBCASE("1 rank of Quick Pockets → cost = 2")
    {
        TestObject* dude = testObjectAlloc(0);
        gDude = dude;
        gStubPerkQuickPocketsRank = 1;
        // 4 - 2*1 = 2
        CHECK(testInventoryGetInvenApCost() == 2);
    }

    gDude = nullptr;
}

TEST_CASE("inventorySetInvenApCost / inventoryResetInvenApCost")
{
    testResetAllPools();
    gInventoryApCost = kDefaultInventoryApCost;

    SUBCASE("set custom AP cost")
    {
        testInventorySetInvenApCost(6);
        CHECK(gInventoryApCost == 6);
        // Verify getter uses new cost
        TestObject* dude = testObjectAlloc(0);
        gDude = dude;
        gStubPerkQuickPocketsRank = 0;
        CHECK(testInventoryGetInvenApCost() == 6);
    }

    SUBCASE("reset to default")
    {
        testInventorySetInvenApCost(10);
        CHECK(gInventoryApCost == 10);
        testInventoryResetInvenApCost();
        CHECK(gInventoryApCost == 4);
    }

    SUBCASE("set then get with QuickPockets reduction")
    {
        TestObject* dude = testObjectAlloc(0);
        gDude = dude;
        gStubPerkQuickPocketsRank = 1;
        testInventorySetInvenApCost(8);
        CHECK(testInventoryGetInvenApCost() == 6); // 8 - 2*1 = 6
    }

    gDude = nullptr;
}

TEST_CASE("inventorySetQuickPocketsApCostReduction")
{
    testResetAllPools();
    gInventoryApCost = kDefaultInventoryApCost;
    kQuickPocketsApCostReduction = 2;

    SUBCASE("set and read back")
    {
        testInventorySetQuickPocketsApCostReduction(3);
        CHECK(kQuickPocketsApCostReduction == 3);
    }

    SUBCASE("change reduction affects AP cost")
    {
        TestObject* dude = testObjectAlloc(0);
        gDude = dude;
        gStubPerkQuickPocketsRank = 2;
        // default reduction = 2: cost = 4 - 2*2 = 0
        CHECK(testInventoryGetInvenApCost() == 0);

        testInventorySetQuickPocketsApCostReduction(1);
        // reduction = 1: cost = 4 - 1*2 = 2
        CHECK(testInventoryGetInvenApCost() == 2);
    }

    SUBCASE("zero reduction")
    {
        testInventorySetQuickPocketsApCostReduction(0);
        TestObject* dude = testObjectAlloc(0);
        gDude = dude;
        gStubPerkQuickPocketsRank = 1;
        // 4 - 0*1 = 4
        CHECK(testInventoryGetInvenApCost() == 4);
    }

    kQuickPocketsApCostReduction = 2;
    gDude = nullptr;
}

TEST_CASE("inventoryResetDude / inventorySetDude")
{
    testResetAllPools();
    gInventoryDude = nullptr;
    gInventoryPid = 0;

    SUBCASE("resetDude with null gDude")
    {
        gDude = nullptr;
        testInventoryResetDude();
        CHECK(gInventoryDude == nullptr);
        CHECK(gInventoryPid == 0x1000000);
    }

    SUBCASE("resetDude with valid gDude")
    {
        TestObject* dude = testObjectAlloc(0);
        gDude = dude;
        testInventoryResetDude();
        CHECK(gInventoryDude == dude);
        CHECK(gInventoryPid == 0x1000000);
    }

    SUBCASE("setDude with valid obj")
    {
        TestObject* druggie = testObjectAlloc(42);
        testInventorySetDude(druggie, 123);
        CHECK(gInventoryDude == druggie);
        CHECK(gInventoryPid == 123);
    }

    SUBCASE("setDude with null obj")
    {
        testInventorySetDude(nullptr, 0);
        CHECK(gInventoryDude == nullptr);
        CHECK(gInventoryPid == 0);
    }

    gDude = nullptr;
}

// ============================================================================
// P2: Equipment Accessor Tests
// ============================================================================

TEST_CASE("critterGetItem1 — left hand item lookup")
{
    testResetAllPools();
    gInventoryLeftHandItem = nullptr;
    gInventoryDude = nullptr;

    SUBCASE("finds item with OBJECT_IN_LEFT_HAND flag")
    {
        TestObject* critter = testObjectAlloc(0); // pid 0 = critter
        TestObject* fist = testMakeEquippedItem(100, LH); // weapon pid 100
        TestInventoryItem* items[1] = { testItemAlloc(fist) };
        testInventorySetup(critter, items, 1);

        CHECK(testCritterGetItem1(critter) == fist);
    }

    SUBCASE("returns nullptr when no item has left hand flag")
    {
        TestObject* critter = testObjectAlloc(0);
        TestObject* junk = testMakeEquippedItem(200, 0); // no equip flags
        TestInventoryItem* items[1] = { testItemAlloc(junk) };
        testInventorySetup(critter, items, 1);

        CHECK(testCritterGetItem1(critter) == nullptr);
    }

    SUBCASE("skips items with only right hand flag")
    {
        TestObject* critter = testObjectAlloc(0);
        TestObject* rightWeapon = testMakeEquippedItem(101, RH);
        TestInventoryItem* items[1] = { testItemAlloc(rightWeapon) };
        testInventorySetup(critter, items, 1);

        CHECK(testCritterGetItem1(critter) == nullptr);
    }

    SUBCASE("returns first matching item when multiple equipped")
    {
        TestObject* critter = testObjectAlloc(0);
        TestObject* fist1 = testMakeEquippedItem(100, LH);
        TestObject* fist2 = testMakeEquippedItem(101, LH);
        TestInventoryItem* items[2] = { testItemAlloc(fist1), testItemAlloc(fist2) };
        testInventorySetup(critter, items, 2);

        CHECK(testCritterGetItem1(critter) == fist1);
    }

    SUBCASE("empty inventory returns nullptr")
    {
        TestObject* critter = testObjectAlloc(0);
        testInventorySetup(critter, nullptr, 0);
        CHECK(testCritterGetItem1(critter) == nullptr);
    }

    SUBCASE("global cache: returns gInventoryLeftHandItem when critter is inventory dude")
    {
        TestObject* dude = testObjectAlloc(0);
        TestObject* fist = testMakeEquippedItem(100, LH);
        TestObject* differentFist = testMakeEquippedItem(101, LH);

        gInventoryDude = dude;
        gInventoryLeftHandItem = fist;

        // Even though inventory has differentFist, cache takes precedence
        TestInventoryItem* items[1] = { testItemAlloc(differentFist) };
        testInventorySetup(dude, items, 1);

        CHECK(testCritterGetItem1(dude) == fist);
    }

    SUBCASE("global cache: not used when critter is different from inventory dude")
    {
        TestObject* dude = testObjectAlloc(0);
        TestObject* npc = testObjectAlloc(0);
        TestObject* npcFist = testMakeEquippedItem(100, LH);

        gInventoryDude = dude;
        gInventoryLeftHandItem = testMakeEquippedItem(101, LH); // cache set for dude

        TestInventoryItem* items[1] = { testItemAlloc(npcFist) };
        testInventorySetup(npc, items, 1);

        // Cache should NOT be used; should search inventory
        CHECK(testCritterGetItem1(npc) == npcFist);
    }

    gInventoryLeftHandItem = nullptr;
    gInventoryDude = nullptr;
}

TEST_CASE("critterGetItem2 — right hand item lookup")
{
    testResetAllPools();
    gInventoryRightHandItem = nullptr;
    gInventoryDude = nullptr;

    SUBCASE("finds item with OBJECT_IN_RIGHT_HAND flag")
    {
        TestObject* critter = testObjectAlloc(0);
        TestObject* weapon = testMakeEquippedItem(100, RH);
        TestInventoryItem* items[1] = { testItemAlloc(weapon) };
        testInventorySetup(critter, items, 1);

        CHECK(testCritterGetItem2(critter) == weapon);
    }

    SUBCASE("returns nullptr when no right hand item")
    {
        TestObject* critter = testObjectAlloc(0);
        TestObject* armor = testMakeEquippedItem(0, WO);
        TestInventoryItem* items[1] = { testItemAlloc(armor) };
        testInventorySetup(critter, items, 1);

        CHECK(testCritterGetItem2(critter) == nullptr);
    }

    SUBCASE("dual-wield: item with both flags found by both accessors")
    {
        TestObject* critter = testObjectAlloc(0);
        TestObject* dualWeapon = testMakeEquippedItem(100, LH | RH);
        TestInventoryItem* items[1] = { testItemAlloc(dualWeapon) };
        testInventorySetup(critter, items, 1);

        CHECK(testCritterGetItem1(critter) == dualWeapon);
        CHECK(testCritterGetItem2(critter) == dualWeapon);
    }

    SUBCASE("global cache: returns gInventoryRightHandItem for inventory dude")
    {
        TestObject* dude = testObjectAlloc(0);
        TestObject* cached = testMakeEquippedItem(100, RH);
        gInventoryDude = dude;
        gInventoryRightHandItem = cached;

        CHECK(testCritterGetItem2(dude) == cached);
    }

    gInventoryRightHandItem = nullptr;
    gInventoryDude = nullptr;
}

TEST_CASE("critterGetArmor — worn armor lookup")
{
    testResetAllPools();
    gInventoryArmor = nullptr;
    gInventoryDude = nullptr;

    SUBCASE("finds item with OBJECT_WORN flag")
    {
        TestObject* critter = testObjectAlloc(0);
        TestObject* armor = testMakeEquippedItem(0, WO);
        TestInventoryItem* items[1] = { testItemAlloc(armor) };
        testInventorySetup(critter, items, 1);

        CHECK(testCritterGetArmor(critter) == armor);
    }

    SUBCASE("returns nullptr when no armor equipped")
    {
        TestObject* critter = testObjectAlloc(0);
        TestObject* weapon = testMakeEquippedItem(100, LH);
        TestInventoryItem* items[1] = { testItemAlloc(weapon) };
        testInventorySetup(critter, items, 1);

        CHECK(testCritterGetArmor(critter) == nullptr);
    }

    SUBCASE("armor and weapon coexist")
    {
        TestObject* critter = testObjectAlloc(0);
        TestObject* armor = testMakeEquippedItem(0, WO);
        TestObject* weapon = testMakeEquippedItem(100, LH);
        TestInventoryItem* items[2] = { testItemAlloc(armor), testItemAlloc(weapon) };
        testInventorySetup(critter, items, 2);

        CHECK(testCritterGetArmor(critter) == armor);
        CHECK(testCritterGetItem1(critter) == weapon);
    }

    SUBCASE("global cache: returns gInventoryArmor for inventory dude")
    {
        TestObject* dude = testObjectAlloc(0);
        TestObject* cached = testMakeEquippedItem(0, WO);
        gInventoryDude = dude;
        gInventoryArmor = cached;

        CHECK(testCritterGetArmor(dude) == cached);
    }

    gInventoryArmor = nullptr;
    gInventoryDude = nullptr;
}

TEST_CASE("equipment accessors — full kit scenario")
{
    testResetAllPools();
    gInventoryLeftHandItem = nullptr;
    gInventoryRightHandItem = nullptr;
    gInventoryArmor = nullptr;
    gInventoryDude = nullptr;

    TestObject* critter = testObjectAlloc(0);
    TestObject* fist = testMakeEquippedItem(100, LH);
    TestObject* knife = testMakeEquippedItem(101, RH);
    TestObject* armor = testMakeEquippedItem(0, WO);
    TestObject* junk = testMakeEquippedItem(200, 0); // ammo, not equipped

    TestInventoryItem* items[4] = {
        testItemAlloc(fist),
        testItemAlloc(knife),
        testItemAlloc(armor),
        testItemAlloc(junk),
    };
    testInventorySetup(critter, items, 4);

    CHECK(testCritterGetItem1(critter) == fist);
    CHECK(testCritterGetItem2(critter) == knife);
    CHECK(testCritterGetArmor(critter) == armor);
    // junk not found by any accessor
}

// ============================================================================
// P4: Object Search Tests
// ============================================================================

TEST_CASE("objectGetCarriedObjectByPid — flat inventory search")
{
    testResetAllPools();

    SUBCASE("finds item by pid in flat inventory")
    {
        TestObject* container = testObjectAlloc(400); // container type
        TestObject* item1 = testMakeItem(42);         // pid 42
        TestObject* item2 = testMakeItem(99);          // pid 99
        TestInventoryItem* items[2] = { testItemAlloc(item1), testItemAlloc(item2) };
        testInventorySetup(container, items, 2);

        CHECK(testObjectGetCarriedObjectByPid(container, 42) == item1);
        CHECK(testObjectGetCarriedObjectByPid(container, 99) == item2);
    }

    SUBCASE("returns nullptr when pid not found")
    {
        TestObject* container = testObjectAlloc(400);
        TestObject* item1 = testMakeItem(42);
        TestInventoryItem* items[1] = { testItemAlloc(item1) };
        testInventorySetup(container, items, 1);

        CHECK(testObjectGetCarriedObjectByPid(container, 999) == nullptr);
    }

    SUBCASE("returns nullptr for empty inventory")
    {
        TestObject* container = testObjectAlloc(400);
        testInventorySetup(container, nullptr, 0);

        CHECK(testObjectGetCarriedObjectByPid(container, 42) == nullptr);
    }

    SUBCASE("returns first match when duplicate PIDs exist")
    {
        TestObject* container = testObjectAlloc(400);
        TestObject* item1 = testMakeItem(42);
        TestObject* item2 = testMakeItem(42); // same PID, different object
        TestInventoryItem* items[2] = { testItemAlloc(item1), testItemAlloc(item2) };
        testInventorySetup(container, items, 2);

        CHECK(testObjectGetCarriedObjectByPid(container, 42) == item1);
    }
}

TEST_CASE("objectGetCarriedObjectByPid — recursive container search")
{
    testResetAllPools();

    // Container A containing: item (pid 10), sub-container B
    // Sub-container B containing: item (pid 20)
    TestObject* containerA = testObjectAlloc(400);
    TestObject* itemPid10 = testMakeItem(10);
    TestObject* containerB = testObjectAlloc(400);
    TestObject* itemPid20 = testMakeItem(20);

    TestInventoryItem* itemsA[2] = { testItemAlloc(itemPid10), testItemAlloc(containerB) };
    testInventorySetup(containerA, itemsA, 2);

    TestInventoryItem* itemsB[1] = { testItemAlloc(itemPid20) };
    testInventorySetup(containerB, itemsB, 1);

    SUBCASE("finds item in nested container before direct item")
    {
        // The function searches top-level first, then recurses into each item
        // Top-level has pid 10 (found first), sub-container has pid 20
        CHECK(testObjectGetCarriedObjectByPid(containerA, 10) == itemPid10);
        CHECK(testObjectGetCarriedObjectByPid(containerA, 20) == itemPid20);
    }

    SUBCASE("deeply nested search")
    {
        // Add another level: containerB contains containerC
        TestObject* containerC = testObjectAlloc(400);
        TestObject* itemPid30 = testMakeItem(30);

        TestInventoryItem* itemsB2[2] = { testItemAlloc(itemPid20), testItemAlloc(containerC) };
        testInventorySetup(containerB, itemsB2, 2);

        TestInventoryItem* itemsC[1] = { testItemAlloc(itemPid30) };
        testInventorySetup(containerC, itemsC, 1);

        CHECK(testObjectGetCarriedObjectByPid(containerA, 30) == itemPid30);
    }
}

TEST_CASE("objectGetCarriedQuantityByPid — quantity counting")
{
    testResetAllPools();

    SUBCASE("counts single item quantity")
    {
        TestObject* container = testObjectAlloc(400);
        TestObject* ammo = testMakeItem(200); // pid 200 = ammo
        TestInventoryItem* items[1] = { testItemAlloc(ammo, 24) };
        testInventorySetup(container, items, 1);

        CHECK(testObjectGetCarriedQuantityByPid(container, 200) == 24);
    }

    SUBCASE("sums quantities from multiple stacks of same pid")
    {
        TestObject* container = testObjectAlloc(400);
        TestObject* ammo1 = testMakeItem(200);
        TestObject* ammo2 = testMakeItem(200);
        TestInventoryItem* items[2] = { testItemAlloc(ammo1, 10), testItemAlloc(ammo2, 5) };
        testInventorySetup(container, items, 2);

        CHECK(testObjectGetCarriedQuantityByPid(container, 200) == 15);
    }

    SUBCASE("returns 0 when pid not found")
    {
        TestObject* container = testObjectAlloc(400);
        TestObject* stim = testMakeItem(300); // drug
        TestInventoryItem* items[1] = { testItemAlloc(stim, 3) };
        testInventorySetup(container, items, 1);

        CHECK(testObjectGetCarriedQuantityByPid(container, 999) == 0);
    }

    SUBCASE("recursive counting in nested containers")
    {
        TestObject* outer = testObjectAlloc(400);
        TestObject* ammo1 = testMakeItem(200);
        TestObject* inner = testObjectAlloc(400);
        TestObject* ammo2 = testMakeItem(200);

        TestInventoryItem* itemsOuter[2] = { testItemAlloc(ammo1, 5), testItemAlloc(inner) };
        testInventorySetup(outer, itemsOuter, 2);

        TestInventoryItem* itemsInner[1] = { testItemAlloc(ammo2, 3) };
        testInventorySetup(inner, itemsInner, 1);

        // 5 in outer + 3 in inner = 8 total
        CHECK(testObjectGetCarriedQuantityByPid(outer, 200) == 8);
    }

    SUBCASE("empty inventory returns 0")
    {
        TestObject* container = testObjectAlloc(400);
        testInventorySetup(container, nullptr, 0);

        CHECK(testObjectGetCarriedQuantityByPid(container, 200) == 0);
    }
}

TEST_CASE("inventoryFindByType — type-based search")
{
    testResetAllPools();

    SUBCASE("finds first item of specified type")
    {
        TestObject* critter = testObjectAlloc(0);
        TestObject* armor   = testMakeItem(0);   // armor (pid 0-99)
        TestObject* weapon  = testMakeItem(100);  // weapon (pid 100-199)
        TestObject* ammo    = testMakeItem(200);  // ammo (pid 200-299)

        TestInventoryItem* items[3] = {
            testItemAlloc(armor),
            testItemAlloc(weapon),
            testItemAlloc(ammo),
        };
        testInventorySetup(critter, items, 3);

        int index = -1;
        CHECK(testInventoryFindByType(critter, TEST_ITEM_TYPE_WEAPON, &index) == weapon);
        CHECK(index == 1);

        // Reset and find ammo
        index = -1;
        CHECK(testInventoryFindByType(critter, TEST_ITEM_TYPE_AMMO, &index) == ammo);
        CHECK(index == 2);
    }

    SUBCASE("itemType -1 returns first item regardless of type")
    {
        TestObject* critter = testObjectAlloc(0);
        TestObject* armor = testMakeItem(0);
        TestObject* weapon = testMakeItem(100);

        TestInventoryItem* items[2] = { testItemAlloc(armor), testItemAlloc(weapon) };
        testInventorySetup(critter, items, 2);

        int index = -1;
        CHECK(testInventoryFindByType(critter, -1, &index) == armor);
        CHECK(index == 0);
    }

    SUBCASE("nullptr indexPtr uses internal dummy")
    {
        TestObject* critter = testObjectAlloc(0);
        TestObject* armor = testMakeItem(0);
        TestInventoryItem* items[1] = { testItemAlloc(armor) };
        testInventorySetup(critter, items, 1);

        // Passing nullptr: function uses local dummy initialized to -1
        CHECK(testInventoryFindByType(critter, TEST_ITEM_TYPE_ARMOR, nullptr) == armor);
    }

    SUBCASE("returns nullptr when no matching type found")
    {
        TestObject* critter = testObjectAlloc(0);
        TestObject* armor = testMakeItem(0);
        TestInventoryItem* items[1] = { testItemAlloc(armor) };
        testInventorySetup(critter, items, 1);

        int index = -1;
        CHECK(testInventoryFindByType(critter, TEST_ITEM_TYPE_DRUG, &index) == nullptr);
    }

    SUBCASE("returns nullptr for empty inventory")
    {
        TestObject* critter = testObjectAlloc(0);
        testInventorySetup(critter, nullptr, 0);

        int index = -1;
        CHECK(testInventoryFindByType(critter, TEST_ITEM_TYPE_WEAPON, &index) == nullptr);
    }
}

TEST_CASE("inventoryFindById — id-based search")
{
    testResetAllPools();

    SUBCASE("finds root object by its own id")
    {
        TestObject* obj = testObjectAlloc(0, 0, 42);
        testInventorySetup(obj, nullptr, 0);

        CHECK(testInventoryFindById(obj, 42) == obj);
    }

    SUBCASE("finds direct child by id")
    {
        TestObject* container = testObjectAlloc(400, 0, 1);
        TestObject* child = testObjectAlloc(100, 0, 99);
        TestInventoryItem* items[1] = { testItemAlloc(child) };
        testInventorySetup(container, items, 1);

        CHECK(testInventoryFindById(container, 99) == child);
    }

    SUBCASE("recursive search in container item")
    {
        TestObject* outer = testObjectAlloc(400, 0, 1);
        TestObject* innerContainer = testObjectAlloc(400, 0, 2); // container type
        TestObject* deepItem = testObjectAlloc(100, 0, 99);

        TestInventoryItem* itemsOuter[1] = { testItemAlloc(innerContainer) };
        testInventorySetup(outer, itemsOuter, 1);

        TestInventoryItem* itemsInner[1] = { testItemAlloc(deepItem) };
        testInventorySetup(innerContainer, itemsInner, 1);

        CHECK(testInventoryFindById(outer, 99) == deepItem);
    }

    SUBCASE("does NOT recurse into non-container items")
    {
        TestObject* outer = testObjectAlloc(0, 0, 1); // critter
        TestObject* weapon = testObjectAlloc(100, 0, 2); // weapon, not container
        TestObject* nested = testObjectAlloc(100, 0, 99);

        // Weapon has an inventory containing nested. Since weapon is not
        // ITEM_TYPE_CONTAINER, findById should NOT recurse into it.
        TestInventoryItem* itemsCritter[1] = { testItemAlloc(weapon) };
        testInventorySetup(outer, itemsCritter, 1);

        TestInventoryItem* itemsWeapon[1] = { testItemAlloc(nested) };
        testInventorySetup(weapon, itemsWeapon, 1);

        // Outer -> weapon (found first at index 0, id=2, not 99)
        // Weapon has id=2, not 99. Since weapon is not a container,
        // recursion does NOT descend. Result: nullptr.
        CHECK(testInventoryFindById(outer, 99) == nullptr);
    }

    SUBCASE("returns nullptr when id not found anywhere")
    {
        TestObject* container = testObjectAlloc(400, 0, 1);
        TestObject* child = testObjectAlloc(100, 0, 2);
        TestInventoryItem* items[1] = { testItemAlloc(child) };
        testInventorySetup(container, items, 1);

        CHECK(testInventoryFindById(container, 999) == nullptr);
    }

    SUBCASE("returns nullptr for empty inventory, non-matching root")
    {
        TestObject* obj = testObjectAlloc(0, 0, 1);
        testInventorySetup(obj, nullptr, 0);

        CHECK(testInventoryFindById(obj, 42) == nullptr);
    }
}

TEST_CASE("inventoryItemByIndex — index-based access")
{
    testResetAllPools();

    SUBCASE("returns item at valid index")
    {
        TestObject* container = testObjectAlloc(400);
        TestObject* item0 = testMakeItem(10);
        TestObject* item1 = testMakeItem(20);
        TestObject* item2 = testMakeItem(30);

        TestInventoryItem* items[3] = {
            testItemAlloc(item0),
            testItemAlloc(item1),
            testItemAlloc(item2),
        };
        testInventorySetup(container, items, 3);

        CHECK(testInventoryItemByIndex(container, 0) == item0);
        CHECK(testInventoryItemByIndex(container, 1) == item1);
        CHECK(testInventoryItemByIndex(container, 2) == item2);
    }

    SUBCASE("returns nullptr for negative index")
    {
        TestObject* container = testObjectAlloc(400);
        TestObject* item = testMakeItem(10);
        TestInventoryItem* items[1] = { testItemAlloc(item) };
        testInventorySetup(container, items, 1);

        CHECK(testInventoryItemByIndex(container, -1) == nullptr);
    }

    SUBCASE("returns nullptr for index >= length")
    {
        TestObject* container = testObjectAlloc(400);
        TestObject* item = testMakeItem(10);
        TestInventoryItem* items[1] = { testItemAlloc(item) };
        testInventorySetup(container, items, 1);

        CHECK(testInventoryItemByIndex(container, 1) == nullptr);
        CHECK(testInventoryItemByIndex(container, 100) == nullptr);
    }

    SUBCASE("returns nullptr for empty inventory")
    {
        TestObject* container = testObjectAlloc(400);
        testInventorySetup(container, nullptr, 0);

        CHECK(testInventoryItemByIndex(container, 0) == nullptr);
    }
}

// ============================================================================
// P5: Strip / Restore Tests
// ============================================================================

TEST_CASE("critterStripEquipped — removes equipped items")
{
    testResetAllPools();
    gStubItemWeight = 5;

    SUBCASE("strips left hand, right hand, and armor")
    {
        TestObject* critter = testObjectAlloc(0);

        TestObject* fist   = testMakeEquippedItem(100, LH);
        TestObject* knife  = testMakeEquippedItem(101, RH);
        TestObject* jacket = testMakeEquippedItem(0, WO);
        TestObject* junk   = testMakeItem(200, 0); // ammo, not equipped

        TestInventoryItem* items[4] = {
            testItemAlloc(fist),
            testItemAlloc(knife),
            testItemAlloc(jacket),
            testItemAlloc(junk),
        };
        int origLength = 4;
        testInventorySetup(critter, items, origLength);

        TestCritterEquipped result = testCritterStripEquipped(critter);

        CHECK(result.leftHand == fist);
        CHECK(result.rightHand == knife);
        CHECK(result.armor == jacket);
        CHECK(result.weight == 15); // 3 equipped items * 5 weight each

        // Inventory should now only contain junk (3 equipped items removed)
        CHECK(critter->inventory.length == 1);
        CHECK(critter->inventory.items[0].item == junk);
    }

    SUBCASE("dual-wield same item in both hands")
    {
        TestObject* critter = testObjectAlloc(0);

        TestObject* dualWield = testMakeEquippedItem(100, LH | RH);

        TestInventoryItem* items[1] = { testItemAlloc(dualWield) };
        testInventorySetup(critter, items, 1);

        TestCritterEquipped result = testCritterStripEquipped(critter);

        CHECK(result.leftHand == dualWield);
        CHECK(result.rightHand == dualWield);
        // Weight counted once (rightHand == leftHand, so no double count)
        CHECK(result.weight == 5);
        // Item removed once
        CHECK(critter->inventory.length == 0);
    }

    SUBCASE("only left hand equipped")
    {
        TestObject* critter = testObjectAlloc(0);
        TestObject* fist = testMakeEquippedItem(100, LH);

        TestInventoryItem* items[1] = { testItemAlloc(fist) };
        testInventorySetup(critter, items, 1);

        TestCritterEquipped result = testCritterStripEquipped(critter);

        CHECK(result.leftHand == fist);
        CHECK(result.rightHand == nullptr);
        CHECK(result.armor == nullptr);
        CHECK(result.weight == 5);
        CHECK(critter->inventory.length == 0);
    }

    SUBCASE("only armor equipped")
    {
        TestObject* critter = testObjectAlloc(0);
        TestObject* armor = testMakeEquippedItem(0, WO);

        TestInventoryItem* items[1] = { testItemAlloc(armor) };
        testInventorySetup(critter, items, 1);

        TestCritterEquipped result = testCritterStripEquipped(critter);

        CHECK(result.leftHand == nullptr);
        CHECK(result.rightHand == nullptr);
        CHECK(result.armor == armor);
        CHECK(result.weight == 5);
        CHECK(critter->inventory.length == 0);
    }

    SUBCASE("empty critter returns empty equipped struct")
    {
        TestObject* critter = testObjectAlloc(0);
        testInventorySetup(critter, nullptr, 0);

        TestCritterEquipped result = testCritterStripEquipped(critter);

        CHECK(result.leftHand == nullptr);
        CHECK(result.rightHand == nullptr);
        CHECK(result.armor == nullptr);
        CHECK(result.weight == 0);
        CHECK(critter->inventory.length == 0);
    }
}

TEST_CASE("critterRestoreEquipped — restores equipped items")
{
    testResetAllPools();

    SUBCASE("restores left hand, right hand, and armor")
    {
        TestObject* critter = testObjectAlloc(0);
        testInventorySetup(critter, nullptr, 0);

        TestObject* fist   = testMakeEquippedItem(100, 0); // flags cleared during strip
        TestObject* knife  = testMakeEquippedItem(101, 0);
        TestObject* jacket = testMakeEquippedItem(0, 0);

        TestCritterEquipped eq;
        eq.leftHand = fist;
        eq.rightHand = knife;
        eq.armor = jacket;

        testCritterRestoreEquipped(critter, eq);

        CHECK(critter->inventory.length == 3);

        // Flags should be restored
        bool hasLeftHand = (fist->flags & TEST_OBJECT_IN_LEFT_HAND) != 0;
        CHECK(hasLeftHand);
        bool hasRightHand = (knife->flags & TEST_OBJECT_IN_RIGHT_HAND) != 0;
        CHECK(hasRightHand);
        bool hasWorn = (jacket->flags & TEST_OBJECT_WORN) != 0;
        CHECK(hasWorn);

        // Equipped struct should be zeroed
        CHECK(eq.leftHand == nullptr);
        CHECK(eq.rightHand == nullptr);
        CHECK(eq.armor == nullptr);
        CHECK(eq.weight == 0);
    }

    SUBCASE("dual-wield restore sets both flags")
    {
        TestObject* critter = testObjectAlloc(0);
        testInventorySetup(critter, nullptr, 0);

        TestObject* dualWield = testMakeEquippedItem(100, 0);

        TestCritterEquipped eq;
        eq.leftHand = dualWield;
        eq.rightHand = dualWield;

        testCritterRestoreEquipped(critter, eq);

        CHECK(critter->inventory.length == 1);
        CHECK(testHasItemFlag(dualWield->flags, TEST_OBJECT_IN_LEFT_HAND));
        CHECK(testHasItemFlag(dualWield->flags, TEST_OBJECT_IN_RIGHT_HAND));
    }

    SUBCASE("restore with null fields skips those")
    {
        TestObject* critter = testObjectAlloc(0);
        testInventorySetup(critter, nullptr, 0);

        TestObject* fist = testMakeEquippedItem(100, 0);

        TestCritterEquipped eq;
        eq.leftHand = fist;
        eq.rightHand = nullptr;
        eq.armor = nullptr;

        testCritterRestoreEquipped(critter, eq);

        CHECK(critter->inventory.length == 1);
        CHECK(testHasItemFlag(fist->flags, TEST_OBJECT_IN_LEFT_HAND));
        CHECK_FALSE(testHasItemFlag(fist->flags, TEST_OBJECT_IN_RIGHT_HAND));
        CHECK_FALSE(testHasItemFlag(fist->flags, TEST_OBJECT_WORN));
    }

    SUBCASE("empty equipped struct restores nothing")
    {
        TestObject* critter = testObjectAlloc(0);
        testInventorySetup(critter, nullptr, 0);

        TestCritterEquipped eq;
        testCritterRestoreEquipped(critter, eq);

        CHECK(critter->inventory.length == 0);
    }
}

TEST_CASE("strip and restore round-trip")
{
    testResetAllPools();
    gStubItemWeight = 7;

    // Set up critter with full kit
    TestObject* critter = testObjectAlloc(0);
    TestObject* fist   = testMakeEquippedItem(100, LH);
    TestObject* knife  = testMakeEquippedItem(101, RH);
    TestObject* jacket = testMakeEquippedItem(0, WO);

    TestInventoryItem* items[3] = {
        testItemAlloc(fist),
        testItemAlloc(knife),
        testItemAlloc(jacket),
    };
    testInventorySetup(critter, items, 3);

    // Strip
    TestCritterEquipped equipped = testCritterStripEquipped(critter);
    CHECK(critter->inventory.length == 0);
    CHECK(equipped.leftHand == fist);
    CHECK(equipped.rightHand == knife);
    CHECK(equipped.armor == jacket);
    CHECK(equipped.weight == 21); // 3 * 7

    // Restore
    testCritterRestoreEquipped(critter, equipped);
    CHECK(critter->inventory.length == 3);

    // Flags should be back
    CHECK(testHasItemFlag(fist->flags, TEST_OBJECT_IN_LEFT_HAND));
    CHECK(testHasItemFlag(knife->flags, TEST_OBJECT_IN_RIGHT_HAND));
    CHECK(testHasItemFlag(jacket->flags, TEST_OBJECT_WORN));

    // Strip again — should work the same
    TestCritterEquipped equipped2 = testCritterStripEquipped(critter);
    CHECK(critter->inventory.length == 0);
    CHECK(equipped2.leftHand == fist);
    CHECK(equipped2.rightHand == knife);
    CHECK(equipped2.armor == jacket);
}

// ============================================================================
// Enum/Constant Validation
// ============================================================================

TEST_CASE("constant values match production")
{
    // Verify OBJECT_IN_LEFT_HAND, OBJECT_IN_RIGHT_HAND, OBJECT_WORN
    // These are bitmask values used by flag-checking code; if they
    // don't match the production values in obj_types.h, the tests
    // would pass but the production code would behave differently.
    CHECK(TEST_OBJECT_IN_LEFT_HAND == 0x1000000);
    CHECK(TEST_OBJECT_IN_RIGHT_HAND == 0x2000000);
    CHECK(TEST_OBJECT_WORN == 0x4000000);

    // Verify bitmask combinations
    unsigned int hands = TEST_OBJECT_IN_LEFT_HAND | TEST_OBJECT_IN_RIGHT_HAND;
    CHECK(hands == 0x3000000);
    unsigned int allSlots = TEST_OBJECT_IN_LEFT_HAND | TEST_OBJECT_IN_RIGHT_HAND | TEST_OBJECT_WORN;
    CHECK(allSlots == 0x7000000);

    // AP cost constants
    CHECK(kDefaultInventoryApCost == 4);
    CHECK(testInventoryGetInvenApCost() >= 0); // clamping invariant
}

TEST_CASE("item type PID range mapping consistency")
{
    testResetAllPools();

    // Verify our PID→type mapping covers all item types
    TestObject* armor     = testMakeItem(0);
    TestObject* weapon    = testMakeItem(100);
    TestObject* ammo      = testMakeItem(200);
    TestObject* drug      = testMakeItem(300);
    TestObject* container = testMakeItem(400);

    CHECK(testItemGetType(armor) == TEST_ITEM_TYPE_ARMOR);
    CHECK(testItemGetType(weapon) == TEST_ITEM_TYPE_WEAPON);
    CHECK(testItemGetType(ammo) == TEST_ITEM_TYPE_AMMO);
    CHECK(testItemGetType(drug) == TEST_ITEM_TYPE_DRUG);
    CHECK(testItemGetType(container) == TEST_ITEM_TYPE_CONTAINER);

    // Out of range returns -1
    TestObject* unknown = testMakeItem(999);
    CHECK(testItemGetType(unknown) == -1);

    // nullptr returns -1 (safe)
    CHECK(testItemGetType(nullptr) == -1);
}
