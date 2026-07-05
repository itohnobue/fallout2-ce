// Unit tests for sfall_opcodes.cc — public API, lifecycle, extern globals.
//
// Tests: sfallOpcodesReset, sfallAnimCallbackReset, sfallVfsCloseAll,
//        sfallOpcodesExit, hookOpcodeGetCurrentCall,
//        extern globals (gPerkFrequencyOverride, gSkillPointsPerLevelMod,
//        gLastAttacker, gLastTarget, gSkillMaxCap, gXpModPercentage).
//
// Classification:
//   UNIT:  lifecycle functions, extern globals — directly testable (this file)
//   INTEGRATION: op_set_sfall_global, op_get_sfall_global_int/float,
//                op_game_loaded, op_set_global_script_repeat/type,
//                op_set_fake_perk — need Program* mock + subsystem stubs
//   FULL: set_car_current_town, force_encounter, set_proto_data,
//         get_proto_data, tap_key, reg_anim_combat_check, party_member_list,
//         get_script, sprintf, round, div — need engine + game loop
//
// See report at tmp/s4-impl-sfall-ops-report.md for stub requirements
// and CMakeLists.txt integration instructions.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "sfall_opcodes.h"

using namespace fallout;

// ============================================================
// Helper: verify all extern globals are at their default values.
// ============================================================
static void verifyExternGlobalsDefault()
{
    CHECK(gPerkFrequencyOverride == 0);
    CHECK(gSkillPointsPerLevelMod == 0);
    CHECK(gLastAttacker == -1);
    CHECK(gLastTarget == -1);
    CHECK(gSkillMaxCap == 300);
    CHECK(gXpModPercentage == 100);
}

// ============================================================
// Helper: set all extern globals to known non-default values.
// ============================================================
static void setExternGlobalsNonDefault()
{
    gPerkFrequencyOverride = 5;
    gSkillPointsPerLevelMod = 10;
    gLastAttacker = 42;
    gLastTarget = 99;
    gSkillMaxCap = 500;
    gXpModPercentage = 200;
}

// ============================================================
// sfallOpcodesReset — reset all opcode-related global state.
// ============================================================

TEST_CASE("sfallOpcodesReset — extern globals")
{
    // Verify starting state matches documented defaults.
    // sfall_opcodes.cc initializes these in global scope:
    //   gPerkFrequencyOverride = 0   (line 2766)
    //   gSkillPointsPerLevelMod = 0  (line 2604)
    //   gLastAttacker = -1           (line 3425)
    //   gLastTarget = -1             (line 3426)
    //   gSkillMaxCap = 300           (line 2799)
    //   gXpModPercentage = 100       (line 3071)

    SUBCASE("extern globals have expected initial values")
    {
        verifyExternGlobalsDefault();
    }

    SUBCASE("reset restores extern globals to defaults after modification")
    {
        setExternGlobalsNonDefault();

        // Verify modification took effect
        CHECK(gPerkFrequencyOverride == 5);
        CHECK(gSkillPointsPerLevelMod == 10);
        CHECK(gLastAttacker == 42);
        CHECK(gLastTarget == 99);
        CHECK(gSkillMaxCap == 500);
        CHECK(gXpModPercentage == 200);

        sfallOpcodesReset();

        // All should be back to defaults per sfall_opcodes.cc lines 3505-3550
        verifyExternGlobalsDefault();
    }

    SUBCASE("double reset is safe — idempotent")
    {
        setExternGlobalsNonDefault();
        sfallOpcodesReset();
        verifyExternGlobalsDefault();

        // Second reset on already-clean state
        sfallOpcodesReset();
        verifyExternGlobalsDefault();
    }

    SUBCASE("reset clears gPerkFrequencyOverride to 0")
    {
        gPerkFrequencyOverride = 3; // set_perk_freq(3)
        sfallOpcodesReset();
        CHECK(gPerkFrequencyOverride == 0);
    }

    SUBCASE("reset clears gSkillPointsPerLevelMod to 0")
    {
        gSkillPointsPerLevelMod = 25; // mod_skill_points_per_level(25)
        sfallOpcodesReset();
        CHECK(gSkillPointsPerLevelMod == 0);
    }

    SUBCASE("reset clears gLastAttacker to -1")
    {
        gLastAttacker = 12345; // set by combat system
        sfallOpcodesReset();
        CHECK(gLastAttacker == -1);
    }

    SUBCASE("reset clears gLastTarget to -1")
    {
        gLastTarget = 67890; // set by combat system
        sfallOpcodesReset();
        CHECK(gLastTarget == -1);
    }

    SUBCASE("reset restores gSkillMaxCap to 300")
    {
        gSkillMaxCap = 999; // set_skill_max(999)
        sfallOpcodesReset();
        CHECK(gSkillMaxCap == 300);
    }

    SUBCASE("reset restores gXpModPercentage to 100")
    {
        gXpModPercentage = 50; // set_xp_mod(50)
        sfallOpcodesReset();
        CHECK(gXpModPercentage == 100);
    }
}

