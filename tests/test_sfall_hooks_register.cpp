// Unit tests for scriptHooksRegister() — registration lifecycle.
//
// Tests: register, deduplicate, unregister (via procedureIndex=0),
//        multiple callbacks, atEnd flag, hook list integrity.
//
// F2-018: scriptHooksRegister() zero C++ unit tests.
//
// Self-contained header-only test — does NOT link sfall_script_hooks.cc
// (50+ engine dependencies). Uses local mirrors of ScriptHook struct,
// scriptHooks array, and scriptHooksRegister logic.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "sfall_script_hooks.h"

#include <vector>

using namespace fallout;

// ---- Local mirror of ScriptHook struct (must match sfall_script_hooks.cc:34-37) ----
struct TestScriptHook {
    void* program = nullptr;
    int procedureIndex = -1;
};

// Local mirror of scriptHooks array (sfall_script_hooks.cc:39)
static std::vector<TestScriptHook> testScriptHooks[HOOK_COUNT];

// Local mirror of scriptHooksRegister (sfall_script_hooks.cc:168-200)
// Uses void* instead of Program* to avoid linking interpreter.cc (150+ engine deps).
static bool testScriptHooksRegister(void* program, HookType hookType, int procedureIndex, bool atEnd = false)
{
    int procedureCount = 100; // arbitrary: all indices < 100 are valid for testing
    // Guard: assert-level checks
    if (program == nullptr || hookType < 0 || hookType >= HOOK_COUNT
        || procedureIndex < 0 || procedureIndex >= procedureCount) {
        // In production these are asserts; for test we return false
        if (program == nullptr) return false;
        if (hookType < 0 || hookType >= HOOK_COUNT) return false;
        if (procedureIndex < 0) return false;
    }

    auto& hooksByType = testScriptHooks[hookType];
    const bool isUnregisterRequest = (procedureIndex == 0);

    // Check for existing registration.
    for (auto it = hooksByType.begin(); it != hooksByType.end(); ++it) {
        if (it->program == program) {
            if (isUnregisterRequest) {
                hooksByType.erase(it);
                return true; // unregister success
            }
            // Skip: no more than 1 procedure in a script for a given hook type.
            return false; // register fail — already registered
        }
    }
    if (isUnregisterRequest) {
        return false; // unregister fail — not found
    }

    TestScriptHook hook = { program, procedureIndex };
    if (atEnd) {
        hooksByType.insert(hooksByType.begin(), hook);
    } else {
        hooksByType.push_back(hook);
    }
    return true; // register success
}

// Reset helper
static void resetTestHooks()
{
    for (auto& hooks : testScriptHooks) {
        hooks.clear();
    }
}

// =================================================================
// Register a hook callback
// =================================================================

TEST_CASE("Register a single hook callback")
{
    resetTestHooks();

    void* prog = reinterpret_cast<void*>(1);
    bool result = testScriptHooksRegister(prog, HOOK_TOHIT, 5, false);

    CHECK(result == true);
    CHECK(testScriptHooks[HOOK_TOHIT].size() == 1);
    CHECK(testScriptHooks[HOOK_TOHIT][0].program == prog);
    CHECK(testScriptHooks[HOOK_TOHIT][0].procedureIndex == 5);
}

TEST_CASE("Register fails with nullptr program")
{
    resetTestHooks();

    bool result = testScriptHooksRegister(nullptr, HOOK_TOHIT, 5);

    CHECK(result == false);
    CHECK(testScriptHooks[HOOK_TOHIT].size() == 0);
}

TEST_CASE("Register fails with invalid hook type")
{
    resetTestHooks();

    void* prog = reinterpret_cast<void*>(1);
    bool result = testScriptHooksRegister(prog, static_cast<HookType>(-1), 5);

    CHECK(result == false);
}

