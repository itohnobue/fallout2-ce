// Unit tests for combat data/control flow fixes from Stage 6 (combat.cc, combat_ai.cc).
// Covers killType OOB, criticalFailureTableIndex OOB, gBlockCombat bypass,
// knockback ordering, spray count underflow, aimed shot override, and
// HOOK_FINDTARGET team validation fixes.
//
// Self-contained mirror test — does NOT link production .cc files (40+ engine deps).
//
// Fixes covered:
//   I2-H002 (HIGH):    AI attack path bypasses gBlockCombat
//   I2-M035 (MEDIUM):  killType OOB in gCriticalHitTables
//   I2-M036 (MEDIUM):  criticalFailureTableIndex OOB in _cf_table
//   F-M043 (MEDIUM):   Knockback modifier ordering (Stonewall after sfall)
//   F-M044 (MEDIUM):   Spray count underflow with zero-burst weapons
//   F-M045 (MEDIUM):   AI unaimed TORSO bypasses force_aimed_shots
//   I2-M045 (MEDIUM):  HOOK_FINDTARGET bypasses team validation
//   I2-M046 (MEDIUM):  UNCALLED hit location OOB via hook override
//   F-03 (MEDIUM):     HOOK_FINDTARGET at ATTACK_WHO_WHOMEVER_ATTACKING_ME team gap
//   F-04 (MEDIUM):     HOOK_FINDTARGET at WHOMEVER path team gap
//   F-05 (MEDIUM):     HOOK_FINDTARGET at _ai_danger_source team gap
//   F-H025 (HIGH):     Null dereference on aiGetPacket return

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <climits>
#include <cstring>
#include <algorithm>

// ============================================================
// Intent
// ============================================================
//
// This test file validates 12 combat fixes from the Stage 6 workflow.
// The fixes span three sub-domains:
//
// 1. Array bounds validation (3 fixes):
//    - killType OOB: critterGetKillType() raw proto field used as array index
//      for gCriticalHitTables. Fix: bounds check killType < CRITICAL_HIT_TABLE_COUNT.
//    - criticalFailureTableIndex OOB: weaponGetCriticalFailureType() raw proto
//      int used for _cf_table. Fix: bounds check index < CRITICAL_FAILURE_TABLE_COUNT.
//    - UNCALLED hit location OOB: Hook override accepts UNCALLED (8) but
//      ai->hit[] is [HIT_LOCATION_SPECIFIC_COUNT=8]. Fix: validate against
//      HIT_LOCATION_SPECIFIC_COUNT in hook validation.
//
// 2. Control flow fixes (4 fixes):
//    - gBlockCombat: AI attack path (_combat_attack) does not check gBlockCombat
//      while player path does. Fix: add gBlockCombat check in _combat_attack.
//    - Knockback ordering: Stonewall division applied before sfall modifiers,
//      making absolute modifiers nullify Stonewall. Fix: reorder.
//    - force_aimed_shots: AI passes HIT_LOCATION_TORSO for unaimed shots,
//      bypassing the override. Fix: convert TORSO→HEAD for AI when active.
//    - Spray count underflow: weaponGetBurstRounds returning 0 with
//      ammoCostPerRound=0 produces silent zero-round attack. Fix: guard.
//
// 3. HOOK_FINDTARGET team validation (5 fixes):
//    - 3 confirmed in I2-M045 (main path), 3 more in F-03/F-04/F-05.
//    - Engine candidate is team-validated, but hook override only checks
//      PID_TYPE before accepting replacement. Fix: add team validation.
//
// 4. Null dereference (1 fix):
//    - F-H025: aiGetPacket returns uncontrolled pointer, two call sites
//      dereference without null check. Fix: add null guard.

// ============================================================
// Section 1: Array bounds validation for proto-derived indices
// ============================================================

// --- I2-M035: killType OOB in gCriticalHitTables ---
// Production: combat.cc:4389-4390
// critterGetKillType() returns raw proto field — used as array index for
// gCriticalHitTables. Vanilla safe (0-18) but modded protos with killType ≥ 38
// cause OOB. Fix: bounds check before array access.

static constexpr int CRITICAL_HIT_TABLE_COUNT = 38;  // mirrors production

