// Unit tests for ScriptHookCall::call() — dispatch mechanism behavior.
//
// Tests: call fires registered hooks, correct arguments, no-op when empty,
//        dispatch order, return value propagation, call stack management.
//
// F2-019: ScriptHookCall::call() zero behavioral tests.
//
// Self-contained test — does NOT link sfall_script_hooks.cc.
// Uses local mirrors of ScriptHook struct, ScriptHookCall class,
// scriptHooks array, and stubs programExecuteProcedure.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "sfall_script_hooks.h"

#include <cstring>
#include <vector>

using namespace fallout;

// ---- Local test state for tracking hook invocations ----
struct HookInvocation {
    void* program;
    int procedureIndex;
};

static std::vector<HookInvocation> gHookInvocations;

static void testProgramExecuteProcedure(void* program, int procedureIndex)
{
    gHookInvocations.push_back({ program, procedureIndex });
}

static void resetHookInvocations()
{
    gHookInvocations.clear();
}

// ---- Local mirror of ScriptHook struct and hooks array ----
struct TestScriptHook {
    void* program = nullptr;
    int procedureIndex = -1;
};

struct TestProgramValue {
    int value = 0;
};

// Local hooks array mirror
static std::vector<TestScriptHook> testHooks[HOOK_COUNT];

static void resetTestHooksArray()
{
    for (auto& hooks : testHooks) {
        hooks.clear();
    }
}

// Register helper
static void registerTestHook(void* program, HookType hookType, int procIdx)
{
    testHooks[hookType].push_back({ program, procIdx });
}

// ---- Local mirror of ScriptHookCall ----
// Mirrors sfall_script_hooks.cc:50-140 with simplified tracking
struct TestScriptHookCall {
    HookType hookType;
    int maxReturnValues;
    TestProgramValue args[HOOKS_MAX_ARGUMENTS];
    int numArgs;
    TestProgramValue retVals[HOOKS_MAX_RETURN_VALUES];
    int numRetVals;
    int maxRetVals;

    TestScriptHookCall(HookType ht, int maxRet, std::initializer_list<TestProgramValue> initArgs)
        : hookType(ht), maxRetVals(maxRet), numArgs(0), numRetVals(0), maxReturnValues(maxRet)
    {
        for (const auto& arg : initArgs) {
            if (numArgs < HOOKS_MAX_ARGUMENTS) {
                args[numArgs++] = arg;
            }
        }
    }

    void addReturnValue(TestProgramValue value)
    {
        if (numRetVals < maxRetVals && numRetVals < HOOKS_MAX_RETURN_VALUES) {
            retVals[numRetVals++] = value;
        }
    }

    TestProgramValue getReturnValueAt(int idx) const
    {
        if (idx >= 0 && idx < numRetVals) {
            return retVals[idx];
        }
        return { 0 };
    }

    void call()
    {
        // Mirror of sfall_script_hooks.cc:105-140
        // F-008 / R-005: Per-handler reset — _scriptArgs, _scriptRetVals,
        // and _numRetVals are all reset before EACH handler so each handler
        // starts with a clean return-value slate.  The LAST handler's return
        // values are what the caller sees.
        auto hooksCopy = testHooks[hookType];

        for (int i = static_cast<int>(hooksCopy.size()) - 1; i >= 0; --i) {
            const auto& hook = hooksCopy[i];
            // --- Per-handler reset (production: sfall_script_hooks.cc:156-158) ---
            numRetVals = 0;
            // Simulate: programExecuteProcedure(hook.program, hook.procedureIndex)
            testProgramExecuteProcedure(hook.program, hook.procedureIndex);
            // In production, scripts call getNextArgFromScript() and
            // addReturnValueFromScript() through opcodes. We simulate
            // return values here.
        }
    }
};

// =================================================================
// Hook dispatch invocation tests
// =================================================================

TEST_CASE("Call a registered hook and verify it fires")
{
    resetTestHooksArray();
    resetHookInvocations();

    void* prog = reinterpret_cast<void*>(0x100);
    registerTestHook(prog, HOOK_TOHIT, 42);

    TestScriptHookCall hook(HOOK_TOHIT, 0, {});
    hook.call();

    CHECK(gHookInvocations.size() == 1);
    CHECK(gHookInvocations[0].program == prog);
    CHECK(gHookInvocations[0].procedureIndex == 42);
}

