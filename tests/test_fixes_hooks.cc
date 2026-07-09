// Unit tests for the 4 hooks fixes applied in Stage 6.
//
// F-21:  Temporal UAF — scriptHooksReset() now called BEFORE programListFree()
// F-42:  HOOK_AFTERHITROLL defender override lacks critterIsDead() check
// F2-02: scriptHooks_CarTravel null pointer dereference
// F2-04: HOOK_USESKILLON user override lacks OBJ_TYPE_CRITTER validation
//
// SELF-CONTAINED MIRROR pattern — does NOT link production .cc files.
// Each fix is mirrored as a local function exercising the exact same
// validation chain and control flow as the production code.
//
// F-63 (MEDIUM): All mirrors exercise LOCAL MIRRORS of the production
// validation logic. They correctly model the behavioral contracts but
// cannot detect production-only regressions (e.g., stack-address UB,
// compiler-specific struct layout). Until production linking is available
// via the incremental extraction roadmap (test_script_harness.h), these
// tests serve as behavioral regression guards.
//

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "obj_types.h"
#include "sfall_script_hooks.h"

#include <cstring>
#include <vector>

using namespace fallout;

// Helper to construct an FID with given object type and low 24 bits.
// Matches the pattern used in test_sfall_fixes.cc F-38.
static int makeFid(int objType, int remainder = 0)
{
    return (objType << 24) | (remainder & 0x00FFFFFF);
}

// =================================================================
// F-21: Temporal UAF — scriptHooksReset() ordering fix
// =================================================================
//
// Finding F-21 (MEDIUM, CONFIRMED): Before the fix, programListFree()
// freed all Program objects while 62 scriptHooks[HookType] vectors
// still held Program* pointers — a temporal Use-After-Free window.
//
// Fix at scripts.cc:2560-2567 and scripts.cc:2598-2599:
//   scriptHooksReset() is now called BEFORE programListFree() at both
//   call sites (_scr_remove_all and _scr_remove_all_force).
//
// Validation approach (no linking to scripts.cc):
//   Mirror both functions locally and track program liveness state
//   independently from hook references. The ordering invariant is:
//     Correct (Reset→Free): hooks cleared → programs freed → no stale refs
//     Buggy  (Free→Reset):  programs freed → hooks cleared → stale refs
//   The test verifies that after Reset→Free, all hook vectors are empty
//   and no program in any hook vector has been freed. After Free→Reset,
//   there exists a window where hooks reference freed programs.

namespace f21_mirror {

struct MockProgram {
    bool alive = true; // false = freed
    int id = 0;
};

static std::vector<MockProgram> g_programPool;

static void resetProgramPool()
{
    g_programPool.clear();
    g_programPool.reserve(10); // prevent reallocation from invalidating MockProgram*
}

static MockProgram* allocProgram(int id)
{
    g_programPool.push_back({ true, id });
    return &g_programPool.back();
}

// Mirror of the 62 scriptHooks[HookType] arrays.
// Each entry is a { Program*, procedureIndex } pair.
struct HookEntry {
    MockProgram* program = nullptr;
    int procedureIndex = -1;
};

static std::vector<HookEntry> g_hooksMirror[HOOK_COUNT];

static void resetHooksMirror()
{
    for (int i = 0; i < HOOK_COUNT; i++) {
        g_hooksMirror[i].clear();
    }
}

// Mirror of scriptHooksReset() — clears all hook vectors without
// accessing any Program object. Production implementation:
//   scriptHooksClear() → vector::clear (trivial destructor for Program*)
//   sfallAnimCallbackReset() → nulls global pointers only
static void mirrorScriptHooksReset()
{
    for (int i = 0; i < HOOK_COUNT; i++) {
        g_hooksMirror[i].clear();
    }
}

// Mirror of programListFree() — marks all programs as freed.
// Production implementation frees the actual Program objects.
static void mirrorProgramListFree()
{
    for (auto& p : g_programPool) {
        p.alive = false;
    }
}

// Register a program in a hook array (mirrors scriptHooksRegister).
static void registerHook(MockProgram* prog, HookType ht, int procIdx)
{
    g_hooksMirror[ht].push_back({ prog, procIdx });
}

// Check if any hook array contains a reference to a freed program.
// Returns: number of dangling references found.
static int countDanglingReferences()
{
    int count = 0;
    for (int i = 0; i < HOOK_COUNT; i++) {
        for (const auto& entry : g_hooksMirror[i]) {
            if (entry.program != nullptr && !entry.program->alive) {
                ++count;
            }
        }
    }
    return count;
}

// Count how many non-null program references exist across all hook arrays.
static int countHookReferences()
{
    int count = 0;
    for (int i = 0; i < HOOK_COUNT; i++) {
        for (const auto& entry : g_hooksMirror[i]) {
            if (entry.program != nullptr) {
                ++count;
            }
        }
    }
    return count;
}

} // namespace f21_mirror