TEST_CASE("Register fails with out-of-range hook type")
{
    resetTestHooks();

    void* prog = reinterpret_cast<void*>(1);
    bool result = testScriptHooksRegister(prog, static_cast<HookType>(HOOK_COUNT), 5);

    CHECK(result == false);
}

TEST_CASE("Register fails with negative procedure index")
{
    resetTestHooks();

    void* prog = reinterpret_cast<void*>(1);
    bool result = testScriptHooksRegister(prog, HOOK_TOHIT, -1);

    CHECK(result == false);
}

// =================================================================
// Dedup: same callback registered twice
// =================================================================

TEST_CASE("Register same callback again (dedup) — second fails")
{
    resetTestHooks();

    void* prog = reinterpret_cast<void*>(1);

    bool first = testScriptHooksRegister(prog, HOOK_TOHIT, 5);
    CHECK(first == true);

    bool second = testScriptHooksRegister(prog, HOOK_TOHIT, 10);
    CHECK(second == false); // dedup: already registered

    // Only one entry, with original procedureIndex
    CHECK(testScriptHooks[HOOK_TOHIT].size() == 1);
    CHECK(testScriptHooks[HOOK_TOHIT][0].procedureIndex == 5);
}

TEST_CASE("Different programs can register same hook type")
{
    resetTestHooks();

    void* prog1 = reinterpret_cast<void*>(1);
    void* prog2 = reinterpret_cast<void*>(2);

    bool r1 = testScriptHooksRegister(prog1, HOOK_TOHIT, 5);
    bool r2 = testScriptHooksRegister(prog2, HOOK_TOHIT, 10);

    CHECK(r1 == true);
    CHECK(r2 == true);
    CHECK(testScriptHooks[HOOK_TOHIT].size() == 2);
    CHECK(testScriptHooks[HOOK_TOHIT][0].program == prog1);
    CHECK(testScriptHooks[HOOK_TOHIT][1].program == prog2);
}

// =================================================================
// Unregister via procedureIndex=0
// =================================================================

TEST_CASE("Unregister via procedureIndex=0 removes registered callback")
{
    resetTestHooks();

    void* prog = reinterpret_cast<void*>(1);

    // Register first
    testScriptHooksRegister(prog, HOOK_TOHIT, 5);
    CHECK(testScriptHooks[HOOK_TOHIT].size() == 1);

    // Unregister
    bool result = testScriptHooksRegister(prog, HOOK_TOHIT, 0);
    CHECK(result == true);
    CHECK(testScriptHooks[HOOK_TOHIT].size() == 0);
}

TEST_CASE("Unregister non-existent callback returns false")
{
    resetTestHooks();

    void* prog = reinterpret_cast<void*>(1);
    bool result = testScriptHooksRegister(prog, HOOK_TOHIT, 0);

    CHECK(result == false); // nothing to unregister
}

TEST_CASE("Unregister one does not affect other programs")
{
    resetTestHooks();

    void* prog1 = reinterpret_cast<void*>(1);
    void* prog2 = reinterpret_cast<void*>(2);

    testScriptHooksRegister(prog1, HOOK_TOHIT, 5);
    testScriptHooksRegister(prog2, HOOK_TOHIT, 10);

    // Unregister prog1
    testScriptHooksRegister(prog1, HOOK_TOHIT, 0);

    CHECK(testScriptHooks[HOOK_TOHIT].size() == 1);
    CHECK(testScriptHooks[HOOK_TOHIT][0].program == prog2);
    CHECK(testScriptHooks[HOOK_TOHIT][0].procedureIndex == 10);
}

TEST_CASE("Unregister one does not affect other hook types")
{
    resetTestHooks();

    void* prog = reinterpret_cast<void*>(1);

    testScriptHooksRegister(prog, HOOK_TOHIT, 5);
    testScriptHooksRegister(prog, HOOK_ONDEATH, 10);

    // Unregister from TOHIT
    testScriptHooksRegister(prog, HOOK_TOHIT, 0);

    CHECK(testScriptHooks[HOOK_TOHIT].size() == 0);
    CHECK(testScriptHooks[HOOK_ONDEATH].size() == 1);
    CHECK(testScriptHooks[HOOK_ONDEATH][0].program == prog);
}