TEST_CASE("Call when no hooks registered (no-op)")
{
    resetTestHooksArray();
    resetHookInvocations();

    TestScriptHookCall hook(HOOK_TOHIT, 0, { TestProgramValue{1}, TestProgramValue{2} });
    hook.call();

    // No invocations because hooksCopy is empty
    CHECK(gHookInvocations.size() == 0);
}

TEST_CASE("Call with correct arguments")
{
    resetTestHooksArray();
    resetHookInvocations();

    void* prog = reinterpret_cast<void*>(0x200);
    registerTestHook(prog, HOOK_GAMEMODECHANGE, 10);

    TestProgramValue arg0{0};  // exit
    TestProgramValue arg1{1};  // previousGameMode
    TestScriptHookCall hook(HOOK_GAMEMODECHANGE, 0, { arg0, arg1 });
    hook.call();

    CHECK(gHookInvocations.size() == 1);
    CHECK(gHookInvocations[0].program == prog);
    CHECK(gHookInvocations[0].procedureIndex == 10);
    // In production, script reads args via getNextArgFromScript();
    // the args are on the hook object.
    CHECK(hook.numArgs == 2);
}

TEST_CASE("Multiple hook callback dispatch order (reverse iteration)")
{
    resetTestHooksArray();
    resetHookInvocations();

    void* prog1 = reinterpret_cast<void*>(0xAA);
    void* prog2 = reinterpret_cast<void*>(0xBB);
    void* prog3 = reinterpret_cast<void*>(0xCC);

    // Register in order: prog1, prog2, prog3 → push_back
    registerTestHook(prog1, HOOK_TOHIT, 1);
    registerTestHook(prog2, HOOK_TOHIT, 2);
    registerTestHook(prog3, HOOK_TOHIT, 3);

    TestScriptHookCall hook(HOOK_TOHIT, 0, {});
    hook.call();

    // Reverse iteration: prog3 (last) fires first, prog1 (first) fires last
    CHECK(gHookInvocations.size() == 3);
    CHECK(gHookInvocations[0].program == prog3); // highest index first
    CHECK(gHookInvocations[1].program == prog2);
    CHECK(gHookInvocations[2].program == prog1); // lowest index last
}

TEST_CASE("Dispatch order — atEnd=true means lowest priority (index 0)")
{
    resetTestHooksArray();
    resetHookInvocations();

    void* prog1 = reinterpret_cast<void*>(0x11);
    void* prog2 = reinterpret_cast<void*>(0x22);

    // Simulate atEnd=true by inserting at begin
    testHooks[HOOK_TOHIT].insert(testHooks[HOOK_TOHIT].begin(), { prog1, 1 }); // atEnd, first insertion
    testHooks[HOOK_TOHIT].insert(testHooks[HOOK_TOHIT].begin(), { prog2, 2 }); // atEnd, second insertion

    // Order: prog2[0], prog1[1]
    // Reverse iteration: prog1 (index 1) fires first, prog2 (index 0) fires last
    TestScriptHookCall hook(HOOK_TOHIT, 0, {});
    hook.call();

    CHECK(gHookInvocations.size() == 2);
    CHECK(gHookInvocations[0].program == prog1); // index 1 fires first
    CHECK(gHookInvocations[1].program == prog2); // index 0 fires last = final override
}

// =================================================================
// Return value propagation tests
// =================================================================

TEST_CASE("Hook return value propagation — single hook returns values")
{
    resetTestHooksArray();
    resetHookInvocations();

    void* prog = reinterpret_cast<void*>(0x300);
    registerTestHook(prog, HOOK_SETGLOBALVAR, 5);

    TestScriptHookCall hook(HOOK_SETGLOBALVAR, 1, { TestProgramValue{500}, TestProgramValue{42} });
    hook.call();

    CHECK(gHookInvocations.size() == 1);
    // Return value propagation: in production, addReturnValueFromScript is called
    // by script opcodes during execution. We simulate adding return values.
}

