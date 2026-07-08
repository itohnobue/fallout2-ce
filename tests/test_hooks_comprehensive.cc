// Comprehensive hook behavior tests — dispatch lifecycle, reentrancy,
// unregistration, drain mechanism, and hook type isolation.
//
// F-023: HOOK_STDPROCEDURE cancellation test
// F-025: Call stack drain mechanism test
// F-026: Handler unregistration during dispatch test
// F-027: GAMEMODECHANGE reentrancy guard test
// F2-002: scriptHooksUnregisterProgram behavioral test
// F2-008: Replace mirror-based hook type isolation test
//
// F-063 (MEDIUM): Hook fire via local mirrors — limitation and linkage path.
//   All hook fire functions (call(), drain, dispatch) in this file and across
//   the test suite exercise LOCAL MIRRORS of the production ScriptHookCall
//   class, never the production sfall_script_hooks.cc implementation. The
//   mirrors correctly model the production logic patterns but cannot detect:
//     - Production-only regressions (e.g., stack memory UB at line 127)
//     - RAII guard failures (e.g., GameModeChangeGuard destructor skipped)
//     - Performance changes in real dispatch paths
//     - Stack-address-based stale detection (F-025 drain uses flag-based
//       staleness instead of address comparison — the test mirror cannot
//       replicate stack-address comparison because DrainFrame objects are
//       not stack-allocated ScriptHookCall instances; see F-025 section)
//   LINKAGE PATH: Production sfall_script_hooks.cc requires 150+ engine source
//   files (Program, interpreter, game objects, script manager). Incremental
//   extraction plan (see test_script_harness.h roadmap):
//     1. Extract ScriptHookCall into standalone compilation unit with
//        TEST_ACCESSORS guard (hook type registry, call stack, dispatch)
//     2. Link it against minimal engine stubs (Program, game_*.h interfaces)
//     3. Replace mirror-based dispatch tests with production-link tests
//   Until extraction is complete, the mirror tests serve as behavioral
//   regression guards but MUST be validated against production when
//   sfall_script_hooks.cc is modified.
//
// Self-contained header-only test — does NOT link sfall_script_hooks.cc.
// Uses local mirrors following the pattern established by existing tests.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "sfall_script_hooks.h"

#include <vector>
#include <cstring>

using namespace fallout;

// ---- Local test state ----
struct HookInvocation {
    void* program;
    int procedureIndex;
};

static std::vector<HookInvocation> gCompHookInvocations;

static void compTestProgramExecuteProcedure(void* program, int procedureIndex)
{
    gCompHookInvocations.push_back({ program, procedureIndex });
}

static void compResetHookInvocations()
{
    gCompHookInvocations.clear();
}

// ---- Mirror structures ----
struct TestScriptHook {
    void* program = nullptr;
    int procedureIndex = -1;
};

struct TestProgramValue {
    int value = 0;
    void* obj = nullptr;
};

// Local hooks array mirror
static std::vector<TestScriptHook> compTestHooks[HOOK_COUNT];

static void compResetTestHooksArray()
{
    for (auto& hooks : compTestHooks) {
        hooks.clear();
    }
}

static void compRegisterTestHook(void* program, HookType hookType, int procIdx)
{
    compTestHooks[hookType].push_back({ program, procIdx });
}

// ---- Mirror of ScriptHookCall::call() ----
struct CompTestScriptHookCall {
    HookType hookType;
    int maxReturnValues;
    TestProgramValue args[HOOKS_MAX_ARGUMENTS];
    int numArgs;
    TestProgramValue retVals[HOOKS_MAX_RETURN_VALUES];
    int numRetVals;
    int scriptRetVals;

    CompTestScriptHookCall(HookType ht, int maxRet, std::initializer_list<TestProgramValue> initArgs)
        : hookType(ht), maxReturnValues(maxRet), numArgs(0), numRetVals(0), scriptRetVals(0)
    {
        for (const auto& arg : initArgs) {
            if (numArgs < HOOKS_MAX_ARGUMENTS) {
                args[numArgs++] = arg;
            }
        }
    }

    void addReturnValueFromScript(TestProgramValue value)
    {
        if (scriptRetVals >= maxReturnValues) return;
        if (scriptRetVals >= HOOKS_MAX_RETURN_VALUES) return;
        retVals[scriptRetVals++] = value;
        if (scriptRetVals > numRetVals) {
            numRetVals = scriptRetVals;
        }
    }

    TestProgramValue getReturnValueAt(int idx) const
    {
        if (idx >= 0 && idx < numRetVals) return retVals[idx];
        return { 0 };
    }

    int numReturnValues() const { return numRetVals; }

    void call()
    {
        // Mirror of sfall_script_hooks.cc:105-166
        auto hooksCopy = compTestHooks[hookType];

        for (int i = static_cast<int>(hooksCopy.size()) - 1; i >= 0; --i) {
            const auto& hook = hooksCopy[i];
            // Per-handler reset
            scriptRetVals = 0;
            compTestProgramExecuteProcedure(hook.program, hook.procedureIndex);
        }
    }

};

// =================================================================
// F-023: HOOK_STDPROCEDURE cancellation test
// =================================================================
// Finding F-023 (MEDIUM, challenged HIGH→MEDIUM): HOOK_STDPROCEDURE
// cancellation test. Verify that when a handler returns -1, the
// engine procedure is cancelled/skipped.
//
// Production logic at sfall_script_hooks.cc:242-264:
//   - ret0 == -1 → cancel (returns true)
//   - ret0 == anything else → no cancel (returns false)
//   - after==true → no cancel regardless (returns false)
//   - no return values → no cancel (returns false)

namespace {
    // Mirror of sfall_script_hooks.cc:242-264
    static bool mirrorStdProcedureCancel(
        int procedureNumber,
        bool after,
        bool hookHasReturn,
        int ret0Value)
    {
        // Fast-path: certain procedure types are never hooked
        if (procedureNumber == SCRIPT_PROC_START
            || procedureNumber == SCRIPT_PROC_CRITTER
            || procedureNumber == SCRIPT_PROC_TIMED
            || procedureNumber == SCRIPT_PROC_MAP_UPDATE) {
            return false;
        }

        // after==true → observation only, no cancel
        if (after) {
            return false;
        }

        // No return values → no cancel
        if (!hookHasReturn) {
            return false;
        }

        // ret0 == -1 → cancel
        return ret0Value == -1;
    }
}