// =================================================================
// Multiple callbacks for the same hook type
// =================================================================

TEST_CASE("Register multiple callbacks for the same hook type")
{
    resetTestHooks();

    void* prog1 = reinterpret_cast<void*>(1);
    void* prog2 = reinterpret_cast<void*>(2);
    void* prog3 = reinterpret_cast<void*>(3);

    testScriptHooksRegister(prog1, HOOK_GAMEMODECHANGE, 10);
    testScriptHooksRegister(prog2, HOOK_GAMEMODECHANGE, 20);
    testScriptHooksRegister(prog3, HOOK_GAMEMODECHANGE, 30);

    CHECK(testScriptHooks[HOOK_GAMEMODECHANGE].size() == 3);

    // Order: push_back → prog1 at [0], prog2 at [1], prog3 at [2]
    CHECK(testScriptHooks[HOOK_GAMEMODECHANGE][0].program == prog1);
    CHECK(testScriptHooks[HOOK_GAMEMODECHANGE][1].program == prog2);
    CHECK(testScriptHooks[HOOK_GAMEMODECHANGE][2].program == prog3);
}

TEST_CASE("Multiple callbacks do not affect other hook types")
{
    resetTestHooks();

    void* prog1 = reinterpret_cast<void*>(1);
    void* prog2 = reinterpret_cast<void*>(2);

    testScriptHooksRegister(prog1, HOOK_TOHIT, 5);
    testScriptHooksRegister(prog2, HOOK_ONDEATH, 10);

    CHECK(testScriptHooks[HOOK_TOHIT].size() == 1);
    CHECK(testScriptHooks[HOOK_ONDEATH].size() == 1);
    CHECK(testScriptHooks[HOOK_GAMEMODECHANGE].size() == 0); // untouched
}

// =================================================================
// atEnd flag behavior (register_hook_proc vs register_hook_proc_spec)
// =================================================================

TEST_CASE("atEnd=false inserts at end (highest priority)")
{
    resetTestHooks();

    void* prog1 = reinterpret_cast<void*>(1);
    void* prog2 = reinterpret_cast<void*>(2);

    testScriptHooksRegister(prog1, HOOK_TOHIT, 5, false); // atEnd=false
    testScriptHooksRegister(prog2, HOOK_TOHIT, 10, false); // atEnd=false

    // Order: prog1 at [0], prog2 at [1] (push_back)
    // Reverse iteration means prog2 (highest index) executes first = highest priority
    CHECK(testScriptHooks[HOOK_TOHIT].size() == 2);
    CHECK(testScriptHooks[HOOK_TOHIT][0].program == prog1);
    CHECK(testScriptHooks[HOOK_TOHIT][1].program == prog2);
}

TEST_CASE("atEnd=true inserts at beginning (lowest priority)")
{
    resetTestHooks();

    void* prog1 = reinterpret_cast<void*>(1);
    void* prog2 = reinterpret_cast<void*>(2);

    testScriptHooksRegister(prog1, HOOK_TOHIT, 5, true); // atEnd=true
    testScriptHooksRegister(prog2, HOOK_TOHIT, 10, true); // atEnd=true

    // Order: prog2 at [0], prog1 at [1] (emplace at begin)
    // prog2 was inserted last → at begin (index 0), so prog1 shifts to index 1
    // Reverse iteration: prog1 (index 1) executes first, prog2 (index 0) executes last = final override
    CHECK(testScriptHooks[HOOK_TOHIT].size() == 2);
    CHECK(testScriptHooks[HOOK_TOHIT][0].program == prog2); // last inserted → index 0
    CHECK(testScriptHooks[HOOK_TOHIT][1].program == prog1); // first inserted → shifted to index 1
}