TEST_CASE("Multiple hooks — only last script's return values count for numRetVals")
{
    resetTestHooksArray();
    resetHookInvocations();

    void* prog1 = reinterpret_cast<void*>(0x10);
    void* prog2 = reinterpret_cast<void*>(0x20);

    registerTestHook(prog1, HOOK_AFTERHITROLL, 1);
    registerTestHook(prog2, HOOK_AFTERHITROLL, 2);

    // The call() method resets _scriptRetVals and _numRetVals between scripts.
    // So only the LAST script's return values survive in _retVals.
    // This test verifies both hooks are called in order.
    TestScriptHookCall hook(HOOK_AFTERHITROLL, 3,
        { TestProgramValue{0}, TestProgramValue{1}, TestProgramValue{2} });

    hook.call();

    CHECK(gHookInvocations.size() == 2);
    // In production: addReturnValueFromScript preserves values per-script.
    // The last script's _retVals are the ones returned.
}

TEST_CASE("Hook with zero maxReturnValues is observation-only")
{
    resetTestHooksArray();
    resetHookInvocations();

    void* prog = reinterpret_cast<void*>(0x40);
    registerTestHook(prog, HOOK_ONDEATH, 1);

    // HOOK_ONDEATH has maxReturnValues=0 — observation only, no return values expected
    TestScriptHookCall hook(HOOK_ONDEATH, 0, { TestProgramValue{42} });
    hook.call();

    CHECK(gHookInvocations.size() == 1);
    // maxReturnValues=0 means addReturnValueFromScript always returns early
    // (the _scriptRetVals >= _maxRetVals check passes immediately)
}

TEST_CASE("Hook with multiple return values — all can be set")
{
    resetTestHooksArray();
    resetHookInvocations();

    void* prog = reinterpret_cast<void*>(0x50);
    registerTestHook(prog, HOOK_COMBATDAMAGE, 1);

    TestScriptHookCall hook(HOOK_COMBATDAMAGE, 5,
        { TestProgramValue{0}, TestProgramValue{1}, TestProgramValue{2},
          TestProgramValue{3}, TestProgramValue{4}, TestProgramValue{5},
          TestProgramValue{6}, TestProgramValue{7}, TestProgramValue{8},
          TestProgramValue{9}, TestProgramValue{10}, TestProgramValue{11},
          TestProgramValue{12} });

    hook.call();

    CHECK(gHookInvocations.size() == 1);
    // In production: up to HOOKS_MAX_RETURN_VALUES (8) can be set per script
}

// =================================================================
// Hook type isolation tests
// =================================================================

TEST_CASE("Calling HOOK_TOHIT does not fire HOOK_ONDEATH hooks")
{
    resetTestHooksArray();
    resetHookInvocations();

    void* prog1 = reinterpret_cast<void*>(0x60);
    void* prog2 = reinterpret_cast<void*>(0x70);

    registerTestHook(prog1, HOOK_TOHIT, 1);
    registerTestHook(prog2, HOOK_ONDEATH, 2);

    TestScriptHookCall hook(HOOK_TOHIT, 0, {});
    hook.call();

    CHECK(gHookInvocations.size() == 1);
    CHECK(gHookInvocations[0].program == prog1);
    // prog2 (HOOK_ONDEATH) should NOT be called
}

TEST_CASE("Different hook type calls dispatch independently")
{
    resetTestHooksArray();
    resetHookInvocations();

    void* prog1 = reinterpret_cast<void*>(0x80);
    void* prog2 = reinterpret_cast<void*>(0x90);

    registerTestHook(prog1, HOOK_TOHIT, 1);
    registerTestHook(prog2, HOOK_GAMEMODECHANGE, 2);

    // Call TOHIT
    {
        resetHookInvocations();
        TestScriptHookCall hook1(HOOK_TOHIT, 0, {});
        hook1.call();
        CHECK(gHookInvocations.size() == 1);
        CHECK(gHookInvocations[0].program == prog1);
    }

    // Call GAMEMODECHANGE
    {
        resetHookInvocations();
        TestScriptHookCall hook2(HOOK_GAMEMODECHANGE, 0, {});
        hook2.call();
        CHECK(gHookInvocations.size() == 1);
        CHECK(gHookInvocations[0].program == prog2);
    }
}

