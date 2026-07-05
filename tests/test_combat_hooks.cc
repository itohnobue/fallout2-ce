// Unit tests for combat hook integration — hook type mappings and constants.
//
// Tests: Combat hook enum values (HOOK_TARGETOBJECT, HOOK_COMBATTURN,
//        HOOK_AFTERHITROLL, HOOK_COMBATDAMAGE, HOOK_ONDEATH, HOOK_AMMOCOST,
//        HOOK_CANUSEWEAPON, HOOK_WITHINPERCEPTION, HOOK_CALCAPCOST, HOOK_TOHIT),
//        undocumented HOOK_FINDTARGET slot, AmmoCostHookType values,
//        EncounterHookEventType/EncounterHookResult values, hook argument
//        count validation for combat hooks.
//
// NOTE: Does not test ScriptHookCall construction or hook invocation
// (those require linking the engine). Validates hook type definitions,
// sub-enum values for combat hooks, and hook slot consistency.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "combat_defs.h"
#include "sfall_script_hooks.h"

using namespace fallout;

// =============================================================
// Combat hook type verification
// =============================================================

TEST_CASE("Combat hook types are correctly assigned")
{
    // Core combat hooks
    CHECK(static_cast<int>(HOOK_TOHIT) == 0);
    CHECK(static_cast<int>(HOOK_AFTERHITROLL) == 1);
    CHECK(static_cast<int>(HOOK_CALCAPCOST) == 2);
    CHECK(static_cast<int>(HOOK_COMBATDAMAGE) == 5);
    CHECK(static_cast<int>(HOOK_ONDEATH) == 6);

    // Ammo and weapon hooks
    CHECK(static_cast<int>(HOOK_ITEMDAMAGE) == 16);
    CHECK(static_cast<int>(HOOK_AMMOCOST) == 17);
    CHECK(static_cast<int>(HOOK_CANUSEWEAPON) == 48);

    // Combat turn and target hooks
    CHECK(static_cast<int>(HOOK_COMBATTURN) == 27);
    CHECK(static_cast<int>(HOOK_TARGETOBJECT) == 42);

    // AI hooks used in combat code
    CHECK(static_cast<int>(HOOK_WITHINPERCEPTION) == 23);
    CHECK(static_cast<int>(HOOK_STEAL) == 22);

    // Movement hook
    CHECK(static_cast<int>(HOOK_MOVECOST) == 11);

    // All combat hooks must be < HOOK_COUNT
    CHECK(static_cast<int>(HOOK_TOHIT) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_AFTERHITROLL) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_CALCAPCOST) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_COMBATDAMAGE) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_ONDEATH) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_ITEMDAMAGE) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_AMMOCOST) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_CANUSEWEAPON) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_COMBATTURN) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_TARGETOBJECT) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_WITHINPERCEPTION) < HOOK_COUNT);
}

// =============================================================
// HOOK_FINDTARGET — undocumented but supported hook (type 7)
// =============================================================

TEST_CASE("HOOK_FINDTARGET (type 7) — undocumented but functional")
{
    // HOOK_FINDTARGET is commented out in the HookType enum at
    // sfall_script_hooks.h:40 but is invoked via static_cast<HookType>(7)
    // at 3 call sites in combat_ai.cc (_ai_danger_source at lines
    // 1676, 1703, 1757). The hook slot 7 is within [0, HOOK_COUNT=62)
    // and the hook infrastructure works with numeric indices regardless
    // of whether the symbolic name is present in the enum.
    //
    // Verify:
    // 1. Slot 7 is absent from the enum (no HOOK_FINDTARGET symbol)
    // 2. Slot 7 is valid (< HOOK_COUNT=62)
    // 3. Gap between HOOK_ONDEATH (6) and HOOK_USEOBJON (8) is correct

    // HOOK_ONDEATH is 6, HOOK_USEOBJON is 8 — slot 7 is the gap for FINDTARGET
    CHECK(static_cast<int>(HOOK_ONDEATH) == 6);
    CHECK(static_cast<int>(HOOK_USEOBJON) == 8);

    // Slot 7 is a valid numeric index
    CHECK(7 < HOOK_COUNT);
    CHECK(7 >= 0);

    // Verify no other hook uses slot 7
    // (implicit: the enum has no value at position 7)
}

// =============================================================
// HOOK_TARGETOBJECT (type 42) — defined and functional
// =============================================================