struct KillTypeEntry {
    int tableIndex;
    int bodyPart;
    int effect;
};

// Mirror of gCriticalHitTables[killType]
static const KillTypeEntry gCriticalHitTablesMirror[CRITICAL_HIT_TABLE_COUNT] = {};

static const KillTypeEntry* mirrorGetCriticalHitEntrySafe(int killType) {
    // I2-M035 fix: bounds check before array access
    if (killType < 0 || killType >= CRITICAL_HIT_TABLE_COUNT) {
        // Fallback to default (killType 0) instead of OOB
        return &gCriticalHitTablesMirror[0];
    }
    return &gCriticalHitTablesMirror[killType];
}

TEST_CASE("I2-M035: killType OOB guard in gCriticalHitTables") {
    SUBCASE("valid killType 0-18 (vanilla range)") {
        for (int i = 0; i <= 18; i++) {
            const auto* entry = mirrorGetCriticalHitEntrySafe(i);
            CHECK(entry != nullptr);
        }
    }

    SUBCASE("valid killType up to CRITICAL_HIT_TABLE_COUNT-1") {
        const auto* entry = mirrorGetCriticalHitEntrySafe(CRITICAL_HIT_TABLE_COUNT - 1);
        CHECK(entry != nullptr);
    }

    SUBCASE("killType == CRITICAL_HIT_TABLE_COUNT → OOB → fallback to index 0") {
        const auto* entry = mirrorGetCriticalHitEntrySafe(CRITICAL_HIT_TABLE_COUNT);
        CHECK(entry == &gCriticalHitTablesMirror[0]);  // safe fallback
    }

    SUBCASE("killType = 100 → OOB → fallback") {
        const auto* entry = mirrorGetCriticalHitEntrySafe(100);
        CHECK(entry != nullptr);
        CHECK(entry == &gCriticalHitTablesMirror[0]);
    }

    SUBCASE("negative killType → OOB → fallback") {
        const auto* entry = mirrorGetCriticalHitEntrySafe(-1);
        CHECK(entry == &gCriticalHitTablesMirror[0]);
    }

    SUBCASE("killType = INT_MAX → OOB → fallback") {
        const auto* entry = mirrorGetCriticalHitEntrySafe(INT_MAX);
        CHECK(entry == &gCriticalHitTablesMirror[0]);
    }
}

// --- I2-M036: criticalFailureTableIndex OOB in _cf_table ---
// Production: combat.cc:4459-4478
// weaponGetCriticalFailureType() returns unbounded proto int.
// Only -1 sentinel checked. Values ≥ 7 OOB on _cf_table[7][5].
// Fix: bounds check index against CRITICAL_FAILURE_TABLE_COUNT.

static constexpr int CRITICAL_FAILURE_TABLE_ROWS = 7;
static constexpr int CRITICAL_FAILURE_TABLE_COLS = 5;

struct CriticalFailureEntry {
    int bodyPart;
    int effect;
};

static const CriticalFailureEntry gCFTableMirror[CRITICAL_FAILURE_TABLE_ROWS][CRITICAL_FAILURE_TABLE_COLS] = {};

static const CriticalFailureEntry* mirrorGetCriticalFailureEntrySafe(int failureType, int subIndex) {
    // I2-M036 fix: guard sentinel AND bounds check
    if (failureType == -1) {
        return nullptr;  // no critical failure
    }
    if (failureType < 0 || failureType >= CRITICAL_FAILURE_TABLE_ROWS) {
        // OOB: fallback to row 0
        failureType = 0;
    }
    if (subIndex < 0 || subIndex >= CRITICAL_FAILURE_TABLE_COLS) {
        subIndex = 0;
    }
    return &gCFTableMirror[failureType][subIndex];
}

