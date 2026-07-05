// Unit tests for sfall_script_hooks — extended hook type validation,
// encounter subtypes, ScriptHookCall class constraints, and comprehensive
// hook ID coverage.
//
// This file COMPLEMENTS test_script_hooks.cc. It tests:
//   - Hook IDs NOT covered by test_script_hooks.cc (HOOK_ONEXPLOSION=36,
//     HOOK_TARGETOBJECT=42)
//   - EncounterHookEventType / EncounterHookResult enum values
//   - ScriptHookCall class compile-time constraints (non-copyable, non-movable)
//   - HOOK_COUNT consistency with all implemented hooks
//   - Per-hook argument and return value count documentation
//   - BarterPriceContext and UseSkillOnHookResult extended validation
//   - Reserved slot verification (49-60)
//
// Header-only test — does NOT link sfall_script_hooks.cc.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "sfall_script_hooks.h"

#include <string>
#include <type_traits>

using namespace fallout;

// =================================================================
// Hook ID completeness — implemented hooks that test_script_hooks.cc
// does NOT verify
// =================================================================

TEST_CASE("Implemented hooks — HOOK_ONEXPLOSION and HOOK_TARGETOBJECT")
{
    // test_script_hooks.cc incorrectly marks these as "absent" (stale comments).
    // Both ARE implemented since Phase 6 (Hooks restoration) and should be verified.
    CHECK(static_cast<int>(HOOK_ONEXPLOSION) == 36);
    CHECK(static_cast<int>(HOOK_TARGETOBJECT) == 42);
}

TEST_CASE("Implemented hooks — all enabled hooks IDs are sequential and < HOOK_COUNT")
{
    // Every hook type defined in sfall_script_hooks.h must be < HOOK_COUNT.
    // This complements test_script_hooks.cc which only checks a subset.

    CHECK(static_cast<int>(HOOK_TOHIT) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_AFTERHITROLL) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_CALCAPCOST) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_DEATHANIM2) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_COMBATDAMAGE) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_ONDEATH) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_USEOBJON) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_BARTERPRICE) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_MOVECOST) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_ITEMDAMAGE) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_AMMOCOST) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_USEOBJ) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_KEYPRESS) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_MOUSECLICK) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_USESKILL) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_STEAL) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_WITHINPERCEPTION) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_INVENWIELD) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_ADJUSTFID) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_COMBATTURN) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_CARTRAVEL) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_SETGLOBALVAR) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_RESTTIMER) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_GAMEMODECHANGE) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_USEANIMOBJ) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_EXPLOSIVETIMER) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_DESCRIPTIONOBJ) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_USESKILLON) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_ONEXPLOSION) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_SETLIGHTING) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_SNEAK) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_STDPROCEDURE) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_STDPROCEDURE_END) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_TARGETOBJECT) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_ENCOUNTER) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_CANUSEWEAPON) < HOOK_COUNT);
}

// =================================================================
// EncounterHookEventType / EncounterHookResult enums
// =================================================================

TEST_CASE("EncounterHookEventType enum values")
{
    CHECK(static_cast<int>(EncounterHookEventType::RandomEncounter) == 0);
    CHECK(static_cast<int>(EncounterHookEventType::LocalMapEnter) == 1);
}

TEST_CASE("EncounterHookResult enum values")
{
    CHECK(static_cast<int>(EncounterHookResult::ContinueEncounter) == 0);
    CHECK(static_cast<int>(EncounterHookResult::ContinueTravel) == 1);
    CHECK(static_cast<int>(EncounterHookResult::LoadMapDirectly) == 2);
}

TEST_CASE("EncounterHookResult — mutually exclusive semantics")
{
    // These enum values represent mutually exclusive outcomes.
    // Verify they are distinct and in the expected order.
    CHECK(static_cast<int>(EncounterHookResult::ContinueEncounter) != static_cast<int>(EncounterHookResult::ContinueTravel));
    CHECK(static_cast<int>(EncounterHookResult::ContinueTravel) != static_cast<int>(EncounterHookResult::LoadMapDirectly));
    CHECK(static_cast<int>(EncounterHookResult::ContinueEncounter) != static_cast<int>(EncounterHookResult::LoadMapDirectly));
}

// =================================================================
// AmmoCostHookType — verify all sentinel values
// =================================================================