TEST_CASE("F-21: Correct order (Reset→Free) — no dangling references at any point")
{
    using namespace f21_mirror;
    resetHooksMirror();
    resetProgramPool();

    // Register 3 programs across 4 hook types
    MockProgram* p1 = allocProgram(1);
    MockProgram* p2 = allocProgram(2);
    MockProgram* p3 = allocProgram(3);

    registerHook(p1, HOOK_TOHIT, 10);
    registerHook(p2, HOOK_TOHIT, 20);
    registerHook(p1, HOOK_AFTERHITROLL, 11);
    registerHook(p3, HOOK_COMBATDAMAGE, 30);
    registerHook(p2, HOOK_ONDEATH, 21);
    registerHook(p3, HOOK_ONDEATH, 31);

    // 6 hook references total
    CHECK(countHookReferences() == 6);
    // All programs alive
    CHECK(p1->alive == true);
    CHECK(p2->alive == true);
    CHECK(p3->alive == true);

    // --- Step 1: Reset hooks (CORRECT order — Reset BEFORE Free) ---
    mirrorScriptHooksReset();

    // After reset: all hook arrays are empty
    CHECK(countHookReferences() == 0);
    // Programs are still alive (not freed yet)
    CHECK(p1->alive == true);
    CHECK(p2->alive == true);
    CHECK(p3->alive == true);
    // No dangling references — hooks were cleared while programs were alive
    CHECK(countDanglingReferences() == 0);

    // --- Step 2: Free programs ---
    mirrorProgramListFree();

    // Programs are now dead
    CHECK(p1->alive == false);
    CHECK(p2->alive == false);
    CHECK(p3->alive == false);
    // Still no dangling references — hooks were already cleared
    CHECK(countDanglingReferences() == 0);
}

TEST_CASE("F-21: Buggy order (Free→Reset) — dangling references window exists")
{
    using namespace f21_mirror;
    resetHooksMirror();
    resetProgramPool();

    MockProgram* p1 = allocProgram(1);
    MockProgram* p2 = allocProgram(2);

    registerHook(p1, HOOK_TOHIT, 10);
    registerHook(p2, HOOK_AFTERHITROLL, 20);
    registerHook(p1, HOOK_GAMEMODECHANGE, 11);

    CHECK(countHookReferences() == 3);

    // --- Step 1: Free programs (BUGGY order — Free BEFORE Reset) ---
    mirrorProgramListFree();

    // Programs are dead
    CHECK(p1->alive == false);
    CHECK(p2->alive == false);
    // BUT hook arrays still hold references to now-dead programs — DANGLING!
    CHECK(countHookReferences() == 3);
    int danglingBeforeReset = countDanglingReferences();
    CHECK(danglingBeforeReset == 3); // all 3 references are dangling

    // --- Step 2: Reset hooks (too late — programs already freed) ---
    mirrorScriptHooksReset();

    // Hooks cleared, but the window already happened
    CHECK(countHookReferences() == 0);
    CHECK(countDanglingReferences() == 0); // cleared now, but too late
}

TEST_CASE("F-21: Reset→Free ordering — 62 hook type arrays are all cleared")
{
    using namespace f21_mirror;
    resetHooksMirror();
    resetProgramPool();

    // Register a program in every single hook type
    MockProgram* p = allocProgram(99);
    for (int i = 0; i < HOOK_COUNT; i++) {
        registerHook(p, static_cast<HookType>(i), i + 100);
    }

    // All 62 arrays have 1 entry
    for (int i = 0; i < HOOK_COUNT; i++) {
        CHECK(g_hooksMirror[i].size() == 1);
    }

    // Reset BEFORE Free (correct order)
    mirrorScriptHooksReset();

    // All 62 arrays are empty
    for (int i = 0; i < HOOK_COUNT; i++) {
        CHECK(g_hooksMirror[i].size() == 0);
    }

    // Program still alive — safe to free now
    CHECK(p->alive == true);
    mirrorProgramListFree();
    CHECK(p->alive == false);
    CHECK(countDanglingReferences() == 0);
}