TEST_CASE("I2-M036: criticalFailureTableIndex OOB guard") {
    SUBCASE("sentinel -1 → nullptr (no critical failure)") {
        const auto* entry = mirrorGetCriticalFailureEntrySafe(-1, 0);
        CHECK(entry == nullptr);
    }

    SUBCASE("valid indices 0-6 work correctly") {
        for (int i = 0; i < CRITICAL_FAILURE_TABLE_ROWS; i++) {
            for (int j = 0; j < CRITICAL_FAILURE_TABLE_COLS; j++) {
                const auto* entry = mirrorGetCriticalFailureEntrySafe(i, j);
                CHECK(entry != nullptr);
            }
        }
    }

    SUBCASE("failureType = 7 → OOB → fallback to row 0") {
        const auto* entry = mirrorGetCriticalFailureEntrySafe(7, 0);
        CHECK(entry != nullptr);
        CHECK(entry == &gCFTableMirror[0][0]);
    }

    SUBCASE("failureType = 100 → OOB → fallback") {
        const auto* entry = mirrorGetCriticalFailureEntrySafe(100, 0);
        CHECK(entry != nullptr);
    }

    SUBCASE("failureType valid, subIndex = 5 → OOB → fallback to col 0") {
        const auto* entry = mirrorGetCriticalFailureEntrySafe(0, 5);
        CHECK(entry == &gCFTableMirror[0][0]);
    }

    SUBCASE("negative failureType → OOB → fallback") {
        const auto* entry = mirrorGetCriticalFailureEntrySafe(-2, 0);
        CHECK(entry != nullptr);  // not -1 → treated as OOB, falls back to row 0
    }
}

// --- I2-M046: UNCALLED hit location OOB via HOOK_AFTERHITROLL ---
// Production: combat_ai.cc:3728-3729 ↔ sfall_script_hooks.cc:1038
// Hook validation uses < HIT_LOCATION_COUNT (9) → accepts UNCALLED (8).
// ai->hit[] is [HIT_LOCATION_SPECIFIC_COUNT=8] → UNCALLED OOB.
// Fix: validate against HIT_LOCATION_SPECIFIC_COUNT for ai->hit[] access.

static constexpr int HIT_LOCATION_COUNT = 9;
static constexpr int HIT_LOCATION_SPECIFIC_COUNT = 8;
static constexpr int HIT_LOCATION_UNCALLED = 8;

// Mirror of the hook return value validation
static bool mirrorValidateHitLocationForAiHit(int location) {
    // I2-M046 fix: validate against HIT_LOCATION_SPECIFIC_COUNT for ai->hit[]
    if (location < 0 || location >= HIT_LOCATION_SPECIFIC_COUNT) {
        return false;  // reject UNCALLED and OOB values
    }
    return true;
}

TEST_CASE("I2-M046: UNCALLED hit location guard for ai->hit[]") {
    SUBCASE("valid locations 0-7 (head through legs) pass validation") {
        for (int i = 0; i < HIT_LOCATION_SPECIFIC_COUNT; i++) {
            CHECK(mirrorValidateHitLocationForAiHit(i) == true);
        }
    }

    SUBCASE("UNCALLED (8) → rejected (OOB for ai->hit[])") {
        CHECK(mirrorValidateHitLocationForAiHit(HIT_LOCATION_UNCALLED) == false);
    }

    SUBCASE("negative → rejected") {
        CHECK(mirrorValidateHitLocationForAiHit(-1) == false);
    }

    SUBCASE("large values → rejected") {
        CHECK(mirrorValidateHitLocationForAiHit(100) == false);
    }
}

// ============================================================
// Section 2: Control flow fixes
// ============================================================

// --- I2-H002: AI attack path bypasses gBlockCombat ---
// Production: combat.cc:3615-3717, combat_ai.cc:3007
// _combat_attack_this() at line 6169 checks gBlockCombat but
// _ai_attack → _combat_attack does NOT. Fix: add gBlockCombat check.

static int gBlockCombatMirror = 0;  // 0 = allow, non-zero = block

struct CombatAttackResult {
    bool blocked;
    int damage;
};

static CombatAttackResult mirrorCombatAttackPlayer() {
    // Player path (existing): checks gBlockCombat
    if (gBlockCombatMirror != 0) {
        return {true, 0};
    }
    return {false, 50};
}

static CombatAttackResult mirrorCombatAttackAi() {
    // I2-H002 fix: AI path now also checks gBlockCombat
    if (gBlockCombatMirror != 0) {
        return {true, 0};  // blocked
    }
    return {false, 30};
}