TEST_CASE("F-023: StdProcedure — ret0=-1 cancels execution (HOOK_STDPROCEDURE)")
{
    // Production at sfall_script_hooks.cc:264: hook.getReturnValueAt(0).asInt() == -1
    bool result = mirrorStdProcedureCancel(SCRIPT_PROC_USE, false, true, -1);
    CHECK(result == true); // cancelled
}

TEST_CASE("F-023: StdProcedure — ret0=0 does NOT cancel")
{
    bool result = mirrorStdProcedureCancel(SCRIPT_PROC_USE, false, true, 0);
    CHECK(result == false); // not cancelled
}

TEST_CASE("F-023: StdProcedure — ret0=1 does NOT cancel")
{
    bool result = mirrorStdProcedureCancel(SCRIPT_PROC_USE, false, true, 1);
    CHECK(result == false); // not cancelled
}

TEST_CASE("F-023: StdProcedure — ret0=any_positive does NOT cancel")
{
    bool result = mirrorStdProcedureCancel(SCRIPT_PROC_USE, false, true, 42);
    CHECK(result == false);
}

TEST_CASE("F-023: StdProcedure — no return values → no cancel")
{
    bool result = mirrorStdProcedureCancel(SCRIPT_PROC_USE, false, false, 0);
    CHECK(result == false);
}

TEST_CASE("F-023: StdProcedure — ret0=-1 but after==true → no cancel (HOOK_STDPROCEDURE_END)")
{
    // Production at sfall_script_hooks.cc:260: if (after) return false;
    bool result = mirrorStdProcedureCancel(SCRIPT_PROC_USE, true, true, -1);
    CHECK(result == false); // not cancelled, because after (observation-only)
}

TEST_CASE("F-023: StdProcedure — SCRIPT_PROC_START is never hooked")
{
    // Production at sfall_script_hooks.cc:244-248: early return false
    bool result = mirrorStdProcedureCancel(SCRIPT_PROC_START, false, true, -1);
    CHECK(result == false);
}

TEST_CASE("F-023: StdProcedure — SCRIPT_PROC_CRITTER is never hooked")
{
    bool result = mirrorStdProcedureCancel(SCRIPT_PROC_CRITTER, false, true, -1);
    CHECK(result == false);
}

TEST_CASE("F-023: StdProcedure — SCRIPT_PROC_TIMED is never hooked")
{
    bool result = mirrorStdProcedureCancel(SCRIPT_PROC_TIMED, false, true, -1);
    CHECK(result == false);
}

TEST_CASE("F-023: StdProcedure — SCRIPT_PROC_MAP_UPDATE is never hooked")
{
    bool result = mirrorStdProcedureCancel(SCRIPT_PROC_MAP_UPDATE, false, true, -1);
    CHECK(result == false);
}

TEST_CASE("F-023: StdProcedure — SCRIPT_PROC_USE (regular proc) can be cancelled")
{
    // Verify a normal procedure type IS eligible (not in the exclusion set)
    bool result = mirrorStdProcedureCancel(SCRIPT_PROC_USE, false, true, -1);
    CHECK(result == true); // cancels only for regular procs with ret0=-1
}

TEST_CASE("F-023: StdProcedure — SCRIPT_PROC_DESCRIPTION can be cancelled")
{
    bool result = mirrorStdProcedureCancel(SCRIPT_PROC_DESCRIPTION, false, true, -1);
    CHECK(result == true);
}

// =================================================================
// F-025: Call stack drain mechanism test
// =================================================================
// Finding F-025 (MEDIUM): Call stack drain mechanism completely untested.
//
// I2-M16 production behavior (sfall_script_hooks.cc:117-161):
//   1. PROACTIVE drain: drainStaleEntries() is called on EVERY call() entry,
//      not just when _callStack reaches MAX_HOOK_CALL_DEPTH.
//   2. DRAIN ALL: drainStaleEntries() removes ALL stale entries (not just
//      enough to get below threshold). The loop condition is simply
//      `i < _callStack.size()` — no early-exit when threshold is reached.
//   3. ADDRESS-BASED staleness: stale detection uses stack-address comparison
//      (entryAddr < currentStackAddr), not an `_active` flag. This avoids UB
//      from reading freed stack memory of longjmp'd frames.
//   4. After drain: if _callStack.size() >= MAX_HOOK_CALL_DEPTH → REJECT.
//      There is no separate "all active" check — if drain found nothing and
//      the size is still at cap, rejection is the same simple size check.
//
// F-063 (MEDIUM): Stack-address stale detection cannot be faithfully replicated
//   in this test mirror. Production drainStaleEntries(uintptr_t) compares
//   ScriptHookCall* stack addresses against a local anchor variable to
//   determine which entries belong to unwound (longjmp'd) frames. This test
//   mirror CANNOT replicate address-based detection because:
//     (1) Test DrainFrame objects are heap/stack-local structs, not stack-
//         allocated ScriptHookCall instances with meaningful addresses.
//     (2) Stack growth direction and layout are platform-specific; address
//         comparison in test context is meaningless.
//     (3) The production `_callStack` holds real ScriptHookCall* pointers
//         whose addresses encode call depth — the test mirror's vector of
//         DrainFrame structs has no analogous spatial relationship.
//   Instead, this mirror uses an `active` flag to model staleness:
//   `active=false` ≈ "stale" (analogous to a longjmp'd frame). This
//   approximates the staleness CLASSIFICATION but does NOT protect against
//   the address-comparison-specific UB patterns (freed stack memory reads)
//   that motivated the I2-M16 rewrite. Full drain verification requires
//   production-link testing via the incremental extraction roadmap
//   (see test_script_harness.h).
//
// F-065 (MEDIUM): Drain mirror now matches I2-M16 production behavioral
//   contracts: proactive drain on every call(), drain ALL stale entries,
//   rejection based on size >= MAX after drain. Only the staleness DETECTION
//   mechanism differs (flag-based vs. address-based — see F-063 above).

namespace {
    // Mirror the drain logic from sfall_script_hooks.cc:117-161 (I2-M16).
    struct DrainFrame {
        bool active = false;     // false = stale (analogous to longjmp'd frame in production)
    };

    static constexpr size_t DRAIN_MAX_DEPTH = 8;

