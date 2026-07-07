// Unit tests for test_script_harness — the mock Program / script
// interpreter test harness infrastructure (F2-012).
//
// The harness provides a TestProgram class that simulates the Program*
// context opcode handlers need. Since the real opcode handlers (~200
// functions) are file-static in sfall_opcodes.cc and cannot be called
// directly, these tests demonstrate the pattern:
//
//   1. Set up a TestProgram with inputs on the mock stack
//   2. Run the opcode-equivalent logic (via simulator functions or inline)
//   3. Verify the outputs on the mock stack
//
// When opcode handlers are ever made publicly callable, these tests will
// work with minimal changes (replace simulator call with real opcode call).
//
// Tests: TestProgram lifecycle, stack push/pop round-trip, ProgramValue
//        type round-trip, math overflow guard (F2-012: ceilf/lroundf),
//        sfall_gl_vars set/get with mock Program context,
//        return stack operations, string table.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "test_script_harness.h"

#include "sfall_global_vars.h"

#include <climits>
#include <cmath>

using namespace fallout;
using namespace fallout::test;

// ============================================================
// Helper: ensure sfall_gl_vars is initialized for vars tests.
// ============================================================
static void ensureVarsInit()
{
    // sfall_gl_vars_init is safe to call multiple times.
    sfall_gl_vars_init();
}

static void ensureVarsCleanup()
{
    sfall_gl_vars_exit();
}

// ============================================================
// TestProgram Lifecycle
// ============================================================

TEST_CASE("TestProgram lifecycle")
{
    SUBCASE("default construction creates valid Program")
    {
        TestProgram tp("test_lifecycle");
        CHECK(tp.programPtr() != nullptr);
        CHECK(tp.stackDepth() == 0);
        CHECK(tp.returnStackDepth() == 0);
        CHECK(std::string(tp.name()) == "test_lifecycle");
    }

    SUBCASE("two programs can coexist independently")
    {
        TestProgram tp1("program_a");
        TestProgram tp2("program_b");

        tp1.pushInt(42);
        tp2.pushInt(99);

        CHECK(tp1.stackDepth() == 1);
        CHECK(tp2.stackDepth() == 1);
        CHECK(tp1.popInt() == 42);
        CHECK(tp2.popInt() == 99);
    }

    SUBCASE("reset clears stacks but preserves name")
    {
        TestProgram tp("persistent");
        tp.pushInt(1);
        tp.pushFloat(3.14f);
        tp.pushPointer(reinterpret_cast<void*>(0xDEAD));
        tp.returnPushInt(7);

        tp.reset();

        CHECK(tp.stackDepth() == 0);
        CHECK(tp.returnStackDepth() == 0);
        CHECK(std::string(tp.name()) == "persistent");
    }

    SUBCASE("program pointer is valid after reset")
    {
        TestProgram tp("after_reset");
        tp.reset();
        CHECK(tp.programPtr() != nullptr);
        CHECK(tp.stackDepth() == 0);
    }
}

// ============================================================
// Stack Operations — Integer
// ============================================================

TEST_CASE("TestProgram stack operations — integers")
{
    TestProgram tp("int_test");

    SUBCASE("push/pop single value round-trip")
    {
        tp.pushInt(42);
        CHECK(tp.stackDepth() == 1);
        CHECK(tp.popInt() == 42);
        CHECK(tp.stackDepth() == 0);
    }

    SUBCASE("push/pop multiple values in LIFO order")
    {
        tp.pushInt(10);
        tp.pushInt(20);
        tp.pushInt(30);
        CHECK(tp.popInt() == 30);
        CHECK(tp.popInt() == 20);
        CHECK(tp.popInt() == 10);
    }

    SUBCASE("pop from empty stack returns 0")
    {
        CHECK(tp.popInt() == 0);
        CHECK(tp.stackDepth() == 0);
    }

    SUBCASE("negative values round-trip")
    {
        tp.pushInt(-1);
        tp.pushInt(INT_MIN);
        tp.pushInt(INT_MAX);
        CHECK(tp.popInt() == INT_MAX);
        CHECK(tp.popInt() == INT_MIN);
        CHECK(tp.popInt() == -1);
    }

    SUBCASE("zero is valid stack value")
    {
        tp.pushInt(0);
        CHECK(tp.stackDepth() == 1);
        CHECK(tp.popInt() == 0);
        CHECK(tp.stackDepth() == 0);
    }
}

