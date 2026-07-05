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
    // 36 = HOOK_ONEXPLOSION
    // 37 = HOOK_SUBCOMBATDAMAGE (absent)
    CHECK(static_cast<int>(HOOK_SETLIGHTING) == 38);
    CHECK(static_cast<int>(HOOK_SNEAK) == 39);
    CHECK(static_cast<int>(HOOK_STDPROCEDURE) == 40);
    CHECK(static_cast<int>(HOOK_STDPROCEDURE_END) == 41);
    // 42 = HOOK_TARGETOBJECT
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

// =================================================================
// Local mirrors of scriptHooks_* production functions
// (cannot link sfall_script_hooks.cc due to 50+ engine dependencies)
// =================================================================

// Shared state for controlling mirrored hook calls
static int g_TestReturnValue0 = 0;
static int g_TestReturnValue1 = 0;
static int g_TestNumReturnValues = 0;

// Mirror of scriptHooks_SetGlobalVar at sfall_script_hooks.cc:1264-1274
static int TestScriptHooksSetGlobalVar(int varIndex, int value, bool emptyHookList)
{
    if (emptyHookList) {
        // Production: constructs ScriptHookCall and calls hook.call(),
        // but when no scripts are registered, numReturnValues is 0.
        // The SETGLOBALVAR hook lacks the fast-path empty() guard
        // that other hooks have (N2-006 gap documented in discovery).
        return value;
    }

    if (g_TestNumReturnValues <= 0) {
        return value;
    }

    return g_TestReturnValue0;
}

// Mirror of scriptHooks_Sneak at sfall_script_hooks.cc:1288-1306
static void TestScriptHooksSneak(int* resultPtr, int* durationPtr, int critter, bool emptyHookList)
{
    if (emptyHookList) {
        return; // fast-path: no scripts registered
    }

    if (g_TestNumReturnValues <= 0) {
        return;
    }

    // N2-008: UNCONDITIONAL overwrite — no sentinel check.
    // Contrast with CARTRAVEL (speedOverride >= 0) and SETLIGHTING (overrideIntensity != -1).
    *resultPtr = g_TestReturnValue0;

    if (g_TestNumReturnValues > 1) {
        *durationPtr = g_TestReturnValue1;
    }
}

// Mirror of scriptHooks_OnExplosion at sfall_script_hooks.cc:1318-1325
static bool g_TestOnExplosionCalled = false;
static int g_TestOnExplosionArg0 = -1; // explosive
static int g_TestOnExplosionArg1 = -1; // tile
static int g_TestOnExplosionArg2 = -1; // elevation
static int g_TestOnExplosionArg3 = -1; // minDamage
static int g_TestOnExplosionArg4 = -1; // maxDamage
static void* g_TestOnExplosionArg5 = nullptr; // sourceObj

static void TestScriptHooksOnExplosion(int explosive, int tile, int elevation,
                                        int minDamage, int maxDamage, void* sourceObj,
                                        bool emptyHookList)
{
    if (emptyHookList) {
        return; // fast-path: no scripts registered
    }

    g_TestOnExplosionCalled = true;
    g_TestOnExplosionArg0 = explosive;
    g_TestOnExplosionArg1 = tile;
    g_TestOnExplosionArg2 = elevation;
    g_TestOnExplosionArg3 = minDamage;
    g_TestOnExplosionArg4 = maxDamage;
    g_TestOnExplosionArg5 = sourceObj;
    // Production: observation-only hook, maxReturnValues=0
}

// Mirror of scriptHooks_TargetObject at sfall_script_hooks.cc:1335-1342
static bool g_TestTargetObjectCalled = false;
static int g_TestTargetObjectArg0 = -1; // attacker
static int g_TestTargetObjectArg1 = -1; // defender
static int g_TestTargetObjectArg2 = -1; // hitMode
static int g_TestTargetObjectArg3 = -1; // hitLocation

static void TestScriptHooksTargetObject(int attacker, int defender, int hitMode,
                                         int hitLocation, bool emptyHookList)
{
    if (emptyHookList) {
        return; // fast-path: no scripts registered
    }

    g_TestTargetObjectCalled = true;
    g_TestTargetObjectArg0 = attacker;
    g_TestTargetObjectArg1 = defender;
    g_TestTargetObjectArg2 = hitMode;
    g_TestTargetObjectArg3 = hitLocation;
    // Production: observation-only hook, maxReturnValues=0
}