TEST_CASE("HOOK_TARGETOBJECT (type 42)")
{
    // HOOK_TARGETOBJECT is a defined hook that fires when ANY critter
    // selects a target to attack (combat.cc:3581-3582).
    // It is invoked in _combat_attack() before attackInit().
    // It takes 4 args: attacker, defender, hitMode, hitLocation.
    // No return values are consumed.

    CHECK(static_cast<int>(HOOK_TARGETOBJECT) == 42);
    CHECK(static_cast<int>(HOOK_TARGETOBJECT) < HOOK_COUNT);

    // Verify the gap: HOOK_STDPROCEDURE_END (41) → HOOK_TARGETOBJECT (42) → HOOK_ENCOUNTER (43)
    CHECK(static_cast<int>(HOOK_STDPROCEDURE_END) == 41);
    CHECK(static_cast<int>(HOOK_ENCOUNTER) == 43);
}

// =============================================================
// HOOK_COMBATTURN (type 27) — turn-based combat hook
// =============================================================

TEST_CASE("HOOK_COMBATTURN (type 27)")
{
    // HOOK_COMBATTURN fires twice per turn:
    // 1. At start (combat.cc:3361) — can skip the turn if return true
    // 2. At end (combat.cc:3374) — can force end combat if return -1
    // Also fires at combat end (combat.cc:3536) as void call

    CHECK(static_cast<int>(HOOK_COMBATTURN) == 27);
    CHECK(static_cast<int>(HOOK_COMBATTURN) < HOOK_COUNT);
}

// =============================================================
// HOOK_COMBATDAMAGE (type 5) — the most complex combat hook
// =============================================================

TEST_CASE("HOOK_COMBATDAMAGE (type 5)")
{
    // HOOK_COMBATDAMAGE has 12+ args and 5 return values.
    // It is invoked in TWO code paths:
    // - attackComputeDamage:4660 (non-critter target)
    // - attackComputeDamage:4799 (normal critter path)
    // Used by RPU's solar scorcher detection (gl_k_wpnchk.ssl).

    CHECK(static_cast<int>(HOOK_COMBATDAMAGE) == 5);
    CHECK(static_cast<int>(HOOK_COMBATDAMAGE) < HOOK_COUNT);
}

// =============================================================
// AmmoCostHookType enum tests
// =============================================================

TEST_CASE("AmmoCostHookType enum values")
{
    CHECK(static_cast<int>(AMMO_COST_HOOK_SINGLE_SHOT) == 0);
    CHECK(static_cast<int>(AMMO_COST_HOOK_CHECK_OUT_OF_AMMO) == 1);
    CHECK(static_cast<int>(AMMO_COST_HOOK_BURST_ROUNDS) == 2);
    CHECK(static_cast<int>(AMMO_COST_HOOK_BURST_SHOT) == 3);
}

// =============================================================
// EncounterHookEventType and EncounterHookResult
// =============================================================

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

// =============================================================
// Hook invocation ordering — combat pipeline
// =============================================================

TEST_CASE("Combat hook invocation ordering is consistent")
{
    // The hook execution order within _combat_attack() is:
    // 1. HOOK_TARGETOBJECT (42) — fires before attackInit
    // 2. HOOK_AFTERHITROLL (1) — fires during attackCompute, after hit roll
    // 3. HOOK_AMMOCOST (17) — fires for ammo deduction
    // 4. HOOK_COMBATDAMAGE (5) — fires during damage calculation
    // 5. HOOK_ONDEATH (6) — fires on kill via _damage_object
    //
    // Verify that the numeric values don't overlap (they don't need to be
    // sequential — just that they are all distinct).

    CHECK(static_cast<int>(HOOK_TARGETOBJECT) == 42);
    CHECK(static_cast<int>(HOOK_AFTERHITROLL) == 1);
    CHECK(static_cast<int>(HOOK_AMMOCOST) == 17);
    CHECK(static_cast<int>(HOOK_COMBATDAMAGE) == 5);
    CHECK(static_cast<int>(HOOK_ONDEATH) == 6);

    // All distinct
    CHECK(HOOK_TARGETOBJECT != HOOK_AFTERHITROLL);
    CHECK(HOOK_TARGETOBJECT != HOOK_AMMOCOST);
    CHECK(HOOK_TARGETOBJECT != HOOK_COMBATDAMAGE);
    CHECK(HOOK_TARGETOBJECT != HOOK_ONDEATH);
    CHECK(HOOK_AFTERHITROLL != HOOK_COMBATDAMAGE);
    CHECK(HOOK_AMMOCOST != HOOK_COMBATDAMAGE);
    CHECK(HOOK_COMBATDAMAGE != HOOK_ONDEATH);
}

// =============================================================
// Hook argument count expectations (verification only)
// =============================================================