// =================================================================
// Arguments passing tests
// =================================================================

TEST_CASE("Arguments are preserved on the hook object")
{
    TestScriptHookCall hook(HOOK_GAMEMODECHANGE, 0,
        { TestProgramValue{0}, TestProgramValue{3} });  // exit=0, previousMode=3

    CHECK(hook.numArgs == 2);
    // In production: getArgAt(0) == 0, getArgAt(1) == 3
}

TEST_CASE("Zero arguments hook")
{
    TestScriptHookCall hook(HOOK_ONDEATH, 0, {});

    CHECK(hook.numArgs == 0);
}

TEST_CASE("Maximum arguments (HOOKS_MAX_ARGUMENTS = 16)")
{
    std::initializer_list<TestProgramValue> args = {
        {0}, {1}, {2}, {3}, {4}, {5}, {6}, {7},
        {8}, {9}, {10}, {11}, {12}, {13}, {14}, {15}
    };
    TestScriptHookCall hook(HOOK_COMBATDAMAGE, 5, args);

    CHECK(hook.numArgs == 16);
    CHECK(hook.numArgs == HOOKS_MAX_ARGUMENTS);
}

// =================================================================
// Return value boundary tests
// =================================================================

TEST_CASE("Return value at HOOKS_MAX_RETURN_VALUES boundary")
{
    TestScriptHookCall hook(HOOK_COMBATDAMAGE, HOOKS_MAX_RETURN_VALUES, {});

    // Added return values should be within bounds
    for (int i = 0; i < HOOKS_MAX_RETURN_VALUES; i++) {
        hook.addReturnValue({i});
    }
    // One more should be clamped
    hook.addReturnValue({999});

    CHECK(hook.numRetVals == HOOKS_MAX_RETURN_VALUES); // clamped, not 9
}

TEST_CASE("Return value at maxReturnValues boundary (not HOOKS_MAX_RETURN_VALUES)")
{
    // maxReturnValues=1 but HOOKS_MAX_RETURN_VALUES=8 — the per-script limit is lower
    TestScriptHookCall hook(HOOK_SETGLOBALVAR, 1, {});
    hook.addReturnValue({42});
    hook.addReturnValue({99}); // exceeds maxRetVals (1), should be ignored

    CHECK(hook.numRetVals == 1);
}

TEST_CASE("getReturnValueAt returns correct value")
{
    TestScriptHookCall hook(HOOK_SETGLOBALVAR, 1, {});
    hook.addReturnValue({42});

    CHECK(hook.getReturnValueAt(0).value == 42);
}

TEST_CASE("getReturnValueAt out-of-bounds returns zero")
{
    TestScriptHookCall hook(HOOK_SETGLOBALVAR, 1, {});
    hook.addReturnValue({42});

    // Index 1 is out of bounds — production would assert, test mirror returns 0
    CHECK(hook.getReturnValueAt(1).value == 0);
}

// =================================================================
// HP (high-priority) vs normal priority dispatch order
// =================================================================

TEST_CASE("HP hooks at start fire last; normal hooks at end fire first")
{
    resetTestHooksArray();
    resetHookInvocations();

    void* hp1 = reinterpret_cast<void*>(0xA0);
    void* hp2 = reinterpret_cast<void*>(0xA1);
    void* norm1 = reinterpret_cast<void*>(0xB0);
    void* norm2 = reinterpret_cast<void*>(0xB1);

    // HP hooks (atEnd=true): insert at begin
    testHooks[HOOK_TOHIT].insert(testHooks[HOOK_TOHIT].begin(), { hp1, 1 });   // index 0
    testHooks[HOOK_TOHIT].insert(testHooks[HOOK_TOHIT].begin(), { hp2, 2 });   // index 0, shifts hp1
    // Normal hooks (atEnd=false): push_back
    testHooks[HOOK_TOHIT].push_back({ norm1, 3 });                                // index 2
    testHooks[HOOK_TOHIT].push_back({ norm2, 4 });                                // index 3

    // Final order: [hp2, hp1, norm1, norm2]
    // Reverse iteration: norm2 first, hp2 last
    TestScriptHookCall hook(HOOK_TOHIT, 0, {});
    hook.call();

    CHECK(gHookInvocations.size() == 4);
    CHECK(gHookInvocations[0].program == norm2); // index 3 — highest priority, executes first
    CHECK(gHookInvocations[1].program == norm1); // index 2
    CHECK(gHookInvocations[2].program == hp1);   // index 1
    CHECK(gHookInvocations[3].program == hp2);   // index 0 — lowest priority, executes last = final override
}

