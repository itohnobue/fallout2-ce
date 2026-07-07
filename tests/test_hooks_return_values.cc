// Return-value propagation tests for ScriptHookCall dispatch.
//
// F-022: Multi-handler return value propagation end-to-end test.
// F-024: AFTERHITROLL multi-return-value boundaries.
// F-029: getReturnValueAt bounds test.
//
// Self-contained header-only test — does NOT link sfall_script_hooks.cc.
// Uses local mirrors of ScriptHook struct, ScriptHookCall class,
// and stubs programExecuteProcedure.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "random.h"
#include "sfall_script_hooks.h"

#include <vector>

using namespace fallout;

// ---- Local test state ----
struct HookInvocation {
    void* program;
    int procedureIndex;
};

static std::vector<HookInvocation> gRetHookInvocations;

static void retTestProgramExecuteProcedure(void* program, int procedureIndex)
{
    gRetHookInvocations.push_back({ program, procedureIndex });
}

static void retResetHookInvocations()
{
    gRetHookInvocations.clear();
}

// ---- Local mirror structures ----
struct TestScriptHook {
    void* program = nullptr;
    int procedureIndex = -1;
};

struct TestProgramValue {
    int value = 0;
    void* obj = nullptr; // for Object* representation

    bool isInt() const { return true; }
    int asInt() const { return value; }
    void* asObject() const { return obj; }
};

// Per-handler return value tracker
struct HandlerReturnValues {
    void* program;
    int procedureIndex;
    std::vector<TestProgramValue> returnValues;
};

static std::vector<HandlerReturnValues> gHandlerReturnSets;

static void retSetHandlerReturns(void* program, int procIdx, std::vector<TestProgramValue> values)
{
    gHandlerReturnSets.push_back({ program, procIdx, values });
}

static void retResetHandlerReturns()
{
    gHandlerReturnSets.clear();
}

// Local hooks array mirror
static std::vector<TestScriptHook> retTestHooks[HOOK_COUNT];

static void retResetTestHooksArray()
{
    for (auto& hooks : retTestHooks) {
        hooks.clear();
    }
}

static void retRegisterTestHook(void* program, HookType hookType, int procIdx)
{
    retTestHooks[hookType].push_back({ program, procIdx });
}

// ---- Local mirror of ScriptHookCall with per-handler return tracking ----
struct RetTestScriptHookCall {
    HookType hookType;
    int maxReturnValues;
    TestProgramValue args[HOOKS_MAX_ARGUMENTS];
    int numArgs;
    TestProgramValue retVals[HOOKS_MAX_RETURN_VALUES];
    void* retValPrograms[HOOKS_MAX_RETURN_VALUES];
    int numRetVals;
    int scriptRetVals;

    RetTestScriptHookCall(HookType ht, int maxRet, std::initializer_list<TestProgramValue> initArgs)
        : hookType(ht), maxReturnValues(maxRet), numArgs(0), numRetVals(0), scriptRetVals(0)
    {
        for (const auto& arg : initArgs) {
            if (numArgs < HOOKS_MAX_ARGUMENTS) {
                args[numArgs++] = arg;
            }
        }
        for (int i = 0; i < HOOKS_MAX_RETURN_VALUES; i++) {
            retValPrograms[i] = nullptr;
        }
    }

    void addReturnValueFromScript(TestProgramValue value, void* program)
    {
        if (scriptRetVals >= maxReturnValues) return;
        if (scriptRetVals >= HOOKS_MAX_RETURN_VALUES) return;

        retVals[scriptRetVals] = value;
        retValPrograms[scriptRetVals] = program;
        scriptRetVals++;

        if (scriptRetVals > numRetVals) {
            numRetVals = scriptRetVals;
        }
    }

    TestProgramValue getReturnValueAt(int idx) const
    {
        if (idx >= 0 && idx < numRetVals) {
            return retVals[idx];
        }
        return { 0 };
    }

