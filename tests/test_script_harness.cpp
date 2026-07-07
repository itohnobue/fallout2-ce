// Implementation of TestProgram mock for opcode handler testing.
// See test_script_harness.h for the full API documentation.

#include "test_script_harness.h"

#include "sfall_global_vars.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace fallout {
namespace test {

// ============================================================
// TestProgram
// ============================================================

TestProgram::TestProgram(const char* name)
    : _name(name)
    , _nextStringId(1000)
{
    // Zero-initialize the Program struct.
    std::memset(&_program, 0, sizeof(_program));

    // Set up the fields opcode handlers actually access.
    // We use const_cast because Program.name is char* (legacy).
    _program.name = const_cast<char*>(_name.c_str());
    _program.stackValues = reinterpret_cast<ProgramStack*>(&_stack);
    _program.returnStackValues = reinterpret_cast<ProgramStack*>(&_returnStack);
}

TestProgram::~TestProgram()
{
    // Pointer fields point to member variables — no explicit cleanup needed.
    // Just break the linkage so no one uses a dangling Program*.
    _program.stackValues = nullptr;
    _program.returnStackValues = nullptr;
    _program.name = nullptr;
}

void TestProgram::reset()
{
    _stack.clear();
    _returnStack.clear();
    _strings.clear();
    _nextStringId = 1000;
    // Name and Program ptrs are preserved.
}

// ---- Stack Operations ----

void TestProgram::pushInt(int value)
{
    _stack.emplace_back(value);
}

int TestProgram::popInt()
{
    if (_stack.empty()) {
        return 0;
    }
    ProgramValue v = _stack.back();
    _stack.pop_back();
    return v.isInt() ? v.integerValue : 0;
}

void TestProgram::pushFloat(float value)
{
    _stack.emplace_back(value);
}

float TestProgram::popFloat()
{
    if (_stack.empty()) {
        return 0.0f;
    }
    ProgramValue v = _stack.back();
    _stack.pop_back();
    return v.isFloat() ? v.floatValue : 0.0f;
}

void TestProgram::pushPointer(void* value)
{
    _stack.emplace_back(static_cast<Object*>(value));
}

void* TestProgram::popPointer()
{
    if (_stack.empty()) {
        return nullptr;
    }
    ProgramValue v = _stack.back();
    _stack.pop_back();
    return v.isPointer() ? v.pointerValue : nullptr;
}

void TestProgram::pushValue(const ProgramValue& value)
{
    _stack.push_back(value);
}

ProgramValue TestProgram::popValue()
{
    if (_stack.empty()) {
        return ProgramValue { 0 };
    }
    ProgramValue v = _stack.back();
    _stack.pop_back();
    return v;
}

void TestProgram::pushString(const char* str)
{
    // Match production: programStackPushString creates a ProgramValue
    // with VALUE_TYPE_STRING opcode and an integer string offset.
    // We simulate by registering in our string table and pushing the ID
    // as a ProgramValue with the internal-string pattern.
    if (str == nullptr) {
        _stack.emplace_back(0);
        return;
    }
    int id = registerString(str);
    // Simulate the string opcode pattern: opcode stores VALUE_TYPE_STRING,
    // integerValue stores the offset/index into the string table.
    ProgramValue sv;
    sv.opcode = VALUE_TYPE_STRING;
    sv.integerValue = id;  // Must be set last — integerValue and pointerValue share a union
    _stack.push_back(sv);
}

const char* TestProgram::popString()
{
    if (_stack.empty()) {
        return "";
    }
    ProgramValue v = _stack.back();
    _stack.pop_back();
    if (v.isString()) {
        return lookupString(v.integerValue);
    }
    return "";
}

// ---- Return Stack Operations ----

void TestProgram::returnPushInt(int value)
{
    _returnStack.emplace_back(value);
}

int TestProgram::returnPopInt()
{
    if (_returnStack.empty()) {
        return 0;
    }
    ProgramValue v = _returnStack.back();
    _returnStack.pop_back();
    return v.isInt() ? v.integerValue : 0;
}

void TestProgram::returnPushPointer(void* value)
{
    _returnStack.emplace_back(static_cast<Object*>(value));
}

void* TestProgram::returnPopPointer()
{
    if (_returnStack.empty()) {
        return nullptr;
    }
    ProgramValue v = _returnStack.back();
    _returnStack.pop_back();
    return v.isPointer() ? v.pointerValue : nullptr;
}

void TestProgram::returnPushValue(const ProgramValue& value)
{
    _returnStack.push_back(value);
}

ProgramValue TestProgram::returnPopValue()
{
    if (_returnStack.empty()) {
        return ProgramValue { 0 };
    }
    ProgramValue v = _returnStack.back();
    _returnStack.pop_back();
    return v;
}

// ---- String Table ----

int TestProgram::registerString(const char* str)
{
    int id = _nextStringId++;
    _strings[id] = std::string(str);
    return id;
}

const char* TestProgram::lookupString(int id) const
{
    auto it = _strings.find(id);
    if (it != _strings.end()) {
        return it->second.c_str();
    }
    return nullptr;
}

// ============================================================
// Opcode Logic Simulators
// ============================================================

void simOpCeil(TestProgram& tp)
{
    // Mirrors op_ceil at sfall_opcodes.cc:715-718:
    //   ProgramValue programValue = programStackPopValue(program);
    //   programStackPushInteger(program, static_cast<int>(ceilf(programValue.asFloat())));
    ProgramValue pv = tp.popValue();
    // NOTE: F2-012 overflow guard would go here:
    //   if (result > INT_MAX || result < INT_MIN) { pushFloat; return; }
    tp.pushInt(static_cast<int>(ceilf(pv.asFloat())));
}

void simOpRound(TestProgram& tp)
{
    // Mirrors op_round at sfall_opcodes.cc:1698-1701:
    //   float floatValue = programStackPopValue(program).asFloat();
    //   programStackPushInteger(program, static_cast<int>(lroundf(floatValue)));
    //
    // F2-012: lroundf with values > INT_MAX produces INT_MIN (wraparound).
    // The fix adds an INT_MAX/INT_MIN guard matching op_power at line 690-695.
    ProgramValue pv = tp.popValue();
    float value = pv.asFloat();

    long rounded = lroundf(value);
    // Use lroundf result for comparison — float comparison near INT_MAX is
    // imprecise because float can't represent 2147483647 exactly.
    if (rounded > static_cast<long>(std::numeric_limits<int>::max())
        || rounded < static_cast<long>(std::numeric_limits<int>::min())) {
        tp.pushFloat(value);
    } else {
        tp.pushInt(static_cast<int>(rounded));
    }
}

void simOpGameLoaded(TestProgram& tp, int loadedValue)
{
    // Mirrors op_game_loaded at sfall_opcodes.cc:293-296:
    //   int loaded = sfall_gl_scr_is_loaded(program);
    //   programStackPushInteger(program, loaded);
    //
    // sfall_gl_scr_is_loaded requires sfall_global_scripts.cc (50+ engine deps),
    // so we accept the value as a parameter for simulation.
    tp.pushInt(loadedValue);
}

void simOpSetSfallGlobalInt(TestProgram& tp, int key, int value)
{
    // Mirrors op_set_sfall_global at sfall_opcodes.cc:471-491 for int-key path:
    //   ProgramValue value = programStackPopValue(program);
    //   ProgramValue variable = programStackPopValue(program);
    //   if (variable.opcode == VALUE_TYPE_INT) {
    //       sfall_gl_vars_store(variable.integerValue, value.integerValue);
    //   }
    sfall_gl_vars_store(key, value);
}

void simOpSetSfallGlobalStr(TestProgram& tp, const char* key, int value)
{
    // Mirrors op_set_sfall_global at sfall_opcodes.cc:471-491 for string-key path.
    sfall_gl_vars_store(key, value);
}

void simOpGetSfallGlobalInt(TestProgram& tp, int key)
{
    // Mirrors op_get_sfall_global_int at sfall_opcodes.cc:495-507 for int-key path:
    //   ProgramValue variable = programStackPopValue(program);
    //   int value = 0;
    //   sfall_gl_vars_fetch(variable.integerValue, value);
    //   programStackPushInteger(program, value);
    int value = 0;
    sfall_gl_vars_fetch(key, value);
    tp.pushInt(value);
}

void simOpGetSfallGlobalIntStr(TestProgram& tp, const char* key)
{
    // Mirrors op_get_sfall_global_int at sfall_opcodes.cc:495-507 for string-key path:
    //   const char* key = programGetString(program, variable.opcode, variable.integerValue);
    //   sfall_gl_vars_fetch(key, value);
    //   programStackPushInteger(program, value);
    int value = 0;
    sfall_gl_vars_fetch(key, value);
    tp.pushInt(value);
}

} // namespace test
} // namespace fallout