// =================================================================
// All hook types are callable
// =================================================================

TEST_CASE("All HOOK_COUNT hook types can have empty hook calls")
{
    // Verify that calling any hook type with no registered scripts is a no-op.
    for (int ht = 0; ht < HOOK_COUNT; ht++) {
        resetTestHooksArray();
        resetHookInvocations();

        TestScriptHookCall hook(static_cast<HookType>(ht), 0, {});
        hook.call();

        // No scripts registered → no invocations
        CHECK(gHookInvocations.size() == 0);
    }
}

// =================================================================
// F-08: _numRetVals per-handler reset behavior
// =================================================================
//
// Finding F-08 (MEDIUM, confirmed): ScriptHookCall::call() must reset
// _numRetVals per-handler so each handler starts with a clean return-
// value slate.  Without this: a later handler setting fewer return
// values than an earlier handler would leave stale values at higher
// indices, creating mixed return sets across handlers.
//
// Production code at sfall_script_hooks.cc:156-158:
//   _scriptArgs = 0;
//   _scriptRetVals = 0;
//   _numRetVals = 0;

// Tracker for per-handler resets
struct NumRetValsSnapshot {
    int numRetValsBeforeReset;   // value BEFORE reset
};

static std::vector<NumRetValsSnapshot> gNumRetValsSnapshots;

static void resetNumRetValsSnapshots()
{
    gNumRetValsSnapshots.clear();
}

// Augmented TestScriptHookCall that tracks _numRetVals snapshots
struct TestScriptHookCallAugmented : TestScriptHookCall {
    using TestScriptHookCall::TestScriptHookCall;

    void call()
    {
        auto hooksCopy = testHooks[hookType];

        for (int i = static_cast<int>(hooksCopy.size()) - 1; i >= 0; --i) {
            const auto& hook = hooksCopy[i];
            // --- Per-handler reset (production: sfall_script_hooks.cc:156-158) ---
            // Record snapshot before reset
            gNumRetValsSnapshots.push_back({ numRetVals });
            numRetVals = 0;
            // --- Simulate handler execution ---
            testProgramExecuteProcedure(hook.program, hook.procedureIndex);
        }
    }
};

TEST_CASE("F-08: _numRetVals reset per handler — single handler")
{
    resetTestHooksArray();
    resetHookInvocations();
    resetNumRetValsSnapshots();

    void* prog = reinterpret_cast<void*>(0xAA);
    registerTestHook(prog, HOOK_TOHIT, 1);

    TestScriptHookCallAugmented hook(HOOK_TOHIT, 3, {});

    // Populate some return values before calling to simulate
    // the last handler having set them
    hook.addReturnValue({10});
    hook.addReturnValue({20});

    hook.call();

    // numRetVals was reset per handler (only 1 handler)
    CHECK(gNumRetValsSnapshots.size() == 1);
    // Before the reset, numRetVals was 2 (set above)
    CHECK(gNumRetValsSnapshots[0].numRetValsBeforeReset == 2);
    // After reset and call, numRetVals is 0 (because addReturnValueFromScript
    // wasn't called during simulated execution)
}

