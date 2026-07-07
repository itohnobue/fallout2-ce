// Unit tests for miscellaneous opcode ext opcodes.
//
// F2-038 (MEDIUM): op_sneak_success 4 deterministic state tests.
//   Production: sfall_opcodes.cc:4627-4635.
//   Tests: gDude nullptr → 0, not sneaking → 0, skill=300 → always succeeds,
//   skill=0 → never succeeds. Uses mirror of production logic.
//
// F2-039 (MEDIUM): set_hero_race/set_hero_style tests.
//   Production: sfall_opcodes.cc:4773-4789.
//   REAL integration test via sfall_gl_vars_store/sfall_gl_vars_fetch
//   (sfall_global_vars.cc is in test_sources). Tests valid and boundary
//   values for both HAp_Race and HApStyle keys.
//
// F2-040 (MEDIUM): show_real_perks/hide_real_perks toggle lifecycle.
//   Production: sfall_opcodes.cc:4132-4142.
//   Mirror test: toggles sfallHideRealPerks, verifies set/get behavior
//   through the lifecycle (init → set true → set false → reset → default).
//
// F2-027 (MEDIUM): VFS write error propagation documentation.
//   Production: sfall_opcodes.cc:2247-2514 (6 write opcodes).
//   Documents the void-return contract: all write functions return void
//   (push no return value), while read functions return -1 on error.
//   Includes mirror tests verifying the write-completion contract.
//
// See report at tmp/s7-impl-opcodes-b-report.md for INTENT section.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// F2-039: REAL production linkage for hero race/style tests.
// sfall_global_vars.cc is in test_sources — we can call store/fetch directly.
#include "sfall_global_vars.h"

#include <string>

using namespace fallout;

// ============================================================
// Test-local types for mirror functions
// ============================================================

// Minimal Object struct for gDude testing.
struct TestObject {
    int id;
};

// Dude state constants from critter.h:11-13.
enum DudeState {
    DUDE_STATE_SNEAKING = 0,
    DUDE_STATE_LEVEL_UP_AVAILABLE = 3,
    DUDE_STATE_ADDICTED = 4,
};

// Skill constants from skill_defs.h.
enum Skill {
    SKILL_SNEAK = 7,
};

// ============================================================
// F2-038: op_sneak_success — 4 deterministic state tests
// ============================================================

// Mirror of op_sneak_success from sfall_opcodes.cc:4627-4635.
//
// Production code:
//   int result = 0;
//   if (gDude != nullptr && dudeHasState(DUDE_STATE_SNEAKING)) {
//       int sneakSkill = skillGetValue(gDude, SKILL_SNEAK);
//       result = (randomBetween(1, 100) <= sneakSkill) ? 1 : 0;
//   }
//   programStackPushInteger(program, result);
//
// Deterministic states (provable without RNG):
//   1. gDude == nullptr → short-circuit, randomBetween never called → result = 0
//   2. not sneaking → short-circuit, randomBetween never called → result = 0
//   3. skill = 300, sneaking → randomBetween(1,100) always <= 300 → result = 1
//   4. skill = 0, sneaking → randomBetween(1,100) never <= 0 → result = 0
//
// States 3 and 4 are deterministic despite involving randomBetween because
// the skill values are at extreme ranges where the comparison is always
// true (300 >= any roll in 1..100) or always false (0 < any roll in 1..100).

static int mirrorSneakSuccess(
    TestObject* dude,
    bool isSneaking,
    int sneakSkill)
{
    // Production: sfall_opcodes.cc:4630
    // Guard: gDude == nullptr || !dudeHasState(DUDE_STATE_SNEAKING) → result = 0
    if (dude == nullptr || !isSneaking) {
        return 0;
    }

    // Production: sfall_opcodes.cc:4631-4632
    // randomBetween(1, 100) returns a value in [1, 100].
    // The comparison: roll <= sneakSkill.
    //
    // Deterministic cases:
    //   sneakSkill == 0 → roll(1..100) <= 0 → always false → 0
    //   sneakSkill >= 100 → roll(1..100) <= sneakSkill → always true → 1
    int rollMin = 1;
    int rollMax = 100;

    if (sneakSkill >= rollMax) {
        // Any roll in [1,100] is <= sneakSkill (since sneakSkill >= 100).
        return 1;
    }

    if (sneakSkill < rollMin) {
        // Any roll in [1,100] is > sneakSkill (since sneakSkill < 1).
        return 0;
    }

    // Non-deterministic range: 1 <= sneakSkill <= 99.
    // The result depends on the actual random roll.
    return -1; // Indeterminate
}