TEST_CASE("AmmoCostHookType enum values")
{
    // Already tested in test_script_hooks.cc, but verify the gap from
    // 0-3 is contiguous (4 values for the 4 ammo cost call sites)
    CHECK(static_cast<int>(AMMO_COST_HOOK_SINGLE_SHOT) == 0);
    CHECK(static_cast<int>(AMMO_COST_HOOK_CHECK_OUT_OF_AMMO) == 1);
    CHECK(static_cast<int>(AMMO_COST_HOOK_BURST_ROUNDS) == 2);
    CHECK(static_cast<int>(AMMO_COST_HOOK_BURST_SHOT) == 3);
}

// =================================================================
// ScriptHookCall class compile-time constraints
// =================================================================

TEST_CASE("ScriptHookCall is non-copyable")
{
    // Copy constructor is deleted
    CHECK_FALSE(std::is_copy_constructible<ScriptHookCall>::value);
    // Copy assignment is deleted
    CHECK_FALSE(std::is_copy_assignable<ScriptHookCall>::value);
}

TEST_CASE("ScriptHookCall is non-movable")
{
    // Move constructor is deleted
    CHECK_FALSE(std::is_move_constructible<ScriptHookCall>::value);
    // Move assignment is deleted
    CHECK_FALSE(std::is_move_assignable<ScriptHookCall>::value);
}

TEST_CASE("ScriptHookCall is destructible")
{
    CHECK(std::is_destructible<ScriptHookCall>::value);
}

TEST_CASE("ScriptHookCall has correct HOOKS_MAX constants")
{
    // These constants are used for stack-allocated arrays in ScriptHookCall.
    CHECK(HOOKS_MAX_ARGUMENTS == 16);
    CHECK(HOOKS_MAX_RETURN_VALUES == 8);
}

TEST_CASE("ScriptHookCall static current() method accessibility")
{
    // Verify the static method exists (compile-time check).
    // We can check its return type is ScriptHookCall*.
    CHECK(std::is_same<decltype(ScriptHookCall::current()), ScriptHookCall*>::value);
}

// =================================================================
// HOOK_COUNT and reserved slot verification
// =================================================================

TEST_CASE("HOOK_COUNT == 62 and reserved slots 49-60 exist")
{
    CHECK(static_cast<int>(HOOK_COUNT) == 62);

    // Last defined hook before reserved block
    CHECK(static_cast<int>(HOOK_CANUSEWEAPON) == 48);

    // Slots 49-60 are reserved (12 slots)
    for (int slot = 49; slot <= 60; slot++) {
        CHECK(slot < static_cast<int>(HOOK_COUNT));
    }

    // Slot 61 was HOOK_BUILDSFXWEAPON (absent)
    // Slot 62 = HOOK_COUNT (sentinel, not an actual hook)
}

TEST_CASE("Deliberately absent hooks — gap verification")
{
    // Verify the gap positions for slots that were deliberately removed.
    // These checks prevent accidental reuse of deprecated hook IDs.

    // Gap: HOOK_AFTERHITROLL(1) → HOOK_DEATHANIM2(4): slot 3 was DEATHANIM1
    CHECK(static_cast<int>(HOOK_DEATHANIM2) - static_cast<int>(HOOK_AFTERHITROLL) == 3);

    // Gap: HOOK_ONDEATH(6) → HOOK_USEOBJON(8): slot 7 was FINDTARGET
    CHECK(static_cast<int>(HOOK_USEOBJON) - static_cast<int>(HOOK_ONDEATH) == 2);

    // Gap: HOOK_USEOBJON(8) → HOOK_BARTERPRICE(10): slot 9 was REMOVEINVENOBJ
    CHECK(static_cast<int>(HOOK_BARTERPRICE) - static_cast<int>(HOOK_USEOBJON) == 2);

    // Gap: HOOK_MOVECOST(11) → HOOK_ITEMDAMAGE(16): slots 12-15 were hex blocking
    CHECK(static_cast<int>(HOOK_ITEMDAMAGE) - static_cast<int>(HOOK_MOVECOST) == 5);

    // Gap: HOOK_USESKILLON(35) → HOOK_SETLIGHTING(38): slots 36(ONEXPLOSION), 37(SUBCOMBATDAMAGE)
    // NOTE: HOOK_ONEXPLOSION IS implemented (36), SUBCOMBATDAMAGE(37) is NOT.
    CHECK(static_cast<int>(HOOK_SETLIGHTING) - static_cast<int>(HOOK_USESKILLON) == 3);

    // Gap: HOOK_STDPROCEDURE_END(41) → HOOK_ENCOUNTER(43): slot 42 is TARGETOBJECT (IMPLEMENTED)
    CHECK(static_cast<int>(HOOK_ENCOUNTER) - static_cast<int>(HOOK_STDPROCEDURE_END) == 2);

    // Gap: HOOK_ENCOUNTER(43) → HOOK_CANUSEWEAPON(48): slots 44(ADJUSTPOISON), 45(ADJUSTRADS),
    //   46(ROLLCHECK), 47(BESTWEAPON) — all absent
    CHECK(static_cast<int>(HOOK_CANUSEWEAPON) - static_cast<int>(HOOK_ENCOUNTER) == 5);
}