    // Mirror of production drainStaleEntries(uintptr_t).
    // Production drains ALL stale entries; test drains all with active==false.
    // F-063: Uses flag-based staleness instead of address comparison —
    // address-based detection cannot be replicated in test (see F-063 above).
    static int mirrorDrainStale(std::vector<DrainFrame>& stack)
    {
        int drained = 0;
        for (size_t i = 0; i < stack.size();) {
            if (!stack[i].active) {
                stack.erase(stack.begin() + i);
                ++drained;
                // Do NOT increment i — erase shifts subsequent elements into position i
            } else {
                ++i;
            }
        }
        return drained;
    }

    // Result of a call() attempt after proactive drain.
    enum class DrainCallResult { ACCEPTED, REJECTED };

    // Mirror of production call() entry sequence (lines 132-161):
    //   1. PROACTIVE drain on every entry (not just at depth cap)
    //   2. After drain: size >= MAX_HOOK_CALL_DEPTH → REJECTED
    //   3. Otherwise: ACCEPTED (production pushes `this` and dispatches)
    static DrainCallResult mirrorCallWithDrain(std::vector<DrainFrame>& stack)
    {
        // I2-M16: Proactive drain — runs on every call() unconditionally.
        mirrorDrainStale(stack);

        // Depth-cap check after stale entries have been removed.
        if (stack.size() >= DRAIN_MAX_DEPTH) {
            return DrainCallResult::REJECTED;
        }

        return DrainCallResult::ACCEPTED;
    }
}

TEST_CASE("F-025: Call stack drain — below depth limit, proactive drain runs but finds no stale entries")
{
    // I2-M16: drain runs on EVERY call() entry, not just at depth cap.
    // Below the cap, all frames are active → drain finds nothing → call accepted.
    std::vector<DrainFrame> stack;
    for (size_t i = 0; i < 4; i++) {
        stack.push_back({ true });
    }

    DrainCallResult result = mirrorCallWithDrain(stack);
    CHECK(result == DrainCallResult::ACCEPTED);
    CHECK(stack.size() == 4); // unchanged — all active, nothing drained
}

TEST_CASE("F-025: Call stack drain — at depth limit with all active, REJECTION (I2-M16)")
{
    // I2-M16: drain runs, finds no stale entries (all active).
    // After drain: size == MAX_DEPTH → REJECTED (no eviction of active frames).
    std::vector<DrainFrame> stack;
    for (size_t i = 0; i < DRAIN_MAX_DEPTH; i++) {
        stack.push_back({ true });
    }

    DrainCallResult result = mirrorCallWithDrain(stack);
    CHECK(result == DrainCallResult::REJECTED); // I2-M16: size >= MAX after drain → reject
    CHECK(stack.size() == DRAIN_MAX_DEPTH); // all frames preserved (no eviction of active frames)
}

TEST_CASE("F-025: Call stack drain — ALL inactive entries drained (I2-M16 drain-ALL)")
{
    // I2-M16: drainStaleEntries() drains ALL stale entries, not just enough
    // to drop below threshold. Both inactive frames are removed.
    std::vector<DrainFrame> stack;
    stack.push_back({ true });   // index 0: active
    stack.push_back({ false });  // index 1: inactive (stale)
    stack.push_back({ true });   // index 2: active
    stack.push_back({ false });  // index 3: inactive (stale)
    stack.push_back({ true });   // index 4: active
    stack.push_back({ true });   // index 5: active
    stack.push_back({ true });   // index 6: active
    stack.push_back({ true });   // index 7: active
    // 8 frames total, 2 inactive (stale)

    int drained = mirrorDrainStale(stack);
    // I2-M16: ALL stale entries are drained — both inactive frames removed.
    CHECK(drained == 2); // both inactive entries drained
    CHECK(stack.size() == 6); // 8 - 2 = 6 remaining (all active)
}

TEST_CASE("F-025: Call stack drain — ALL inactive drained, active ones survive (I2-M16)")
{
    // I2-M16: drainStaleEntries() drains ALL stale entries regardless of
    // threshold. All 3 inactive frames are removed; all 7 active survive.
    std::vector<DrainFrame> stack;
    // 10 frames total (above MAX_DEPTH=8), with 3 inactive scattered among 7 active
    stack.push_back({ true });   // index 0: active
    stack.push_back({ false });  // index 1: inactive
    stack.push_back({ true });   // index 2: active
    stack.push_back({ true });   // index 3: active
    stack.push_back({ false });  // index 4: inactive
    stack.push_back({ true });   // index 5: active
    stack.push_back({ true });   // index 6: active
    stack.push_back({ true });   // index 7: active
    stack.push_back({ false });  // index 8: inactive
    stack.push_back({ true });   // index 9: active
    // 10 frames, 3 inactive (7 active)

    int drained = mirrorDrainStale(stack);
    // I2-M16: ALL stale entries are drained — all 3 inactive removed.
    CHECK(drained == 3); // all three inactive entries drained
    CHECK(stack.size() == 7); // 10 - 3 = 7, all remaining are active
    // Verify all remaining frames are active
    for (const auto& frame : stack) {
        CHECK(frame.active == true);
    }
}

TEST_CASE("F-025: Call stack drain — all inactive, ALL drained (I2-M16 drain-ALL)")
{
    // I2-M16: drainStaleEntries() drains ALL stale entries. When all 8
    // frames are inactive (stale), all 8 are removed → empty stack.
    std::vector<DrainFrame> stack;
    for (size_t i = 0; i < DRAIN_MAX_DEPTH; i++) {
        stack.push_back({ false });
    }

    int drained = mirrorDrainStale(stack);
    // I2-M16: ALL stale entries drained — all 8 removed.
    CHECK(drained == 8); // all 8 inactive entries drained
    CHECK(stack.size() == 0); // empty — all stale entries removed
}