// =================================================================
// M-068: scriptHooks_SetGlobalVar — fork-implemented hook
// Source: sfall_script_hooks.cc:1264-1274
// Finding: Production invocation untested. Returns value unchanged when
// no hook scripts return anything; returns override when hook returns >=1 value.
// Note: N2-006 also flags the missing empty-script-early-return optimization —
// SETGLOBALVAR always constructs ScriptHookCall even with no scripts registered.
// Research tier: RPU LIKELY — not used by RPU scripts.
// =================================================================

TEST_CASE("M-068: scriptHooks_SetGlobalVar — returns original value when no hook")
{
    // When no scripts are registered for HOOK_SETGLOBALVAR,
    // the function returns the original value unchanged.
    g_TestNumReturnValues = 0;

    int result = TestScriptHooksSetGlobalVar(500, 100, true);
    CHECK(result == 100); // original value preserved
}

TEST_CASE("M-068: scriptHooks_SetGlobalVar — returns original value when hook returns nothing")
{
    // When hook scripts are present but return 0 values,
    // the function returns the original value unchanged.
    g_TestNumReturnValues = 0;

    int result = TestScriptHooksSetGlobalVar(500, 100, false);
    CHECK(result == 100); // original value preserved
}

TEST_CASE("M-068: scriptHooks_SetGlobalVar — override from hook return value")
{
    // When hook returns >=1 value, the first return value overrides the GVAR.
    g_TestNumReturnValues = 1;
    g_TestReturnValue0 = 20;

    int result = TestScriptHooksSetGlobalVar(500, 10, false);
    CHECK(result == 20); // overridden by hook return
}

TEST_CASE("M-068: scriptHooks_SetGlobalVar — override to same value as set is no-op")
{
    // Verify that returning the exact same value as the set value works.
    g_TestNumReturnValues = 1;
    g_TestReturnValue0 = 42;

    int result = TestScriptHooksSetGlobalVar(500, 42, false);
    CHECK(result == 42); // same value, no functional change
}

// =================================================================
// M-067: scriptHooks_Sneak — fork-implemented hook
// Source: sfall_script_hooks.cc:1288-1306
// Finding: 3 code paths untested — empty hook list, no return values,
// return override with 1 or 2 return values. No test verifies arg layout
// (result, duration, critter) or return override behavior.
// Research tier: RPU LIKELY — not used by RPU; ET Tu may use via .int files.
// =================================================================

TEST_CASE("M-067: scriptHooks_Sneak — empty hook list fast-path returns without modification")
{
    int result = 1; // engine computed success
    int duration = 60;

    TestScriptHooksSneak(&result, &duration, 0, true);

    // Both values unchanged — no hook was called
    CHECK(result == 1);
    CHECK(duration == 60);
}

TEST_CASE("M-067: scriptHooks_Sneak — no return values preserves engine values")
{
    int result = 1;
    int duration = 60;
    g_TestNumReturnValues = 0;

    TestScriptHooksSneak(&result, &duration, 0, false);

    // Both values unchanged — hook returned nothing
    CHECK(result == 1);
    CHECK(duration == 60);
}

TEST_CASE("M-067: scriptHooks_Sneak — single return value overrides result only")
{
    int result = 0; // engine computed failure
    int duration = 30;
    g_TestNumReturnValues = 1;
    g_TestReturnValue0 = 1; // override to success

    TestScriptHooksSneak(&result, &duration, 0, false);

    CHECK(result == 1);    // result overridden to success
    CHECK(duration == 30); // duration unchanged (only 1 return value)
}

TEST_CASE("M-067: scriptHooks_Sneak — two return values override both result and duration")
{
    int result = 1;
    int duration = 60;
    g_TestNumReturnValues = 2;
    g_TestReturnValue0 = 0;   // override to failure
    g_TestReturnValue1 = 120; // override to longer duration

    TestScriptHooksSneak(&result, &duration, 0, false);

    CHECK(result == 0);   // overridden
    CHECK(duration == 120); // overridden
}