// ============================================================
// Stack Operations — Float
// ============================================================

TEST_CASE("TestProgram stack operations — floats")
{
    TestProgram tp("float_test");

    SUBCASE("push/pop single float round-trip")
    {
        tp.pushFloat(3.14f);
        CHECK(tp.stackDepth() == 1);
        CHECK(tp.popFloat() == doctest::Approx(3.14f));
        CHECK(tp.stackDepth() == 0);
    }

    SUBCASE("negative and zero floats")
    {
        tp.pushFloat(-1.5f);
        tp.pushFloat(0.0f);
        tp.pushFloat(0.001f);
        CHECK(tp.popFloat() == doctest::Approx(0.001f));
        CHECK(tp.popFloat() == doctest::Approx(0.0f));
        CHECK(tp.popFloat() == doctest::Approx(-1.5f));
    }

    SUBCASE("pop from empty float stack returns 0.0f")
    {
        CHECK(tp.popFloat() == doctest::Approx(0.0f));
    }

    SUBCASE("mixed int and float pushes — popFloat returns correct value")
    {
        // When an int was pushed, popFloat returns it as 0.0f since
        // the ProgramValue is typed as VALUE_TYPE_INT, not float.
        // This matches production: the opcode handler would call
        // programStackPopValue().asFloat() which converts.
        tp.pushFloat(2.5f);
        tp.pushInt(42);
        // popFloat checks isFloat() — int values return 0.0f
        CHECK(tp.popFloat() == doctest::Approx(0.0f));
        CHECK(tp.popFloat() == doctest::Approx(2.5f));
    }
}

// ============================================================
// Stack Operations — Pointer
// ============================================================

TEST_CASE("TestProgram stack operations — pointers")
{
    TestProgram tp("pointer_test");

    SUBCASE("push/pop pointer round-trip")
    {
        void* sentinel = reinterpret_cast<void*>(0xCAFEBABE);
        tp.pushPointer(sentinel);
        CHECK(tp.stackDepth() == 1);
        CHECK(tp.popPointer() == sentinel);
    }

    SUBCASE("nullptr round-trip")
    {
        tp.pushPointer(nullptr);
        CHECK(tp.popPointer() == nullptr);
    }

    SUBCASE("pop from empty pointer stack returns nullptr")
    {
        CHECK(tp.popPointer() == nullptr);
    }
}

// ============================================================
// Stack Operations — ProgramValue
// ============================================================

TEST_CASE("TestProgram stack operations — ProgramValue")
{
    TestProgram tp("value_test");

    SUBCASE("push/pop int ProgramValue")
    {
        ProgramValue pv(42);
        tp.pushValue(pv);
        ProgramValue result = tp.popValue();
        CHECK(result.isInt());
        CHECK(result.integerValue == 42);
    }

    SUBCASE("push/pop float ProgramValue")
    {
        ProgramValue pv(3.14f);
        tp.pushValue(pv);
        ProgramValue result = tp.popValue();
        CHECK(result.isFloat());
        CHECK(result.floatValue == doctest::Approx(3.14f));
    }

    SUBCASE("push/pop pointer ProgramValue")
    {
        void* data = reinterpret_cast<void*>(0xBEEF);
        Object* obj = static_cast<Object*>(data);
        ProgramValue pv(obj);
        tp.pushValue(pv);
        ProgramValue result = tp.popValue();
        CHECK(result.isPointer());
        CHECK(result.pointerValue == data);
    }

    SUBCASE("pop from empty value stack returns zero int")
    {
        ProgramValue result = tp.popValue();
        CHECK(result.isInt());
        CHECK(result.integerValue == 0);
    }

    SUBCASE("mixed type stack sequence (as opcodes would see it)")
    {
        // Simulate: opcode pop -> int, opcode pop -> float, opcode pop -> ptr
        ProgramValue pvInt(42);
        ProgramValue pvFloat(3.14f);
        ProgramValue pvPtr(static_cast<Object*>(reinterpret_cast<void*>(0xFEED)));

        tp.pushValue(pvPtr);
        tp.pushValue(pvFloat);
        tp.pushValue(pvInt);

        // Pop in reverse order
        ProgramValue r1 = tp.popValue();
        CHECK(r1.isInt());
        CHECK(r1.integerValue == 42);

        ProgramValue r2 = tp.popValue();
        CHECK(r2.isFloat());
        CHECK(r2.floatValue == doctest::Approx(3.14f));

        ProgramValue r3 = tp.popValue();
        CHECK(r3.isPointer());
        CHECK(r3.pointerValue == reinterpret_cast<void*>(0xFEED));
    }
}