    int numReturnValues() const { return numRetVals; }
    int numScriptReturnValues() const { return scriptRetVals; }

    void call()
    {
        // Mirror of sfall_script_hooks.cc:105-166 with per-handler reset
        auto hooksCopy = retTestHooks[hookType];

        for (int i = static_cast<int>(hooksCopy.size()) - 1; i >= 0; --i) {
            const auto& hook = hooksCopy[i];
            // Per-handler reset (production: sfall_script_hooks.cc:156-158)
            scriptRetVals = 0;
            numRetVals = 0;
            // Simulate program execution
            retTestProgramExecuteProcedure(hook.program, hook.procedureIndex);
        }
    }

    // Call with explicit per-handler return value simulation
    void callWithReturns(std::vector<std::vector<TestProgramValue>> perHandlerReturns)
    {
        auto hooksCopy = retTestHooks[hookType];
        int handlerIdx = 0;

        for (int i = static_cast<int>(hooksCopy.size()) - 1; i >= 0; --i) {
            const auto& hook = hooksCopy[i];
            // Per-handler reset
            scriptRetVals = 0;
            numRetVals = 0;
            // Simulate program execution
            retTestProgramExecuteProcedure(hook.program, hook.procedureIndex);
            // Simulate this handler setting return values
            if (handlerIdx < static_cast<int>(perHandlerReturns.size())) {
                for (const auto& rv : perHandlerReturns[handlerIdx]) {
                    addReturnValueFromScript(rv, hook.program);
                }
            }
            handlerIdx++;
        }
    }
};

// =================================================================
// F-022: Multi-handler return value propagation end-to-end test
// =================================================================
// Finding F-022 (MEDIUM): Multi-handler return value propagation
// end-to-end test. Register multiple handlers for same hook type,
// verify return values propagate correctly through the dispatch chain.
// Last handler's return values are what the caller sees (per production
// sfall_script_hooks.cc:149-153 per-handler reset semantics).

TEST_CASE("F-022: Single handler — return value survives")
{
    retResetTestHooksArray();
    retResetHookInvocations();

    void* prog = reinterpret_cast<void*>(0x100);
    retRegisterTestHook(prog, HOOK_SETGLOBALVAR, 1);

    RetTestScriptHookCall hook(HOOK_SETGLOBALVAR, 1, { TestProgramValue{500}, TestProgramValue{42} });

    std::vector<std::vector<TestProgramValue>> returns = {
        { TestProgramValue{999} }
    };
    hook.callWithReturns(returns);

    CHECK(hook.numReturnValues() == 1);
    CHECK(hook.getReturnValueAt(0).asInt() == 999);
}

TEST_CASE("F-022: Two handlers — last handler return value wins")
{
    retResetTestHooksArray();
    retResetHookInvocations();

    void* prog1 = reinterpret_cast<void*>(0x10);
    void* prog2 = reinterpret_cast<void*>(0x20);

    retRegisterTestHook(prog1, HOOK_SETGLOBALVAR, 1);
    retRegisterTestHook(prog2, HOOK_SETGLOBALVAR, 2);

    RetTestScriptHookCall hook(HOOK_SETGLOBALVAR, 1, { TestProgramValue{500}, TestProgramValue{42} });

    // Handler 1 (higher index, fires FIRST) returns 100
    // Handler 2 (lower index, fires LAST) returns 200
    // Per per-handler reset, only handler 2's return survives
    std::vector<std::vector<TestProgramValue>> returns = {
        { TestProgramValue{100} },  // prog2 (index 1) — highest priority, fires first
        { TestProgramValue{200} }   // prog1 (index 0) — fires last, final override
    };
    hook.callWithReturns(returns);

    CHECK(hook.numReturnValues() == 1);
    CHECK(hook.getReturnValueAt(0).asInt() == 200); // last handler wins
}