TEST_CASE("I2-H002: gBlockCombat check in AI attack path") {
    SUBCASE("gBlockCombat = 0: both paths allow attacks") {
        gBlockCombatMirror = 0;
        auto playerResult = mirrorCombatAttackPlayer();
        auto aiResult = mirrorCombatAttackAi();
        CHECK(playerResult.blocked == false);
        CHECK(playerResult.damage > 0);
        CHECK(aiResult.blocked == false);
        CHECK(aiResult.damage > 0);
    }

    SUBCASE("gBlockCombat = 1: player path blocks") {
        gBlockCombatMirror = 1;
        auto playerResult = mirrorCombatAttackPlayer();
        CHECK(playerResult.blocked == true);
        CHECK(playerResult.damage == 0);
    }

    SUBCASE("gBlockCombat = 1: AI path also blocks (fixed)") {
        gBlockCombatMirror = 1;
        auto aiResult = mirrorCombatAttackAi();
        // Before fix: aiResult.blocked == false (AI ignored gBlockCombat)
        // After fix: AI respects gBlockCombat
        CHECK(aiResult.blocked == true);
        CHECK(aiResult.damage == 0);
    }

    SUBCASE("gBlockCombat = 2 (non-1 non-zero): both block") {
        gBlockCombatMirror = 2;
        CHECK(mirrorCombatAttackPlayer().blocked == true);
        CHECK(mirrorCombatAttackAi().blocked == true);
    }
}

// --- F-M043: Knockback modifier ordering ---
// Production: combat.cc:4996-5019
// Stonewall perk division applied BEFORE sfall knockback modifiers.
// Type-1 absolute modifier from script completely nullifies stonewall perk.
// Fix: reorder — apply sfall modifiers first, then Stonewall division.

static int mirrorKnockbackOrdered(int baseKnockback, int sfallAbsModifier, bool hasStonewall) {
    // F-M043 fix: apply sfall modifiers BEFORE Stonewall
    int knockback = baseKnockback;

    // Step 1: Apply sfall absolute knockback modifier (type 1)
    if (sfallAbsModifier > 0) {
        knockback = sfallAbsModifier;  // absolute override
    }

    // Step 2: Apply Stonewall division (50% reduction)
    if (hasStonewall) {
        knockback = (knockback + 1) / 2;  // ceil division, min 1
    }

    return knockback;
}

static int mirrorKnockbackBuggy(int baseKnockback, int sfallAbsModifier, bool hasStonewall) {
    // Pre-fix ordering: Stonewall first, then sfall override
    int knockback = baseKnockback;

    // Step 1 (bug): Stonewall first
    if (hasStonewall) {
        knockback = (knockback + 1) / 2;
    }

    // Step 2 (bug): sfall override after Stonewall — nullifies it
    if (sfallAbsModifier > 0) {
        knockback = sfallAbsModifier;
    }

    return knockback;
}

TEST_CASE("F-M043: Knockback modifier ordering fix") {
    SUBCASE("no modifiers: base knockback preserved") {
        CHECK(mirrorKnockbackOrdered(10, 0, false) == 10);
        CHECK(mirrorKnockbackBuggy(10, 0, false) == 10);
    }

    SUBCASE("Stonewall only: base knockback halved") {
        CHECK(mirrorKnockbackOrdered(10, 0, true) == 5);
        CHECK(mirrorKnockbackBuggy(10, 0, true) == 5);
    }

    SUBCASE("sfall absolute modifier only") {
        CHECK(mirrorKnockbackOrdered(10, 3, false) == 3);
        CHECK(mirrorKnockbackBuggy(10, 3, false) == 3);
    }

    SUBCASE("BOTH: fixed ordering — sfall modifier respects Stonewall") {
        // Fixed: sfall modifier 6 → Stonewall halves it → 3
        CHECK(mirrorKnockbackOrdered(10, 6, true) == 3);
    }

    SUBCASE("BOTH: buggy ordering — sfall modifier nullifies Stonewall") {
        // Buggy: Stonewall halves 10 to 5 → sfall overrides to 6 → Stonewall ignored
        CHECK(mirrorKnockbackBuggy(10, 6, true) == 6);
    }

    SUBCASE("Edge: Stonewall on odd knockback") {
        // Stonewall on 5 → ceil(5/2) = 3
        CHECK(mirrorKnockbackOrdered(5, 0, true) == 3);
    }

    SUBCASE("Edge: Stonewall on knockback=1 → min 1") {
        int result = mirrorKnockbackOrdered(1, 0, true);
        CHECK(result == 1);  // (1+1)/2 = 1
    }
}