TEST_CASE("sfallOpcodesReset — boundary values on extern globals")
{
    SUBCASE("gXpModPercentage clamped extremes are reset to 100")
    {
        // op_set_xp_mod clamps to [0, 10000] — verify reset after clamps
        gXpModPercentage = 0; // minimum clamp
        sfallOpcodesReset();
        CHECK(gXpModPercentage == 100);

        gXpModPercentage = 10000; // maximum clamp
        sfallOpcodesReset();
        CHECK(gXpModPercentage == 100);

        // Below minimum (negative) — clamped to 0, then reset to 100
        gXpModPercentage = -1; // would be clamped to 0 by opcode
        sfallOpcodesReset();
        CHECK(gXpModPercentage == 100);
    }

    SUBCASE("gSkillMaxCap extremes are reset to 300")
    {
        // op_set_skill_max clamps negative to 300, allows any non-negative
        gSkillMaxCap = 1000;
        sfallOpcodesReset();
        CHECK(gSkillMaxCap == 300);

        gSkillMaxCap = 1;
        sfallOpcodesReset();
        CHECK(gSkillMaxCap == 300);

        gSkillMaxCap = 0;
        sfallOpcodesReset();
        CHECK(gSkillMaxCap == 300);
    }

    SUBCASE("gLastAttacker/gLastTarget reset from valid object IDs")
    {
        // These are object IDs set by combat system; valid range 0+
        gLastAttacker = 0;
        gLastTarget = 0;
        sfallOpcodesReset();
        CHECK(gLastAttacker == -1);
        CHECK(gLastTarget == -1);
    }
}

// ============================================================
// sfallOpcodesReset — knockback globals (file-static in sfall_opcodes.cc)
// ============================================================

TEST_CASE("sfallOpcodesReset — knockback state isolation")
{
    // Note: sfallWeapon/Target/AttackerKnockbackType/Value are file-static
    // in sfall_opcodes.cc and cannot be directly accessed from this test.
    // However, sfallOpcodesReset() sets them to defaults (line 3512-3518):
    //   sfallWeaponKnockbackType = 0
    //   sfallWeaponKnockbackValue = 0.0f
    //   sfallTargetKnockbackType = 0
    //   sfallTargetKnockbackValue = 0.0f
    //   sfallAttackerKnockbackType = 0
    //   sfallAttackerKnockbackValue = 0.0f
    //
    // These are verified indirectly: calling reset does not crash or hang,
    // and subsequent extern globals remain at defaults, confirming the
    // reset path executed without fault.

    SUBCASE("reset is safe when knockback state is at defaults")
    {
        sfallOpcodesReset();
        verifyExternGlobalsDefault();
    }

    SUBCASE("reset is idempotent for knockback state")
    {
        sfallOpcodesReset();
        sfallOpcodesReset();
        verifyExternGlobalsDefault();
    }
}

// ============================================================
// sfallAnimCallbackReset — reset animation callback state.
// ============================================================

TEST_CASE("sfallAnimCallbackReset")
{
    // sfallAnimCallbackReset() resets (line 2456-2460):
    //   sfallAnimCallbackProgram = nullptr
    //   sfallAnimCallbackProcedureIndex = -1
    //
    // Both are file-static — verified indirectly via safe call + no crash.

    SUBCASE("anim callback reset does not crash")
    {
        sfallAnimCallbackReset();
        // Callback state is reset; extern globals should be unaffected
        verifyExternGlobalsDefault();
    }

    SUBCASE("anim callback reset is idempotent")
    {
        sfallAnimCallbackReset();
        sfallAnimCallbackReset();
        verifyExternGlobalsDefault();
    }
}

// ============================================================
// sfallVfsCloseAll — close all VFS file handles.
// ============================================================

TEST_CASE("sfallVfsCloseAll")
{
    // sfallVfsCloseAll() iterates all 100 VFS handle slots and calls
    // sfallVfsFreeHandle() on each (line 1789-1794).
    // sfallVfsFreeHandle(): fclose(handle) if non-null, sets nullptr + false.
    //
    // The VFS arrays are file-static, so we verify indirectly.

    SUBCASE("VFS close all does not crash (empty state)")
    {
        sfallVfsCloseAll();
        verifyExternGlobalsDefault();
    }

    SUBCASE("VFS close all is idempotent")
    {
        sfallVfsCloseAll();
        sfallVfsCloseAll();
        verifyExternGlobalsDefault();
    }

    SUBCASE("VFS close all after reset does not crash")
    {
        sfallOpcodesReset();
        sfallVfsCloseAll();
        verifyExternGlobalsDefault();
    }
}