TEST_CASE("F-022: Three handlers — no cross-handler leakage")
{
    retResetTestHooksArray();
    retResetHookInvocations();

    void* prog1 = reinterpret_cast<void*>(0xAA);
    void* prog2 = reinterpret_cast<void*>(0xBB);
    void* prog3 = reinterpret_cast<void*>(0xCC);

    retRegisterTestHook(prog1, HOOK_SETGLOBALVAR, 1);
    retRegisterTestHook(prog2, HOOK_SETGLOBALVAR, 2);
    retRegisterTestHook(prog3, HOOK_SETGLOBALVAR, 3);

    RetTestScriptHookCall hook(HOOK_SETGLOBALVAR, 1, { TestProgramValue{500}, TestProgramValue{42} });

    // Handler 1 (highest index, fires first): returns 10
    // Handler 2: returns 20 (overwrites handler 1)
    // Handler 3 (fires last): returns 30 (overwrites handler 2)
    std::vector<std::vector<TestProgramValue>> returns = {
        { TestProgramValue{10} },   // prog3 — fires first
        { TestProgramValue{20} },   // prog2
        { TestProgramValue{30} }    // prog1 — fires last, final value
    };
    hook.callWithReturns(returns);

    CHECK(hook.numReturnValues() == 1);
    CHECK(hook.getReturnValueAt(0).asInt() == 30); // last handler wins
    CHECK(gRetHookInvocations.size() == 3); // all 3 handlers fired
}

TEST_CASE("F-022: Early handler returns nothing, later handler returns value")
{
    retResetTestHooksArray();
    retResetHookInvocations();

    void* prog1 = reinterpret_cast<void*>(0xD0);
    void* prog2 = reinterpret_cast<void*>(0xD1);

    retRegisterTestHook(prog1, HOOK_SETGLOBALVAR, 1);
    retRegisterTestHook(prog2, HOOK_SETGLOBALVAR, 2);

    RetTestScriptHookCall hook(HOOK_SETGLOBALVAR, 1, { TestProgramValue{500}, TestProgramValue{42} });

    // Handler 1 (fires first): returns nothing (empty)
    // Handler 2 (fires last): returns 42
    std::vector<std::vector<TestProgramValue>> returns = {
        {},                           // prog2 — no return values
        { TestProgramValue{42} }      // prog1 — returns 42
    };
    hook.callWithReturns(returns);

    CHECK(hook.numReturnValues() == 1);
    CHECK(hook.getReturnValueAt(0).asInt() == 42);
}

TEST_CASE("F-022: Later handler with fewer returns doesn't leak earlier values")
{
    retResetTestHooksArray();
    retResetHookInvocations();

    void* prog1 = reinterpret_cast<void*>(0xE0);
    void* prog2 = reinterpret_cast<void*>(0xE1);

    retRegisterTestHook(prog1, HOOK_COMBATDAMAGE, 1);
    retRegisterTestHook(prog2, HOOK_COMBATDAMAGE, 2);

    RetTestScriptHookCall hook(HOOK_COMBATDAMAGE, 5, {});

    // Handler 1 (fires first): returns 3 values {10, 20, 30}
    // Handler 2 (fires last): returns 1 value {99}
    // Per-handler reset: handler 2's numRetVals should be 1, not 3+1
    std::vector<std::vector<TestProgramValue>> returns = {
        { TestProgramValue{10}, TestProgramValue{20}, TestProgramValue{30} }, // prog2 — fires first
        { TestProgramValue{99} }                                                // prog1 — fires last
    };
    hook.callWithReturns(returns);

    CHECK(hook.numReturnValues() == 1); // only 1 value from last handler
    CHECK(hook.getReturnValueAt(0).asInt() == 99);
}