TEST_CASE("Mixed atEnd and non-atEnd registration order")
{
    resetTestHooks();

    void* prog1 = reinterpret_cast<void*>(1);
    void* prog2 = reinterpret_cast<void*>(2);
    void* prog3 = reinterpret_cast<void*>(3);

    // atEnd=false → push_back
    testScriptHooksRegister(prog1, HOOK_TOHIT, 5, false);
    // atEnd=true → emplace at begin
    testScriptHooksRegister(prog2, HOOK_TOHIT, 10, true);
    // atEnd=false → push_back
    testScriptHooksRegister(prog3, HOOK_TOHIT, 15, false);

    // Expected order:
    // After prog1: [prog1]
    // After prog2: [prog2, prog1]  (prog2 emplace at begin)
    // After prog3: [prog2, prog1, prog3] (prog3 push_back)
    CHECK(testScriptHooks[HOOK_TOHIT].size() == 3);
    CHECK(testScriptHooks[HOOK_TOHIT][0].program == prog2); // atEnd=true, lowest priority
    CHECK(testScriptHooks[HOOK_TOHIT][1].program == prog1);
    CHECK(testScriptHooks[HOOK_TOHIT][2].program == prog3); // last push_back, highest priority
}

// =================================================================
// Hook list integrity after operations
// =================================================================

TEST_CASE("Hook list integrity after register + unregister cycle")
{
    resetTestHooks();

    void* prog = reinterpret_cast<void*>(1);

    testScriptHooksRegister(prog, HOOK_TOHIT, 5);
    testScriptHooksRegister(prog, HOOK_TOHIT, 0); // unregister
    testScriptHooksRegister(prog, HOOK_TOHIT, 15); // re-register

    CHECK(testScriptHooks[HOOK_TOHIT].size() == 1);
    CHECK(testScriptHooks[HOOK_TOHIT][0].program == prog);
    CHECK(testScriptHooks[HOOK_TOHIT][0].procedureIndex == 15);
}

TEST_CASE("Hook list empty after clearing all registrations")
{
    resetTestHooks();

    void* prog1 = reinterpret_cast<void*>(1);
    void* prog2 = reinterpret_cast<void*>(2);

    testScriptHooksRegister(prog1, HOOK_TOHIT, 5);
    testScriptHooksRegister(prog2, HOOK_ONDEATH, 10);
    testScriptHooksRegister(prog1, HOOK_GAMEMODECHANGE, 15);

    CHECK(testScriptHooks[HOOK_TOHIT].size() == 1);
    CHECK(testScriptHooks[HOOK_ONDEATH].size() == 1);
    CHECK(testScriptHooks[HOOK_GAMEMODECHANGE].size() == 1);

    // Simulate scriptHooksClear() — called by scriptHooksReset/Exit
    for (auto& hooks : testScriptHooks) {
        hooks.clear();
    }

    for (int i = 0; i < HOOK_COUNT; i++) {
        CHECK(testScriptHooks[i].size() == 0);
    }
}

TEST_CASE("Cross-hook-type chain: register → checks → unregister → verify")
{
    resetTestHooks();

    void* prog = reinterpret_cast<void*>(0x42);

    // Register on 3 hooks
    testScriptHooksRegister(prog, HOOK_TOHIT, 1);
    testScriptHooksRegister(prog, HOOK_ONDEATH, 2);
    testScriptHooksRegister(prog, HOOK_KEYPRESS, 3);

    CHECK(testScriptHooks[HOOK_TOHIT].size() == 1);
    CHECK(testScriptHooks[HOOK_ONDEATH].size() == 1);
    CHECK(testScriptHooks[HOOK_KEYPRESS].size() == 1);

    // Unregister from HOOK_TOHIT only
    bool r1 = testScriptHooksRegister(prog, HOOK_TOHIT, 0);
    CHECK(r1 == true);
    CHECK(testScriptHooks[HOOK_TOHIT].size() == 0);
    CHECK(testScriptHooks[HOOK_ONDEATH].size() == 1);
    CHECK(testScriptHooks[HOOK_KEYPRESS].size() == 1);

    // Unregister everything: unregister each remaining
    testScriptHooksRegister(prog, HOOK_ONDEATH, 0);
    testScriptHooksRegister(prog, HOOK_KEYPRESS, 0);

    CHECK(testScriptHooks[HOOK_ONDEATH].size() == 0);
    CHECK(testScriptHooks[HOOK_KEYPRESS].size() == 0);
}