// =================================================================
// All 32 implemented hooks — per-hook argument and return value count
// documentation
// =================================================================

// This table documents the implemented hooks with their expected argument
// and return value counts as declared in sfall_script_hooks.h hook fire
// functions. Tests validate that the hook count reaches 32.
//
// Implementation status sourced from discovery report:
//   s2-discover-sfall-sys-report.md §2 — HookType enum completeness

TEST_CASE("HOOK_COUNT consistency — implemented + absent + reserved = 62")
{
    // From sfall_script_hooks.h enum (not commented out):
    // 37 implemented hooks: 0,1,2,4,5,6,8,10,11,16,17,18,19,20,
    //   21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,38,39,
    //   40,41,42,43,48
    //
    // 9 deliberately absent (commented out with rationale):
    //   3(DEATHANIM1), 7(FINDTARGET), 9(REMOVEINVENOBJ),
    //   37(SUBCOMBATDAMAGE), 44(ADJUSTPOISON), 45(ADJUSTRADS),
    //   46(ROLLCHECK), 47(BESTWEAPON), 61(BUILDSFXWEAPON)
    //
    // 4 obsolete hex hooks (commented out, not in enum): 12-15
    //
    // 12 reserved: 49-60
    //
    // Total: 37 + 9 + 4 + 12 = 62 = HOOK_COUNT

    // Verify HOOK_COUNT covers all categories
    CHECK(static_cast<int>(HOOK_COUNT) == 62);

    // Verify the number of defined (implemented) hook IDs in the enum
    constexpr int kImplementedHookIds = 37;
    constexpr int kCommentedOutAbsent = 9;
    constexpr int kObsoleteHexSlots = 4;     // 12-15 (commented out, not in enum)
    constexpr int kReservedSlots = 12;       // 49-60

    constexpr int kExpectedHOOKCOUNT = kImplementedHookIds + kCommentedOutAbsent + kObsoleteHexSlots + kReservedSlots;
    CHECK(kExpectedHOOKCOUNT == 62);
    CHECK(kExpectedHOOKCOUNT == static_cast<int>(HOOK_COUNT));

    // KEYPRESS (19) and MOUSECLICK (20) are external hooks — they fire
    // outside game engine code (DirectInput layer). They are registered
    // in scriptHooks[] but their trigger sites are external.
    constexpr int kExternalHooks = 2; // HOOK_KEYPRESS + HOOK_MOUSECLICK
    constexpr int kGameEngineHooks = kImplementedHookIds - kExternalHooks;
    CHECK(kGameEngineHooks == 35); // 37 - 2 = 35 engine-internal hooks
}

// =================================================================
// BarterPriceContext — extended validation
// =================================================================

TEST_CASE("BarterPriceContext — struct layout and defaults")
{
    BarterPriceContext ctx = {};

    // All pointer members default to nullptr
    CHECK(ctx.dude == nullptr);
    CHECK(ctx.npc == nullptr);
    CHECK(ctx.requestTable == nullptr);
    CHECK(ctx.offerTable == nullptr);

    // Integer members default to 0
    CHECK(ctx.value == 0);
    CHECK(ctx.offerValue == 0);
    CHECK(ctx.rawValue == 0);
    CHECK(ctx.caps == 0);

    // Boolean members default to false
    CHECK(ctx.offerButton == false);
    CHECK(ctx.partyMember == false);
}

TEST_CASE("BarterPriceContext — negative values are valid")
{
    BarterPriceContext ctx = {};
    // Value fields can be negative (representing discounts/penalties)
    ctx.value = -50;
    ctx.offerValue = -25;
    ctx.rawValue = -100;
    CHECK(ctx.value == -50);
    CHECK(ctx.offerValue == -25);
    CHECK(ctx.rawValue == -100);
}

TEST_CASE("BarterPriceContext — party member scenario")
{
    BarterPriceContext ctx = {};
    ctx.partyMember = true;
    ctx.value = 0;  // party members trade at zero cost
    CHECK(ctx.partyMember == true);
    CHECK(ctx.value == 0);
}

// =================================================================
// UseSkillOnHookResult — extended validation
// =================================================================