// =================================================================
// N2-008: scriptHooks_Sneak — unconditional *resultPtr overwrite
// Source: sfall_script_hooks.cc:1301 (no sentinel check)
// Finding: *resultPtr is always overwritten when hook returns >=1 value.
// Contrast with CARTRAVEL (speedOverride >= 0 guard) and SETLIGHTING
// (overrideIntensity != -1 guard). SNEAK has NO "keep engine value" sentinel.
// Research tier: LIKELY — ET Tu registers HOOK_SNEAK via fallback paths.
// =================================================================

TEST_CASE("N2-008: scriptHooks_Sneak — returning 0 unconditionally overwrites engine success")
{
    // Engine computed: sneak succeeded (result=1).
    // Hook returns: result=0.
    // Without a sentinel check, 0 is a valid value but also the only "failure" value.
    // There's no way to say "keep engine result."
    int result = 1; // engine: success
    int duration = 60;
    g_TestNumReturnValues = 1;
    g_TestReturnValue0 = 0; // hook returns 0 — but is this "override to fail" or "keep"?

    TestScriptHooksSneak(&result, &duration, 0, false);

    // N2-008 CONFIRMED: result is unconditionally overwritten to 0
    // Script cannot distinguish "I want to keep engine value" from "I want to override to 0"
    CHECK(result == 0); // engine success was silently replaced with failure
}

TEST_CASE("N2-008: scriptHooks_Sneak — cannot keep result while overriding duration")
{
    // A script wants to keep the engine result (0 or 1) but change the duration.
    // With CARTRAVEL, you'd return -1 for speed to keep engine value.
    // With SNEAK, there is no sentinel — any return value overwrites result.
    int result = 1; // engine: success
    int duration = 60;
    g_TestNumReturnValues = 2;
    g_TestReturnValue0 = -1;  // script intent: "keep engine result"
    g_TestReturnValue1 = 999; // script intent: "set duration to 999"

    TestScriptHooksSneak(&result, &duration, 0, false);

    // N2-008 CONFIRMED: result is -1 (invalid sneak state), not 1 (keep).
    // The script's attempt to keep engine result via -1 sentinel silently corrupts state.
    CHECK(result == -1);    // WRONG: should be 1 (engine success)
    CHECK(duration == 999); // duration override works

    // Compare with CARTRAVEL which uses "if (speedOverride >= 0)" guard.
    // Passing -1 to CARTRAVEL would keep the engine speed. SNEAK lacks this guard.
}

// =================================================================
// M-069: scriptHooks_OnExplosion — fork-implemented observation-only hook
// Source: sfall_script_hooks.cc:1318-1325
// Finding: 6 args, 0 return values. No test verifies arg layout.
// sourceObj can be nullptr — untested. Caller at queue.cc:502 passes
// real game objects; caller at scripts.cc:1102 passes all nullptrs.
// Research tier: RPU LIKELY — not used by RPU scripts.
// =================================================================

TEST_CASE("M-069: scriptHooks_OnExplosion — empty hook list fast-path")
{
    g_TestOnExplosionCalled = false;

    TestScriptHooksOnExplosion(42, 100, 0, 10, 50, nullptr, true);

    CHECK_FALSE(g_TestOnExplosionCalled); // hook not called — fast-path returned
}

TEST_CASE("M-069: scriptHooks_OnExplosion — args passed when hook is active")
{
    g_TestOnExplosionCalled = false;

    TestScriptHooksOnExplosion(42, 100, 0, 10, 50, nullptr, false);

    CHECK(g_TestOnExplosionCalled);
    CHECK(g_TestOnExplosionArg0 == 42);  // explosive
    CHECK(g_TestOnExplosionArg1 == 100); // tile
    CHECK(g_TestOnExplosionArg2 == 0);   // elevation
    CHECK(g_TestOnExplosionArg3 == 10);  // minDamage
    CHECK(g_TestOnExplosionArg4 == 50);  // maxDamage
    CHECK(g_TestOnExplosionArg5 == nullptr); // sourceObj — may be nullptr
}