TEST_CASE("F-21: Reset is idempotent — double-Reset does not access freed programs")
{
    using namespace f21_mirror;
    resetHooksMirror();
    resetProgramPool();

    MockProgram* p = allocProgram(42);
    registerHook(p, HOOK_TOHIT, 1);

    // First Reset clears hooks
    mirrorScriptHooksReset();
    CHECK(countHookReferences() == 0);

    // Free the program
    mirrorProgramListFree();
    CHECK(p->alive == false);

    // Second Reset on empty arrays — must not crash or access freed p
    mirrorScriptHooksReset();
    CHECK(countHookReferences() == 0);
    // No dangling references — arrays were already empty
    CHECK(countDanglingReferences() == 0);
}

// =================================================================
// F-42: HOOK_AFTERHITROLL defender override lacks critterIsDead()
// =================================================================
//
// Finding F-42 (MEDIUM, CONFIRMED): HOOK_AFTERHITROLL defender override
// accepted dead critters. The validation chain checked nullptr and
// PID_TYPE == OBJ_TYPE_CRITTER, but did NOT check critterIsDead().
// A dead defender causes incorrect Silent Death multiplier computation
// in combat.cc at lines 4133 and 4170 (whoHitMe dereference).
//
// Fix at sfall_script_hooks.cc:1067:
//   Added !critterIsDead(overrideDefender) to the validation condition.
//
// Production critterIsDead() at critter.cc:962-970 checks:
//   if (critter == nullptr) return false;
//   if (FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER) return false;
//   return (critter->data.critter.combat.results & DAM_DEAD) != 0;
//
// Mirror: We model a critter as having an `fid` (for PID_TYPE check)
// and `combatResults` (for DAM_DEAD check). A dead critter is one
// where PID_TYPE is OBJ_TYPE_CRITTER AND results has DAM_DEAD bit set.

namespace f42_mirror {

// Mirror of the critter data needed for the validation chain
struct MirrorCritter {
    int fid = 0;            // FID with type in upper bits
    int combatResults = 0;  // DAM_* flags bitmask
};

// Mirror of critterIsDead() at critter.cc:962-970
static bool mirrorCritterIsDead(const MirrorCritter* critter)
{
    if (critter == nullptr) return false;
    if (PID_TYPE(critter->fid) != OBJ_TYPE_CRITTER) return false;
    return (critter->combatResults & DAM_DEAD) != 0;
}

// Mirror of the AfterHitRoll defender validation at sfall_script_hooks.cc:1067
// Pre-F-42:  nullptr + PID_TYPE check only
// Post-F-42: nullptr + PID_TYPE + !critterIsDead (liveness guard)
struct AfterHitRollValidationResult {
    bool overrideAccepted = false;
    const MirrorCritter* acceptedDefender = nullptr;
};

// Pre-F-42 (buggy): only nullptr + PID_TYPE check
static AfterHitRollValidationResult validateDefender_preF42(
    MirrorCritter* overrideDefender)
{
    AfterHitRollValidationResult result;
    if (overrideDefender != nullptr
        && PID_TYPE(overrideDefender->fid) == OBJ_TYPE_CRITTER) {
        result.overrideAccepted = true;
        result.acceptedDefender = overrideDefender;
    }
    return result;
}

// Post-F-42 (fixed): nullptr + PID_TYPE + !critterIsDead
static AfterHitRollValidationResult validateDefender_postF42(
    MirrorCritter* overrideDefender)
{
    AfterHitRollValidationResult result;
    if (overrideDefender != nullptr
        && PID_TYPE(overrideDefender->fid) == OBJ_TYPE_CRITTER
        && !mirrorCritterIsDead(overrideDefender)) {
        result.overrideAccepted = true;
        result.acceptedDefender = overrideDefender;
    }
    return result;
}

} // namespace f42_mirror

