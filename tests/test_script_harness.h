// Minimal mock Program for testing opcode handlers and script functions.
//
// The real opcode handlers (~200 functions in sfall_opcodes.cc) are file-static
// and cannot be called directly from test files. This harness provides:
//
//   1. A mock Program struct with stack operations matching the interpreter
//      API (push/pop int, float, pointer, string) so tests can simulate
//      what opcode handlers do internally.
//
//   2. A string table for functions that call programGetString() — used by
//      opcodes like op_set_sfall_global (0x8157) and op_get_sfall_global_int
//      (0x8158) which work with string-typed sfall global variable keys.
//
//   3. A reusable pattern: tests set up stack inputs, run opcode-equivalent
//      logic inline, and verify outputs. If the opcode handlers are ever
//      made publicly callable (e.g. via test-only linkage), the tests
//      written against this harness will work with minimal changes.
//
//   4. Integration with sfall_gl_vars (already in test_sources) for testing
//      global variable store/fetch — the only opcode subsystem that is
//      currently linkable without linking sfall_opcodes.cc itself.
//
// I2-M67: INCREMENTAL EXTRACTION ROADMAP
// ---------------------------------------
// The current harness relies on local simulators (opcode logic reimplemented
// inline) because ~185 file-static opcode handlers cannot be called directly.
// This is a structural limitation, not immutable. The following phased plan
// provides a forward path for incremental test integration:
//
// PHASE 1 — TEST_ACCESSORS Guard (near-term, ~20 opcodes)
//   Add a compile-time guard to sfall_opcodes.cc:
//     #ifdef TEST_ACCESSORS
//     OpcodeFunc sfall_test_get_opcode_handler(int opcodeNum);
//     #endif
//   Expose the ~20 opcodes that have ZERO engine dependencies (math ops:
//   op_ceil, op_round, op_power, op_abs, op_clamp; string ops: op_strlen,
//   op_substr; misc: op_sprintf opcode entry but Program* still needed).
//   These would be callable directly from test files without the simulator
//   indirection, providing genuine production-code coverage.
//
// PHASE 2 — Per-Domain Compilation Units (medium-term, ~60 opcodes)
//   Split sfall_opcodes.cc into linkable domains:
//     sfall_opcodes_globals.cc     — extern globals + reset/init (DONE: already linkable)
//     sfall_opcodes_vfs.cc         — VFS handlers (requires file I/O stubs)
//     sfall_opcodes_perk.cc        — perk/trait handlers (requires perk.cc stubs)
//     sfall_opcodes_combat.cc      — combat stat handlers (requires combat.cc stubs)
//     sfall_opcodes_critter.cc     — critter stat handlers (requires critter.cc stubs)
//   Each compilation unit exports its op_* functions under a test-namespace
//   when TEST_ACCESSORS is defined, and keeps them file-static otherwise.
//   This preserves production encapsulation while enabling per-domain
//   testing with domain-specific stubs.
//
// PHASE 3 — Full Engine Linkage (long-term)
//   When the 50+ engine dependencies are shimmed (Object mock, Proto mock,
//   Combat mock, etc.), link sfall_opcodes.cc directly into test executables.
//   This replaces ALL simulators with genuine opcode dispatch through the
//   production interpreter loop or a thin test-wrapper calling op_* directly.
//
// Current coverage: 7/267 (2.6%) opcodes linkable through sfall_gl_vars.
// Phase 1 would add ~20 opcodes (math/string), raising to ~10%.
// Phase 2 would add ~60 opcodes, raising to ~33%.
// Phase 3 would reach ~90%+ (remaining ~10% require full game loop).
//
// Known mirror-production semantic divergences (I2-M62):
//   - getReturnValueAt OOB: mirrors return 0 safely; production assert-fails
//     (debug) or accesses UB (release). This divergence means mirror tests
//     validate different behavior than production. Documented here so that
//     any test using getReturnValueAt via a mirror is aware of the gap.
//     Resolution: either add the production assert pattern to mirrors
//     (runtime assert parity) or document this as intentional test-safety
//     divergence with a dedicated test that verifies the production assert
//     fires when the function is linkable (Phase 3).
//
// The 7 INTEGRATION-level opcodes documented in test_sfall_opcodes.cc:20-22
// that need Program* mock:
//   op_set_sfall_global, op_get_sfall_global_int, op_get_sfall_global_float,
//   op_game_loaded, op_set_global_script_repeat, op_set_global_script_type,
//   op_set_fake_perk
//
// Of these, op_set_sfall_global and op_get_sfall_global_int/float can be
// partially tested via the sfall_gl_vars subsystem (sfall_global_vars.cc is
// in test_sources). The others require sfall_global_scripts.cc linkage
// (50+ engine deps) and remain INTEGRATION-only.
//
// F-064 (MEDIUM): Harness provides local simulators, not production wrappers.
//   Only 7 of ~267 opcode handlers (2.6%) are testable through this harness.
//   The remaining ~260 handlers are file-static in sfall_opcodes.cc and
//   require linking 150+ engine source files (Program, interpreter, script
//   manager, game objects, map, combat, AI). The structural blocker is that
//   sfall_opcodes.cc has no TEST_ACCESSORS or public accessor separation.
//
//   INCREMENTAL EXTRACTION ROADMAP (proposed):
//   Phase 1 — Separable globals (~15, low effort):
//     Move extern globals (knockback types, perk globals, XP modifiers,
//     hit chance mods) into a dedicated sfall_opcodes_state.cc/.h pair.
//     These already have extern declarations in sfall_opcodes.h and stubs
//     in test_common_stubs.cc. Creating a separate compilation unit adds
//     zero new engine dependencies and makes state directly testable.
//   Phase 2 — Modifier maps (~8, medium effort):
//     Extract skill/pickpocket/perk property modifier maps into
//     sfall_opcodes_modifiers.cc/.h. These are std::unordered_map<>
//     containers with file-static helper functions — no engine deps
//     beyond critter.h (which is already in test_sources).
//   Phase 3 — Lifecycle functions (~5, low effort):
//     sfallOpcodesReset/Exit, sfallVfsCloseAll, sfallAnimCallbackReset
//     are already declared in sfall_opcodes.h and stubbed. Moving them
//     to a separate unit with minimal engine deps makes integration
//     testing of state transitions possible.
//   Phase 4 — Per-opcode extraction (per-domain, ongoing):
//     Group opcode handlers by subsystem (perk ops, skill ops, inventory
//     ops, etc.) into per-domain compilation units behind TEST_ACCESSORS
//     guards. Each unit links only its specific engine dependencies.
//
//   This roadmap prioritizes low-hanging fruit (Phase 1: ~2 days effort)
//   over full extraction (Phase 4: weeks, requires engine modularization).
//   TODO markers in test_opcodes_core_ext.cc track Phase 1 readiness.