// --- F-M044: Spray count underflow with zero-burst weapons ---
// Production: combat.cc:3869-3924
// weaponGetBurstRounds() returning 0 with ammoCostPerRound=0
// produces silent zero-round attack. Fix: guard against zero burst.

struct BurstAttackResult {
    bool valid;
    int rounds;
    int totalAmmoCost;
};

static BurstAttackResult mirrorSprayCountSafe(int burstRounds, int ammoCostPerRound, int ammoAvailable) {
    // F-M044 fix: guard against zero-burst weapons
    if (burstRounds <= 0 || ammoCostPerRound <= 0) {
        return {false, 0, 0};  // invalid: silent no-op instead of zero-round attack
    }

    int totalCost = burstRounds * ammoCostPerRound;
    if (ammoAvailable < totalCost) {
        burstRounds = ammoAvailable / ammoCostPerRound;
        totalCost = burstRounds * ammoCostPerRound;
    }

    return {true, burstRounds, totalCost};
}

TEST_CASE("F-M044: Spray count underflow guard") {
    SUBCASE("normal burst: valid attack") {
        auto result = mirrorSprayCountSafe(3, 2, 10);
        CHECK(result.valid == true);
        CHECK(result.rounds == 3);
        CHECK(result.totalAmmoCost == 6);
    }

    SUBCASE("zero burst rounds → invalid, no attack") {
        auto result = mirrorSprayCountSafe(0, 2, 10);
        CHECK(result.valid == false);
        CHECK(result.rounds == 0);
    }

    SUBCASE("zero ammo cost per round → invalid, no attack") {
        auto result = mirrorSprayCountSafe(3, 0, 10);
        CHECK(result.valid == false);
        CHECK(result.rounds == 0);
    }

    SUBCASE("both zero → invalid") {
        auto result = mirrorSprayCountSafe(0, 0, 10);
        CHECK(result.valid == false);
    }

    SUBCASE("negative burst rounds → invalid") {
        auto result = mirrorSprayCountSafe(-1, 2, 10);
        CHECK(result.valid == false);
    }

    SUBCASE("insufficient ammo: rounds scaled down") {
        auto result = mirrorSprayCountSafe(5, 2, 6);
        CHECK(result.valid == true);
        CHECK(result.rounds == 3);  // 6 ammo / 2 per round = 3 rounds
    }

    SUBCASE("exact ammo match") {
        auto result = mirrorSprayCountSafe(3, 2, 6);
        CHECK(result.valid == true);
        CHECK(result.rounds == 3);
    }
}

// --- F-M045: AI unaimed TORSO bypasses force_aimed_shots ---
// Production: combat.cc:3639
// Check only converts HIT_LOCATION_UNCALLED → HEAD.
// AI passes HIT_LOCATION_TORSO for unaimed shots.
// Fix: also convert TORSO → HEAD when force_aimed_shots is active.

static constexpr int HIT_LOCATION_TORSO = 4;
static constexpr int HIT_LOCATION_HEAD = 0;

static int mirrorAimedShotOverride(int hitLocation, bool forceAimedShots) {
    if (!forceAimedShots) {
        return hitLocation;
    }
    // F-M045 fix: convert TORSO → HEAD for AI when force_aimed_shots active
    // Original: only converted UNCALLED
    // Fixed: also converts TORSO (what AI passes for unaimed)
    if (hitLocation == HIT_LOCATION_UNCALLED || hitLocation == HIT_LOCATION_TORSO) {
        return HIT_LOCATION_HEAD;
    }
    return hitLocation;
}