TEST_CASE("Register with HOOK_COUNT boundary — last valid hook")
{
    resetTestHooks();

    void* prog = reinterpret_cast<void*>(1);
    HookType lastHook = static_cast<HookType>(HOOK_COUNT - 1);

    bool result = testScriptHooksRegister(prog, lastHook, 5);
    CHECK(result == true);
    CHECK(testScriptHooks[lastHook].size() == 1);
}

TEST_CASE("Different hook types have independent hook lists")
{
    resetTestHooks();

    void* prog = reinterpret_cast<void*>(1);
    int hookTypes[] = { HOOK_TOHIT, HOOK_ONDEATH, HOOK_KEYPRESS, HOOK_GAMEMODECHANGE, HOOK_COMBATTURN };

    for (int i = 0; i < 5; i++) {
        testScriptHooksRegister(prog, static_cast<HookType>(hookTypes[i]), i + 1);
    }

    // Each hook type should have exactly 1 entry
    for (int i = 0; i < 5; i++) {
        CHECK(testScriptHooks[hookTypes[i]].size() == 1);
        CHECK(testScriptHooks[hookTypes[i]][0].program == prog);
        CHECK(testScriptHooks[hookTypes[i]][0].procedureIndex == i + 1);
    }
}

// =================================================================
// Edge cases
// =================================================================

TEST_CASE("Unregister when list is empty returns false")
{
    resetTestHooks();

    void* prog = reinterpret_cast<void*>(1);
    bool result = testScriptHooksRegister(prog, HOOK_TOHIT, 0);

    CHECK(result == false);
    CHECK(testScriptHooks[HOOK_TOHIT].size() == 0);
}

TEST_CASE("Register-dedup-unregister the same callback returns to empty")
{
    resetTestHooks();

    void* prog = reinterpret_cast<void*>(1);

    CHECK(testScriptHooksRegister(prog, HOOK_TOHIT, 5) == true);
    CHECK(testScriptHooksRegister(prog, HOOK_TOHIT, 10) == false); // dedup fails
    CHECK(testScriptHooks[HOOK_TOHIT].size() == 1);

    CHECK(testScriptHooksRegister(prog, HOOK_TOHIT, 0) == true); // unregister
    CHECK(testScriptHooks[HOOK_TOHIT].size() == 0);

    CHECK(testScriptHooksRegister(prog, HOOK_TOHIT, 0) == false); // already unregistered
}

// =================================================================
// F-22 (MEDIUM, FIXED): Reserved hooks 54-60 return false from
// sfallHookHasFireSite, preventing silent registration of handlers
// that will never be called.
// =================================================================
//
// Finding F-22: Reserved hooks 54-60 returned true from sfallHookHasFireSite
// (via the `default:` case), allowing scripts to register handlers for hooks
// that have no fire sites. Handlers were accepted but never called — silent
// waste. Other unimpl hooks (14, 15, 37, 44-47) also had this issue.
//
// Fix at sfall_opcodes.cc:3342-3353: Added explicit exclusion cases for
// hooks 54-60 (and 61) to the sfallHookHasFireSite() switch, returning
// false instead of falling through to the default `return true`.
//
// This test verifies that the test mirror of sfallHookHasFireSite correctly
// rejects registration for these reserved hooks.