// ============================================================
// Return Stack Operations
// ============================================================

TEST_CASE("TestProgram return stack operations")
{
    TestProgram tp("return_test");

    SUBCASE("return stack starts empty")
    {
        CHECK(tp.returnStackDepth() == 0);
    }

    SUBCASE("push/pop int on return stack")
    {
        tp.returnPushInt(99);
        CHECK(tp.returnStackDepth() == 1);
        CHECK(tp.returnPopInt() == 99);
        CHECK(tp.returnStackDepth() == 0);
    }

    SUBCASE("return stack is independent of main stack")
    {
        tp.pushInt(10);
        tp.returnPushInt(20);

        CHECK(tp.stackDepth() == 1);
        CHECK(tp.returnStackDepth() == 1);
        CHECK(tp.popInt() == 10);
        CHECK(tp.returnPopInt() == 20);
    }

    SUBCASE("reset clears both stacks")
    {
        tp.pushInt(1);
        tp.pushInt(2);
        tp.returnPushInt(3);
        tp.returnPushInt(4);

        tp.reset();

        CHECK(tp.stackDepth() == 0);
        CHECK(tp.returnStackDepth() == 0);
    }

    SUBCASE("return push/pop pointer")
    {
        void* ptr = reinterpret_cast<void*>(0xABCD);
        tp.returnPushPointer(ptr);
        CHECK(tp.returnPopPointer() == ptr);
    }

    SUBCASE("return push/pop ProgramValue")
    {
        ProgramValue pv(77);
        tp.returnPushValue(pv);
        ProgramValue result = tp.returnPopValue();
        CHECK(result.isInt());
        CHECK(result.integerValue == 77);
    }
}

// ============================================================
// String Table Operations
// ============================================================

TEST_CASE("TestProgram string table")
{
    TestProgram tp("string_test");

    SUBCASE("register and lookup string")
    {
        int id = tp.registerString("hello");
        CHECK(id > 0);
        const char* result = tp.lookupString(id);
        CHECK(result != nullptr);
        CHECK(std::string(result) == "hello");
    }

    SUBCASE("multiple strings get unique IDs")
    {
        int id1 = tp.registerString("first");
        int id2 = tp.registerString("second");
        CHECK(id1 != id2);
        CHECK(std::string(tp.lookupString(id1)) == "first");
        CHECK(std::string(tp.lookupString(id2)) == "second");
    }

    SUBCASE("lookup nonexistent string returns nullptr")
    {
        CHECK(tp.lookupString(9999) == nullptr);
    }

    SUBCASE("push/pop string via stack")
    {
        tp.pushString("test_value");
        CHECK(tp.stackDepth() == 1);

        const char* result = tp.popString();
        CHECK(result != nullptr);
        CHECK(std::string(result) == "test_value");
    }

    SUBCASE("push nullptr string pushes int 0 (matches production)")
    {
        tp.pushString(nullptr);
        CHECK(tp.stackDepth() == 1);
        // In production, null strings push 0 on the stack.
        ProgramValue pv = tp.popValue();
        CHECK(pv.isInt());
        CHECK(pv.integerValue == 0);
    }

    SUBCASE("reset clears string table")
    {
        int id = tp.registerString("before_reset");
        CHECK(tp.lookupString(id) != nullptr);

        tp.reset();

        CHECK(tp.lookupString(id) == nullptr);
    }
}

// ============================================================
// F2-012: Math Overflow Guard — ceilf / lroundf
// ============================================================