#ifndef TEST_SCRIPT_HARNESS_H_
#define TEST_SCRIPT_HARNESS_H_

#include "interpreter.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace fallout {
namespace test {

// Minimum set of fields from struct Program that opcode handlers access.
// Mirrors the layout in interpreter.h:187-213. We only include the fields
// that are actually read/written by opcode handlers:
//
//   program->name            read by ~40 opcodes for error messages
//   program->stackValues      read/written by ALL opcodes for push/pop
//   program->returnStackValues read/written by set_sfall_arg/return
//
// The other Program fields (data, instructionPointer, flags, etc.) are
// used by the interpreter loop and opcode dispatch, not by individual
// opcode handler bodies.
class TestProgram {
public:
    // Construct a mock program with the given name (for error messages).
    explicit TestProgram(const char* name);

    // Clean up allocated resources.
    ~TestProgram();

    // Get a Program* pointer usable anywhere a Program* is expected.
    // The returned pointer references the internal Program struct whose
    // stackValues/returnStackValues point to our mock vectors.
    Program* programPtr() { return &_program; }

    // ---- Stack Operations ----
    // Integer stack (matches programStackPushInteger/PopInteger)
    void pushInt(int value);
    int popInt();

    // Float stack (matches programStackPushFloat)
    void pushFloat(float value);
    float popFloat();