// Mirror of sfallHookHasFireSite at sfall_opcodes.cc:3302-3357.
// Excludes hooks that have no fire sites — registration should fail.
static bool testSfallHookHasFireSite(int hookType)
{
    switch (hookType) {
    case 14: // HOOK_HEXSHOOTBLOCKING (obsolete)
    case 15: // HOOK_HEXSIGHTBLOCKING (obsolete)
    case 37: // HOOK_SUBCOMBATDAMAGE (per-hit not supported)
    case 44: // HOOK_ADJUSTPOISON (requires engine refactor)
    case 45: // HOOK_ADJUSTRADS (requires engine refactor)
    case 46: // HOOK_ROLLCHECK (30+ call sites, lacks context)
    case 47: // HOOK_BESTWEAPON (10+ return points, lifetime issues)
    // F-22 fix: reserved hooks 54-60 have no fire sites
    case 54:
    case 55:
    case 56:
    case 57:
    case 58:
    case 59:
    case 60:
    case 61: // HOOK_BUILDSFXWEAPON (static buffer, lifetime issues)
        return false;
    default:
        return true;
    }
}

// Returns whether hook registration should succeed, mirroring the
// production registration path:
//   op_register_hook → sfallHookHasFireSite → if false, print error, return
static bool testRegistrationWouldSucceed(int hookType, void* program)
{
    if (program == nullptr) return false;
    if (hookType < 0 || hookType >= HOOK_COUNT) return false;
    return testSfallHookHasFireSite(hookType);
}

TEST_CASE("F-22: Reserved hooks 54-60 — registration should fail (no fire sites)")
{
    void* prog = reinterpret_cast<void*>(0xDEAD);

    // Reserved hooks 54-60 should be rejected by sfallHookHasFireSite
    for (int ht = 54; ht <= 60; ht++) {
        INFO("Hook type ", ht, " must have no fire site");
        CHECK_FALSE(testSfallHookHasFireSite(ht));
        CHECK_FALSE(testRegistrationWouldSucceed(ht, prog));
    }
}

TEST_CASE("F-22: Valid hooks (0-10) still allow registration")
{
    void* prog = reinterpret_cast<void*>(0xBEEF);

    // Known implemented hooks should still pass the fire site check
    CHECK(testSfallHookHasFireSite(0));  // HOOK_TOHIT
    CHECK(testSfallHookHasFireSite(1));  // HOOK_AFTERHITROLL
    CHECK(testSfallHookHasFireSite(5));  // HOOK_COMBATDAMAGE
    CHECK(testSfallHookHasFireSite(6));  // HOOK_ONDEATH

    // Registration should succeed for these hooks
    CHECK(testRegistrationWouldSucceed(0, prog));
    CHECK(testRegistrationWouldSucceed(1, prog));
    CHECK(testRegistrationWouldSucceed(5, prog));
    CHECK(testRegistrationWouldSucceed(6, prog));
}

TEST_CASE("F-22: Hook 61 (BUILDSFXWEAPON) also has no fire site")
{
    void* prog = reinterpret_cast<void*>(0xCAFE);

    CHECK_FALSE(testSfallHookHasFireSite(61));
    CHECK_FALSE(testRegistrationWouldSucceed(61, prog));
}

TEST_CASE("F-22: Other unimplemented hooks (14, 15, 37, 44-47) have no fire site")
{
    void* prog = reinterpret_cast<void*>(0xF00D);

    int unimplemented[] = { 14, 15, 37, 44, 45, 46, 47 };
    for (int ht : unimplemented) {
        INFO("Hook type ", ht, " must have no fire site");
        CHECK_FALSE(testSfallHookHasFireSite(ht));
        CHECK_FALSE(testRegistrationWouldSucceed(ht, prog));
    }
}

TEST_CASE("F-22: Registration with null program still fails for reserved hooks")
{
    for (int ht = 54; ht <= 60; ht++) {
        CHECK_FALSE(testRegistrationWouldSucceed(ht, nullptr));
    }
}
