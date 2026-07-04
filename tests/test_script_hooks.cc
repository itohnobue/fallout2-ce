// Unit tests for sfall_script_hooks — hook type mapping and constants.
//
// Tests: HookType enum values, HOOK_COUNT, sub-enum values,
//        constant correctness, deliberately absent hooks verification.
//
// NOTE: Does not test ScriptHookCall constructors or lifecycle functions
// (those require linking sfall_script_hooks.cc, which has 50+ engine
// dependencies). This test validates the hook type definitions from the header.
// Functional hook tests belong in the .ssl integration test suite.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// Include the hooks header for type definitions.
#include "sfall_script_hooks.h"

using namespace fallout;

// ---- HookType Enum Tests ----

TEST_CASE("HookType enum values are correct")
{
    // Verify the enum values are zero-based and sequential per the specification
    CHECK(static_cast<int>(HOOK_TOHIT) == 0);
    CHECK(static_cast<int>(HOOK_AFTERHITROLL) == 1);
    CHECK(static_cast<int>(HOOK_CALCAPCOST) == 2);
    // 3 = HOOK_DEATHANIM1 (absent)
    CHECK(static_cast<int>(HOOK_DEATHANIM2) == 4);
    CHECK(static_cast<int>(HOOK_COMBATDAMAGE) == 5);
    CHECK(static_cast<int>(HOOK_ONDEATH) == 6);
    // 7 = HOOK_FINDTARGET (absent)
    CHECK(static_cast<int>(HOOK_USEOBJON) == 8);
    // 9 = HOOK_REMOVEINVENOBJ (absent)
    CHECK(static_cast<int>(HOOK_BARTERPRICE) == 10);
    CHECK(static_cast<int>(HOOK_MOVECOST) == 11);
    // 12-15 = hex blocking hooks (obsolete)
    CHECK(static_cast<int>(HOOK_ITEMDAMAGE) == 16);
    CHECK(static_cast<int>(HOOK_AMMOCOST) == 17);
    CHECK(static_cast<int>(HOOK_USEOBJ) == 18);
    CHECK(static_cast<int>(HOOK_KEYPRESS) == 19);
    CHECK(static_cast<int>(HOOK_MOUSECLICK) == 20);
    CHECK(static_cast<int>(HOOK_USESKILL) == 21);
    CHECK(static_cast<int>(HOOK_STEAL) == 22);
    CHECK(static_cast<int>(HOOK_WITHINPERCEPTION) == 23);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE) == 24);
    CHECK(static_cast<int>(HOOK_INVENWIELD) == 25);
    CHECK(static_cast<int>(HOOK_ADJUSTFID) == 26);
    CHECK(static_cast<int>(HOOK_COMBATTURN) == 27);
    CHECK(static_cast<int>(HOOK_CARTRAVEL) == 28);
    CHECK(static_cast<int>(HOOK_SETGLOBALVAR) == 29);
    CHECK(static_cast<int>(HOOK_RESTTIMER) == 30);
    CHECK(static_cast<int>(HOOK_GAMEMODECHANGE) == 31);
    CHECK(static_cast<int>(HOOK_USEANIMOBJ) == 32);
    CHECK(static_cast<int>(HOOK_EXPLOSIVETIMER) == 33);
    CHECK(static_cast<int>(HOOK_DESCRIPTIONOBJ) == 34);
    CHECK(static_cast<int>(HOOK_USESKILLON) == 35);
    // 36 = HOOK_ONEXPLOSION (absent)
    // 37 = HOOK_SUBCOMBATDAMAGE (absent)
    CHECK(static_cast<int>(HOOK_SETLIGHTING) == 38);
    CHECK(static_cast<int>(HOOK_SNEAK) == 39);
    CHECK(static_cast<int>(HOOK_STDPROCEDURE) == 40);
    CHECK(static_cast<int>(HOOK_STDPROCEDURE_END) == 41);
    // 42 = HOOK_TARGETOBJECT (absent)
    CHECK(static_cast<int>(HOOK_ENCOUNTER) == 43);
    // 44 = HOOK_ADJUSTPOISON (absent)
    // 45 = HOOK_ADJUSTRADS (absent)
    // 46 = HOOK_ROLLCHECK (absent)
    // 47 = HOOK_BESTWEAPON (absent)
    CHECK(static_cast<int>(HOOK_CANUSEWEAPON) == 48);
    // 49-60 = reserved
    // 61 = HOOK_BUILDSFXWEAPON (absent)
}

TEST_CASE("HOOK_COUNT covers all defined hook types")
{
    CHECK(static_cast<int>(HOOK_COUNT) == 62);

    // Every defined hook type must be < HOOK_COUNT
    CHECK(static_cast<int>(HOOK_TOHIT) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_CANUSEWEAPON) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_STDPROCEDURE_END) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_ENCOUNTER) < HOOK_COUNT);
}