TEST_CASE("F-022: All handlers return nothing — caller sees zero returns")
{
    retResetTestHooksArray();
    retResetHookInvocations();

    void* prog1 = reinterpret_cast<void*>(0xF0);
    void* prog2 = reinterpret_cast<void*>(0xF1);

    retRegisterTestHook(prog1, HOOK_SETGLOBALVAR, 1);
    retRegisterTestHook(prog2, HOOK_SETGLOBALVAR, 2);

    RetTestScriptHookCall hook(HOOK_SETGLOBALVAR, 1, { TestProgramValue{500}, TestProgramValue{42} });

    std::vector<std::vector<TestProgramValue>> returns = {
        {},
        {}
    };
    hook.callWithReturns(returns);

    CHECK(hook.numReturnValues() == 0); // no returns from any handler
}

TEST_CASE("F-022: Multi-handler with multi-return hook type (AFTERHITROLL)")
{
    retResetTestHooksArray();
    retResetHookInvocations();

    void* prog1 = reinterpret_cast<void*>(0x01);
    void* prog2 = reinterpret_cast<void*>(0x02);
    void* prog3 = reinterpret_cast<void*>(0x03);

    retRegisterTestHook(prog1, HOOK_AFTERHITROLL, 1);
    retRegisterTestHook(prog2, HOOK_AFTERHITROLL, 2);
    retRegisterTestHook(prog3, HOOK_AFTERHITROLL, 3);

    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});

    // Each handler returns different values; last handler is authoritative
    std::vector<std::vector<TestProgramValue>> returns = {
        { TestProgramValue{1}, TestProgramValue{2}, TestProgramValue{3} },    // prog3 — fires first
        { TestProgramValue{4}, TestProgramValue{5}, TestProgramValue{6} },    // prog2
        { TestProgramValue{7}, TestProgramValue{8}, TestProgramValue{9} }     // prog1 — fires last, wins
    };
    hook.callWithReturns(returns);

    CHECK(hook.numReturnValues() == 3);
    CHECK(hook.getReturnValueAt(0).asInt() == 7);
    CHECK(hook.getReturnValueAt(1).asInt() == 8);
    CHECK(hook.getReturnValueAt(2).asInt() == 9);
}

// =================================================================
// F-024: AFTERHITROLL multi-return-value boundaries
// =================================================================
// Finding F-024 (MEDIUM): AFTERHITROLL multi-return-value boundaries.
// Test all return value combinations for multi-arg hooks.
// Production logic from sfall_script_hooks.cc:872-921:
//   ret0: -99..4 (ROLL_CRITICAL_FAILURE=0..ROLL_CRITICAL_SUCCESS=3)
//   ret1: 0..HIT_LOCATION_COUNT-1 (HIT_LOCATION_COUNT=8)
//   ret2: nullptr → reject override

namespace {
    // Mirror of AFTERHITROLL return value processing
    struct AfterHitRollMirror {
        int roll = 2; // default hit
        int hitLocation = 0; // HIT_LOCATION_UNARMED
        void* defender = nullptr;
        bool defenderOverridden = false;
        int overrideCount = 0;

        void processReturns(RetTestScriptHookCall& hook)
        {
            if (hook.numReturnValues() <= 0) return;

            // ret0: roll override
            int rollOverride = hook.getReturnValueAt(0).asInt();
            if (rollOverride >= ROLL_CRITICAL_FAILURE && rollOverride <= ROLL_CRITICAL_SUCCESS) {
                roll = rollOverride;
                overrideCount++;
            }

            // ret1: hit location override
            if (hook.numReturnValues() > 1) {
                int hitLocationOverride = hook.getReturnValueAt(1).asInt();
                if (hitLocationOverride >= 0 && hitLocationOverride < HIT_LOCATION_COUNT) {
                    hitLocation = hitLocationOverride;
                    overrideCount++;
                }
            }

            // ret2: defender override
            if (hook.numReturnValues() > 2) {
                void* overrideDefender = hook.getReturnValueAt(2).asObject();
                if (overrideDefender != nullptr) {
                    defender = overrideDefender;
                    defenderOverridden = true;
                    overrideCount++;
                }
            }
        }
    };
}