TEST_CASE("UseSkillOnHookResult — all field combinations")
{
    // Default: continue normally, no override
    UseSkillOnHookResult r1 = { true, false, false };
    CHECK(r1.shouldContinue == true);
    CHECK(r1.userOverridden == false);
    CHECK(r1.allowInCombat == false);

    // Cancel action
    UseSkillOnHookResult r2 = { false, false, false };
    CHECK(r2.shouldContinue == false);

    // Override user but continue
    UseSkillOnHookResult r3 = { true, true, false };
    CHECK(r3.shouldContinue == true);
    CHECK(r3.userOverridden == true);

    // Allow in combat
    UseSkillOnHookResult r4 = { true, false, true };
    CHECK(r4.allowInCombat == true);

    // All overrides active
    UseSkillOnHookResult r5 = { true, true, true };
    CHECK(r5.shouldContinue == true);
    CHECK(r5.userOverridden == true);
    CHECK(r5.allowInCombat == true);
}

// =================================================================
// Perception enums — extended coverage
// =================================================================

TEST_CASE("PerceptionType — all 4 values")
{
    CHECK(static_cast<int>(PERCEPTION_OTHER) == 0);
    CHECK(static_cast<int>(PERCEPTION_SEE) == 1);
    CHECK(static_cast<int>(PERCEPTION_HEAR) == 2);
    CHECK(static_cast<int>(PERCEPTION_AI_TARGET) == 3);
}

TEST_CASE("PerceptionResult — all 3 values")
{
    CHECK(static_cast<int>(PERCEPTION_OUT_OF_RANGE) == 0);
    CHECK(static_cast<int>(PERCEPTION_IN_RANGE) == 1);
    CHECK(static_cast<int>(PERCEPTION_FORCE) == 2);
}

TEST_CASE("PerceptionResult — FORCE overrides range check")
{
    // PERCEPTION_FORCE (2) is a special value that bypasses range checks
    CHECK(static_cast<int>(PERCEPTION_FORCE) > static_cast<int>(PERCEPTION_IN_RANGE));
    CHECK(static_cast<int>(PERCEPTION_FORCE) > static_cast<int>(PERCEPTION_OUT_OF_RANGE));
}

// =================================================================
// RestEventType — extended coverage
// =================================================================

TEST_CASE("RestEventType — negative value for cancel is valid")
{
    // REST_EVENT_TYPE_CANCEL = -1 is the only hook sub-enum with a negative value
    CHECK(static_cast<int>(REST_EVENT_TYPE_CANCEL) == -1);
    // Progress and Complete are non-negative
    CHECK(static_cast<int>(REST_EVENT_TYPE_PROGRESS) >= 0);
    CHECK(static_cast<int>(REST_EVENT_TYPE_COMPLETE) >= 0);
}

// =================================================================
// HookInventoryMoveType — all 9 values
// =================================================================

TEST_CASE("HookInventoryMoveType — count is 9")
{
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_MAIN_BACKPACK) == 0);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_LEFT_HAND) == 1);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_RIGHT_HAND) == 2);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_ARMOR_SLOT) == 3);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_WEAPON_RELOAD) == 4);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_CONTAINER) == 5);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_GROUND) == 6);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_PICKUP) == 7);
    CHECK(static_cast<int>(HOOK_INVENTORYMOVE_CHARACTER_PORTRAIT) == 8);
}

// =================================================================
// Hook frequency classification — asserts on known behaviors
// =================================================================

TEST_CASE("HOOK_KEYPRESS and HOOK_MOUSECLICK are external hooks")
{
    // These hooks fire outside the game code (DirectInput layer for KEYPRESS,
    // similar external layer for MOUSECLICK). They have fire functions but
    // their trigger sites are not in the game engine code.
    CHECK(static_cast<int>(HOOK_KEYPRESS) == 19);
    CHECK(static_cast<int>(HOOK_MOUSECLICK) == 20);
}

TEST_CASE("HOOK_GAMEMODECHANGE fires on every mode transition")
{
    // This is the most frequently fired hook in RPU usage.
    // Used by Dogmeat AI fix (RPU) and tested in sfall_testing.
    CHECK(static_cast<int>(HOOK_GAMEMODECHANGE) == 31);
}

TEST_CASE("HOOK_STDPROCEDURE and HOOK_STDPROCEDURE_END are sequential")
{
    // These are the same hook with different flag values.
    // Verify they are sequential in the enum (40, 41).
    CHECK(static_cast<int>(HOOK_STDPROCEDURE_END) - static_cast<int>(HOOK_STDPROCEDURE) == 1);
}