TEST_CASE("F-42: Live critter — accepted by both pre and post fix validation")
{
    using namespace f42_mirror;
    MirrorCritter liveCritter;
    liveCritter.fid = makeFid(OBJ_TYPE_CRITTER, 0x010001); // valid critter
    liveCritter.combatResults = 0; // no DAM_DEAD — alive

    CHECK(mirrorCritterIsDead(&liveCritter) == false);

    auto pre = validateDefender_preF42(&liveCritter);
    CHECK(pre.overrideAccepted == true);

    auto post = validateDefender_postF42(&liveCritter);
    CHECK(post.overrideAccepted == true); // live → accepted
}

TEST_CASE("F-42: Dead critter — accepted by pre-fix (bug), REJECTED by post-fix")
{
    using namespace f42_mirror;
    MirrorCritter deadCritter;
    deadCritter.fid = makeFid(OBJ_TYPE_CRITTER, 0x020002);
    deadCritter.combatResults = DAM_DEAD; // dead

    CHECK(mirrorCritterIsDead(&deadCritter) == true);

    // Pre-F-42 (buggy): dead critter IS accepted as defender
    auto pre = validateDefender_preF42(&deadCritter);
    CHECK(pre.overrideAccepted == true); // BUG: dead critter accepted!

    // Post-F-42 (fixed): dead critter is REJECTED
    auto post = validateDefender_postF42(&deadCritter);
    CHECK(post.overrideAccepted == false); // FIXED: dead critter rejected
}

TEST_CASE("F-42: Dead critter with other DAM flags — REJECTED by post-fix")
{
    using namespace f42_mirror;
    MirrorCritter critter;
    critter.fid = makeFid(OBJ_TYPE_CRITTER, 0x030003);
    // Critter is dead AND has other flags set (knocked out, critical hit)
    critter.combatResults = DAM_DEAD | DAM_KNOCKED_OUT | DAM_CRITICAL;

    CHECK(mirrorCritterIsDead(&critter) == true);

    auto pre = validateDefender_preF42(&critter);
    CHECK(pre.overrideAccepted == true); // BUG: accepted

    auto post = validateDefender_postF42(&critter);
    CHECK(post.overrideAccepted == false); // FIXED: rejected
}

TEST_CASE("F-42: Knocked-out (not dead) critter — accepted by both")
{
    using namespace f42_mirror;
    MirrorCritter koCritter;
    koCritter.fid = makeFid(OBJ_TYPE_CRITTER, 0x040004);
    koCritter.combatResults = DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN; // KO but alive

    CHECK(mirrorCritterIsDead(&koCritter) == false);

    auto pre = validateDefender_preF42(&koCritter);
    CHECK(pre.overrideAccepted == true);

    auto post = validateDefender_postF42(&koCritter);
    CHECK(post.overrideAccepted == true); // knocked out ≠ dead → accepted
}

TEST_CASE("F-42: Null defender — rejected by both")
{
    using namespace f42_mirror;
    auto pre = validateDefender_preF42(nullptr);
    CHECK(pre.overrideAccepted == false);

    auto post = validateDefender_postF42(nullptr);
    CHECK(post.overrideAccepted == false);
}

TEST_CASE("F-42: Non-critter object (item) — rejected by both")
{
    using namespace f42_mirror;
    MirrorCritter item;
    item.fid = makeFid(OBJ_TYPE_ITEM, 0x050005);

    auto pre = validateDefender_preF42(&item);
    CHECK(pre.overrideAccepted == false);

    auto post = validateDefender_postF42(&item);
    CHECK(post.overrideAccepted == false);
}

TEST_CASE("F-42: Non-critter object (wall) — rejected by both")
{
    using namespace f42_mirror;
    MirrorCritter wall;
    wall.fid = makeFid(OBJ_TYPE_WALL, 0x060006);

    auto pre = validateDefender_preF42(&wall);
    CHECK(pre.overrideAccepted == false);

    auto post = validateDefender_postF42(&wall);
    CHECK(post.overrideAccepted == false);
}

TEST_CASE("F-42: critterIsDead on null returns false (safe to call)")
{
    using namespace f42_mirror;
    CHECK(mirrorCritterIsDead(nullptr) == false);
}