TEST_CASE("F-024: AFTERHITROLL — ret0=critical_miss is valid")
{
    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});
    hook.addReturnValueFromScript({ROLL_CRITICAL_FAILURE}, nullptr);

    AfterHitRollMirror mirror;
    mirror.processReturns(hook);

    CHECK(mirror.roll == ROLL_CRITICAL_FAILURE);
    CHECK(mirror.overrideCount == 1);
}

TEST_CASE("F-024: AFTERHITROLL — ret0=miss is valid")
{
    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});
    hook.addReturnValueFromScript({ROLL_FAILURE}, nullptr);

    AfterHitRollMirror mirror;
    mirror.processReturns(hook);

    CHECK(mirror.roll == ROLL_FAILURE);
}

TEST_CASE("F-024: AFTERHITROLL — ret0=hit is valid")
{
    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});
    hook.addReturnValueFromScript({ROLL_SUCCESS}, nullptr);

    AfterHitRollMirror mirror;
    mirror.processReturns(hook);

    CHECK(mirror.roll == ROLL_SUCCESS);
}

TEST_CASE("F-024: AFTERHITROLL — ret0=critical_hit is valid")
{
    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});
    hook.addReturnValueFromScript({ROLL_CRITICAL_SUCCESS}, nullptr);

    AfterHitRollMirror mirror;
    mirror.processReturns(hook);

    CHECK(mirror.roll == ROLL_CRITICAL_SUCCESS);
}

TEST_CASE("F-024: AFTERHITROLL — ret0=-1 is invalid, roll unchanged")
{
    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});
    hook.addReturnValueFromScript({-1}, nullptr);

    AfterHitRollMirror mirror;
    mirror.processReturns(hook);

    CHECK(mirror.roll == ROLL_SUCCESS); // default unchanged
    CHECK(mirror.overrideCount == 0);
}

TEST_CASE("F-024: AFTERHITROLL — ret0=5 is invalid (beyond ROLL_CRITICAL_SUCCESS=3)")
{
    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});
    hook.addReturnValueFromScript({5}, nullptr);

    AfterHitRollMirror mirror;
    mirror.processReturns(hook);

    CHECK(mirror.roll == ROLL_SUCCESS); // unchanged
    CHECK(mirror.overrideCount == 0);
}

TEST_CASE("F-024: AFTERHITROLL — ret1=0 is valid (first hit location)")
{
    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});
    hook.addReturnValueFromScript({ROLL_SUCCESS}, nullptr);
    hook.addReturnValueFromScript({0}, nullptr);

    AfterHitRollMirror mirror;
    mirror.processReturns(hook);

    CHECK(mirror.hitLocation == 0);
}

TEST_CASE("F-024: AFTERHITROLL — ret1=HIT_LOCATION_COUNT-1 is valid (last)")
{
    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});
    hook.addReturnValueFromScript({ROLL_SUCCESS}, nullptr);
    hook.addReturnValueFromScript({HIT_LOCATION_COUNT - 1}, nullptr);

    AfterHitRollMirror mirror;
    mirror.processReturns(hook);

    CHECK(mirror.hitLocation == HIT_LOCATION_COUNT - 1);
}

TEST_CASE("F-024: AFTERHITROLL — ret1=-1 is invalid (OOB low)")
{
    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});
    hook.addReturnValueFromScript({ROLL_SUCCESS}, nullptr);
    hook.addReturnValueFromScript({-1}, nullptr);

    AfterHitRollMirror mirror;
    mirror.processReturns(hook);

    CHECK(mirror.hitLocation == 0); // unchanged
}

TEST_CASE("F-024: AFTERHITROLL — ret1=HIT_LOCATION_COUNT is invalid (OOB high)")
{
    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});
    hook.addReturnValueFromScript({ROLL_SUCCESS}, nullptr);
    hook.addReturnValueFromScript({HIT_LOCATION_COUNT}, nullptr);

    AfterHitRollMirror mirror;
    mirror.processReturns(hook);

    CHECK(mirror.hitLocation == 0); // unchanged
}