// ============================================================
// sfallOpcodesExit — teardown for engine shutdown.
// ============================================================

TEST_CASE("sfallOpcodesExit")
{
    // sfallOpcodesExit() calls (line 4163-4167):
    //   sfallAnimCallbackReset()
    //   sfallVfsCloseAll()
    //
    // Verified indirectly: call does not crash, globals unchanged.

    SUBCASE("exit after clean state does not crash")
    {
        sfallOpcodesExit();
        // extern globals should not be affected by exit
        CHECK(gPerkFrequencyOverride == 0);
        CHECK(gSkillPointsPerLevelMod == 0);
    }

    SUBCASE("exit after reset is safe")
    {
        sfallOpcodesReset();
        sfallOpcodesExit();
        verifyExternGlobalsDefault();
    }

    SUBCASE("exit is idempotent")
    {
        sfallOpcodesExit();
        sfallOpcodesExit();
        verifyExternGlobalsDefault();
    }
}

// ============================================================
// Lifecycle: full cycle init -> use -> reset -> exit.
// ============================================================

TEST_CASE("sfallOpcodes lifecycle: set -> reset -> verify")
{
    SUBCASE("full cycle without init (globals only)")
    {
        verifyExternGlobalsDefault();

        // Simulate opcode calls by setting globals directly
        setExternGlobalsNonDefault();
        CHECK(gPerkFrequencyOverride == 5);
        CHECK(gSkillMaxCap == 500);

        // Reset
        sfallOpcodesReset();
        verifyExternGlobalsDefault();

        // Exit
        sfallOpcodesExit();
        verifyExternGlobalsDefault();
    }

    SUBCASE("multi-cycle: set -> reset -> set -> reset")
    {
        // First cycle
        setExternGlobalsNonDefault();
        sfallOpcodesReset();
        verifyExternGlobalsDefault();

        // Second cycle — reset state is clean starting point
        gPerkFrequencyOverride = 8;
        gXpModPercentage = 150;
        sfallOpcodesReset();

        CHECK(gPerkFrequencyOverride == 0);
        CHECK(gXpModPercentage == 100);
    }

    SUBCASE("reset between 'save/load' cycles preserves external state")
    {
        // sfallOpcodesReset() is called from gameReset() on save/load.
        // It should clear opcode-specific state without affecting other
        // engine globals (which we verify by checking reset is idempotent).
        sfallOpcodesReset();
        sfallOpcodesReset();
        verifyExternGlobalsDefault();
    }
}

// ============================================================
// hookOpcodeGetCurrentCall — get current script hook call.
// ============================================================

TEST_CASE("hookOpcodeGetCurrentCall")
{
    // hookOpcodeGetCurrentCall() is used by get_sfall_arg, get_sfall_args,
    // set_sfall_arg, set_sfall_return (line 2462-2469).
    //
    // Without an active ScriptHookCall context, this should return nullptr
    // and call programPrintError via the stubs.

    SUBCASE("returns nullptr when no hook is active")
    {
        ScriptHookCall* call = hookOpcodeGetCurrentCall("test_opcode");
        // In test environment with no active hooks, this should be nullptr
        CHECK(call == nullptr);
    }

    SUBCASE("returns nullptr when called with different opcode names")
    {
        CHECK(hookOpcodeGetCurrentCall("get_sfall_arg") == nullptr);
        CHECK(hookOpcodeGetCurrentCall("get_sfall_args") == nullptr);
        CHECK(hookOpcodeGetCurrentCall("set_sfall_arg") == nullptr);
        CHECK(hookOpcodeGetCurrentCall("set_sfall_return") == nullptr);
    }
}

// ============================================================
// Extern globals — individual value range tests.
// ============================================================

TEST_CASE("gPerkFrequencyOverride — isolation")
{
    // Default: 0 (uses engine default of 3 levels or 4 with Skilled).
    // Set by: op_set_perk_freq (0x8247) via op_set_perk_freq().
    // Read by: character_editor.cc (characterEditorUpdateLevel).

    SUBCASE("default value is 0")
    {
        CHECK(gPerkFrequencyOverride == 0);
    }

    SUBCASE("after reset restores to 0")
    {
        gPerkFrequencyOverride = 4;
        sfallOpcodesReset();
        CHECK(gPerkFrequencyOverride == 0);
    }
}