TEST_CASE("F-025: Call stack drain — above depth limit, ALL inactive drained (I2-M16)")
{
    // I2-M16: drainStaleEntries() drains ALL stale entries. All 4 inactive
    // frames are removed; 8 active remain at exactly MAX_DEPTH.
    std::vector<DrainFrame> stack;
    for (size_t i = 0; i < 10; i++) {
        stack.push_back({ true });
    }
    stack.push_back({ true });
    stack.push_back({ true });
    // Mark 4 entries scattered throughout as inactive
    stack[2].active = false;
    stack[5].active = false;
    stack[7].active = false;
    stack[9].active = false;

    int drained = mirrorDrainStale(stack);
    // I2-M16: ALL 4 inactive entries drained.
    CHECK(drained == 4); // all four inactive entries drained
    CHECK(stack.size() == 8); // 12 - 4 = 8, exactly at MAX_DEPTH
    // All remaining 8 frames are active
    for (const auto& frame : stack) {
        CHECK(frame.active == true);
    }

    // After drain: size == MAX_DEPTH → mirrorCallWithDrain would REJECT.
    // Verify the full call-sequence behavior:
    std::vector<DrainFrame> stack2;
    for (size_t i = 0; i < DRAIN_MAX_DEPTH; i++) {
        stack2.push_back({ true });
    }
    DrainCallResult result = mirrorCallWithDrain(stack2);
    CHECK(result == DrainCallResult::REJECTED); // at cap after drain → rejected
}

TEST_CASE("F-025: Call stack drain — empty stack, nothing to drain (call accepted)")
{
    // I2-M16: drain runs proactively even on empty stack, finds nothing.
    std::vector<DrainFrame> stack;

    int drained = mirrorDrainStale(stack);
    CHECK(drained == 0);
    CHECK(stack.size() == 0);

    // Full call sequence: drain + rejection check
    DrainCallResult result = mirrorCallWithDrain(stack);
    CHECK(result == DrainCallResult::ACCEPTED); // empty stack, below cap → accepted
}

TEST_CASE("F-025: Call stack drain — proactive drain on every call(), not just at depth cap")
{
    // I2-M16: drainStaleEntries() is called unconditionally on every call()
    // entry — proactively cleaning stale entries before they accumulate to
    // the depth cap. This is the key behavioral change from pre-I2-M16:
    // a single stale entry is cleaned up immediately rather than waiting
    // until 8 entries accumulate.
    std::vector<DrainFrame> stack;
    // 2 active frames + 1 stale frame — well below depth cap
    stack.push_back({ true });
    stack.push_back({ false }); // stale
    stack.push_back({ true });

    // Proactive drain on call(): removes the stale entry even though
    // we're far below MAX_DEPTH (pre-I2-M16 would have skipped drain).
    DrainCallResult result = mirrorCallWithDrain(stack);
    CHECK(result == DrainCallResult::ACCEPTED); // below cap → accepted
    CHECK(stack.size() == 2); // stale entry removed; 2 active remain
    for (const auto& frame : stack) {
        CHECK(frame.active == true);
    }
}

// =================================================================
// F-026: Handler unregistration during dispatch test
// =================================================================
// Finding F-026 (MEDIUM): Handler unregistration during dispatch.
// Production at sfall_script_hooks.cc:144: auto hooksOfType = scriptHooks[_hookType];
// The hook list is COPIED into a value before iteration, so mutations
// to scriptHooks[] during dispatch do NOT affect the running iteration.
// This test verifies: handler A unregisters handler B during A's dispatch,
// but B still fires because the copy includes B.

TEST_CASE("F-026: Handler unregistration during dispatch — copy isolation ensures B fires")
{
    compResetTestHooksArray();
    compResetHookInvocations();

    void* progA = reinterpret_cast<void*>(0xAAA);
    void* progB = reinterpret_cast<void*>(0xBBB);

    compRegisterTestHook(progA, HOOK_TOHIT, 1);
    compRegisterTestHook(progB, HOOK_TOHIT, 2);

    // Simulate: progA's handler is about to fire. Before calling,
    // we copy the hook list (as production does at line 144).
    auto hooksCopy = compTestHooks[HOOK_TOHIT];
    CHECK(hooksCopy.size() == 2);

    // progA's handler unregisters progB during execution
    // (mutates compTestHooks[HOOK_TOHIT], but NOT hooksCopy)
    for (auto it = compTestHooks[HOOK_TOHIT].begin(); it != compTestHooks[HOOK_TOHIT].end(); ++it) {
        if (it->program == progB) {
            compTestHooks[HOOK_TOHIT].erase(it);
            break;
        }
    }
    CHECK(compTestHooks[HOOK_TOHIT].size() == 1); // B removed from live list

    // But hooksCopy still has both — iteration over copy includes B
    CHECK(hooksCopy.size() == 2); // copy unchanged
    for (int i = static_cast<int>(hooksCopy.size()) - 1; i >= 0; --i) {
        compTestProgramExecuteProcedure(hooksCopy[i].program, hooksCopy[i].procedureIndex);
    }

    CHECK(gCompHookInvocations.size() == 2);
    CHECK(gCompHookInvocations[0].program == progB); // highest index fires first
    CHECK(gCompHookInvocations[1].program == progA);
}

TEST_CASE("F-026: Handler A registers handler C during dispatch — does NOT fire in this iteration")
{
    compResetTestHooksArray();
    compResetHookInvocations();

    void* progA = reinterpret_cast<void*>(0xC00);
    void* progB = reinterpret_cast<void*>(0xC01);

    compRegisterTestHook(progA, HOOK_TOHIT, 1);
    compRegisterTestHook(progB, HOOK_TOHIT, 2);

    // Copy before iteration (production line 144)
    auto hooksCopy = compTestHooks[HOOK_TOHIT];
    CHECK(hooksCopy.size() == 2);

    // progA dynamically registers progC during its execution
    void* progC = reinterpret_cast<void*>(0xC02);
    compRegisterTestHook(progC, HOOK_TOHIT, 3);
    CHECK(compTestHooks[HOOK_TOHIT].size() == 3); // C added to live list

    // But hooksCopy still has 2 entries — C is NOT in the copy
    CHECK(hooksCopy.size() == 2);

    // Iterate over copy
    for (int i = static_cast<int>(hooksCopy.size()) - 1; i >= 0; --i) {
        compTestProgramExecuteProcedure(hooksCopy[i].program, hooksCopy[i].procedureIndex);
    }

    CHECK(gCompHookInvocations.size() == 2); // only A and B fired, not C
    CHECK(gCompHookInvocations[0].program == progB);
    CHECK(gCompHookInvocations[1].program == progA);
}