TEST_CASE("F-024: AFTERHITROLL — ret1=100 is invalid (far OOB)")
{
    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});
    hook.addReturnValueFromScript({ROLL_SUCCESS}, nullptr);
    hook.addReturnValueFromScript({100}, nullptr);

    AfterHitRollMirror mirror;
    mirror.processReturns(hook);

    CHECK(mirror.hitLocation == 0); // unchanged
}

TEST_CASE("F-024: AFTERHITROLL — ret2=nullptr is rejected")
{
    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});
    hook.addReturnValueFromScript({ROLL_SUCCESS}, nullptr);
    hook.addReturnValueFromScript({0}, nullptr);
    TestProgramValue nullObj;
    nullObj.obj = nullptr;
    hook.addReturnValueFromScript(nullObj, nullptr);

    AfterHitRollMirror mirror;
    mirror.processReturns(hook);

    CHECK_FALSE(mirror.defenderOverridden);
    CHECK(mirror.defender == nullptr);
}

TEST_CASE("F-024: AFTERHITROLL — ret2=non-null overrides defender")
{
    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});
    hook.addReturnValueFromScript({ROLL_SUCCESS}, nullptr);
    hook.addReturnValueFromScript({0}, nullptr);

    void* fakeObj = reinterpret_cast<void*>(0xFEED);
    TestProgramValue objVal;
    objVal.obj = fakeObj;
    hook.addReturnValueFromScript(objVal, nullptr);

    AfterHitRollMirror mirror;
    mirror.processReturns(hook);

    CHECK(mirror.defenderOverridden);
    CHECK(mirror.defender == fakeObj);
}

TEST_CASE("F-024: AFTERHITROLL — all three returns valid together")
{
    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});
    void* fakeTarget = reinterpret_cast<void*>(0xBEEF);

    hook.addReturnValueFromScript({ROLL_CRITICAL_SUCCESS}, nullptr);
    hook.addReturnValueFromScript({3}, nullptr);   // HIT_LOCATION_TORSO
    TestProgramValue objVal;
    objVal.obj = fakeTarget;
    hook.addReturnValueFromScript(objVal, nullptr);

    AfterHitRollMirror mirror;
    mirror.processReturns(hook);

    CHECK(mirror.roll == ROLL_CRITICAL_SUCCESS);
    CHECK(mirror.hitLocation == 3);
    CHECK(mirror.defenderOverridden);
    CHECK(mirror.defender == fakeTarget);
    CHECK(mirror.overrideCount == 3);
}

TEST_CASE("F-024: AFTERHITROLL — ret0 valid, ret1 invalid, ret2 valid")
{
    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});
    void* fakeTarget = reinterpret_cast<void*>(0xC0DE);

    hook.addReturnValueFromScript({ROLL_FAILURE}, nullptr);
    hook.addReturnValueFromScript({-5}, nullptr);   // invalid hit location
    TestProgramValue objVal;
    objVal.obj = fakeTarget;
    hook.addReturnValueFromScript(objVal, nullptr);

    AfterHitRollMirror mirror;
    mirror.processReturns(hook);

    CHECK(mirror.roll == ROLL_FAILURE);    // ret0 applied
    CHECK(mirror.hitLocation == 0);        // ret1 rejected, default kept
    CHECK(mirror.defenderOverridden);      // ret2 applied
    CHECK(mirror.defender == fakeTarget);
    CHECK(mirror.overrideCount == 2);      // only ret0 + ret2 applied
}

TEST_CASE("F-024: AFTERHITROLL — no return values keeps all defaults")
{
    RetTestScriptHookCall hook(HOOK_AFTERHITROLL, 3, {});

    AfterHitRollMirror mirror;
    mirror.processReturns(hook);

    CHECK(mirror.roll == ROLL_SUCCESS); // default
    CHECK(mirror.hitLocation == 0);     // default
    CHECK_FALSE(mirror.defenderOverridden);
    CHECK(mirror.overrideCount == 0);
}