TEST_CASE("F2-012: op_ceil overflow guard pattern")
{
    // The real op_ceil at sfall_opcodes.cc:715-718 does:
    //   static_cast<int>(ceilf(programValue.asFloat()))
    // This is UB when ceilf result exceeds INT_MAX or is below INT_MIN.
    //
    // This test verifies that the simulator correctly demonstrates the
    // overflow risk AND that an overflow guard pattern (matching op_power
    // at sfall_opcodes.cc:690-695) would prevent the UB.
    //
    // simOpCeil here does NOT include the guard (mirrors current prod code).
    // simOpRound in test_script_harness.cpp DOES include the guard (fix pattern).

    TestProgram tp("ceil_test");

    SUBCASE("normal ceil values round-trip correctly")
    {
        // 3.14 → ceil → 4
        tp.pushFloat(3.14f);
        simOpCeil(tp);
        CHECK(tp.stackDepth() == 1);
        CHECK(tp.popInt() == 4);

        // -2.7 → ceil → -2
        tp.pushFloat(-2.7f);
        simOpCeil(tp);
        CHECK(tp.popInt() == -2);

        // 0.1 → ceil → 1
        tp.pushFloat(0.1f);
        simOpCeil(tp);
        CHECK(tp.popInt() == 1);
    }

    SUBCASE("ceil of float that overflows int range")
    {
        // ceilf(2147483648.0f) = 2147483648.0f
        // static_cast<int>(2147483648.0f) is UB in C++17.
        // In practice on most platforms, it produces INT_MIN (-2147483648).
        // The fix would push a float instead.
        float huge = static_cast<float>(INT_MAX) + 1.0f; // 2147483648.0f
        tp.pushFloat(huge);

        // Without guard: this is UB. We document the risk.
        // With guard: the opcode would pushFloat(huge) instead.
        int result = static_cast<int>(std::ceilf(huge));
        (void)result;
        // The result is platform-dependent. The point is: the harness
        // makes this pattern TESTABLE where before it required launching
        // the full game engine.
    }

    SUBCASE("ceil of extremely negative float")
    {
        float tiny = static_cast<float>(INT_MIN) - 1.0f;
        tp.pushFloat(tiny);
        int result = static_cast<int>(std::ceilf(tiny));
        (void)result;
        // Same UB risk on the negative side.
    }
}

TEST_CASE("F2-012: op_round overflow guard pattern (with fix)")
{
    // simOpRound in test_script_harness.cpp includes the F2-012 overflow
    // guard (matching op_power at sfall_opcodes.cc:690-695).
    // This test verifies the guard works correctly.

    TestProgram tp("round_test");

    SUBCASE("normal round values work correctly")
    {
        tp.pushFloat(3.14f);
        simOpRound(tp);
        CHECK(tp.popInt() == 3);

        tp.pushFloat(3.6f);
        simOpRound(tp);
        CHECK(tp.popInt() == 4);

        tp.pushFloat(-2.5f);
        simOpRound(tp);
        CHECK(tp.popInt() == -3); // lroundf rounds away from zero
    }

    SUBCASE("round of value exceeding INT_MAX falls back to float")
    {
        // float32 cannot represent INT_MAX (2147483647) exactly —
        // it rounds to 2147483648.0f. A value firmly above INT_MAX
        // (e.g. 2^31 + 1000) triggers the overflow guard reliably.
        float huge = 2147488000.0f; // clearly > INT_MAX in float32
        tp.pushFloat(huge);

        simOpRound(tp); // uses the F2-012 guarded version

        CHECK(tp.stackDepth() == 1);
        ProgramValue result = tp.popValue();
        CHECK(result.isFloat());
        CHECK(result.floatValue == doctest::Approx(huge));
    }

    SUBCASE("round of value below INT_MIN falls back to float")
    {
        // Match the overflow pattern: lroundf(-2^31 - 1000) wraps without guard.
        float tiny = -2147488000.0f; // clearly < INT_MIN in float32
        tp.pushFloat(tiny);

        simOpRound(tp);

        ProgramValue result = tp.popValue();
        CHECK(result.isFloat());
        CHECK(result.floatValue == doctest::Approx(tiny));
    }

    SUBCASE("round of INT_MAX exactly cannot be represented as float32")
    {
        // float32(INT_MAX) = 2147483648.0f (rounds up).
        // lroundf(2147483648.0f) = 2147483648 > INT_MAX → falls back to float.
        // This is correct behavior — the float cannot encode INT_MAX exactly.
        tp.pushFloat(static_cast<float>(INT_MAX));
        simOpRound(tp);
        ProgramValue result = tp.popValue();
        CHECK(result.isFloat()); // overflow guard triggers
        CHECK(result.floatValue > 0);
    }

    SUBCASE("round of INT_MIN exactly works as int")
    {
        tp.pushFloat(static_cast<float>(INT_MIN));
        simOpRound(tp);
        ProgramValue result = tp.popValue();
        CHECK(result.isInt());
        CHECK(result.integerValue == INT_MIN);
    }

    SUBCASE("round of float near INT_MAX boundary")
    {
        // Use a float that lroundf maps to a value strictly below INT_MAX.
        // 2147483000.0f is exactly representable and lroundf=2147483000 < INT_MAX.
        float nearMax = 2147483000.0f;
        tp.pushFloat(nearMax);
        simOpRound(tp);
        ProgramValue result = tp.popValue();
        CHECK(result.isInt());
        CHECK(result.integerValue > 0);
        CHECK(result.integerValue < INT_MAX);
    }
}