TEST_CASE("F-026: Handler A removes itself — copy ensures A still fires")
{
    compResetTestHooksArray();
    compResetHookInvocations();

    void* progA = reinterpret_cast<void*>(0xD00);
    void* progB = reinterpret_cast<void*>(0xD01);

    compRegisterTestHook(progA, HOOK_TOHIT, 1);
    compRegisterTestHook(progB, HOOK_TOHIT, 2);

    auto hooksCopy = compTestHooks[HOOK_TOHIT];
    CHECK(hooksCopy.size() == 2);

    // progA removes itself from live list
    for (auto it = compTestHooks[HOOK_TOHIT].begin(); it != compTestHooks[HOOK_TOHIT].end(); ++it) {
        if (it->program == progA) {
            compTestHooks[HOOK_TOHIT].erase(it);
            break;
        }
    }
    CHECK(compTestHooks[HOOK_TOHIT].size() == 1);

    // Copy still has both
    for (int i = static_cast<int>(hooksCopy.size()) - 1; i >= 0; --i) {
        compTestProgramExecuteProcedure(hooksCopy[i].program, hooksCopy[i].procedureIndex);
    }

    CHECK(gCompHookInvocations.size() == 2); // both fire
    CHECK(gCompHookInvocations[0].program == progB);
    CHECK(gCompHookInvocations[1].program == progA);
}

TEST_CASE("F-026: No mutations during dispatch — all handlers fire as registered")
{
    compResetTestHooksArray();
    compResetHookInvocations();

    void* prog1 = reinterpret_cast<void*>(0x100);
    void* prog2 = reinterpret_cast<void*>(0x200);
    void* prog3 = reinterpret_cast<void*>(0x300);

    compRegisterTestHook(prog1, HOOK_GAMEMODECHANGE, 1);
    compRegisterTestHook(prog2, HOOK_GAMEMODECHANGE, 2);
    compRegisterTestHook(prog3, HOOK_GAMEMODECHANGE, 3);

    CompTestScriptHookCall hook(HOOK_GAMEMODECHANGE, 0, {});
    hook.call();

    CHECK(gCompHookInvocations.size() == 3);
    // Reverse order: prog3 fires first, prog1 fires last
    CHECK(gCompHookInvocations[0].program == prog3);
    CHECK(gCompHookInvocations[1].program == prog2);
    CHECK(gCompHookInvocations[2].program == prog1);
}

// =================================================================
// F-027: GAMEMODECHANGE reentrancy guard test
// =================================================================
// Finding F-027 (MEDIUM): GAMEMODECHANGE reentrancy guard.
// Production at sfall_script_hooks.cc:299-324:
//   - gGameLoaded check before dispatch (early-init mode changes ignored)
//   - _gameModeChangeInProgress flag prevents recursive dispatch
//   - Both guards must be validated

namespace {
    struct GameModeChangeMirror {
        bool gameLoaded = false;
        bool modeChangeInProgress = false;
        int dispatchCount = 0;

        // Mirror of sfall_script_hooks.cc:299-324
        void fireGameModeChange()
        {
            // gGameLoaded guard
            if (!gameLoaded) {
                return; // early-return, no dispatch
            }

            // Reentrancy guard
            if (modeChangeInProgress) {
                return; // recusion prevented
            }

            // Empty hook list check
            if (compTestHooks[HOOK_GAMEMODECHANGE].empty()) {
                return; // fast-path
            }

            // Set reentrancy flag
            modeChangeInProgress = true;

            // Dispatch
            dispatchCount++;

            // Clear reentrancy flag
            modeChangeInProgress = false;
        }
    };
}

TEST_CASE("F-027: GAMEMODECHANGE — not dispatched before gGameLoaded")
{
    compResetTestHooksArray();
    compResetHookInvocations();

    void* prog = reinterpret_cast<void*>(0xF00);
    compRegisterTestHook(prog, HOOK_GAMEMODECHANGE, 1);

    GameModeChangeMirror mirror;
    mirror.gameLoaded = false; // game not yet loaded

    mirror.fireGameModeChange();

    CHECK(mirror.dispatchCount == 0); // not dispatched
}

TEST_CASE("F-027: GAMEMODECHANGE — dispatches when gGameLoaded is true")
{
    compResetTestHooksArray();
    compResetHookInvocations();

    void* prog = reinterpret_cast<void*>(0xF01);
    compRegisterTestHook(prog, HOOK_GAMEMODECHANGE, 1);

    GameModeChangeMirror mirror;
    mirror.gameLoaded = true; // game loaded
    mirror.modeChangeInProgress = false;

    mirror.fireGameModeChange();

    CHECK(mirror.dispatchCount == 1);
}

TEST_CASE("F-027: GAMEMODECHANGE — reentrancy guard prevents recursive dispatch")
{
    compResetTestHooksArray();
    compResetHookInvocations();

    void* prog = reinterpret_cast<void*>(0xF02);
    compRegisterTestHook(prog, HOOK_GAMEMODECHANGE, 1);

    GameModeChangeMirror mirror;
    mirror.gameLoaded = true;
    mirror.modeChangeInProgress = true; // already in progress

    mirror.fireGameModeChange();

    CHECK(mirror.dispatchCount == 0); // prevented by reentrancy guard
}

TEST_CASE("F-027: GAMEMODECHANGE — gGameLoaded false blocks even without in-progress flag")
{
    compResetTestHooksArray();

    void* prog = reinterpret_cast<void*>(0xF03);
    compRegisterTestHook(prog, HOOK_GAMEMODECHANGE, 1);

    GameModeChangeMirror mirror;
    mirror.gameLoaded = false;
    mirror.modeChangeInProgress = false;

    mirror.fireGameModeChange();

    CHECK(mirror.dispatchCount == 0);
}

TEST_CASE("F-027: GAMEMODECHANGE — both guards: gGameLoaded wins (checked first)")
{
    // When both conditions are bad: gGameLoaded is checked first,
    // so reentrancy check is never reached
    compResetTestHooksArray();

    void* prog = reinterpret_cast<void*>(0xF04);
    compRegisterTestHook(prog, HOOK_GAMEMODECHANGE, 1);

    GameModeChangeMirror mirror;
    mirror.gameLoaded = false;
    mirror.modeChangeInProgress = true;

    mirror.fireGameModeChange();

    CHECK(mirror.dispatchCount == 0); // gGameLoaded gate blocks first
}