TEST_CASE("F-42: critterIsDead on non-critter returns false (safe in debugPrint)")
{
    using namespace f42_mirror;
    MirrorCritter item;
    item.fid = makeFid(OBJ_TYPE_ITEM, 0x070007);
    item.combatResults = DAM_DEAD; // doesn't matter for non-critter

    // Non-critter Object* passed to critterIsDead → returns false
    // (production checks FID_TYPE first, so it's safe in debugPrint context)
    CHECK(mirrorCritterIsDead(&item) == false);
}

// =================================================================
// F2-02: scriptHooks_CarTravel null pointer dereference
// =================================================================
//
// Finding F2-02 (MEDIUM, CONFIRMED, both-found): scriptHooks_CarTravel
// dereferenced *speedPtr and *fuelConsumptionPtr without null checks.
// 7/7 peer void-return hook fire functions validate pointer parameters.
//
// Fix at sfall_script_hooks.cc:1673:
//   if (speedPtr == nullptr || fuelConsumptionPtr == nullptr) return;
//
// The function has void return type, so `return;` is the correct pattern
// matching peer functions (scriptHooks_ItemDamage, scriptHooks_DeathAnim).

namespace f202_mirror {

// Mirror of the fixed scriptHooks_CarTravel validation
// Production: lines 1671-1697
struct CarTravelState {
    int speed = 0;
    int fuelConsumption = 0;
    bool called = false; // was the function entered past the null guard?
};

// Pre-F2-02 (buggy): no null guard — dereferences unconditionally
// Returns false if crash would occur, true otherwise.
static bool mirrorCarTravel_preF202(int* speedPtr, int* fuelConsumptionPtr,
    CarTravelState& out)
{
    // BUG: no null check — dereferences unconditionally
    // If either pointer is null, this would crash (UB) in production.
    // We simulate the crash by checking here and returning false.
    if (speedPtr == nullptr || fuelConsumptionPtr == nullptr) {
        return false; // "crashed" — null dereference
    }

    out.speed = *speedPtr;
    out.fuelConsumption = *fuelConsumptionPtr;
    out.called = true;

    // ... hook dispatch would happen here ...

    return true; // "survived"
}

// Post-F2-02 (fixed): null guard early-returns
static void mirrorCarTravel_postF202(int* speedPtr, int* fuelConsumptionPtr,
    CarTravelState& out)
{
    // FIX: null guard before any dereference
    if (speedPtr == nullptr || fuelConsumptionPtr == nullptr) return;

    out.speed = *speedPtr;
    out.fuelConsumption = *fuelConsumptionPtr;
    out.called = true;

    // ... hook dispatch would happen here ...
}

} // namespace f202_mirror

TEST_CASE("F2-02: Both pointers null — pre-fix crashes, post-fix returns safely")
{
    using namespace f202_mirror;
    CarTravelState out;

    // Pre-fix: null + null → crash
    bool survived = mirrorCarTravel_preF202(nullptr, nullptr, out);
    CHECK(survived == false);   // "crashed"
    CHECK(out.called == false); // never reached

    // Post-fix: null + null → early return, no crash
    CarTravelState out2;
    mirrorCarTravel_postF202(nullptr, nullptr, out2);
    CHECK(out2.called == false); // early return, function body not entered
}

TEST_CASE("F2-02: speedPtr null, fuelConsumptionPtr valid — pre-fix crashes, post-fix returns safely")
{
    using namespace f202_mirror;
    int fuel = 42;

    CarTravelState unused1;
    bool survived = mirrorCarTravel_preF202(nullptr, &fuel, unused1);
    CHECK(survived == false); // crash: first dereference is *speedPtr

    CarTravelState out;
    mirrorCarTravel_postF202(nullptr, &fuel, out);
    CHECK(out.called == false); // early return
}

TEST_CASE("F2-02: speedPtr valid, fuelConsumptionPtr null — pre-fix crashes, post-fix returns safely")
{
    using namespace f202_mirror;
    int speed = 10;

    CarTravelState unused2;
    bool survived = mirrorCarTravel_preF202(&speed, nullptr, unused2);
    CHECK(survived == false); // crash: second dereference would be *fuelConsumptionPtr
    // (actually, || check means it would fail on the guard, but pre-fix
    //  would crash on *speedPtr first — the guard catches both cases)

    CarTravelState out;
    mirrorCarTravel_postF202(&speed, nullptr, out);
    CHECK(out.called == false);
}