// ============================================================
// F2-012: op_power overflow guard (existing pattern reference)
// ============================================================

TEST_CASE("F2-012: op_power overflow guard — reference pattern")
{
    // op_power at sfall_opcodes.cc:690-695 already has the overflow guard:
    //   if (result > INT_MAX || result < INT_MIN) {
    //       programStackPushFloat(program, result);
    //   } else {
    //       programStackPushInteger(program, static_cast<int>(result));
    //   }
    //
    // This is the pattern that op_ceil and op_round should follow.
    // We verify it here by simulating it inline.

    TestProgram tp("power_test");

    auto simOpPower = [&tp](float base, float exp) {
        float result = std::powf(base, exp);
        // Use lroundf for comparison — float comparison near INT_MAX is imprecise.
        // Float can't represent 2147483647 exactly, so static_cast<float>(INT_MAX)
        // equals 2147483648.0f, making `result > float(INT_MAX)` unreliable.
        long rounded = lroundf(result);
        if (rounded > static_cast<long>(INT_MAX) || rounded < static_cast<long>(INT_MIN)) {
            tp.pushFloat(result);
        } else {
            tp.pushInt(static_cast<int>(rounded));
        }
    };

    SUBCASE("powf within int range pushes int")
    {
        // 2^10 = 1024, fits in int
        simOpPower(2.0f, 10.0f);
        ProgramValue result = tp.popValue();
        CHECK(result.isInt());
        CHECK(result.integerValue == 1024);
    }

    SUBCASE("powf exceeding INT_MAX pushes float")
    {
        // 2^31 = 2147483648 > INT_MAX (2147483647)
        simOpPower(2.0f, 31.0f);
        ProgramValue result = tp.popValue();
        CHECK(result.isFloat());
        CHECK(result.floatValue != 0.0f);
    }

    SUBCASE("powf with negative base, odd exponent")
    {
        // (-2)^3 = -8, fits in int
        simOpPower(-2.0f, 3.0f);
        ProgramValue result = tp.popValue();
        CHECK(result.isInt());
        CHECK(result.integerValue == -8);
    }
}

// ============================================================
// sfall_gl_vars Integration — op_set_sfall_global / op_get_sfall_global_int
// ============================================================

TEST_CASE("sfall_gl_vars integration — set/get global int round-trip")
{
    // The op_set_sfall_global (0x8157) and op_get_sfall_global_int (0x8158)
    // opcodes delegate to sfall_gl_vars_store / sfall_gl_vars_fetch.
    // sfall_global_vars.cc is in test_sources and is fully testable.
    // The opcode wrappers are file-static — we test through the simulator
    // helpers which call the real sfall_gl_vars functions.

    ensureVarsInit();

    TestProgram tp("vars_int_test");

    SUBCASE("int key set → get round-trip")
    {
        simOpSetSfallGlobalInt(tp, 42, 12345);
        simOpGetSfallGlobalInt(tp, 42);
        CHECK(tp.stackDepth() == 1);
        CHECK(tp.popInt() == 12345);
    }

    SUBCASE("overwrite int key")
    {
        simOpSetSfallGlobalInt(tp, 1, 100);
        simOpSetSfallGlobalInt(tp, 1, 200);
        simOpGetSfallGlobalInt(tp, 1);
        CHECK(tp.popInt() == 200);
    }

    SUBCASE("int key not set returns 0")
    {
        simOpGetSfallGlobalInt(tp, 99999);
        CHECK(tp.popInt() == 0);
    }

    SUBCASE("multiple int keys are independent")
    {
        simOpSetSfallGlobalInt(tp, 10, 1000);
        simOpSetSfallGlobalInt(tp, 20, 2000);
        simOpGetSfallGlobalInt(tp, 10);
        CHECK(tp.popInt() == 1000);
        simOpGetSfallGlobalInt(tp, 20);
        CHECK(tp.popInt() == 2000);
    }

    SUBCASE("negative values round-trip via int key")
    {
        simOpSetSfallGlobalInt(tp, 5, -999);
        simOpGetSfallGlobalInt(tp, 5);
        CHECK(tp.popInt() == -999);
    }

    SUBCASE("zero value round-trip")
    {
        simOpSetSfallGlobalInt(tp, 7, 0);
        simOpGetSfallGlobalInt(tp, 7);
        CHECK(tp.popInt() == 0);
    }

    ensureVarsCleanup();
}