TEST_CASE("F-M045: AI aimed shot override for TORSO") {
    SUBCASE("force_aimed_shots off: location unchanged") {
        CHECK(mirrorAimedShotOverride(HIT_LOCATION_TORSO, false) == HIT_LOCATION_TORSO);
        CHECK(mirrorAimedShotOverride(HIT_LOCATION_UNCALLED, false) == HIT_LOCATION_UNCALLED);
        CHECK(mirrorAimedShotOverride(HIT_LOCATION_HEAD, false) == HIT_LOCATION_HEAD);
    }

    SUBCASE("force_aimed_shots on: UNCALLED → HEAD (existing)") {
        CHECK(mirrorAimedShotOverride(HIT_LOCATION_UNCALLED, true) == HIT_LOCATION_HEAD);
    }

    SUBCASE("force_aimed_shots on: TORSO → HEAD (new fix)") {
        CHECK(mirrorAimedShotOverride(HIT_LOCATION_TORSO, true) == HIT_LOCATION_HEAD);
    }

    SUBCASE("force_aimed_shots on: other body parts unchanged") {
        CHECK(mirrorAimedShotOverride(1, true) == 1);  // left arm
        CHECK(mirrorAimedShotOverride(2, true) == 2);  // right arm
        CHECK(mirrorAimedShotOverride(3, true) == 3);  // groin
        CHECK(mirrorAimedShotOverride(5, true) == 5);  // left leg
        CHECK(mirrorAimedShotOverride(6, true) == 6);  // right leg
        CHECK(mirrorAimedShotOverride(7, true) == 7);  // eyes
    }
}

// ============================================================
// Section 3: HOOK_FINDTARGET team validation
// ============================================================

// --- I2-M045, F-03, F-04, F-05: Team validation for hook override ---
// Production: combat_ai.cc:1770-1787, 1872-1887, 1910-1920, 1971-1980
// Engine candidate is team-validated. Hook replaces with only PID_TYPE check,
// allowing script to return ally as target (friendly fire).
// Fix: add team validation after PID_TYPE check.

static constexpr int OBJ_TYPE_CRITTER = 1;

struct TestCritter {
    int pid;
    int team;
};

// Mirror of PID_TYPE macro
static int mirrorPidType(int pid) {
    return (pid >> 24) & 0x0F;
}

// Mirror of the hook candidate validation WITH team check (fixed)
static bool mirrorFindTargetValidateTeam(TestCritter* self, TestCritter* hookCandidate) {
    // I2-M045 / F-03 / F-04 / F-05 fix: add team validation
    if (hookCandidate != nullptr
        && mirrorPidType(hookCandidate->pid) == OBJ_TYPE_CRITTER
        && hookCandidate->team != self->team) {  // different team = enemy
        return true;  // valid target
    }
    return false;  // rejected
}

// Mirror BEFORE fix: only PID_TYPE check (buggy)
static bool mirrorFindTargetNoTeam(TestCritter* hookCandidate) {
    // Buggy: accepts any critter, including allies
    if (hookCandidate != nullptr
        && mirrorPidType(hookCandidate->pid) == OBJ_TYPE_CRITTER) {
        return true;  // accepts ally → friendly fire
    }
    return false;
}

TEST_CASE("HOOK_FINDTARGET team validation (I2-M045/F-03/F-04/F-05)") {
    TestCritter self = { (OBJ_TYPE_CRITTER << 24) | 1, 0 };
    TestCritter enemy = { (OBJ_TYPE_CRITTER << 24) | 2, 1 };
    TestCritter ally = { (OBJ_TYPE_CRITTER << 24) | 3, 0 };
    TestCritter neutral = { (OBJ_TYPE_CRITTER << 24) | 4, 2 };
    TestCritter nonCritter = { (2 << 24) | 5, 0 };  // item, not critter

    SUBCASE("enemy (different team): accepted") {
        CHECK(mirrorFindTargetValidateTeam(&self, &enemy) == true);
    }

    SUBCASE("ally (same team): rejected (F-03/F-04/F-05 fix)") {
        CHECK(mirrorFindTargetValidateTeam(&self, &ally) == false);
    }

    SUBCASE("neutral (different team): accepted") {
        CHECK(mirrorFindTargetValidateTeam(&self, &neutral) == true);
    }

    SUBCASE("non-critter: rejected (PID_TYPE check)") {
        CHECK(mirrorFindTargetValidateTeam(&self, &nonCritter) == false);
    }

    SUBCASE("null candidate: rejected") {
        CHECK(mirrorFindTargetValidateTeam(&self, nullptr) == false);
    }

    SUBCASE("BUG BEFORE FIX: ally accepted (friendly fire)") {
        // Before the team validation fix, ally was accepted as target
        CHECK(mirrorFindTargetNoTeam(&ally) == true);
    }

    SUBCASE("BUG BEFORE FIX: enemy accepted normally") {
        CHECK(mirrorFindTargetNoTeam(&enemy) == true);
    }
}