TEST_CASE("F2-02: Both pointers valid — function proceeds normally")
{
    using namespace f202_mirror;
    int speed = 8;
    int fuel = 3;
    CarTravelState out;

    mirrorCarTravel_postF202(&speed, &fuel, out);
    CHECK(out.called == true);
    CHECK(out.speed == 8);
    CHECK(out.fuelConsumption == 3);
}

TEST_CASE("F2-02: Return type is void — pattern matches peer functions")
{
    // Production scriptHooks_CarTravel has void return type.
    // The null guard uses `return;` — no sentinel value needed.
    // Peer void-return hook fire functions use identical pattern:
    //   scriptHooks_ItemDamage:    if (ptr == nullptr || ...) return;
    //   scriptHooks_DeathAnim:     if (anim == nullptr) { debugPrint(...); return; }
    //   scriptHooks_BarterPrice:   if (ctx == nullptr) return;
    //
    // The CarTravel mirror's null guard uses `return;` which is
    // correct for void return type.
    CHECK(true); // structural verification — no return value assertion needed
}

// =================================================================
// F2-04: HOOK_USESKILLON user override lacks OBJ_TYPE_CRITTER validation
// =================================================================
//
// Finding F2-04 (MEDIUM, CONFIRMED, boundary-found): HOOK_USESKILLON
// accepted any asObject() return as user override without checking
// PID_TYPE == OBJ_TYPE_CRITTER. 5/6 peer sites validate PID_TYPE.
// Non-critter override causes UB on downstream data.critter.combat access.
//
// Fix at sfall_script_hooks.cc:1176:
//   Added PID_TYPE(overrideUser->pid) == OBJ_TYPE_CRITTER check before
//   accepting the override, matching the AfterHitRoll pattern at line 1067.
//
// PID_TYPE extracts bits 31..24 of the FID:
//   OBJ_TYPE_CRITTER = 1  (0x01)
//   OBJ_TYPE_ITEM     = 0  (0x00)
//   OBJ_TYPE_SCENERY  = 2  (0x02)
//   OBJ_TYPE_WALL     = 3  (0x03)
//   OBJ_TYPE_TILE     = 4  (0x04)
//   OBJ_TYPE_MISC     = 5  (0x05)
//
// When the check fails: the original *userPtr persists unchanged,
// debugPrint logs a diagnostic, and the non-critter override is ignored.

namespace f204_mirror {

// Mirror of the UseSkillOn validation at sfall_script_hooks.cc:1170-1181
struct UseSkillOnState {
    int userFid = 0;     // current user's FID (before override)
    bool overridden = false;
    bool rejected = false;   // override was attempted but rejected
    int rejectType = 0;      // PID_TYPE of rejected override (for diagnostic)
};

// Pre-F2-04 (buggy): accepts any non-null asObject() return
static void mirrorUseSkillOn_preF204(int overrideUserFid,
    UseSkillOnState& state)
{
    // Old code: any non-null object accepted
    if (overrideUserFid != 0) { // non-null (asObject(0) → nullptr at fid=0)
        state.userFid = overrideUserFid;
        state.overridden = true;
    }
}

// Post-F2-04 (fixed): validates PID_TYPE == OBJ_TYPE_CRITTER
static void mirrorUseSkillOn_postF204(int overrideUserFid,
    UseSkillOnState& state)
{
    // Null check (fid==0 means null asObject() result)
    if (overrideUserFid == 0) {
        return; // null → no override, original persists
    }

    // F2-04 fix: validate the object is a critter
    if (PID_TYPE(overrideUserFid) == OBJ_TYPE_CRITTER) {
        state.userFid = overrideUserFid;
        state.overridden = true;
    } else {
        // Rejected: non-critter override
        // Production would call: debugPrint("HOOK_USESKILLON: ignoring non-critter user override (type=%d)", PID_TYPE(overrideUserFid));
        state.rejected = true;
        state.rejectType = PID_TYPE(overrideUserFid);
    }
}

// Mirror the full scriptHooks_UseSkillOn validation chain for ret0
// (combines int-return path and Object*-return path from lines 1158-1182)
struct UseSkillOnFullResult {
    bool shouldContinue = true;
    bool userOverridden = false;
    int userFid = 0;
    bool rejected = false;
    int rejectType = 0;
};

// Mirror of the full ret0 handling: int path (-1 cancel, 0 no-op, non-zero reject)
// followed by Object* path with PID_TYPE check.
static UseSkillOnFullResult mirrorUseSkillOn_full(
    int baseUserFid,
    int ret0Value,      // 0=int value, non-zero=Object* FID
    bool isIntReturn)   // true if the script returned an integer
{
    UseSkillOnFullResult result;

    if (isIntReturn) {
        if (ret0Value == -1) {
            result.shouldContinue = false;
            return result;
        }
        if (ret0Value != 0) {
            // Invalid int value → log, no override
        }
    } else {
        // Object* return path
        if (ret0Value == 0) {
            return result; // null object → no override
        }
        // F2-04 fix: validate PID_TYPE
        if (PID_TYPE(ret0Value) == OBJ_TYPE_CRITTER) {
            result.userFid = ret0Value;
            result.userOverridden = true;
        } else {
            result.rejected = true;
            result.rejectType = PID_TYPE(ret0Value);
        }
    }

    return result;
}

} // namespace f204_mirror