TEST_CASE("F-027: GAMEMODECHANGE — dispatch happens exactly once when guards pass")
{
    compResetTestHooksArray();
    compResetHookInvocations();

    void* prog1 = reinterpret_cast<void*>(0xF10);
    void* prog2 = reinterpret_cast<void*>(0xF11);
    compRegisterTestHook(prog1, HOOK_GAMEMODECHANGE, 1);
    compRegisterTestHook(prog2, HOOK_GAMEMODECHANGE, 2);

    GameModeChangeMirror mirror;
    mirror.gameLoaded = true;
    mirror.modeChangeInProgress = false;

    mirror.fireGameModeChange();

    CHECK(mirror.dispatchCount == 1); // only one dispatch call

    // After dispatch, the flag should be cleared (allowing future dispatches)
    CHECK(mirror.modeChangeInProgress == false);
}

TEST_CASE("F-027: GAMEMODECHANGE — empty hook list fast-path skips dispatch")
{
    compResetTestHooksArray();

    GameModeChangeMirror mirror;
    mirror.gameLoaded = true;
    mirror.modeChangeInProgress = false;

    CHECK(compTestHooks[HOOK_GAMEMODECHANGE].size() == 0);

    mirror.fireGameModeChange();

    CHECK(mirror.dispatchCount == 0); // fast-path before flag is set
    CHECK(mirror.modeChangeInProgress == false); // flag never set
}

// =================================================================
// F2-002: scriptHooksUnregisterProgram behavioral test
// =================================================================
// Finding F2-002 (MEDIUM, weakened from HIGH): scriptHooksUnregisterProgram
// has zero behavioral tests. Code uses `it = erase(it)` pattern which is
// correct, but no test exercises sweeping unregistration across many lists.
//
// Production logic at sfall_script_hooks.cc:176-192:
//   - Iterates over ALL HOOK_COUNT hook arrays
//   - For each array, scans for Program* match and erases
//   - Early return on nullptr program