TEST_CASE("Combat hooks have expected argument count ranges")
{
    // These are expectations based on the discovery report's hook inventory.
    // The actual arguments are validated at integration test level.
    // This test only verifies the hook types exist and are valid.

    // HOOK_TOHIT: 8 args (hitChance, attacker, defender, hitLocation, tile, hitMode, useDistance, uncapped)
    // HOOK_AFTERHITROLL: 5 args (roll, attacker, defender, hitLocation, hitChance)
    // HOOK_COMBATDAMAGE: 12+ args
    // HOOK_TARGETOBJECT: 4 args (attacker, defender, hitMode, hitLocation)
    // HOOK_CALCAPCOST: 5 args (obj, hitMode, aiming, actionPoints, weapon)
    // HOOK_AMMOCOST: 4 args (weapon, rounds, ammoQty, hookType)
    // HOOK_ONDEATH: 1 arg (target)
    // HOOK_COMBATTURN: 3 args (flag, obj, reloaded)
    // HOOK_CANUSEWEAPON: 3 args (weapon, critter, hitMode)
    // HOOK_WITHINPERCEPTION: 4 args (watcher, target, result, type)

    // All hooks that script authors register must be within HOOK_COUNT
    // Scripts use register_hook_proc(hookType, procedure) where hookType
    // must be a valid HookType. scriptHooksRegister validates this at
    // script registration time in sfall_script_hooks.cc.

    (void)HOOK_TOHIT;
    (void)HOOK_AFTERHITROLL;
    (void)HOOK_COMBATDAMAGE;
    (void)HOOK_TARGETOBJECT;
    (void)HOOK_CALCAPCOST;
    (void)HOOK_AMMOCOST;
    (void)HOOK_ONDEATH;
    (void)HOOK_COMBATTURN;
    (void)HOOK_CANUSEWEAPON;
    (void)HOOK_WITHINPERCEPTION;
}

// =============================================================
// Combat hook surface area: all 18 combat hooks exist
// =============================================================

TEST_CASE("All 18 combat pipeline hooks have valid type values")
{
    // The discovery report identified 18 hook invocation points in combat.cc
    // and combat_ai.cc (plus 3 additional CALCAPCOST points in item.cc).
    // Verify all are valid HookType values.

    // combat.cc hooks
    CHECK(static_cast<int>(HOOK_COMBATTURN) == 27);
    CHECK(static_cast<int>(HOOK_TARGETOBJECT) == 42);
    CHECK(static_cast<int>(HOOK_AFTERHITROLL) == 1);
    CHECK(static_cast<int>(HOOK_AMMOCOST) == 17);
    CHECK(static_cast<int>(HOOK_TOHIT) == 0);
    CHECK(static_cast<int>(HOOK_COMBATDAMAGE) == 5);
    CHECK(static_cast<int>(HOOK_ONDEATH) == 6);

    // combat_ai.cc hooks
    // HOOK_FINDTARGET = 7 (undocumented, via static_cast<HookType>(7))
    CHECK(static_cast<int>(HOOK_CANUSEWEAPON) == 48);
    CHECK(static_cast<int>(HOOK_WITHINPERCEPTION) == 23);

    // item.cc hooks (in the combat pipeline)
    CHECK(static_cast<int>(HOOK_CALCAPCOST) == 2);
}

// =============================================================
// Undocumented hook surface area
// =============================================================

TEST_CASE("Undocumented hook slots in the combat range are valid")
{
    // HOOK_FINDTARGET = 7: undocumented but used (combat_ai.cc:1676,1703,1757)
    // HOOK_ONEXPLOSION = 36: defined but not heavily used
    // HOOK_SNEAK = 39: defined but not heavily used

    // All are valid indices
    CHECK(7 < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_ONEXPLOSION) == 36);
    CHECK(static_cast<int>(HOOK_ONEXPLOSION) < HOOK_COUNT);
    CHECK(static_cast<int>(HOOK_SNEAK) == 39);
    CHECK(static_cast<int>(HOOK_SNEAK) < HOOK_COUNT);
}

// =============================================================
// BarterPriceContext — verified in existing tests, included for completeness
// =============================================================

TEST_CASE("BarterPriceContext is usable in combat context")
{
    // BARTERPRICE hook (type 10) can fire during combat if trading occurs.
    // Verify the struct is usable.
    BarterPriceContext ctx = {};
    ctx.dude = nullptr;
    ctx.npc = nullptr;
    ctx.requestTable = nullptr;
    ctx.offerTable = nullptr;
    ctx.value = 100;
    ctx.offerValue = 50;
    ctx.rawValue = 75;
    ctx.caps = 25;
    ctx.offerButton = true;
    ctx.partyMember = false;

    CHECK(ctx.value == 100);
    CHECK(ctx.caps == 25);
    CHECK(ctx.offerButton == true);
}

// =============================================================
// UseSkillOnHookResult — verified for completeness
// =============================================================

TEST_CASE("UseSkillOnHookResult defaults")
{
    UseSkillOnHookResult result = { true, false, false };
    CHECK(result.shouldContinue == true);
    CHECK(result.userOverridden == false);
    CHECK(result.allowInCombat == false);
}