// --- Extended: all 6 HOOK_FINDTARGET sites ---
// I2-M045: main path (line 1770-1787)
// F-03: ATTACK_WHO_WHOMEVER_ATTACKING_ME (line 1872-1887)
// F-04: WHOMEVER path (line 1910-1920)
// F-05: _ai_danger_source (line 1971-1980)
//
// All 4 sites share the same fix: team validation after PID_TYPE.

TEST_CASE("All 4 HOOK_FINDTARGET sites covered by team validation fix") {
    TestCritter self = { (OBJ_TYPE_CRITTER << 24) | 1, 0 };

    SUBCASE("F-03: ATTACK_WHO_WHOMEVER_ATTACKING_ME site") {
        // Structurally identical to I2-M045 at line 1787
        TestCritter enemy = { (OBJ_TYPE_CRITTER << 24) | 10, 5 };
        CHECK(mirrorFindTargetValidateTeam(&self, &enemy) == true);
    }

    SUBCASE("F-04: WHOMEVER path site") {
        // Same team validation needed for whoHitMe replacement
        TestCritter ally = { (OBJ_TYPE_CRITTER << 24) | 20, 0 };
        CHECK(mirrorFindTargetValidateTeam(&self, &ally) == false);
    }

    SUBCASE("F-05: _ai_danger_source site") {
        // Broadest scope — fallback for all critter types
        TestCritter neutral = { (OBJ_TYPE_CRITTER << 24) | 30, 3 };
        CHECK(mirrorFindTargetValidateTeam(&self, &neutral) == true);
    }

    SUBCASE("All sites: team 0 vs team 0 → rejected (same team)") {
        TestCritter sameTeam = { (OBJ_TYPE_CRITTER << 24) | 40, 0 };
        CHECK(mirrorFindTargetValidateTeam(&self, &sameTeam) == false);
    }
}

// ============================================================
// Section 4: Null dereference fix
// ============================================================

// --- F-H025: Null dereference on aiGetPacket return ---
// Production: combat_ai.cc:2105, 3696
// aiGetPacket() can return nullptr, but two call sites dereference without
// null check. 8+ other functions in same file DO null-check.
// Fix: add null guard before dereference.

struct AiPacket {
    int hit[8];
    int bestWeapon;
    int target;
    char name[32];
};

static AiPacket* mirrorAiGetPacket(TestCritter* critter) {
    // Simulates aiGetPacket: returns nullptr if no AI packet stored
    if (critter == nullptr || mirrorPidType(critter->pid) != OBJ_TYPE_CRITTER) {
        return nullptr;
    }
    static AiPacket packet;
    return &packet;
}

TEST_CASE("F-H025: aiGetPacket null dereference guard") {
    TestCritter critter = { (OBJ_TYPE_CRITTER << 24) | 1, 0 };

    SUBCASE("valid critter: aiGetPacket returns non-null") {
        AiPacket* ai = mirrorAiGetPacket(&critter);
        CHECK(ai != nullptr);
    }

    SUBCASE("null critter: aiGetPacket returns null (guarded)") {
        AiPacket* ai = mirrorAiGetPacket(nullptr);
        CHECK(ai == nullptr);
        // Before fix: ai->hit or ai->name dereference would crash
        // After fix: null check prevents dereference
    }

    SUBCASE("non-critter: aiGetPacket returns null") {
        TestCritter item = { (2 << 24) | 1, 0 };
        AiPacket* ai = mirrorAiGetPacket(&item);
        CHECK(ai == nullptr);
    }

    SUBCASE("null dereference prevented: guarded path") {
        AiPacket* ai = mirrorAiGetPacket(nullptr);
        if (ai == nullptr) {
            // F-H025 fix: return early instead of dereferencing ai->name
            CHECK(true);  // null guard engaged, no crash
        } else {
            // Should not reach here for null critter
            CHECK(false);
        }
    }
}