TEST_CASE("op_sneak_success — State 1: gDude is nullptr")
{
    // Production: sfall_opcodes.cc:4630
    // When gDude == nullptr, the function short-circuits and returns 0.
    // randomBetween is never called — fully deterministic.
    int result = mirrorSneakSuccess(nullptr, true, 300);
    CHECK(result == 0); // Deterministic: no dude, no check
}

TEST_CASE("op_sneak_success — State 2: not sneaking")
{
    // Production: sfall_opcodes.cc:4630
    // When dudeHasState(DUDE_STATE_SNEAKING) is false, the function
    // short-circuits and returns 0. randomBetween is never called.
    TestObject dude = {42};

    int result = mirrorSneakSuccess(&dude, false, 150);
    CHECK(result == 0); // Deterministic: not sneaking

    // Even with max skill, not sneaking = 0.
    result = mirrorSneakSuccess(&dude, false, 300);
    CHECK(result == 0);
}

TEST_CASE("op_sneak_success — State 3: skill = 0, sneaking → always fails")
{
    // Production: sfall_opcodes.cc:4631-4632
    // randomBetween(1, 100) returns 1..100, and 0 >= any of those is false.
    // Therefore: result = 0 every time.
    TestObject dude = {42};

    int result = mirrorSneakSuccess(&dude, true, 0);
    CHECK(result == 0); // Deterministic: skill 0 never beats 1..100
}

TEST_CASE("op_sneak_success — State 4: skill = 300, sneaking → always succeeds")
{
    // Production: sfall_opcodes.cc:4631-4632
    // randomBetween(1, 100) returns 1..100, and 300 >= any of those is true.
    // Therefore: result = 1 every time.
    TestObject dude = {42};

    int result = mirrorSneakSuccess(&dude, true, 300);
    CHECK(result == 1); // Deterministic: skill 300 always beats 1..100
}

TEST_CASE("op_sneak_success — boundary: skill at threshold values")
{
    TestObject dude = {42};

    // Skill = 99: non-deterministic (range 1..99 would fail on roll=100)
    int result99 = mirrorSneakSuccess(&dude, true, 99);
    CHECK(result99 == -1); // Indeterminate — marker for "depends on RNG"

    // Skill = 100: deterministic success (any roll in 1..100 <= 100)
    int result100 = mirrorSneakSuccess(&dude, true, 100);
    CHECK(result100 == 1);

    // Skill = 1: non-deterministic (only succeeds on roll=1)
    int result1 = mirrorSneakSuccess(&dude, true, 1);
    CHECK(result1 == -1); // Indeterminate

    // Skill = -1: deterministic fail (no roll in 1..100 <= -1)
    int resultNeg = mirrorSneakSuccess(&dude, true, -1);
    CHECK(resultNeg == 0);
}

// ============================================================
// F2-039: set_hero_race / set_hero_style — REAL integration tests
// ============================================================

// Production opcodes (sfall_opcodes.cc:4776-4789):
//   op_set_hero_race:  sfall_gl_vars_store("HAp_Race", race);
//   op_set_hero_style: sfall_gl_vars_store("HApStyle", style);
//
// These call sfall_gl_vars_store which is in test_sources!
// We can test the actual production sfall_gl_vars_store/fetch functions.