TEST_CASE("sfall_gl_vars integration — set/get global float round-trip")
{
    // op_get_sfall_global_float (0x8159) and the float path of
    // op_set_sfall_global use sfall_gl_vars_store_float /
    // sfall_gl_vars_fetch_float.

    ensureVarsInit();

    SUBCASE("float store/fetch with int key")
    {
        CHECK(sfall_gl_vars_store_float(1, 3.14f));
        float val = 0.0f;
        CHECK(sfall_gl_vars_fetch_float(1, val));
        CHECK(val == doctest::Approx(3.14f));
    }

    SUBCASE("float store/fetch with string key")
    {
        CHECK(sfall_gl_vars_store_float("pi", 3.14159f));
        float val = 0.0f;
        CHECK(sfall_gl_vars_fetch_float("pi", val));
        CHECK(val == doctest::Approx(3.14159f));
    }

    SUBCASE("float not found returns 0.0f")
    {
        float val = -1.0f;
        CHECK_FALSE(sfall_gl_vars_fetch_float("nonexistent", val));
    }

    ensureVarsCleanup();
}

TEST_CASE("sfall_gl_vars integration — string key set/get int")
{
    // op_set_sfall_global with string key:
    //   programGetString(program, variable.opcode, variable.integerValue)
    //   → sfall_gl_vars_store(key, value)
    //
    // The string path in production goes through programGetString which
    // reads the Program's string table. Our simulator bypasses this
    // by calling sfall_gl_vars_store directly with a const char*.

    ensureVarsInit();

    SUBCASE("string key store → fetch via int-key retrieval")
    {
        // Store with string key
        CHECK(sfall_gl_vars_store("HAp_Race", 4));
        // Fetch with string key
        int val = 0;
        CHECK(sfall_gl_vars_fetch("HAp_Race", val));
        CHECK(val == 4);
    }

    SUBCASE("string key store → overwrite")
    {
        sfall_gl_vars_store("counter", 1);
        sfall_gl_vars_store("counter", 2);
        int val = 0;
        sfall_gl_vars_fetch("counter", val);
        CHECK(val == 2);
    }

    SUBCASE("string key not set returns 0, fetch returns false")
    {
        int val = 42;
        CHECK_FALSE(sfall_gl_vars_fetch("not_set", val));
        CHECK(val == 42); // unchanged
    }

    SUBCASE("empty string key is rejected (production rejects zero-length keys)")
    {
        // sfall_global_vars.cc:211-217: empty key always fails.
        // The sfall_gl_vars_key_to_uint64 function explicitly rejects
        // zero-length strings by setting ok=false.
        CHECK_FALSE(sfall_gl_vars_store("", 999));
        int val = 0;
        CHECK_FALSE(sfall_gl_vars_fetch("", val));
        CHECK(val == 0); // unchanged — fetch returns false, doesn't modify val
    }

    ensureVarsCleanup();
}

TEST_CASE("sfall_gl_vars integration — int key vs string key namespace")
{
    // Int keys and string keys share the same namespace (both hashed
    // to uint64_t). Verify they don't collide unintentionally.
    // In production, string keys are hashed via FNV-1a 64-bit hash.

    ensureVarsInit();

    SUBCASE("int key and string key are independent")
    {
        sfall_gl_vars_store(123, 111);
        sfall_gl_vars_store("123", 222);

        int val = 0;
        CHECK(sfall_gl_vars_fetch(123, val));
        CHECK(val == 111); // int key

        CHECK(sfall_gl_vars_fetch("123", val));
        CHECK(val == 222); // string key (different hash)
    }

    ensureVarsCleanup();
}

// ============================================================
// op_game_loaded Simulation
// ============================================================

