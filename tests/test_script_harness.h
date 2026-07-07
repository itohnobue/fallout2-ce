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