TEST_CASE("gSkillPointsPerLevelMod — isolation")
{
    // Default: 0 (no modifier).
    // Set by: op_mod_skill_points_per_level (0x8246).
    // Read by: character_editor.cc (characterEditorUpdateLevel).

    SUBCASE("default value is 0")
    {
        CHECK(gSkillPointsPerLevelMod == 0);
    }

    SUBCASE("can be negative (subtracts from base skill points)")
    {
        gSkillPointsPerLevelMod = -10;
        CHECK(gSkillPointsPerLevelMod == -10);
        sfallOpcodesReset();
        CHECK(gSkillPointsPerLevelMod == 0);
    }
}

TEST_CASE("gSkillMaxCap — isolation")
{
    // Default: 300 (vanilla match).
    // Set by: op_set_skill_max (0x81A2). Clamps negative to 300.
    // The opcode is: if (value < 0) value = 300; gSkillMaxCap = value;

    SUBCASE("default value is 300")
    {
        CHECK(gSkillMaxCap == 300);
    }

    SUBCASE("can be set to any non-negative value")
    {
        gSkillMaxCap = 500;
        CHECK(gSkillMaxCap == 500);
        gSkillMaxCap = 100;
        CHECK(gSkillMaxCap == 100);
        gSkillMaxCap = 0;
        CHECK(gSkillMaxCap == 0);
    }

    SUBCASE("reset restores to 300")
    {
        gSkillMaxCap = 200;
        sfallOpcodesReset();
        CHECK(gSkillMaxCap == 300);
    }
}

TEST_CASE("gXpModPercentage — isolation")
{
    // Default: 100 (no modification).
    // Set by: op_set_xp_mod (0x81AA). Clamps to [0, 10000].
    // Integration point: pcAddExperience() in stat.cc.

    SUBCASE("default value is 100")
    {
        CHECK(gXpModPercentage == 100);
    }

    SUBCASE("can be set within valid range [0, 10000]")
    {
        gXpModPercentage = 200;
        CHECK(gXpModPercentage == 200);
        gXpModPercentage = 0;
        CHECK(gXpModPercentage == 0);
        gXpModPercentage = 10000;
        CHECK(gXpModPercentage == 10000);
    }

    SUBCASE("reset restores to 100")
    {
        gXpModPercentage = 50;
        sfallOpcodesReset();
        CHECK(gXpModPercentage == 100);
    }
}

TEST_CASE("gLastAttacker / gLastTarget — isolation")
{
    // Default: -1 for both (no last target/attacker).
    // Set by: combat system.
    // Read by: op_get_last_target (0x8248), op_get_last_attacker (0x8249).

    SUBCASE("default values are -1")
    {
        CHECK(gLastAttacker == -1);
        CHECK(gLastTarget == -1);
    }

    SUBCASE("can be set independently")
    {
        gLastAttacker = 42;
        CHECK(gLastAttacker == 42);
        CHECK(gLastTarget == -1); // unchanged

        gLastTarget = 99;
        CHECK(gLastTarget == 99);
        CHECK(gLastAttacker == 42); // unchanged
    }

    SUBCASE("reset restores both to -1")
    {
        gLastAttacker = 100;
        gLastTarget = 200;
        sfallOpcodesReset();
        CHECK(gLastAttacker == -1);
        CHECK(gLastTarget == -1);
    }
}

// ============================================================
// Reset validation: all globals reset simultaneously.
// ============================================================

TEST_CASE("sfallOpcodesReset — atomicity: all globals reset together")
{
    // Set ALL globals to non-default values
    setExternGlobalsNonDefault();

    // Verify all are changed
    CHECK(gPerkFrequencyOverride != 0);
    CHECK(gSkillPointsPerLevelMod != 0);
    CHECK(gLastAttacker != -1);
    CHECK(gLastTarget != -1);
    CHECK(gSkillMaxCap != 300);
    CHECK(gXpModPercentage != 100);

    // Single reset call
    sfallOpcodesReset();

    // ALL should be reset — no partial reset
    CHECK(gPerkFrequencyOverride == 0);
    CHECK(gSkillPointsPerLevelMod == 0);
    CHECK(gLastAttacker == -1);
    CHECK(gLastTarget == -1);
    CHECK(gSkillMaxCap == 300);
    CHECK(gXpModPercentage == 100);
}

// ============================================================
// INTEGRATION test prerequisites — documented stub requirements.
//
// The following sections document the stub requirements for testing
// the ~150 static op_*() functions in sfall_opcodes.cc. These functions
// are NOT directly callable from this test file because:
//   a) They are file-static in sfall_opcodes.cc
//   b) They require Program* with full stack operations
//   c) Most require engine types (Object, Proto, Script, etc.)
//
// See the full stub requirements in tmp/s4-impl-sfall-ops-report.md.
// ============================================================