    // Pointer stack (matches programStackPushPointer/PopPointer)
    void pushPointer(void* value);
    void* popPointer();

    // Value stack (matches programStackPushValue/PopValue)
    void pushValue(const ProgramValue& value);
    ProgramValue popValue();

    // String stack (matches programStackPushString)
    void pushString(const char* str);
    const char* popString();

    // ---- Return Stack Operations ----
    void returnPushInt(int value);
    int returnPopInt();
    void returnPushPointer(void* value);
    void* returnPopPointer();
    void returnPushValue(const ProgramValue& value);
    ProgramValue returnPopValue();

    // ---- String Table (for programGetString emulation) ----
    // Returns a unique ID for the string (positive, non-zero).
    int registerString(const char* str);
    // Look up a string by its ID. Returns nullptr if not found.
    const char* lookupString(int id) const;

    // ---- State queries ----
    int stackDepth() const { return static_cast<int>(_stack.size()); }
    int returnStackDepth() const { return static_cast<int>(_returnStack.size()); }
    const char* name() const { return _name.c_str(); }

    // ---- Reset ----
    // Clear all stacks and string table. Name is preserved.
    void reset();

    // Prevent copy/move — mock owns its Program struct
    TestProgram(const TestProgram&) = delete;
    TestProgram& operator=(const TestProgram&) = delete;
    TestProgram(TestProgram&&) = delete;
    TestProgram& operator=(TestProgram&&) = delete;

private:
    std::string _name;
    Program _program;
    std::vector<ProgramValue> _stack;
    std::vector<ProgramValue> _returnStack;
    std::unordered_map<int, std::string> _strings;
    int _nextStringId;
};

// ---- Opcode Logic Simulators ----
// These functions mirror the internal logic of opcode handlers.
// Call them with a TestProgram to simulate what the static opcode
// handler would do with the current stack state.
//
// TODO (I2-M67): Replace simulators with direct opcode calls per the
// incremental extraction roadmap (see header comment, Phase 1-3).
// Each simulator has a corresponding target opcode:
//   simOpCeil        → op_ceil        (Phase 1: math op, zero engine deps)
//   simOpRound       → op_round       (Phase 1: math op, zero engine deps)
//   simOpGameLoaded  → op_game_loaded (Phase 2: needs script engine stubs)
//   simOpSet*/Get*   → op_set/get_sfall_global (Phase 1: sfall_gl_vars linkable)
//
// When the actual opcode handlers are made linkable (by moving them
// from file-static to a test-exported compilation unit), these
// simulator helpers can be replaced with direct opcode calls.

// Simulate op_ceil (line ~715): pop float, push int(ceilf).
void simOpCeil(TestProgram& tp);

// Simulate op_round (line ~1698): pop float, push int(lroundf).
// Tests the F2-012 overflow guard pattern: lroundf(> INT_MAX) wraps
// to INT_MIN. The fix adds an INT_MAX/INT_MIN guard before the cast.
void simOpRound(TestProgram& tp);

// Simulate op_game_loaded (line ~293): push sfall_gl_scr_is_loaded result.
void simOpGameLoaded(TestProgram& tp, int loadedValue);

// Simulate op_set_sfall_global (line ~471) with int key and int value.
void simOpSetSfallGlobalInt(TestProgram& tp, int key, int value);

// Simulate op_set_sfall_global (line ~471) with string key and int value.
void simOpSetSfallGlobalStr(TestProgram& tp, const char* key, int value);

// Simulate op_get_sfall_global_int (line ~495) with int key.
void simOpGetSfallGlobalInt(TestProgram& tp, int key);

// Simulate op_get_sfall_global_int (line ~495) with string key.
void simOpGetSfallGlobalIntStr(TestProgram& tp, const char* key);

} // namespace test
} // namespace fallout

#endif // TEST_SCRIPT_HARNESS_H_