// =================================================================
// F-029: getReturnValueAt bounds test
// =================================================================
// Finding F-029 (MEDIUM): getReturnValueAt() assert-only bounds.
// Production uses assert(idx >= 0 && idx < _numRetVals), so OOB
// access is undefined in release builds. Test boundary conditions.

TEST_CASE("F-029: getReturnValueAt with zero return values — index 0 is OOB")
{
    RetTestScriptHookCall hook(HOOK_SETGLOBALVAR, 1, {});

    // In production: assert fires. In test mirror: returns default.
    TestProgramValue result = hook.getReturnValueAt(0);
    CHECK(result.asInt() == 0); // default value returned
}

TEST_CASE("F-029: getReturnValueAt with negative index")
{
    RetTestScriptHookCall hook(HOOK_SETGLOBALVAR, 1, {});
    hook.addReturnValueFromScript({42}, nullptr);

    TestProgramValue result = hook.getReturnValueAt(-1);
    CHECK(result.asInt() == 0); // default value for OOB
}

TEST_CASE("F-029: getReturnValueAt at valid boundary index 0")
{
    RetTestScriptHookCall hook(HOOK_SETGLOBALVAR, 1, {});
    hook.addReturnValueFromScript({42}, nullptr);

    TestProgramValue result = hook.getReturnValueAt(0);
    CHECK(result.asInt() == 42);
}

TEST_CASE("F-029: getReturnValueAt at valid boundary index numRetVals-1")
{
    RetTestScriptHookCall hook(HOOK_COMBATDAMAGE, 5, {});

    for (int i = 0; i < 5; i++) {
        hook.addReturnValueFromScript({i * 10}, nullptr);
    }

    TestProgramValue result = hook.getReturnValueAt(4); // index = numRetVals-1
    CHECK(result.asInt() == 40);
}

TEST_CASE("F-029: getReturnValueAt at index == numRetVals (OOB)")
{
    RetTestScriptHookCall hook(HOOK_COMBATDAMAGE, 5, {});

    for (int i = 0; i < 3; i++) {
        hook.addReturnValueFromScript({i * 10}, nullptr);
    }

    TestProgramValue result = hook.getReturnValueAt(3); // numRetVals == 3
    CHECK(result.asInt() == 0); // OOB returns default
}

TEST_CASE("F-029: getReturnValueAt far OOB index")
{
    RetTestScriptHookCall hook(HOOK_SETGLOBALVAR, 1, {});
    hook.addReturnValueFromScript({42}, nullptr);

    TestProgramValue result = hook.getReturnValueAt(100);
    CHECK(result.asInt() == 0); // far OOB returns default
}

TEST_CASE("F-029: getReturnValueAt with max return values filled")
{
    RetTestScriptHookCall hook(HOOK_COMBATDAMAGE, HOOKS_MAX_RETURN_VALUES, {});

    // Fill all 8 slots
    for (int i = 0; i < HOOKS_MAX_RETURN_VALUES; i++) {
        hook.addReturnValueFromScript({i + 1}, nullptr);
    }

    CHECK(hook.numReturnValues() == HOOKS_MAX_RETURN_VALUES);

    // All 8 indices are valid
    for (int i = 0; i < HOOKS_MAX_RETURN_VALUES; i++) {
        CHECK(hook.getReturnValueAt(i).asInt() == i + 1);
    }

    // Index 8 is OOB
    TestProgramValue result = hook.getReturnValueAt(HOOKS_MAX_RETURN_VALUES);
    CHECK(result.asInt() == 0);
}

TEST_CASE("F-029: getReturnValueAt indices are zero-based")
{
    RetTestScriptHookCall hook(HOOK_SETGLOBALVAR, 1, {});
    hook.addReturnValueFromScript({7}, nullptr);

    CHECK(hook.getReturnValueAt(0).asInt() == 7);
}