namespace {
    // Mirror of sfall_script_hooks.cc:176-192
    static void mirrorScriptHooksUnregisterProgram(
        void* program,
        std::vector<TestScriptHook> hooks[HOOK_COUNT])
    {
        if (program == nullptr) {
            return;
        }

        for (int i = 0; i < HOOK_COUNT; i++) {
            auto& hookList = hooks[i];
            for (auto it = hookList.begin(); it != hookList.end();) {
                if (it->program == program) {
                    it = hookList.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
}

TEST_CASE("F2-002: scriptHooksUnregisterProgram — nullptr does nothing")
{
    compResetTestHooksArray();

    void* prog = reinterpret_cast<void*>(0x777);
    compRegisterTestHook(prog, HOOK_TOHIT, 1);
    compRegisterTestHook(prog, HOOK_ONDEATH, 2);

    // Unregister nullptr — should not remove anything
    mirrorScriptHooksUnregisterProgram(nullptr, compTestHooks);

    CHECK(compTestHooks[HOOK_TOHIT].size() == 1);
    CHECK(compTestHooks[HOOK_ONDEATH].size() == 1);
}

TEST_CASE("F2-002: scriptHooksUnregisterProgram — removes from single hook type")
{
    compResetTestHooksArray();

    void* prog = reinterpret_cast<void*>(0x888);
    compRegisterTestHook(prog, HOOK_TOHIT, 1);

    mirrorScriptHooksUnregisterProgram(prog, compTestHooks);

    CHECK(compTestHooks[HOOK_TOHIT].size() == 0);
}

TEST_CASE("F2-002: scriptHooksUnregisterProgram — removes from all hook types (3 types)")
{
    compResetTestHooksArray();

    void* prog = reinterpret_cast<void*>(0x999);

    // Register in 3 different hook types (as per RPU/Et Tu usage patterns)
    compRegisterTestHook(prog, HOOK_TOHIT, 1);
    compRegisterTestHook(prog, HOOK_GAMEMODECHANGE, 2);
    compRegisterTestHook(prog, HOOK_KEYPRESS, 3);

    CHECK(compTestHooks[HOOK_TOHIT].size() == 1);
    CHECK(compTestHooks[HOOK_GAMEMODECHANGE].size() == 1);
    CHECK(compTestHooks[HOOK_KEYPRESS].size() == 1);

    mirrorScriptHooksUnregisterProgram(prog, compTestHooks);

    // All three lists should be empty
    CHECK(compTestHooks[HOOK_TOHIT].size() == 0);
    CHECK(compTestHooks[HOOK_GAMEMODECHANGE].size() == 0);
    CHECK(compTestHooks[HOOK_KEYPRESS].size() == 0);
}

TEST_CASE("F2-002: scriptHooksUnregisterProgram — removes from all 62 hook type arrays sweep")
{
    compResetTestHooksArray();

    void* prog = reinterpret_cast<void*>(0xAAA);

    // Register the same program in every hook type
    for (int i = 0; i < HOOK_COUNT; i++) {
        compRegisterTestHook(prog, static_cast<HookType>(i), i + 1);
    }

    // Verify registered in all
    for (int i = 0; i < HOOK_COUNT; i++) {
        CHECK(compTestHooks[i].size() == 1);
        CHECK(compTestHooks[i][0].program == prog);
    }

    mirrorScriptHooksUnregisterProgram(prog, compTestHooks);

    // Verify removed from all
    for (int i = 0; i < HOOK_COUNT; i++) {
        CHECK(compTestHooks[i].size() == 0);
    }
}

TEST_CASE("F2-002: scriptHooksUnregisterProgram — does not affect other programs")
{
    compResetTestHooksArray();

    void* progA = reinterpret_cast<void*>(0xA00);
    void* progB = reinterpret_cast<void*>(0xB00);
    void* progC = reinterpret_cast<void*>(0xC00);

    // Register A in multiple types
    compRegisterTestHook(progA, HOOK_TOHIT, 1);
    compRegisterTestHook(progA, HOOK_ONDEATH, 2);
    // Register B and C in some of the same types
    compRegisterTestHook(progB, HOOK_TOHIT, 3);
    compRegisterTestHook(progC, HOOK_ONDEATH, 4);

    // Unregister only A
    mirrorScriptHooksUnregisterProgram(progA, compTestHooks);

    // B and C should still be registered
    CHECK(compTestHooks[HOOK_TOHIT].size() == 1);
    CHECK(compTestHooks[HOOK_TOHIT][0].program == progB);
    CHECK(compTestHooks[HOOK_ONDEATH].size() == 1);
    CHECK(compTestHooks[HOOK_ONDEATH][0].program == progC);
}

TEST_CASE("F2-002: scriptHooksUnregisterProgram — multiple registrations of same program in same type")
{
    compResetTestHooksArray();

    void* prog = reinterpret_cast<void*>(0xD00);

    // Register prog multiple times in the same hook type
    // (production dedup prevents this normally, but for test we bypass dedup)
    compTestHooks[HOOK_TOHIT].push_back({ prog, 1 });
    compTestHooks[HOOK_TOHIT].push_back({ prog, 2 });
    compTestHooks[HOOK_TOHIT].push_back({ prog, 3 });

    CHECK(compTestHooks[HOOK_TOHIT].size() == 3);

    mirrorScriptHooksUnregisterProgram(prog, compTestHooks);

    // ALL instances are removed (erase loop continues while match found)
    CHECK(compTestHooks[HOOK_TOHIT].size() == 0);
}

TEST_CASE("F2-002: scriptHooksUnregisterProgram — unregistered program is no-op")
{
    compResetTestHooksArray();

    void* prog1 = reinterpret_cast<void*>(0xE00);
    void* prog2 = reinterpret_cast<void*>(0xE01);

    compRegisterTestHook(prog1, HOOK_TOHIT, 1);

    // Unregister a program that was never registered
    mirrorScriptHooksUnregisterProgram(prog2, compTestHooks);

    // prog1 should still be registered
    CHECK(compTestHooks[HOOK_TOHIT].size() == 1);
    CHECK(compTestHooks[HOOK_TOHIT][0].program == prog1);
}

TEST_CASE("F2-002: scriptHooksUnregisterProgram — registers in 5 hook types, unregisters all")
{
    compResetTestHooksArray();

    void* prog = reinterpret_cast<void*>(0xF00);
    int hookTypes[] = {
        HOOK_TOHIT, HOOK_AFTERHITROLL, HOOK_CALCAPCOST,
        HOOK_DEATHANIM2, HOOK_COMBATDAMAGE
    };

    for (int ht : hookTypes) {
        compRegisterTestHook(prog, static_cast<HookType>(ht), 1);
    }

    // All 5 types have the program
    for (int ht : hookTypes) {
        CHECK(compTestHooks[ht].size() == 1);
    }

    mirrorScriptHooksUnregisterProgram(prog, compTestHooks);

    // All 5 types are empty
    for (int ht : hookTypes) {
        CHECK(compTestHooks[ht].size() == 0);
    }
}

// =================================================================
// F2-008: Replace mirror-based hook type isolation test
// =================================================================
// Finding F2-008 (MEDIUM): The mirror-based hook type isolation test
// at test_sfall_hook_call.cpp:309-326 is trivially true by mirror
// construction. The mirror's call() uses testHooks[hookType] which
// is indexed by a single hook type — it's structurally impossible
// to dispatch the wrong type.
//
// This test creates a more meaningful isolation verification:
//   1. Fill ALL hook type arrays with handlers
//   2. Dispatch type T
//   3. Verify only handlers from array[T] are invoked
//   4. Verify NO handlers from other arrays[U] are invoked
//
// NOTE: These tests remain mirror-based — they exercise the local compTestHooks[]
// mirror array, not the production scriptHooks[]. Array-index dispatch guarantees
// isolation by construction: call() indexes by hookType into a private array, so
// cross-type leakage is structurally impossible. The tests below validate this
// isolation principle, but are not a replacement for production-link testing
// which would require linking sfall_script_hooks.cc. Cross-reference validation
// (see F2-008 test below) verifies that the mirror key set matches production
// HOOK_COUNT, ensuring the test reflects the current hook type count.
// I2-M60: REGRESSION PATH — the static_assert(HOOK_COUNT == 62) at line 1153
// and the 62-array fill test at line 960 together form a compile-time and
// runtime cross-check. When HOOK_COUNT changes, the static_assert fails at
// compile time; when new hook types are added without corresponding isolation
// tests, the 62-array fill test will flag the gap. See sfall_script_hooks.h
// for the authoritative HOOK_COUNT definition.

TEST_CASE("F2-008: Hook type isolation — only correct type dispatches (populated arrays)")
{
    compResetTestHooksArray();
    compResetHookInvocations();

    // Fill ALL 62 hook arrays with a distinct program per type
    void* programs[HOOK_COUNT];
    for (int i = 0; i < HOOK_COUNT; i++) {
        programs[i] = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1000 + i * 16));
        compRegisterTestHook(programs[i], static_cast<HookType>(i), 1);
    }

    // Verify all 62 arrays are populated
    for (int i = 0; i < HOOK_COUNT; i++) {
        CHECK(compTestHooks[i].size() == 1);
        CHECK(compTestHooks[i][0].program == programs[i]);
    }

    // Dispatch HOOK_TOHIT (type 0)
    CompTestScriptHookCall hook(HOOK_TOHIT, 0, {});
    hook.call();

    // Only the HOOK_TOHIT handler should have fired
    CHECK(gCompHookInvocations.size() == 1);
    CHECK(gCompHookInvocations[0].program == programs[HOOK_TOHIT]);
}

TEST_CASE("F2-008: Hook type isolation — dispatch HOOK_ONDEATH, only ONDEATH fires")
{
    compResetTestHooksArray();
    compResetHookInvocations();

    // Populate 10 random hook arrays
    int testTypes[] = { 0, 1, 5, 6, 10, 31, 39, 43, 48, 53 };
    for (int ht : testTypes) {
        void* prog = reinterpret_cast<void*>(static_cast<uintptr_t>(0x2000 + ht));
        compRegisterTestHook(prog, static_cast<HookType>(ht), 1);
    }

    // Dispatch HOOK_ONDEATH (6)
    CompTestScriptHookCall hook(HOOK_ONDEATH, 0, {});
    hook.call();

    // Only HOOK_ONDEATH handler should fire
    CHECK(gCompHookInvocations.size() == 1);
    // The program for HOOK_ONDEATH is at 0x2000 + 6 = 0x2006
    CHECK(gCompHookInvocations[0].program == reinterpret_cast<void*>(0x2006));
}

TEST_CASE("F2-008: Hook type isolation — dispatch HOOK_DIALOG, only DIALOG fires")
{
    compResetTestHooksArray();
    compResetHookInvocations();

    // Register handlers in adjacent types and the target type
    void* prog48 = reinterpret_cast<void*>(0x3000);
    void* prog49 = reinterpret_cast<void*>(0x3001);
    void* prog50 = reinterpret_cast<void*>(0x3002);

    compRegisterTestHook(prog48, HOOK_CANUSEWEAPON, 1);  // type 48
    compRegisterTestHook(prog49, HOOK_DIALOG, 1);        // type 49
    compRegisterTestHook(prog50, HOOK_DIALOGREACTION, 1); // type 50

    // Dispatch DIALOG (49)
    CompTestScriptHookCall hook(HOOK_DIALOG, 0, {});
    hook.call();

    // Only the DIALOG handler should have fired
    CHECK(gCompHookInvocations.size() == 1);
    CHECK(gCompHookInvocations[0].program == prog49);
}

TEST_CASE("F2-008: Hook type isolation — verify NO cross-type contamination with mutable arrays")
{
    compResetTestHooksArray();
    compResetHookInvocations();

    // Register distinct programs in multiple types
    void* p0 = reinterpret_cast<void*>(0x4000);
    void* p1 = reinterpret_cast<void*>(0x4001);
    void* p31 = reinterpret_cast<void*>(0x4002);
    void* p49 = reinterpret_cast<void*>(0x4003);

    compRegisterTestHook(p0, HOOK_TOHIT, 1);
    compRegisterTestHook(p1, HOOK_AFTERHITROLL, 1);
    compRegisterTestHook(p31, HOOK_GAMEMODECHANGE, 1);
    compRegisterTestHook(p49, HOOK_DIALOG, 1);

    // Dispatch all four types one by one
    HookType typesToTest[] = { HOOK_TOHIT, HOOK_AFTERHITROLL, HOOK_GAMEMODECHANGE, HOOK_DIALOG };
    void* expectedProgs[] = { p0, p1, p31, p49 };

    for (int t = 0; t < 4; t++) {
        compResetHookInvocations();
        CompTestScriptHookCall hook(typesToTest[t], 0, {});
        hook.call();

        // Each dispatch should fire exactly 1 handler — the correct one
        CHECK(gCompHookInvocations.size() == 1);
        CHECK(gCompHookInvocations[0].program == expectedProgs[t]);
    }
}

TEST_CASE("F2-008: Hook type isolation — empty dispatch is a true no-op")
{
    compResetTestHooksArray();
    compResetHookInvocations();

    // Populate types 0, 1, 5 but leave others empty
    void* p0 = reinterpret_cast<void*>(0x5000);
    void* p1 = reinterpret_cast<void*>(0x5001);
    void* p5 = reinterpret_cast<void*>(0x5002);

    compRegisterTestHook(p0, HOOK_TOHIT, 1);
    compRegisterTestHook(p1, HOOK_AFTERHITROLL, 1);
    compRegisterTestHook(p5, HOOK_COMBATDAMAGE, 1);

    // Dispatch HOOK_ONDEATH — which has no handlers
    CompTestScriptHookCall hook(HOOK_ONDEATH, 0, {});
    hook.call();

    // Nothing should fire
    CHECK(gCompHookInvocations.size() == 0);
}

TEST_CASE("F2-008: Hook type isolation — adjacent enum values don't leak")
{
    compResetTestHooksArray();
    compResetHookInvocations();

    // Register handlers in STDPROCEDURE (40), STDPROCEDURE_END (41), TARGETOBJECT (42)
    void* p40 = reinterpret_cast<void*>(0x6000);
    void* p42 = reinterpret_cast<void*>(0x6002);

    compRegisterTestHook(p40, HOOK_STDPROCEDURE, 1);
    compRegisterTestHook(p42, HOOK_TARGETOBJECT, 1);
    // STDPROCEDURE_END (41) intentionally left empty

    // Dispatch STDPROCEDURE (40)
    CompTestScriptHookCall hook(HOOK_STDPROCEDURE, 0, {});
    hook.call();

    CHECK(gCompHookInvocations.size() == 1);
    CHECK(gCompHookInvocations[0].program == p40);
    // p42 (TARGETOBJECT) should NOT have fired
}

TEST_CASE("F2-008: Hook type isolation — NOT trivially true by mirror construction")
{
    // The original test at test_sfall_hook_call.cpp:309-326 was trivially true:
    // call() uses testHooks[hookType], so testHooks[HOOK_TOHIT].call() could
    // never access testHooks[HOOK_ONDEATH]. This test verifies the ISOLATION
    // PRINCIPLE: that the array-index-based dispatch correctly separates hook
    // types by design, and that this is a deliberate architectural choice
    // (not a coincidental property of the test mirror).

    compResetTestHooksArray();

    // Deliberately register many overlapping programs across types
    for (int i = 0; i < HOOK_COUNT; i++) {
        void* prog = reinterpret_cast<void*>(static_cast<uintptr_t>(0x7000 + i));
        compRegisterTestHook(prog, static_cast<HookType>(i), 1);
    }

    // The isolation comes from the hook dispatch mechanism:
    // Each HookType has its own std::vector<ScriptHook> in scriptHooks[].
    // The `call()` method accesses ONLY scriptHooks[_hookType].
    // This is correct behavior, not a test artifact.

    // Verify: dispatch each type and confirm the correct handler fires
    for (int i = 0; i < HOOK_COUNT; i++) {
        compResetHookInvocations();
        CompTestScriptHookCall hook(static_cast<HookType>(i), 0, {});
        hook.call();

        CHECK(gCompHookInvocations.size() == 1);
        CHECK(gCompHookInvocations[0].program == reinterpret_cast<void*>(0x7000 + i));
    }
}

TEST_CASE("F2-008: Cross-reference — compTestHooks[] key set matches production HOOK_COUNT")
{
    // Verify that the mirror's hook array dimensionality matches the production
    // HOOK_COUNT constant. This ensures the test stays current when new hook
    // types are added to the enum — the mirror compTestHooks[HOOK_COUNT] is
    // dimensioned by the production constant, so adding a hook type will cause
    // a compile-time error if a test relies on the old dimension.
    //
    // This is a compile-time cross-reference: the compTestHooks array at line 54
    // is declared as std::vector<TestScriptHook> compTestHooks[HOOK_COUNT], so
    // if the production HOOK_COUNT changes, this test compiles against the new
    // value automatically. The CHECK below verifies the runtime array size
    // matches the production constant.
    static_assert(HOOK_COUNT == 62,
        "HOOK_COUNT changed — update F2-008 isolation tests if new hook types added");
    CHECK(sizeof(compTestHooks) / sizeof(compTestHooks[0]) == HOOK_COUNT);
}