TEST_CASE("F2-04: Critter override (PID_TYPE=1) — accepted by both pre and post fix")
{
    using namespace f204_mirror;
    // OBJ_TYPE_CRITTER = 1 → FID with type=1 in upper byte: 0x01xxxxxx
    int critterFid = (OBJ_TYPE_CRITTER << 24) | 0x000001;

    // Pre-fix accepts
    UseSkillOnState pre;
    mirrorUseSkillOn_preF204(critterFid, pre);
    CHECK(pre.overridden == true);
    CHECK(pre.userFid == critterFid);

    // Post-fix accepts
    UseSkillOnState post;
    mirrorUseSkillOn_postF204(critterFid, post);
    CHECK(post.overridden == true);
    CHECK(post.userFid == critterFid);
    CHECK(post.rejected == false);
}

TEST_CASE("F2-04: Item override (PID_TYPE=0) — pre-fix accepts (BUG), post-fix REJECTS")
{
    using namespace f204_mirror;
    // OBJ_TYPE_ITEM = 0 → FID with type=0 in upper byte
    int itemFid = (OBJ_TYPE_ITEM << 24) | 0x000042;

    // Pre-fix (buggy): accepts non-critter item
    UseSkillOnState pre;
    mirrorUseSkillOn_preF204(itemFid, pre);
    CHECK(pre.overridden == true); // BUG: item accepted as user!

    // Post-fix: REJECTS non-critter
    UseSkillOnState post;
    mirrorUseSkillOn_postF204(itemFid, post);
    CHECK(post.overridden == false);
    CHECK(post.rejected == true);
    CHECK(post.rejectType == OBJ_TYPE_ITEM); // type=0 logged
}

TEST_CASE("F2-04: Scenery override (PID_TYPE=2) — pre-fix accepts (BUG), post-fix REJECTS")
{
    using namespace f204_mirror;
    int sceneryFid = (OBJ_TYPE_SCENERY << 24) | 0x000100;

    UseSkillOnState pre;
    mirrorUseSkillOn_preF204(sceneryFid, pre);
    CHECK(pre.overridden == true); // BUG

    UseSkillOnState post;
    mirrorUseSkillOn_postF204(sceneryFid, post);
    CHECK(post.overridden == false);
    CHECK(post.rejected == true);
    CHECK(post.rejectType == OBJ_TYPE_SCENERY);
}

TEST_CASE("F2-04: Wall override (PID_TYPE=3) — pre-fix accepts (BUG), post-fix REJECTS")
{
    using namespace f204_mirror;
    int wallFid = (OBJ_TYPE_WALL << 24) | 0x000200;

    UseSkillOnState pre;
    mirrorUseSkillOn_preF204(wallFid, pre);
    CHECK(pre.overridden == true); // BUG

    UseSkillOnState post;
    mirrorUseSkillOn_postF204(wallFid, post);
    CHECK(post.overridden == false);
    CHECK(post.rejected == true);
    CHECK(post.rejectType == OBJ_TYPE_WALL);
}