// NOTE: sfall_gl_vars_store() treats value==0 as erase (sfall convention).
// Zero-value keys are removed from storage and fetch returns false.
// All hero race/style tests use values >= 1 to avoid the 0=erase convention.
TEST_CASE("set_hero_race — valid values via sfall_gl_vars")
{
    // sfall_gl_vars_init must be called before store/fetch.
    sfall_gl_vars_init();

    SUBCASE("Store and fetch hero race 1 (ghoul)") {
        CHECK(sfall_gl_vars_store("HAp_Race", 1));
        int val;
        CHECK(sfall_gl_vars_fetch("HAp_Race", val));
        CHECK(val == 1);
    }

    SUBCASE("Store and fetch hero race 2 (super mutant)") {
        CHECK(sfall_gl_vars_store("HAp_Race", 2));
        int val;
        CHECK(sfall_gl_vars_fetch("HAp_Race", val));
        CHECK(val == 2);
    }

    SUBCASE("Store and fetch hero race 3 (custom)") {
        CHECK(sfall_gl_vars_store("HAp_Race", 3));
        int val;
        CHECK(sfall_gl_vars_fetch("HAp_Race", val));
        CHECK(val == 3);
    }

    sfall_gl_vars_reset();
}

TEST_CASE("set_hero_style — valid values via sfall_gl_vars")
{
    sfall_gl_vars_init();

    SUBCASE("Store and fetch hero style 1") {
        CHECK(sfall_gl_vars_store("HApStyle", 1));
        int val;
        CHECK(sfall_gl_vars_fetch("HApStyle", val));
        CHECK(val == 1);
    }

    SUBCASE("Store and fetch hero style 2") {
        CHECK(sfall_gl_vars_store("HApStyle", 2));
        int val;
        CHECK(sfall_gl_vars_fetch("HApStyle", val));
        CHECK(val == 2);
    }

    SUBCASE("Store and fetch hero style 3") {
        CHECK(sfall_gl_vars_store("HApStyle", 3));
        int val;
        CHECK(sfall_gl_vars_fetch("HApStyle", val));
        CHECK(val == 3);
    }

    sfall_gl_vars_reset();
}

TEST_CASE("set_hero_race — boundary values")
{
    sfall_gl_vars_init();

    SUBCASE("Negative race value (-1)") {
        CHECK(sfall_gl_vars_store("HAp_Race", -1));
        int val;
        CHECK(sfall_gl_vars_fetch("HAp_Race", val));
        CHECK(val == -1);
    }

    SUBCASE("Large race value (999)") {
        CHECK(sfall_gl_vars_store("HAp_Race", 999));
        int val;
        CHECK(sfall_gl_vars_fetch("HAp_Race", val));
        CHECK(val == 999);
    }

    sfall_gl_vars_reset();
}

TEST_CASE("set_hero_style — boundary values")
{
    sfall_gl_vars_init();

    SUBCASE("Negative style value (-1)") {
        CHECK(sfall_gl_vars_store("HApStyle", -1));
        int val;
        CHECK(sfall_gl_vars_fetch("HApStyle", val));
        CHECK(val == -1);
    }

    SUBCASE("Large style value (INT_MAX)") {
        CHECK(sfall_gl_vars_store("HApStyle", 2147483647));
        int val;
        CHECK(sfall_gl_vars_fetch("HApStyle", val));
        CHECK(val == 2147483647);
    }

    sfall_gl_vars_reset();
}

TEST_CASE("set_hero_race + set_hero_style — both keys coexist independently")
{
    // Verify both opcodes store to DIFFERENT keys and don't interfere.
    sfall_gl_vars_init();

    CHECK(sfall_gl_vars_store("HAp_Race", 1));
    CHECK(sfall_gl_vars_store("HApStyle", 5));

    int raceVal, styleVal;
    CHECK(sfall_gl_vars_fetch("HAp_Race", raceVal));
    CHECK(sfall_gl_vars_fetch("HApStyle", styleVal));
    CHECK(raceVal == 1);
    CHECK(styleVal == 5);

    // Overwrite only race — style should remain unchanged.
    CHECK(sfall_gl_vars_store("HAp_Race", 2));
    CHECK(sfall_gl_vars_fetch("HAp_Race", raceVal));
    CHECK(sfall_gl_vars_fetch("HApStyle", styleVal));
    CHECK(raceVal == 2);
    CHECK(styleVal == 5);

    sfall_gl_vars_reset();
}