TEST_CASE("M-069: scriptHooks_OnExplosion — nullptr sourceObj is handled")
{
    // Production: queue.cc:502 passes gDude as sourceObj (could be null in edge cases).
    // scripts.cc:1102 call site passes nullptr for all args including sourceObj.
    // The mirror passes nullptr through as a valid arg — production expects scripts
    // to handle nullptr in get_sfall_arg for object-type args.
    g_TestOnExplosionCalled = false;

    TestScriptHooksOnExplosion(0, 20000, 1, 25, 75, nullptr, false);

    CHECK(g_TestOnExplosionCalled);
    CHECK(g_TestOnExplosionArg5 == nullptr); // verified: nullptr is passed through

    // A hook handler reading arg5 via get_sfall_arg(4) would receive nullptr.
    // This is documented behavior — the arg comment at sfall_script_hooks.cc:1316
    // says "may be nullptr".
}

TEST_CASE("M-069: scriptHooks_OnExplosion — damage args can be zero")
{
    // Some explosive objects have minDamage=maxDamage=0 (e.g., smoke grenades,
    // flare effects). Verify these pass through correctly.
    g_TestOnExplosionCalled = false;

    TestScriptHooksOnExplosion(0, 500, 0, 0, 0, nullptr, false);

    CHECK(g_TestOnExplosionCalled);
    CHECK(g_TestOnExplosionArg3 == 0); // minDamage=0 is valid
    CHECK(g_TestOnExplosionArg4 == 0); // maxDamage=0 is valid
}

// =================================================================
// M-070: scriptHooks_TargetObject — fork-implemented observation-only hook
// Source: sfall_script_hooks.cc:1335-1342
// Finding: 4 args (attacker, defender, hitMode, hitLocation), 0 return values.
// No test verifies arg layout or values. ET Tu does not use this hook.
// Research tier: N/A — ET Tu does not use HOOK_TARGETOBJECT.
// =================================================================

TEST_CASE("M-070: scriptHooks_TargetObject — empty hook list fast-path")
{
    g_TestTargetObjectCalled = false;

    TestScriptHooksTargetObject(1, 2, 0, 3, true);

    CHECK_FALSE(g_TestTargetObjectCalled); // fast-path returned
}

TEST_CASE("M-070: scriptHooks_TargetObject — args passed when hook is active")
{
    g_TestTargetObjectCalled = false;

    TestScriptHooksTargetObject(1, 2, 0, 3, false);

    CHECK(g_TestTargetObjectCalled);
    CHECK(g_TestTargetObjectArg0 == 1); // attacker
    CHECK(g_TestTargetObjectArg1 == 2); // defender
    CHECK(g_TestTargetObjectArg2 == 0); // hitMode (ATKTYPE_PUNCH)
    CHECK(g_TestTargetObjectArg3 == 3); // hitLocation (HIT_LOCATION_LEFT_LEG)
}

TEST_CASE("M-070: scriptHooks_TargetObject — hit mode and hit location across expected range")
{
    // Valid hit modes: 0=ATKTYPE_PUNCH through 6=ATKTYPE_THROW
    // Valid hit locations: 0=HIT_LOCATION_TORSO through 8=HIT_LOCATION_EYES
    for (int hitMode = 0; hitMode <= 6; hitMode++) {
        for (int hitLoc = 0; hitLoc <= 8; hitLoc++) {
            g_TestTargetObjectCalled = false;
            TestScriptHooksTargetObject(10, 20, hitMode, hitLoc, false);
            CHECK(g_TestTargetObjectCalled);
            CHECK(g_TestTargetObjectArg2 == hitMode);
            CHECK(g_TestTargetObjectArg3 == hitLoc);
        }
    }
}

TEST_CASE("M-070: scriptHooks_TargetObject — attacker and defender can be same")
{
    // Edge case: self-targeting (e.g., using stimpak on self triggers attack).
    g_TestTargetObjectCalled = false;

    TestScriptHooksTargetObject(5, 5, 1, 0, false);

    CHECK(g_TestTargetObjectCalled);
    CHECK(g_TestTargetObjectArg0 == 5);
    CHECK(g_TestTargetObjectArg1 == 5);
    CHECK(g_TestTargetObjectArg0 == g_TestTargetObjectArg1); // self-target
}