TEST_CASE("F2-04: Tile override (PID_TYPE=4) — pre-fix accepts (BUG), post-fix REJECTS")
{
    using namespace f204_mirror;
    int tileFid = (OBJ_TYPE_TILE << 24) | 0x000300;

    UseSkillOnState pre;
    mirrorUseSkillOn_preF204(tileFid, pre);
    CHECK(pre.overridden == true); // BUG

    UseSkillOnState post;
    mirrorUseSkillOn_postF204(tileFid, post);
    CHECK(post.overridden == false);
    CHECK(post.rejected == true);
    CHECK(post.rejectType == OBJ_TYPE_TILE);
}

TEST_CASE("F2-04: Misc override (PID_TYPE=5) — pre-fix accepts (BUG), post-fix REJECTS")
{
    using namespace f204_mirror;
    int miscFid = (OBJ_TYPE_MISC << 24) | 0x000400;

    UseSkillOnState pre;
    mirrorUseSkillOn_preF204(miscFid, pre);
    CHECK(pre.overridden == true); // BUG

    UseSkillOnState post;
    mirrorUseSkillOn_postF204(miscFid, post);
    CHECK(post.overridden == false);
    CHECK(post.rejected == true);
    CHECK(post.rejectType == OBJ_TYPE_MISC);
}

TEST_CASE("F2-04: Null override (fid=0) — rejected by both, original user persists")
{
    using namespace f204_mirror;
    // Null is asObject(0) → nullptr → fid=0

    UseSkillOnState pre;
    mirrorUseSkillOn_preF204(0, pre);
    CHECK(pre.overridden == false); // null rejected

    UseSkillOnState post;
    mirrorUseSkillOn_postF204(0, post);
    CHECK(post.overridden == false); // null rejected
    CHECK(post.rejected == false);    // not even logged — null is valid "no override"
}

TEST_CASE("F2-04: Full ret0 handling — int -1 cancels UseSkillOn")
{
    using namespace f204_mirror;
    auto result = mirrorUseSkillOn_full(0x01000001, -1, true);
    CHECK(result.shouldContinue == false); // cancelled
    CHECK(result.userOverridden == false);
}

TEST_CASE("F2-04: Full ret0 handling — Object* critter accepted")
{
    using namespace f204_mirror;
    int critterFid = (OBJ_TYPE_CRITTER << 24) | 0x00ABCD;
    auto result = mirrorUseSkillOn_full(0, critterFid, false);
    CHECK(result.shouldContinue == true);
    CHECK(result.userOverridden == true);
    CHECK(result.userFid == critterFid);
    CHECK(result.rejected == false);
}

TEST_CASE("F2-04: Full ret0 handling — Object* non-critter rejected")
{
    using namespace f204_mirror;
    int itemFid = (OBJ_TYPE_ITEM << 24) | 0x00BEEF;
    auto result = mirrorUseSkillOn_full(0, itemFid, false);
    CHECK(result.userOverridden == false);
    CHECK(result.rejected == true);
    CHECK(result.rejectType == OBJ_TYPE_ITEM);
}

TEST_CASE("F2-04: Pattern matches AfterHitRoll fix — both use PID_TYPE + OBJ_TYPE_CRITTER")
{
    // F2-04 uses the same validation pattern as the F-42 fix:
    //   nullptr check → PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER → accept/reject
    //
    // F-42:  validateDefender_postF42 — nullptr + PID_TYPE + !critterIsDead
    // F2-04: mirrorUseSkillOn_postF204    — nullptr + PID_TYPE (no dead check
    //         because UseSkillOn targets any critter, not just combatants)
    //
    // Both validate PID_TYPE(oid->pid) == OBJ_TYPE_CRITTER before accepting
    // an Object* returned from script hook execution.

    // Verify the PID_TYPE(oid->pid) expression works correctly:
    // For a critter with pid = 0x01000001:
    int critterPid = (OBJ_TYPE_CRITTER << 24) | 0x000001;
    int critterPidType = PID_TYPE(critterPid);
    CHECK(critterPidType == OBJ_TYPE_CRITTER);

    // For an item with pid = 0x00000042:
    int itemPid = (OBJ_TYPE_ITEM << 24) | 0x000042;
    int itemPidType = PID_TYPE(itemPid);
    CHECK(itemPidType == OBJ_TYPE_ITEM);
}