TEST_CASE("Hook sub-enum values are correct")
{
    // RestEventType
    CHECK(static_cast<int>(REST_EVENT_TYPE_CANCEL) == -1);
    CHECK(static_cast<int>(REST_EVENT_TYPE_PROGRESS) == 0);
    CHECK(static_cast<int>(REST_EVENT_TYPE_COMPLETE) == 1);

    // HookInventoryMoveType
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_MAIN_BACKPACK) == 0);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_LEFT_HAND) == 1);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_RIGHT_HAND) == 2);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_ARMOR_SLOT) == 3);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_WEAPON_RELOAD) == 4);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_CONTAINER) == 5);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_GROUND) == 6);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_PICKUP) == 7);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_CHARACTER_PORTRAIT) == 8);

    // PerceptionType
    CHECK(static_cast<int>(PERCEPTION_OTHER) == 0);
    CHECK(static_cast<int>(PERCEPTION_SEE) == 1);
    CHECK(static_cast<int>(PERCEPTION_HEAR) == 2);
    CHECK(static_cast<int>(PERCEPTION_AI_TARGET) == 3);

    // PerceptionResult
    CHECK(static_cast<int>(PERCEPTION_OUT_OF_RANGE) == 0);
    CHECK(static_cast<int>(PERCEPTION_IN_RANGE) == 1);
    CHECK(static_cast<int>(PERCEPTION_FORCE) == 2);

    // AmmoCostHookType
    CHECK(static_cast<int>(AMMO_COST_HOOK_SINGLE_SHOT) == 0);
    CHECK(static_cast<int>(AMMO_COST_HOOK_CHECK_OUT_OF_AMMO) == 1);
    CHECK(static_cast<int>(AMMO_COST_HOOK_BURST_ROUNDS) == 2);
    CHECK(static_cast<int>(AMMO_COST_HOOK_BURST_SHOT) == 3);

    // EncounterHookEventType
    CHECK(static_cast<int>(EncounterHookEventType::RandomEncounter) == 0);
    CHECK(static_cast<int>(EncounterHookEventType::LocalMapEnter) == 1);
}

TEST_CASE("HOOKS constants are reasonable")
{
    CHECK(HOOKS_MAX_ARGUMENTS == 16);
    CHECK(HOOKS_MAX_RETURN_VALUES == 8);
}

TEST_CASE("Deliberately absent hook slots are verified")
{
    // These slots were deliberately left out per sfall_script_hooks.h.
    // Verify the gap positions so new hooks don't accidentally reuse them.

    // Gap between HOOK_AFTERHITROLL (1) and HOOK_DEATHANIM2 (4):
    // slot 3 was HOOK_DEATHANIM1
    CHECK(static_cast<int>(HOOK_DEATHANIM2) == 4);

    // Gap between HOOK_ONDEATH (6) and HOOK_USEOBJON (8):
    // slot 7 was HOOK_FINDTARGET
    CHECK(static_cast<int>(HOOK_USEOBJON) == 8);

    // Gap between HOOK_USESKILLON (35) and HOOK_SETLIGHTING (38):
    // slots 36 (HOOK_ONEXPLOSION), 37 (HOOK_SUBCOMBATDAMAGE)
    CHECK(static_cast<int>(HOOK_SETLIGHTING) == 38);

    // Gap between HOOK_STDPROCEDURE_END (41) and HOOK_ENCOUNTER (43):
    // slot 42 was HOOK_TARGETOBJECT
    CHECK(static_cast<int>(HOOK_ENCOUNTER) == 43);

    // Gap after HOOK_CANUSEWEAPON (48):
    // slots 49-60 are reserved, 61 is HOOK_BUILDSFXWEAPON (absent)
    CHECK(static_cast<int>(HOOK_CANUSEWEAPON) == 48);
}

TEST_CASE("BarterPriceContext is usable")
{
    BarterPriceContext ctx = {};
    ctx.value = 100;
    ctx.offerValue = 50;
    ctx.caps = 25;
    ctx.offerButton = true;
    ctx.partyMember = false;

    CHECK(ctx.value == 100);
    CHECK(ctx.offerValue == 50);
    CHECK(ctx.caps == 25);
    CHECK(ctx.offerButton == true);
    CHECK(ctx.partyMember == false);
}

TEST_CASE("UseSkillOnHookResult defaults")
{
    UseSkillOnHookResult result = { true, false, false };
    CHECK(result.shouldContinue == true);
    CHECK(result.userOverridden == false);
    CHECK(result.allowInCombat == false);
}