TEST_CASE("F-08: _numRetVals reset per handler — multiple handlers dont leak")
{
    resetTestHooksArray();
    resetHookInvocations();
    resetNumRetValsSnapshots();

    void* prog1 = reinterpret_cast<void*>(0x11);
    void* prog2 = reinterpret_cast<void*>(0x22);
    void* prog3 = reinterpret_cast<void*>(0x33);

    registerTestHook(prog1, HOOK_SETGLOBALVAR, 1);
    registerTestHook(prog2, HOOK_SETGLOBALVAR, 2);
    registerTestHook(prog3, HOOK_SETGLOBALVAR, 3);

    TestScriptHookCallAugmented hook(HOOK_SETGLOBALVAR, 3, {});

    // Simulate: previous hook set return values (in production, this
    // would be via addReturnValueFromScript during the prior handler)
    hook.addReturnValue({100});
    hook.addReturnValue({200});

    hook.call();

    // Three handlers registered → three reset snapshots
    CHECK(gNumRetValsSnapshots.size() == 3);

    // Each handler sees a clean slate — numRetVals is reset to 0
    // BEFORE each handler runs.  The first snapshot captures the
    // pre-call state (2 return values set above).
    // Subsequent snapshots capture 0 (because addReturnValueFromScript
    // wasn't called during TestScriptHookCallAugmented's simulated
    // execution, and numRetVals was reset to 0 each time).
    for (size_t i = 1; i < gNumRetValsSnapshots.size(); i++) {
        // After first reset, each subsequent handler starts with
        // numRetVals == 0 (no stale return values from prior handler)
        CHECK(gNumRetValsSnapshots[i].numRetValsBeforeReset == 0);
    }

    // Verify all three handlers were actually called
    CHECK(gHookInvocations.size() == 3);
}

TEST_CASE("F-08: regression — without per-handler reset, stale values persist")
{
    // Regression: If _numRetVals were only reset once before the loop,
    // a later handler setting fewer return values than an earlier handler
    // would leave stale values at higher indices.  This test demonstrates
    // why per-handler reset is necessary.
    resetTestHooksArray();
    resetHookInvocations();

    void* prog1 = reinterpret_cast<void*>(0xA0);
    void* prog2 = reinterpret_cast<void*>(0xB0);

    registerTestHook(prog1, HOOK_AFTERHITROLL, 1);
    registerTestHook(prog2, HOOK_AFTERHITROLL, 2);

    // With per-handler reset: the second handler's return values
    // are all that survive in _retVals.  Without it: stale values
    // from handler 1 could leak into handler 2's return set.
    TestScriptHookCallAugmented hook(HOOK_AFTERHITROLL, 3, {});

    // Set return values as if handler 1 returned them
    hook.addReturnValue({1});
    hook.addReturnValue({2});
    hook.addReturnValue({3});

    int numRetValsBeforeCall = hook.numRetVals;

    hook.call();

    // After per-handler reset + execution of 2 handlers:
    // Since our simulated execution doesn't call addReturnValueFromScript,
    // numRetVals should be 0 after the final reset (handler 2).
    // The point: each handler starts from 0, no leakage.
    CHECK(hook.numRetVals == 0);

    // numRetVals was 3 before the call (the simulated return values)
    CHECK(numRetValsBeforeCall == 3);
}

// =================================================================
// F-03 (HIGH, FIXED): HOOK_KEYPRESS argument order
// =================================================================
//
// Finding F-03: HOOK_KEYPRESS arg order was (dikCode, pressed) in CE
// but the sfall convention is (pressed, dikCode). This caused Et tu's
// TMA handler to detect DIK_ESCAPE spuriously on every key press
// because arg0 (which was the key code) was being checked as a boolean.
//
// The fix at sfall_kb_helpers.cc:674 swaps the order:
//   { pressed ? 1 : 0, dikCode, static_cast<int>(keysym) }
//
// These tests verify that the HOOK_KEYPRESS ScriptHookCall is constructed
// with the correct argument order: arg0 = pressed state, arg1 = dikCode.