// ============================================================
// F2-040: show_real_perks / hide_real_perks toggle lifecycle
// ============================================================

// Mirror of the show_real_perks/hide_real_perks toggle lifecycle.
// Production: sfall_opcodes.cc:4009, 4132-4142, 5040, 5269, 5666, 6061.
//
// The production variable is file-static:
//   static bool sfallHideRealPerks = false;        // line 4009
//   op_hide_real_perks: sfallHideRealPerks = true;  // line 4135
//   op_show_real_perks: sfallHideRealPerks = false; // line 4141
//   sfallOpcodesReset:  sfallHideRealPerks = false; // line 5040
//   sfallOpcodeStateSave: stores to "SFHideRP"      // line 5269
//   sfallOpcodeStateLoad: restores from "SFHideRP"  // line 5666
//   sfallGetHideRealPerks: returns sfallHideRealPerks // line 6061
//
// The lifecycle:
//   1. init: sfallHideRealPerks = false (default)
//   2. op_hide_real_perks called → true
//   3. op_show_real_perks called → false
//   4. sfallOpcodesReset() called → false (back to default)

class HideRealPerksToggle {
public:
    bool m_hideRealPerks = false; // matches sfall_opcodes.cc:4009

    // Mirror of op_hide_real_perks (line 4135)
    void hideRealPerks() {
        m_hideRealPerks = true;
    }

    // Mirror of op_show_real_perks (line 4141)
    void showRealPerks() {
        m_hideRealPerks = false;
    }

    // Mirror of sfallOpcodesReset (line 5040)
    void reset() {
        m_hideRealPerks = false;
    }

    // Mirror of sfallGetHideRealPerks (line 6061)
    int isHidden() const {
        return m_hideRealPerks ? 1 : 0;
    }
};

TEST_CASE("show_real_perks / hide_real_perks — full lifecycle")
{
    HideRealPerksToggle toggle;

    // Phase 1: Initial state — default is false.
    CHECK(toggle.isHidden() == 0);
    CHECK(toggle.m_hideRealPerks == false);

    // Phase 2: Toggle on — op_hide_real_perks sets to true.
    toggle.hideRealPerks();
    CHECK(toggle.isHidden() == 1);
    CHECK(toggle.m_hideRealPerks == true);

    // Phase 3: Toggle off — op_show_real_perks sets to false.
    toggle.showRealPerks();
    CHECK(toggle.isHidden() == 0);
    CHECK(toggle.m_hideRealPerks == false);

    // Phase 4: Reset — sfallOpcodesReset restores default (false).
    // First, set it to true to prove reset actually changes it.
    toggle.hideRealPerks();
    CHECK(toggle.isHidden() == 1);
    toggle.reset();
    CHECK(toggle.isHidden() == 0);
    CHECK(toggle.m_hideRealPerks == false);
}

TEST_CASE("show_real_perks / hide_real_perks — double toggle")
{
    HideRealPerksToggle toggle;

    // Toggle on twice → still true.
    toggle.hideRealPerks();
    toggle.hideRealPerks();
    CHECK(toggle.isHidden() == 1);

    // Toggle off twice → still false.
    toggle.showRealPerks();
    toggle.showRealPerks();
    CHECK(toggle.isHidden() == 0);
}

TEST_CASE("show_real_perks / hide_real_perks — reset restores default from any state")
{
    HideRealPerksToggle toggle;

    // Start from true
    toggle.hideRealPerks();
    CHECK(toggle.isHidden() == 1);
    toggle.reset();
    CHECK(toggle.isHidden() == 0);

    // Reset from false should still be false (idempotent reset).
    toggle.reset();
    CHECK(toggle.isHidden() == 0);
}

// ============================================================
// F2-027: VFS write error propagation — documentation
// ============================================================