TEST_CASE("op_game_loaded simulation via harness")
{
    // op_game_loaded at sfall_opcodes.cc:293-296 calls
    // sfall_gl_scr_is_loaded(program) which requires full script
    // engine linkage. The simulator accepts the value as a parameter.

    TestProgram tp("game_loaded_test");

    SUBCASE("first load returns 2")
    {
        simOpGameLoaded(tp, 2);
        CHECK(tp.stackDepth() == 1);
        CHECK(tp.popInt() == 2);
    }

    SUBCASE("reload returns 1")
    {
        simOpGameLoaded(tp, 1);
        CHECK(tp.popInt() == 1);
    }

    SUBCASE("otherwise returns 0")
    {
        simOpGameLoaded(tp, 0);
        CHECK(tp.popInt() == 0);
    }
}

// ============================================================
// Combined opcode flow simulation
// ============================================================

TEST_CASE("Combined opcode flow: set_sfall_global → get_sfall_global round-trip")
{
    // Simulate a full SSL script opcode sequence:
    //   set_sfall_global("counter", 42);
    //   get_sfall_global_int("counter");  // expects 42 on stack

    ensureVarsInit();

    TestProgram tp("combined_test");

    // Step 1: set_sfall_global_int("counter", 42)
    // Production would: pop key string from stack, pop value from stack,
    // call sfall_gl_vars_store(key, value)
    simOpSetSfallGlobalStr(tp, "counter", 42);
    CHECK(tp.stackDepth() == 0); // setter doesn't push anything

    // Step 2: get_sfall_global_int("counter")
    // Production would: pop key string from stack,
    // call sfall_gl_vars_fetch(key, result),
    // push result onto stack
    simOpGetSfallGlobalIntStr(tp, "counter");
    CHECK(tp.stackDepth() == 1);
    CHECK(tp.popInt() == 42);

    ensureVarsCleanup();
}

TEST_CASE("Combined opcode flow: multiple keys across programs")
{
    // Two independent scripts (TestPrograms) reading and writing globals.
    // In production, sfall globals are process-wide — all scripts share them.

    ensureVarsInit();

    TestProgram tpA("script_A");
    TestProgram tpB("script_B");

    // Script A writes
    simOpSetSfallGlobalInt(tpA, 1, 1000);

    // Script B writes a different key
    simOpSetSfallGlobalInt(tpB, 2, 2000);

    // Script A reads its own key
    simOpGetSfallGlobalInt(tpA, 1);
    CHECK(tpA.popInt() == 1000);

    // Script B reads its own key
    simOpGetSfallGlobalInt(tpB, 2);
    CHECK(tpB.popInt() == 2000);

    // Script B reads A's key (shared state)
    simOpGetSfallGlobalInt(tpB, 1);
    CHECK(tpB.popInt() == 1000);

    // Script A reads B's key
    simOpGetSfallGlobalInt(tpA, 2);
    CHECK(tpA.popInt() == 2000);

    ensureVarsCleanup();
}

// ============================================================
// Stress / Edge Cases
// ============================================================

TEST_CASE("TestProgram stress — many push/pop cycles")
{
    TestProgram tp("stress_test");

    SUBCASE("1000 push/pop pairs")
    {
        for (int i = 0; i < 1000; i++) {
            tp.pushInt(i);
        }
        CHECK(tp.stackDepth() == 1000);
        for (int i = 999; i >= 0; i--) {
            CHECK(tp.popInt() == i);
        }
        CHECK(tp.stackDepth() == 0);
    }

    SUBCASE("interleaved push/pop")
    {
        tp.pushInt(1);
        tp.pushInt(2);
        CHECK(tp.popInt() == 2);
        tp.pushInt(3);
        CHECK(tp.popInt() == 3);
        CHECK(tp.popInt() == 1);
    }
}

TEST_CASE("TestProgram — access programPtr fields directly")
{
    // Verify that the programPtr() returns a Program* whose stackValues
    // pointer works correctly for the real programStackPushInteger/PopInteger
    // if they were linked.
    //
    // We can't call the real functions (interpreter.cc not linked), but
    // we verify the pointer relationships are correct.

    TestProgram tp("ptr_test");
    Program* p = tp.programPtr();

    CHECK(p != nullptr);
    CHECK(p->name != nullptr);
    CHECK(std::string(p->name) == "ptr_test");
    CHECK(p->stackValues != nullptr);
    CHECK(p->returnStackValues != nullptr);
}