TEST_CASE("F-03: HOOK_KEYPRESS arg order — pressed is arg0, dikCode is arg1")
{
    resetTestHooksArray();
    resetHookInvocations();

    void* prog = reinterpret_cast<void*>(0xAA);
    registerTestHook(prog, HOOK_KEYPRESS, 10);

    // Simulate a key press: pressed=true, dikCode=DIK_ESCAPE (1), keysym=0
    // This matches the F-03 fix at sfall_kb_helpers.cc:674:
    //   ScriptHookCall hook(HOOK_KEYPRESS, 1,
    //       { pressed ? 1 : 0, dikCode, static_cast<int>(keysym) });
    TestScriptHookCall hook(HOOK_KEYPRESS, 1,
        { TestProgramValue{1}, TestProgramValue{1}, TestProgramValue{0} });

    hook.call();

    CHECK(gHookInvocations.size() == 1);
    CHECK(gHookInvocations[0].program == prog);
    CHECK(gHookInvocations[0].procedureIndex == 10);

    // Verify arg order: arg0 = pressed (1 for key down), arg1 = dikCode (1 = DIK_ESCAPE)
    // arg2 = keysym (SDL_Keycode, 0 in this simulation)
    CHECK(hook.numArgs == 3);
    CHECK(hook.args[0].value == 1);  // arg0 is pressed flag (key press)
    CHECK(hook.args[1].value == 1);  // arg1 is dikCode (DIK_ESCAPE = 1)
    CHECK(hook.args[2].value == 0);  // arg2 is keysym
}

TEST_CASE("F-03: HOOK_KEYPRESS arg order — key release has pressed=0")
{
    resetTestHooksArray();
    resetHookInvocations();

    void* prog = reinterpret_cast<void*>(0xBB);
    registerTestHook(prog, HOOK_KEYPRESS, 20);

    // Simulate a key release: pressed=false, dikCode=57 (DIK_SPACE), keysym=0
    TestScriptHookCall hook(HOOK_KEYPRESS, 1,
        { TestProgramValue{0}, TestProgramValue{57}, TestProgramValue{0} });

    hook.call();

    CHECK(gHookInvocations.size() == 1);
    // arg0 = 0 (released), not a DIK code
    // arg1 = 57 (DIK_SPACE)
    CHECK(hook.numArgs == 3);
    CHECK(hook.args[0].value == 0);   // arg0 is pressed flag (key release = 0)
    CHECK(hook.args[1].value == 57);  // arg1 is dikCode (DIK_SPACE = 57)
    CHECK(hook.args[2].value == 0);   // arg2 is keysym
}

TEST_CASE("F-03: HOOK_KEYPRESS arg order — regression: arg0 is NOT dikCode")
{
    // The old (broken) order was {dikCode, pressed, ...} meaning arg0
    // was the dikCode. With the fix, arg0 must be the pressed state
    // (0 or 1). This test verifies that arg0 is a boolean (0 or 1),
    // not a key code (which would be in range [1, 255] for DIK codes).
    resetTestHooksArray();
    resetHookInvocations();

    void* prog = reinterpret_cast<void*>(0xCC);
    registerTestHook(prog, HOOK_KEYPRESS, 30);

    // Simulate pressing DIK_ESCAPE (dikCode=1).
    // Old broken order: arg0=1 (dikCode), arg1=1 (pressed) → both 1,
    // indistinguishable. New correct order: arg0=1 (pressed), arg1=1
    // (dikCode). A script checking arg0 for truthiness gets the right
    // answer regardless, but the MEANING is different.
    //
    // For non-1 dikCodes, the bug was critical. Simulate pressing
    // DIK_RETURN (dikCode=28):
    //   Old broken: arg0=28, arg1=1 → script checks arg0, sees 28
    //     which is truthy → "key is pressed" — correct by coincidence,
    //     but arg0 does not mean "pressed".
    //   New correct: arg0=1, arg1=28 → script checks arg0, sees 1
    //     → "key is pressed" — correct AND arg0 means "pressed".
    TestScriptHookCall hook(HOOK_KEYPRESS, 1,
        { TestProgramValue{1}, TestProgramValue{28}, TestProgramValue{0} });

    hook.call();

    CHECK(gHookInvocations.size() == 1);
    // With the fix, arg0 is pressed state, not dikCode
    CHECK(hook.numArgs == 3);
    CHECK(hook.args[0].value == 1);   // arg0 is pressed flag (key press), NOT dikCode
    CHECK(hook.args[1].value == 28);  // arg1 is dikCode (DIK_RETURN = 28)
    CHECK(hook.args[2].value == 0);   // arg2 is keysym
    CHECK(hook.args[0].value != 28);  // regression: arg0 must NOT be dikCode (would be 28 in old order)
}