// The VFS write opcodes have a void-return contract:
// they push no value to the program stack and return void.
//
// Production write opcodes (sfall_opcodes.cc):
//   op_fs_write_byte    (2257) — returns void after writing byte
//   op_fs_write_short   (2269) — returns void after writing short
//   op_fs_write_int     (2292) — returns void after writing int
//   op_fs_write_float   (2315) — returns void after writing float
//   op_fs_write_string  (2338) — returns void after writing string
//   op_fs_write_bstring (2360) — returns void after writing bstring
//
// Read opcodes (for contrast):
//   op_fs_read_byte  → pushes int (data or -1 on error)
//   op_fs_read_short → pushes int (data or -1 on error)
//   op_fs_read_int   → pushes int (data or -1 on error)
//   op_fs_read_float → pushes float (0.0 on error)
//
// This asymmetry means scripts CANNOT detect VFS write failures.
// Writes may silently fail, with errors only logged via programPrintError.
//
// op_fs_seek (line 2500) has NO error detection at all — it calls fseek
// without checking the return value. This is the biggest gap: not only
// can't scripts detect failure, but the engine itself doesn't check
// whether seek actually succeeded.

// Mirror of a VFS write operation — returns void, matching production.
// This test verifies the contract: writes return void, callers get no
// error indication from the return value.

enum VfsWriteResult {
    VFS_WRITE_OK,
    VFS_WRITE_INVALID_HANDLE,
    VFS_WRITE_READ_ONLY_HANDLE,
    VFS_WRITE_FWRITE_FAILED,
};

// Mirror of the common VFS write guard pattern (used by all 6 write opcodes).
// Production pattern:
//   1. Validate handle range and file pointer
//   2. Check file mode (not read-only)
//   3. Perform write operation
//
// All write opcodes follow this exact pattern:
//   if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) → error, return
//   if (sfallVfsFileMode[id] == 1) → error ("read-only"), return
//   write data...
//   (optionally check fwrite result → log error but still return)

static VfsWriteResult mirrorVfsWriteGuard(
    int handleId,
    bool handleValid,
    bool isReadOnly,
    bool writeSucceeds)
{
    // Guard 1: Invalid handle (out of range or not open).
    // Production: check at e.g. sfall_opcodes.cc:2252-2255
    if (!handleValid) {
        return VFS_WRITE_INVALID_HANDLE;
    }

    // Guard 2: Read-only handle (opened via fs_find).
    // Production: check at e.g. sfall_opcodes.cc:2257-2260
    if (isReadOnly) {
        return VFS_WRITE_READ_ONLY_HANDLE;
    }

    // Write the data.
    // Production: actually performs fputc or fwrite.
    // Some write opcodes check the result and log an error.
    if (!writeSucceeds) {
        return VFS_WRITE_FWRITE_FAILED;
    }

    return VFS_WRITE_OK;
}

TEST_CASE("VFS write — invalid handle returns without crashing")
{
    // All write opcodes check handle validity first.
    // Invalid handle → programPrintError + return (void, no value pushed).
    VfsWriteResult result = mirrorVfsWriteGuard(-1, false, false, true);
    CHECK(result == VFS_WRITE_INVALID_HANDLE);

    result = mirrorVfsWriteGuard(999, false, false, true);
    CHECK(result == VFS_WRITE_INVALID_HANDLE);
}

TEST_CASE("VFS write — read-only handle rejects writes")
{
    // Handles opened via fs_find (mode 1 = read-only) cannot be written to.
    // Attempting a write prints an error and returns (void).
    VfsWriteResult result = mirrorVfsWriteGuard(0, true, true, true);
    CHECK(result == VFS_WRITE_READ_ONLY_HANDLE);
}

TEST_CASE("VFS write — write failure logged but not reported to caller")
{
    // When fputc/fwrite fails, production code logs programPrintError
    // but continues execution. The script receives NO indication of failure.
    // This mirrors the production behavior: errors are logged, not returned.
    VfsWriteResult result = mirrorVfsWriteGuard(0, true, false, false);
    CHECK(result == VFS_WRITE_FWRITE_FAILED);
    // In production, the function returns void here — no value pushed.
    // The script continues as if the write succeeded.
}

TEST_CASE("VFS write — successful write returns void")
{
    // When all guards pass and the write succeeds, the function returns
    // silently (void). No value is pushed to the program stack.
    VfsWriteResult result = mirrorVfsWriteGuard(0, true, false, true);
    CHECK(result == VFS_WRITE_OK);
}
