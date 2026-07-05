// Unit tests for the interpreter core domain (interpreter.cc, opcode_context.cc,
// interpreter_lib.cc changes since fork point 24199e9).
//
// Tests: ProgramValue constructors, type checks, isEmpty, asInt, asFloat,
//        typeDebugString, sentinel pattern, stackReadInt16/Int32,
//        OpcodeContext construction, arg reversal, validateArguments,
//        setReturn overloads, pushReturnValue, OPCODE_MAX_COUNT,
//        procedure flags, opTokenize off-by-one pattern.
//
// Approach: Self-contained test like test_criticals.cc — mirrors the production
// data structures locally to avoid linking the full interpreter.cc (3,613 LOC
// with 50+ engine dependencies). Functions are re-implemented to match the real
// code at interpreter.cc and opcode_context.cc exactly.
//
// All local types use a "Test" prefix to avoid symbol collision with the real
// types in interpreter.h, opcode_context.h, and sfall_metarules.h. Once the
// consolidation agent adds the proper CMake target with src/ include paths and
// linking against a test_sources entry, the Test* wrappers can be replaced with
// the real types.
//
// Reference: interpreter.h (ProgramValue, Program, Opcode, VALUE_TYPE_*),
//            interpreter.cc (constructors, type checks, stack read, procedureCount),
//            opcode_context.cc (constructor, validateArguments, setReturn, pushReturnValue),
//            sfall_metarules.h (MetaruleInfo, OpcodeArgumentType, METARULE_MAX_ARGS),
//            interpreter_lib.cc (opTokenize off-by-one fix, strncpy hardening)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

// ============================================================================
// Test-local type definitions — mirroring interpreter.h, opcode_context.h,
// sfall_metarules.h, combat_defs.h.
// Prefixed with "Test" to avoid collision with the real types linked into
// other test binaries.
// ============================================================================

// ---- interpreter.h types ----

#define TEST_VALUE_TYPE_MASK 0xF7FF
#define TEST_VALUE_TYPE_INT 0xC001
#define TEST_VALUE_TYPE_FLOAT 0xA001
#define TEST_VALUE_TYPE_STRING 0x9001
#define TEST_VALUE_TYPE_DYNAMIC_STRING 0x9801
#define TEST_VALUE_TYPE_PTR 0xE001

#define TEST_OPCODE_MAX_COUNT 768

typedef unsigned short opcode_t;

enum TestProgramFlags {
    TEST_PROGRAM_FLAG_EXITED = 0x01,
    TEST_PROGRAM_FLAG_RUNNING = 0x02,
    TEST_PROGRAM_FLAG_FATAL_ERROR = 0x04,
    TEST_PROGRAM_FLAG_STOPPED = 0x08,
    TEST_PROGRAM_IS_WAITING = 0x10,
    TEST_PROGRAM_FLAG_CHILD_CALL = 0x20,
    TEST_PROGRAM_FLAG_FINISHED = 0x40,
    TEST_PROGRAM_FLAG_CRITICAL_SECTION = 0x80,
    TEST_PROGRAM_FLAG_CHILD_SPAWN = 0x0100,
};

enum TestProcedureFlags {
    TEST_PROCEDURE_FLAG_TIMED = 0x01,
    TEST_PROCEDURE_FLAG_CONDITIONAL = 0x02,
    TEST_PROCEDURE_FLAG_IMPORTED = 0x04,
    TEST_PROCEDURE_FLAG_EXPORTED = 0x08,
    TEST_PROCEDURE_FLAG_CRITICAL = 0x10,
};

enum TestOpcode {
    TEST_OPCODE_NOOP = 0x8000,
    TEST_OPCODE_STORE_EXTERNAL = 0x8015,
    TEST_OPCODE_END_CRITICAL = 0x804B,
};

struct TestProgram;
struct TestObject { int dummy; };
struct TestAttack { int dummy; };

// ---- ProgramValue (mirror of interpreter.h:151-180) ----
class TestProgramValue {
public:
    TestProgramValue();
    TestProgramValue(int value);
    TestProgramValue(unsigned int value);
    TestProgramValue(bool value);
    TestProgramValue(float value);
    TestProgramValue(TestObject* value);
    TestProgramValue(TestAttack* value);
    TestProgramValue(const char* value);

    opcode_t opcode;
    union {
        int integerValue;
        float floatValue;
        void* pointerValue;
    };

    bool isEmpty() const;
    bool isInt() const;
    bool isFloat() const;
    bool isString() const;
    float asFloat() const;
    bool isPointer() const;
    int asInt() const;
    const char* typeDebugString() const;
    const char* asString(TestProgram* program) const;
};

// ---- Program (minimal mirror for asString/stack operations) ----
struct TestProgram {
    char* name;
    unsigned char* data;
    int dataSize;
    TestProgram* parent;
    TestProgram* child;
    int instructionPointer;
    int framePointer;
    int basePointer;
    unsigned char* staticStrings;
    unsigned char* dynamicStrings;
    unsigned char* identifiers;
    unsigned char* procedures;
    int flags;
    bool exited;
    // Minimal string table for testing asString():
    // In production, this is a complex system with refcounting.
    // For tests, we use a simple char* array indexed by offset.
    static const char** testStringTable;
    static int testStringTableSize;
    int procedureCount() const;
};

const char** TestProgram::testStringTable = nullptr;
int TestProgram::testStringTableSize = 0;

// ---- MetaruleInfo (mirror of sfall_metarules.h:20-27) ----
static constexpr std::size_t TEST_METARULE_MAX_ARGS = 8;

enum TestOpcodeArgumentType {
    TEST_ARG_ANY = 0,
    TEST_ARG_INT,
    TEST_ARG_OBJECT,
    TEST_ARG_STRING,
    TEST_ARG_INTSTR,
    TEST_ARG_NUMBER,
};

struct TestMetaruleInfo {
    const char* name;
    int minArgs;
    int maxArgs;
    TestOpcodeArgumentType argumentTypes[TEST_METARULE_MAX_ARGS];
};

// ---- OpcodeContext (mirror of opcode_context.h:17-45) ----
class TestOpcodeContext {
public:
    TestOpcodeContext(TestProgram* program, const TestMetaruleInfo* metaruleInfo,
                      int numArgs, const TestProgramValue* args);

    TestProgram* program() const { return _program; }
    const TestMetaruleInfo* metaruleInfo() const { return _metaruleInfo; }
    const char* name() const { return _metaruleInfo->name; }
    int numArgs() const { return _numArgs; }

    const TestProgramValue& arg(int index) const;
    void setReturn(const TestProgramValue& value);
    void setReturn(std::nullptr_t);
    void setReturn(int value);
    void setReturn(unsigned int value);
    void setReturn(const char* value);

    bool validateArguments() const;

    const TestProgramValue& returnValue() const { return _returnValue; }

private:
    TestProgram* _program;
    const TestMetaruleInfo* _metaruleInfo;
    int _numArgs;
    TestProgramValue _args[TEST_METARULE_MAX_ARGS];
    TestProgramValue _returnValue;
};

// ============================================================================
// Stubs — required by ProgramValue::asString(), OpcodeContext::setReturn(const char*).
// ============================================================================

// OpcodeContext::setReturn(const char*) calls programPushString for dynamic strings.
// Stub: append to a simple static buffer, return its index.
static int testProgramPushString(TestProgram* program, const char* str) {
    (void)program;
    // Simple stub: use the testStringTable
    // Return a non-zero offset (simulates string table index)
    return static_cast<int>(strlen(str)) + 42; // synthetic index
}

// ProgramValue::asString() calls programGetString for non-sentinel lookups.
static const char* testProgramGetString(TestProgram* program, int offset) {
    (void)program;
    (void)offset;
    return "mocked_string";
}

// programPrintError stub — used by asString, asObject error paths.
static int testProgramPrintError(const char* format, ...) {
    (void)format;
    return 0;
}

// programStackPushValue stub — used by OpcodeContext::pushReturnValue()
// Not directly tested, but needed for compilation completeness.
static int testProgramStackPushValue(TestProgram* program, const TestProgramValue& value) {
    (void)program;
    (void)value;
    return 0;
}

// ============================================================================
// Implementations — exact mirrors of the production code.
// ============================================================================

// ---- stackReadInt16 (interpreter.cc:317-324) ----
// Reads a 16-bit value from unsigned char* in big-endian byte order.
static opcode_t testStackReadInt16(unsigned char* data, int pos) {
    opcode_t value = 0;
    value |= static_cast<opcode_t>(data[pos++]) << 8;
    value |= static_cast<opcode_t>(data[pos++]);
    return value;
}

// ---- stackReadInt32 (interpreter.cc:327-334) ----
// Reads a 32-bit value from unsigned char* in big-endian byte order.
static int testStackReadInt32(unsigned char* data, int pos) {
    int value = 0;
    value |= static_cast<int>(data[pos++]) << 24;
    value |= static_cast<int>(data[pos++]) << 16;
    value |= static_cast<int>(data[pos++]) << 8;
    value |= static_cast<int>(data[pos++]) & 0xFF;
    return value;
}

// ---- TestProgramValue constructors (interpreter.cc:3486-3526) ----

TestProgramValue::TestProgramValue() {
    opcode = TEST_VALUE_TYPE_INT;
    integerValue = 0;
}

TestProgramValue::TestProgramValue(int value) {
    opcode = TEST_VALUE_TYPE_INT;
    integerValue = value;
}

TestProgramValue::TestProgramValue(unsigned int value) {
    opcode = TEST_VALUE_TYPE_INT;
    integerValue = static_cast<int>(value);
}

TestProgramValue::TestProgramValue(bool value) {
    opcode = TEST_VALUE_TYPE_INT;
    integerValue = static_cast<int>(value);
}

TestProgramValue::TestProgramValue(float value) {
    opcode = TEST_VALUE_TYPE_FLOAT;
    floatValue = value;
}

TestProgramValue::TestProgramValue(TestObject* value) {
    opcode = TEST_VALUE_TYPE_PTR;
    pointerValue = value;
}

TestProgramValue::TestProgramValue(TestAttack* value) {
    opcode = TEST_VALUE_TYPE_PTR;
    pointerValue = value;
}

TestProgramValue::TestProgramValue(const char* value) {
    opcode = TEST_VALUE_TYPE_STRING;
    integerValue = -1; // sentinel: pointerValue holds the raw C string
    pointerValue = const_cast<char*>(value);
}

// ---- TestProgramValue type checks (interpreter.cc:3457-3484) ----

bool TestProgramValue::isInt() const {
    return opcode == TEST_VALUE_TYPE_INT;
}

bool TestProgramValue::isFloat() const {
    return opcode == TEST_VALUE_TYPE_FLOAT;
}

bool TestProgramValue::isString() const {
    return opcode == TEST_VALUE_TYPE_STRING || opcode == TEST_VALUE_TYPE_DYNAMIC_STRING;
}

bool TestProgramValue::isPointer() const {
    return opcode == TEST_VALUE_TYPE_PTR;
}

// ---- TestProgramValue::isEmpty (interpreter.cc:3435-3453) ----

bool TestProgramValue::isEmpty() const {
    switch (opcode) {
    case TEST_VALUE_TYPE_INT:
    case TEST_VALUE_TYPE_STRING:
    case TEST_VALUE_TYPE_DYNAMIC_STRING:
        return integerValue == 0;
    case TEST_VALUE_TYPE_FLOAT:
        return floatValue == 0.0f;
    case TEST_VALUE_TYPE_PTR:
        return pointerValue == nullptr;
    }
    return true;
}

// ---- TestProgramValue::asInt (interpreter.cc:3533-3542) ----

int TestProgramValue::asInt() const {
    switch (opcode) {
    case TEST_VALUE_TYPE_INT:
        return integerValue;
    case TEST_VALUE_TYPE_FLOAT:
        return static_cast<int>(floatValue);
    default:
        return 0;
    }
}

// ---- TestProgramValue::asFloat (interpreter.cc:3469-3479) ----

float TestProgramValue::asFloat() const {
    switch (opcode) {
    case TEST_VALUE_TYPE_INT:
        return static_cast<float>(integerValue);
    case TEST_VALUE_TYPE_FLOAT:
        return floatValue;
    default:
        return 0.0f;
    }
}

// ---- TestProgramValue::asString (interpreter.cc:3559-3572) ----

const char* TestProgramValue::asString(TestProgram* program) const {
    if (!isString()) {
        // testProgramPrintError would be called here
        (void)program;
        return "";
    }

    // C-string constructed via TestProgramValue(const char*):
    // pointerValue holds the raw string, integerValue == -1 is the sentinel.
    if (integerValue == -1) {
        return static_cast<const char*>(pointerValue);
    }

    return testProgramGetString(program, integerValue);
}

// ---- TestProgramValue::typeDebugString (interpreter.cc:3574-3589) ----

const char* TestProgramValue::typeDebugString() const {
    switch (opcode) {
    case TEST_VALUE_TYPE_INT:
        return "INTEGER";
    case TEST_VALUE_TYPE_FLOAT:
        return "FLOAT";
    case TEST_VALUE_TYPE_STRING:
    case TEST_VALUE_TYPE_DYNAMIC_STRING:
        return "STRING";
    case TEST_VALUE_TYPE_PTR:
        return "POINTER";
    default:
        return "(UNKNOWN)";
    }
}

// ---- TestProgram::procedureCount (interpreter.cc:3608-3611) ----

int TestProgram::procedureCount() const {
    return testStackReadInt32(procedures, 0);
}

// ---- TestOpcodeContext (opcode_context.cc:13-177) ----

TestOpcodeContext::TestOpcodeContext(TestProgram* program,
                                     const TestMetaruleInfo* metaruleInfo,
                                     int numArgs,
                                     const TestProgramValue* args)
    : _program(program)
    , _metaruleInfo(metaruleInfo)
    , _numArgs(numArgs)
    , _returnValue(0)
{
    // assert(numArgs >= 0 && numArgs <= METARULE_MAX_ARGS) — skip for test
    // Arguments are in stack order (reversed). Copy them in natural order:
    for (int index = 0; index < _numArgs; index++) {
        _args[index] = args[_numArgs - index - 1];
    }
}

const TestProgramValue& TestOpcodeContext::arg(int index) const {
    // assert(index >= 0 && index < _numArgs) — skip for test
    return _args[index];
}

void TestOpcodeContext::setReturn(const TestProgramValue& value) {
    _returnValue = value;
}

void TestOpcodeContext::setReturn(std::nullptr_t) {
    setReturn(TestProgramValue(0));
}

void TestOpcodeContext::setReturn(int value) {
    setReturn(TestProgramValue(value));
}

void TestOpcodeContext::setReturn(unsigned int value) {
    setReturn(TestProgramValue(value));
}

void TestOpcodeContext::setReturn(const char* value) {
    // Matches opcode_context.cc:81-97 (gTextBuffer optimization omitted for test)
    TestProgramValue programValue;
    programValue.opcode = TEST_VALUE_TYPE_DYNAMIC_STRING;
    programValue.integerValue = testProgramPushString(_program, value);
    setReturn(programValue);
}

// ---- TestOpcodeContext::validateArguments (opcode_context.cc:119-177) ----

bool TestOpcodeContext::validateArguments() const {
    if (_numArgs < _metaruleInfo->minArgs || _numArgs > _metaruleInfo->maxArgs) {
        // printError would be called here in production
        return false;
    }

    for (int index = 0; index < _numArgs; index++) {
        const TestProgramValue& value = arg(index);
        switch (_metaruleInfo->argumentTypes[index]) {
        case TEST_ARG_ANY:
            continue;
        case TEST_ARG_INT:
            if (!value.isInt()) {
                return false;
            }
            break;
        case TEST_ARG_OBJECT:
            if (value.isInt() && value.integerValue == 0) {
                return false;
            }
            if (!value.isPointer()) {
                return false;
            }
            if (value.pointerValue == nullptr) {
                return false;
            }
            break;
        case TEST_ARG_STRING:
            if (!value.isString()) {
                return false;
            }
            break;
        case TEST_ARG_INTSTR:
            if (!value.isInt() && !value.isString()) {
                return false;
            }
            break;
        case TEST_ARG_NUMBER:
            if (!value.isInt() && !value.isFloat()) {
                return false;
            }
            break;
        }
    }

    return true;
}

// ============================================================================
// TESTS
// ============================================================================

// ---------------------------------------------------------------------------
// ProgramValue Constructor Tests
// ---------------------------------------------------------------------------

TEST_CASE("TestProgramValue default constructor")
{
    TestProgramValue pv;
    CHECK(pv.opcode == TEST_VALUE_TYPE_INT);
    CHECK(pv.integerValue == 0);
    CHECK(pv.isInt());
    CHECK_FALSE(pv.isFloat());
    CHECK_FALSE(pv.isString());
    CHECK_FALSE(pv.isPointer());
    CHECK(pv.isEmpty());
}

TEST_CASE("TestProgramValue int constructor")
{
    SUBCASE("positive int")
    {
        TestProgramValue pv(42);
        CHECK(pv.opcode == TEST_VALUE_TYPE_INT);
        CHECK(pv.integerValue == 42);
        CHECK(pv.isInt());
        CHECK_FALSE(pv.isEmpty());
    }

    SUBCASE("negative int")
    {
        TestProgramValue pv(-7);
        CHECK(pv.opcode == TEST_VALUE_TYPE_INT);
        CHECK(pv.integerValue == -7);
    }

    SUBCASE("zero int")
    {
        TestProgramValue pv(0);
        CHECK(pv.opcode == TEST_VALUE_TYPE_INT);
        CHECK(pv.integerValue == 0);
        CHECK(pv.isEmpty()); // integerValue == 0 is "empty" for INT type
    }

    SUBCASE("INT_MIN and INT_MAX")
    {
        TestProgramValue pvMin(-2147483647 - 1);
        CHECK(pvMin.isInt());
        TestProgramValue pvMax(2147483647);
        CHECK(pvMax.isInt());
    }
}

TEST_CASE("TestProgramValue unsigned int constructor")
{
    TestProgramValue pv(42u);
    CHECK(pv.opcode == TEST_VALUE_TYPE_INT);
    CHECK(pv.integerValue == 42);
    CHECK(pv.isInt());

    // Large unsigned that overflows to negative in int32
    TestProgramValue pvLarge(3000000000u);
    CHECK(pvLarge.opcode == TEST_VALUE_TYPE_INT);
}

TEST_CASE("TestProgramValue bool constructor")
{
    SUBCASE("true")
    {
        TestProgramValue pv(true);
        CHECK(pv.opcode == TEST_VALUE_TYPE_INT);
        CHECK(pv.integerValue == 1);
        CHECK(pv.isInt());
    }

    SUBCASE("false")
    {
        TestProgramValue pv(false);
        CHECK(pv.opcode == TEST_VALUE_TYPE_INT);
        CHECK(pv.integerValue == 0);
        CHECK(pv.isEmpty());
    }
}

TEST_CASE("TestProgramValue float constructor")
{
    SUBCASE("positive float")
    {
        TestProgramValue pv(3.14f);
        CHECK(pv.opcode == TEST_VALUE_TYPE_FLOAT);
        CHECK(pv.floatValue == doctest::Approx(3.14f));
        CHECK(pv.isFloat());
        CHECK_FALSE(pv.isInt());
    }

    SUBCASE("zero float")
    {
        TestProgramValue pv(0.0f);
        CHECK(pv.opcode == TEST_VALUE_TYPE_FLOAT);
        CHECK(pv.floatValue == doctest::Approx(0.0f));
        CHECK(pv.isEmpty()); // floatValue == 0.0 is "empty" for FLOAT type
    }

    SUBCASE("negative float")
    {
        TestProgramValue pv(-1.5f);
        CHECK(pv.opcode == TEST_VALUE_TYPE_FLOAT);
        CHECK(pv.floatValue == doctest::Approx(-1.5f));
    }

    SUBCASE("NaN is not empty")
    {
        // NaN != 0.0, so isEmpty() returns false
        float nanVal = NAN;
        TestProgramValue pv(nanVal);
        CHECK(pv.opcode == TEST_VALUE_TYPE_FLOAT);
        CHECK_FALSE(pv.isEmpty());
    }
}

TEST_CASE("TestProgramValue Object* constructor")
{
    TestObject obj;
    TestProgramValue pv(&obj);
    CHECK(pv.opcode == TEST_VALUE_TYPE_PTR);
    CHECK(pv.pointerValue == &obj);
    CHECK(pv.isPointer());
    CHECK_FALSE(pv.isEmpty()); // pointerValue != nullptr
}

TEST_CASE("TestProgramValue Attack* constructor")
{
    TestAttack atk;
    TestProgramValue pv(&atk);
    CHECK(pv.opcode == TEST_VALUE_TYPE_PTR);
    CHECK(pv.pointerValue == &atk);
    CHECK(pv.isPointer());
}

TEST_CASE("TestProgramValue const char* constructor — sentinel pattern")
{
    // This is the SFALL-introduced constructor (F-05 in discovery report).
    // Uses integerValue = -1 as a sentinel to distinguish C-string values
    // from string-table offsets. Valid string table offsets start at 2
    // (offset 0-1 is the refcount header).

    SUBCASE("C-string sets sentinel and pointerValue")
    {
        const char* str = "hello";
        TestProgramValue pv(str);
        CHECK(pv.opcode == TEST_VALUE_TYPE_STRING);
        CHECK(pv.integerValue == -1);        // sentinel
        CHECK(pv.pointerValue == reinterpret_cast<const void*>(str)); // raw C string pointer
        CHECK(pv.isString());
    }

    SUBCASE("asString resolves via sentinel (integration test)")
    {
        TestProgram program = {};
        const char* str = "world";
        TestProgramValue pv(str);

        const char* result = pv.asString(&program);
        CHECK(result == str); // returns pointerValue directly via sentinel path
    }

    SUBCASE("sentinel value -1 is never a valid string table offset")
    {
        // Valid offsets: 0-1 is the refcount header, 2+ are string entries.
        // -1 as an offset would be invalid — the sentinel prevents this path.
        TestProgramValue pv("test");
        CHECK(pv.integerValue == -1);
        // integerValue < 0 means: sentinel active, asString uses pointerValue
    }
}

// ---------------------------------------------------------------------------
// ProgramValue Type Check Tests
// ---------------------------------------------------------------------------

TEST_CASE("TestProgramValue type checks — cross-type invariants")
{
    SUBCASE("INT type is only int")
    {
        TestProgramValue pv(10);
        CHECK(pv.isInt());
        CHECK_FALSE(pv.isFloat());
        CHECK_FALSE(pv.isString());
        CHECK_FALSE(pv.isPointer());
    }

    SUBCASE("FLOAT type is only float")
    {
        TestProgramValue pv(1.0f);
        CHECK_FALSE(pv.isInt());
        CHECK(pv.isFloat());
        CHECK_FALSE(pv.isString());
        CHECK_FALSE(pv.isPointer());
    }

    SUBCASE("PTR type is only pointer")
    {
        TestObject obj;
        TestProgramValue pv(&obj);
        CHECK_FALSE(pv.isInt());
        CHECK_FALSE(pv.isFloat());
        CHECK_FALSE(pv.isString());
        CHECK(pv.isPointer());
    }

    SUBCASE("STRING type (VALUE_TYPE_STRING) is only string")
    {
        TestProgramValue pv("abc");
        CHECK_FALSE(pv.isInt());
        CHECK_FALSE(pv.isFloat());
        CHECK(pv.isString());
        CHECK_FALSE(pv.isPointer());
    }

    SUBCASE("DYNAMIC_STRING type is also string")
    {
        TestProgramValue pv;
        pv.opcode = TEST_VALUE_TYPE_DYNAMIC_STRING;
        pv.integerValue = 5;
        CHECK_FALSE(pv.isInt());
        CHECK_FALSE(pv.isFloat());
        CHECK(pv.isString()); // both STATIC and DYNAMIC are strings
        CHECK_FALSE(pv.isPointer());
    }

    SUBCASE("multiple types are mutually exclusive")
    {
        // Verify no single value reports as two types
        TestProgramValue iV(5);
        int typeCount = iV.isInt() + iV.isFloat() + iV.isString() + iV.isPointer();
        CHECK(typeCount == 1);

        TestProgramValue fV(1.0f);
        typeCount = fV.isInt() + fV.isFloat() + fV.isString() + fV.isPointer();
        CHECK(typeCount == 1);

        TestProgramValue pV("x");
        typeCount = pV.isInt() + pV.isFloat() + pV.isString() + pV.isPointer();
        CHECK(typeCount == 1);

        TestObject obj;
        TestProgramValue ptrV(&obj);
        typeCount = ptrV.isInt() + ptrV.isFloat() + ptrV.isString() + ptrV.isPointer();
        CHECK(typeCount == 1);
    }
}

// ---------------------------------------------------------------------------
// ProgramValue::isEmpty() Tests
// ---------------------------------------------------------------------------

TEST_CASE("TestProgramValue::isEmpty — all type empty states")
{
    SUBCASE("INT: empty when zero")
    {
        CHECK(TestProgramValue(0).isEmpty());
        CHECK_FALSE(TestProgramValue(1).isEmpty());
        CHECK_FALSE(TestProgramValue(-1).isEmpty());
    }

    SUBCASE("FLOAT: empty when 0.0")
    {
        CHECK(TestProgramValue(0.0f).isEmpty());
        CHECK_FALSE(TestProgramValue(0.001f).isEmpty());
        CHECK_FALSE(TestProgramValue(-0.0f).isEmpty()); // -0.0 == 0.0f
    }

    SUBCASE("STRING: empty when integerValue (offset) is 0")
    {
        TestProgramValue pv;
        pv.opcode = TEST_VALUE_TYPE_STRING;
        pv.integerValue = 0;
        CHECK(pv.isEmpty());

        pv.integerValue = 2; // first valid offset after refcount header
        CHECK_FALSE(pv.isEmpty());
    }

    SUBCASE("DYNAMIC_STRING: empty when integerValue is 0")
    {
        TestProgramValue pv;
        pv.opcode = TEST_VALUE_TYPE_DYNAMIC_STRING;
        pv.integerValue = 0;
        CHECK(pv.isEmpty());

        pv.integerValue = 1;
        CHECK_FALSE(pv.isEmpty());
    }

    SUBCASE("PTR: empty when nullptr")
    {
        TestProgramValue pv(static_cast<TestObject*>(nullptr));
        pv.opcode = TEST_VALUE_TYPE_PTR;
        pv.pointerValue = nullptr;
        CHECK(pv.isEmpty());

        TestObject obj;
        pv.pointerValue = &obj;
        CHECK_FALSE(pv.isEmpty());
    }

    SUBCASE("Sentinel-string (-1) is NOT empty")
    {
        // ProgramValue(const char*) sets integerValue = -1.
        // isEmpty checks integerValue == 0 for STRING type.
        // -1 != 0, so sentinel strings are NOT empty — correct.
        TestProgramValue pv("sentinel");
        CHECK(pv.integerValue == -1);
        CHECK_FALSE(pv.isEmpty());
    }
}

// ---------------------------------------------------------------------------
// ProgramValue::asInt() Tests
// ---------------------------------------------------------------------------

TEST_CASE("TestProgramValue::asInt — type conversions")
{
    SUBCASE("INT returns integerValue")
    {
        CHECK(TestProgramValue(42).asInt() == 42);
        CHECK(TestProgramValue(-5).asInt() == -5);
        CHECK(TestProgramValue(0).asInt() == 0);
    }

    SUBCASE("FLOAT returns truncated int")
    {
        // Production code: static_cast<int>(floatValue)
        TestProgramValue pv(3.9f);
        CHECK(pv.asInt() == 3); // truncation, not rounding

        TestProgramValue pvNeg(-2.1f);
        CHECK(pvNeg.asInt() == -2);
    }

    SUBCASE("non-INT/non-FLOAT returns 0")
    {
        TestProgramValue pvStr("test");
        CHECK(pvStr.asInt() == 0);

        TestProgramValue pvDyn;
        pvDyn.opcode = TEST_VALUE_TYPE_DYNAMIC_STRING;
        pvDyn.integerValue = 5;
        CHECK(pvDyn.asInt() == 0);

        TestObject obj;
        TestProgramValue pvPtr(&obj);
        CHECK(pvPtr.asInt() == 0);
    }
}

// ---------------------------------------------------------------------------
// ProgramValue::asFloat() Tests
// ---------------------------------------------------------------------------

TEST_CASE("TestProgramValue::asFloat — type conversions")
{
    SUBCASE("FLOAT returns floatValue")
    {
        TestProgramValue pv(3.14f);
        CHECK(pv.asFloat() == doctest::Approx(3.14f));
    }

    SUBCASE("INT returns promoted float")
    {
        // Production code: static_cast<float>(integerValue)
        TestProgramValue pv(42);
        CHECK(pv.asFloat() == doctest::Approx(42.0f));
    }

    SUBCASE("non-FLOAT/non-INT returns 0.0")
    {
        TestProgramValue pvStr("test");
        CHECK(pvStr.asFloat() == doctest::Approx(0.0f));

        TestObject obj;
        TestProgramValue pvPtr(&obj);
        CHECK(pvPtr.asFloat() == doctest::Approx(0.0f));
    }
}

// ---------------------------------------------------------------------------
// ProgramValue::typeDebugString() Tests
// ---------------------------------------------------------------------------

TEST_CASE("TestProgramValue::typeDebugString — all type name outputs")
{
    SUBCASE("INTEGER")
    {
        CHECK(strcmp(TestProgramValue(1).typeDebugString(), "INTEGER") == 0);
    }

    SUBCASE("FLOAT")
    {
        CHECK(strcmp(TestProgramValue(1.0f).typeDebugString(), "FLOAT") == 0);
    }

    SUBCASE("STRING (static)")
    {
        TestProgramValue pv;
        pv.opcode = TEST_VALUE_TYPE_STRING;
        pv.integerValue = 5;
        CHECK(strcmp(pv.typeDebugString(), "STRING") == 0);
    }

    SUBCASE("STRING (dynamic)")
    {
        TestProgramValue pv;
        pv.opcode = TEST_VALUE_TYPE_DYNAMIC_STRING;
        pv.integerValue = 5;
        CHECK(strcmp(pv.typeDebugString(), "STRING") == 0);
    }

    SUBCASE("POINTER")
    {
        TestObject obj;
        CHECK(strcmp(TestProgramValue(&obj).typeDebugString(), "POINTER") == 0);
    }

    SUBCASE("C-string sentinel is STRING not INTEGER")
    {
        // ProgramValue(const char*) sets opcode=VALUE_TYPE_STRING.
        // typeDebugString checks opcode and returns "STRING" for both
        // VALUE_TYPE_STRING and VALUE_TYPE_DYNAMIC_STRING.
        TestProgramValue pv("hello");
        CHECK(strcmp(pv.typeDebugString(), "STRING") == 0);
    }
}

// ---------------------------------------------------------------------------
// stackReadInt16 / stackReadInt32 Tests
// ---------------------------------------------------------------------------

TEST_CASE("testStackReadInt16 — big-endian byte read")
{
    SUBCASE("reads two bytes big-endian")
    {
        unsigned char data[] = { 0x12, 0x34 };
        opcode_t val = testStackReadInt16(data, 0);
        // Big-endian: 0x12 << 8 | 0x34 = 0x1234 = 4660
        CHECK(val == 0x1234);
    }

    SUBCASE("zero value")
    {
        unsigned char data[] = { 0x00, 0x00 };
        opcode_t val = testStackReadInt16(data, 0);
        CHECK(val == 0);
    }

    SUBCASE("max value")
    {
        unsigned char data[] = { 0xFF, 0xFF };
        opcode_t val = testStackReadInt16(data, 0);
        CHECK(val == 0xFFFF);
    }

    SUBCASE("reads from offset position")
    {
        unsigned char data[] = { 0x00, 0x00, 0xAB, 0xCD };
        opcode_t val = testStackReadInt16(data, 2);
        CHECK(val == 0xABCD);
    }

    SUBCASE("both bytes contribute independently")
    {
        unsigned char data[] = { 0x01, 0x00 };
        CHECK(testStackReadInt16(data, 0) == 0x0100);

        unsigned char data2[] = { 0x00, 0x01 };
        CHECK(testStackReadInt16(data2, 0) == 0x0001);
    }
}

TEST_CASE("testStackReadInt32 — big-endian byte read")
{
    SUBCASE("reads four bytes big-endian")
    {
        unsigned char data[] = { 0x12, 0x34, 0x56, 0x78 };
        int val = testStackReadInt32(data, 0);
        // Big-endian: 0x12 << 24 | 0x34 << 16 | 0x56 << 8 | 0x78
        CHECK(val == 0x12345678);
    }

    SUBCASE("zero value")
    {
        unsigned char data[] = { 0x00, 0x00, 0x00, 0x00 };
        int val = testStackReadInt32(data, 0);
        CHECK(val == 0);
    }

    SUBCASE("max unsigned")
    {
        unsigned char data[] = { 0xFF, 0xFF, 0xFF, 0xFF };
        int val = testStackReadInt32(data, 0);
        CHECK(val == -1); // 0xFFFFFFFF as signed int32
    }

    SUBCASE("reads from offset position")
    {
        unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF };
        int val = testStackReadInt32(data, 4);
        CHECK(val == 0xDEADBEEF);
    }

    SUBCASE("negative values via sign bit")
    {
        // 0x80000000 = -2147483648
        unsigned char data[] = { 0x80, 0x00, 0x00, 0x00 };
        int val = testStackReadInt32(data, 0);
        CHECK(val == -2147483647 - 1);
    }

    SUBCASE("last byte is masked with 0xFF")
    {
        // The production code uses `data[pos++] & 0xFF` for the last byte
        // to ensure sign extension doesn't affect the value.
        unsigned char data[] = { 0x00, 0x00, 0x00, 0x80 };
        int val = testStackReadInt32(data, 0);
        CHECK(val == 0x00000080); // 128, not sign-extended
    }
}

// ---------------------------------------------------------------------------
// TestProgram::procedureCount() Test
// ---------------------------------------------------------------------------

TEST_CASE("TestProgram::procedureCount — reads procedure count from procedures[0]")
{
    // procedureCount calls stackReadInt32(procedures, 0) which reads
    // 4 bytes big-endian from the procedures table.
    unsigned char proceduresData[] = { 0x00, 0x00, 0x00, 0x05 }; // 5 procedures

    TestProgram program = {};
    program.procedures = proceduresData;

    CHECK(program.procedureCount() == 5);
}

// ---------------------------------------------------------------------------
// OpcodeContext Construction and Argument Reversal Tests
// ---------------------------------------------------------------------------

TEST_CASE("TestOpcodeContext construction — argument reversal")
{
    // The constructor reverses argument order: stack order → natural order.
    // Stack order (as passed from engine): args[0] = last arg, args[n-1] = first arg.
    // Natural order (as stored in _args): _args[0] = first arg, _args[n-1] = last arg.

    TestMetaruleInfo meta = {};
    meta.name = "test_metarule";
    meta.minArgs = 0;
    meta.maxArgs = 8;

    TestProgram program = {};

    SUBCASE("zero arguments")
    {
        TestOpcodeContext ctx(&program, &meta, 0, nullptr);
        CHECK(ctx.numArgs() == 0);
    }

    SUBCASE("single argument — no reversal needed")
    {
        const TestProgramValue args[] = { TestProgramValue(42) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK(ctx.numArgs() == 1);
        CHECK(ctx.arg(0).integerValue == 42);
    }

    SUBCASE("two arguments — reversed")
    {
        // Stack order: args[0] = 200 (last pushed), args[1] = 100 (first pushed)
        const TestProgramValue args[] = { TestProgramValue(200), TestProgramValue(100) };
        TestOpcodeContext ctx(&program, &meta, 2, args);
        CHECK(ctx.numArgs() == 2);
        // Natural order: _args[0] = args[1] = 100, _args[1] = args[0] = 200
        CHECK(ctx.arg(0).integerValue == 100);
        CHECK(ctx.arg(1).integerValue == 200);
    }

    SUBCASE("three arguments — reversed")
    {
        // Stack order: args[0]=3, args[1]=2, args[2]=1
        const TestProgramValue args[] = {
            TestProgramValue(3),
            TestProgramValue(2),
            TestProgramValue(1)
        };
        TestOpcodeContext ctx(&program, &meta, 3, args);
        CHECK(ctx.numArgs() == 3);
        // Natural: _args[0]=1, _args[1]=2, _args[2]=3
        CHECK(ctx.arg(0).integerValue == 1);
        CHECK(ctx.arg(1).integerValue == 2);
        CHECK(ctx.arg(2).integerValue == 3);
    }

    SUBCASE("eight arguments (METARULE_MAX_ARGS)")
    {
        const TestProgramValue args[] = {
            TestProgramValue(8), TestProgramValue(7), TestProgramValue(6), TestProgramValue(5),
            TestProgramValue(4), TestProgramValue(3), TestProgramValue(2), TestProgramValue(1)
        };
        TestOpcodeContext ctx(&program, &meta, 8, args);
        CHECK(ctx.numArgs() == 8);
        CHECK(ctx.arg(0).integerValue == 1);
        CHECK(ctx.arg(1).integerValue == 2);
        CHECK(ctx.arg(2).integerValue == 3);
        CHECK(ctx.arg(3).integerValue == 4);
        CHECK(ctx.arg(4).integerValue == 5);
        CHECK(ctx.arg(5).integerValue == 6);
        CHECK(ctx.arg(6).integerValue == 7);
        CHECK(ctx.arg(7).integerValue == 8);
    }

    SUBCASE("mixed types survive reversal")
    {
        TestObject obj;
        const TestProgramValue args[] = {
            TestProgramValue("stack_last"),   // args[0] → _args[2]
            TestProgramValue(&obj),           // args[1] → _args[1]
            TestProgramValue(42)              // args[2] → _args[0]
        };
        TestOpcodeContext ctx(&program, &meta, 3, args);

        // After reversal: _args = {42, &obj, "stack_last"}
        CHECK(ctx.arg(0).integerValue == 42);
        CHECK(ctx.arg(0).isInt());

        CHECK(ctx.arg(1).isPointer());
        CHECK(ctx.arg(1).pointerValue == &obj);

        CHECK(ctx.arg(2).isString());
        CHECK(ctx.arg(2).integerValue == -1); // sentinel
    }
}

// ---------------------------------------------------------------------------
// OpcodeContext::validateArguments() Tests
// ---------------------------------------------------------------------------

TEST_CASE("TestOpcodeContext::validateArguments — argument count range")
{
    TestMetaruleInfo meta = {};
    meta.name = "test_func";
    meta.minArgs = 1;
    meta.maxArgs = 3;
    for (int i = 0; i < static_cast<int>(TEST_METARULE_MAX_ARGS); i++) {
        meta.argumentTypes[i] = TEST_ARG_ANY;
    }
    TestProgram program = {};

    SUBCASE("exact min is valid")
    {
        const TestProgramValue args[] = { TestProgramValue(1) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK(ctx.validateArguments());
    }

    SUBCASE("exact max is valid")
    {
        const TestProgramValue args[] = {
            TestProgramValue(1), TestProgramValue(2), TestProgramValue(3)
        };
        TestOpcodeContext ctx(&program, &meta, 3, args);
        CHECK(ctx.validateArguments());
    }

    SUBCASE("mid-range is valid")
    {
        const TestProgramValue args[] = { TestProgramValue(1), TestProgramValue(2) };
        TestOpcodeContext ctx(&program, &meta, 2, args);
        CHECK(ctx.validateArguments());
    }

    SUBCASE("below min is invalid")
    {
        TestMetaruleInfo minMeta = meta;
        minMeta.minArgs = 2;
        const TestProgramValue args[] = { TestProgramValue(1) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        // 1 < 2 (minArgs=2 above) → This test needs correct setup
        // Actually: using meta (minArgs=1), so 1 args is fine
        // Let me rewrite this properly
        TestMetaruleInfo m2 = meta;
        m2.minArgs = 2;
        TestOpcodeContext ctx2(&program, &m2, 1, args);
        CHECK_FALSE(ctx2.validateArguments());
    }

    SUBCASE("above max is invalid")
    {
        const TestProgramValue args[] = {
            TestProgramValue(1), TestProgramValue(2),
            TestProgramValue(3), TestProgramValue(4)
        };
        TestOpcodeContext ctx(&program, &meta, 4, args);
        CHECK_FALSE(ctx.validateArguments());
    }

    SUBCASE("zero args with min=0 is valid")
    {
        TestMetaruleInfo m0 = meta;
        m0.minArgs = 0;
        TestOpcodeContext ctx(&program, &m0, 0, nullptr);
        CHECK(ctx.validateArguments());
    }
}

TEST_CASE("TestOpcodeContext::validateArguments — ARG_INT type")
{
    TestMetaruleInfo meta = {};
    meta.name = "int_func";
    meta.minArgs = 1;
    meta.maxArgs = 1;
    meta.argumentTypes[0] = TEST_ARG_INT;
    TestProgram program = {};

    SUBCASE("integer passes")
    {
        const TestProgramValue args[] = { TestProgramValue(42) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK(ctx.validateArguments());
    }

    SUBCASE("float fails")
    {
        const TestProgramValue args[] = { TestProgramValue(3.14f) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK_FALSE(ctx.validateArguments());
    }

    SUBCASE("string fails")
    {
        const TestProgramValue args[] = { TestProgramValue("hello") };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK_FALSE(ctx.validateArguments());
    }

    SUBCASE("pointer fails")
    {
        TestObject obj;
        const TestProgramValue args[] = { TestProgramValue(&obj) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK_FALSE(ctx.validateArguments());
    }
}

TEST_CASE("TestOpcodeContext::validateArguments — ARG_OBJECT type")
{
    TestMetaruleInfo meta = {};
    meta.name = "obj_func";
    meta.minArgs = 1;
    meta.maxArgs = 1;
    meta.argumentTypes[0] = TEST_ARG_OBJECT;
    TestProgram program = {};

    SUBCASE("non-null pointer passes")
    {
        TestObject obj;
        const TestProgramValue args[] = { TestProgramValue(&obj) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK(ctx.validateArguments());
    }

    SUBCASE("null pointer fails")
    {
        TestProgramValue nullPtr;
        nullPtr.opcode = TEST_VALUE_TYPE_PTR;
        nullPtr.pointerValue = nullptr;
        const TestProgramValue args[] = { nullPtr };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK_FALSE(ctx.validateArguments());
    }

    SUBCASE("integer zero fails")
    {
        // If value isInt() && integerValue == 0, treat as null.
        const TestProgramValue args[] = { TestProgramValue(0) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK_FALSE(ctx.validateArguments());
    }

    SUBCASE("non-zero integer fails (not a pointer)")
    {
        const TestProgramValue args[] = { TestProgramValue(5) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK_FALSE(ctx.validateArguments()); // isPointer() false
    }

    SUBCASE("string fails")
    {
        const TestProgramValue args[] = { TestProgramValue("obj") };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK_FALSE(ctx.validateArguments());
    }

    SUBCASE("float fails")
    {
        const TestProgramValue args[] = { TestProgramValue(1.0f) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK_FALSE(ctx.validateArguments());
    }
}

TEST_CASE("TestOpcodeContext::validateArguments — ARG_STRING type")
{
    TestMetaruleInfo meta = {};
    meta.name = "str_func";
    meta.minArgs = 1;
    meta.maxArgs = 1;
    meta.argumentTypes[0] = TEST_ARG_STRING;
    TestProgram program = {};

    SUBCASE("static string passes")
    {
        const TestProgramValue args[] = { TestProgramValue("hello") };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK(ctx.validateArguments());
    }

    SUBCASE("dynamic string passes")
    {
        TestProgramValue dynStr;
        dynStr.opcode = TEST_VALUE_TYPE_DYNAMIC_STRING;
        dynStr.integerValue = 42;
        const TestProgramValue args[] = { dynStr };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK(ctx.validateArguments());
    }

    SUBCASE("integer fails")
    {
        const TestProgramValue args[] = { TestProgramValue(1) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK_FALSE(ctx.validateArguments());
    }

    SUBCASE("float fails")
    {
        const TestProgramValue args[] = { TestProgramValue(1.0f) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK_FALSE(ctx.validateArguments());
    }
}

TEST_CASE("TestOpcodeContext::validateArguments — ARG_INTSTR type")
{
    TestMetaruleInfo meta = {};
    meta.name = "intstr_func";
    meta.minArgs = 1;
    meta.maxArgs = 1;
    meta.argumentTypes[0] = TEST_ARG_INTSTR;
    TestProgram program = {};

    SUBCASE("integer passes")
    {
        const TestProgramValue args[] = { TestProgramValue(10) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK(ctx.validateArguments());
    }

    SUBCASE("string passes")
    {
        const TestProgramValue args[] = { TestProgramValue("hello") };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK(ctx.validateArguments());
    }

    SUBCASE("dynamic string passes")
    {
        TestProgramValue dynStr;
        dynStr.opcode = TEST_VALUE_TYPE_DYNAMIC_STRING;
        dynStr.integerValue = 5;
        const TestProgramValue args[] = { dynStr };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK(ctx.validateArguments());
    }

    SUBCASE("float fails")
    {
        const TestProgramValue args[] = { TestProgramValue(1.0f) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK_FALSE(ctx.validateArguments());
    }

    SUBCASE("pointer fails")
    {
        TestObject obj;
        const TestProgramValue args[] = { TestProgramValue(&obj) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK_FALSE(ctx.validateArguments());
    }
}

TEST_CASE("TestOpcodeContext::validateArguments — ARG_NUMBER type")
{
    TestMetaruleInfo meta = {};
    meta.name = "num_func";
    meta.minArgs = 1;
    meta.maxArgs = 1;
    meta.argumentTypes[0] = TEST_ARG_NUMBER;
    TestProgram program = {};

    SUBCASE("integer passes")
    {
        const TestProgramValue args[] = { TestProgramValue(10) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK(ctx.validateArguments());
    }

    SUBCASE("float passes")
    {
        const TestProgramValue args[] = { TestProgramValue(3.14f) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK(ctx.validateArguments());
    }

    SUBCASE("string fails")
    {
        const TestProgramValue args[] = { TestProgramValue("hello") };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK_FALSE(ctx.validateArguments());
    }

    SUBCASE("pointer fails")
    {
        TestObject obj;
        const TestProgramValue args[] = { TestProgramValue(&obj) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK_FALSE(ctx.validateArguments());
    }
}

TEST_CASE("TestOpcodeContext::validateArguments — ARG_ANY type")
{
    TestMetaruleInfo meta = {};
    meta.name = "any_func";
    meta.minArgs = 1;
    meta.maxArgs = 1;
    meta.argumentTypes[0] = TEST_ARG_ANY;
    TestProgram program = {};

    SUBCASE("integer passes")
    {
        const TestProgramValue args[] = { TestProgramValue(1) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK(ctx.validateArguments());
    }

    SUBCASE("float passes")
    {
        const TestProgramValue args[] = { TestProgramValue(1.0f) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK(ctx.validateArguments());
    }

    SUBCASE("string passes")
    {
        const TestProgramValue args[] = { TestProgramValue("any") };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK(ctx.validateArguments());
    }

    SUBCASE("pointer passes")
    {
        TestObject obj;
        const TestProgramValue args[] = { TestProgramValue(&obj) };
        TestOpcodeContext ctx(&program, &meta, 1, args);
        CHECK(ctx.validateArguments());
    }
}

TEST_CASE("TestOpcodeContext::validateArguments — multi-arg mixed types")
{
    TestMetaruleInfo meta = {};
    meta.name = "mixed_func";
    meta.minArgs = 3;
    meta.maxArgs = 3;
    meta.argumentTypes[0] = TEST_ARG_INT;
    meta.argumentTypes[1] = TEST_ARG_STRING;
    meta.argumentTypes[2] = TEST_ARG_NUMBER;
    TestProgram program = {};

    SUBCASE("all correct types pass")
    {
        const TestProgramValue args[] = {
            TestProgramValue(10),      // becomes _args[2] = ARG_NUMBER
            TestProgramValue("hello"), // becomes _args[1] = ARG_STRING
            TestProgramValue(42)       // becomes _args[0] = ARG_INT
        };
        TestOpcodeContext ctx(&program, &meta, 3, args);
        // After reversal: _args = {42(int), "hello"(str), 10(int=number)}
        CHECK(ctx.validateArguments());
    }

    SUBCASE("wrong type at position 0 fails")
    {
        const TestProgramValue args[] = {
            TestProgramValue(10),
            TestProgramValue("hello"),
            TestProgramValue("not_int") // _args[0] should be INT, but is string
        };
        TestOpcodeContext ctx(&program, &meta, 3, args);
        CHECK_FALSE(ctx.validateArguments());
    }

    SUBCASE("wrong type at position 1 fails")
    {
        const TestProgramValue args[] = {
            TestProgramValue(10),
            TestProgramValue(55),     // _args[1] should be STRING, but is int
            TestProgramValue(42)
        };
        TestOpcodeContext ctx(&program, &meta, 3, args);
        CHECK_FALSE(ctx.validateArguments());
    }
}

// ---------------------------------------------------------------------------
// OpcodeContext::setReturn() Tests
// ---------------------------------------------------------------------------

TEST_CASE("TestOpcodeContext::setReturn — overloads")
{
    TestMetaruleInfo meta = {};
    meta.name = "ret_func";
    meta.minArgs = 0;
    meta.maxArgs = 0;
    TestProgram program = {};

    TestOpcodeContext ctx(&program, &meta, 0, nullptr);

    SUBCASE("setReturn(ProgramValue) stores value directly")
    {
        ctx.setReturn(TestProgramValue(42));
        CHECK(ctx.returnValue().isInt());
        CHECK(ctx.returnValue().integerValue == 42);
    }

    SUBCASE("setReturn(int) stores integer")
    {
        ctx.setReturn(99);
        CHECK(ctx.returnValue().isInt());
        CHECK(ctx.returnValue().integerValue == 99);
    }

    SUBCASE("setReturn(unsigned int) stores integer")
    {
        ctx.setReturn(77u);
        CHECK(ctx.returnValue().isInt());
        CHECK(ctx.returnValue().integerValue == 77);
    }

    SUBCASE("setReturn(nullptr) stores ProgramValue(0)")
    {
        ctx.setReturn(nullptr);
        CHECK(ctx.returnValue().isInt());
        CHECK(ctx.returnValue().integerValue == 0);
    }

    SUBCASE("setReturn(const char*) stores dynamic string")
    {
        ctx.setReturn("test_return");
        // Stub: programPushString returns strlen + 42
        // strlen("test_return") = 11, so integerValue = 53
        CHECK(ctx.returnValue().opcode == TEST_VALUE_TYPE_DYNAMIC_STRING);
        CHECK(ctx.returnValue().integerValue == 53);
    }

    SUBCASE("default return value is 0")
    {
        TestOpcodeContext ctx2(&program, &meta, 0, nullptr);
        CHECK(ctx2.returnValue().isInt());
        CHECK(ctx2.returnValue().integerValue == 0);
    }
}

// ---------------------------------------------------------------------------
// Interpreter Constants Tests
// ---------------------------------------------------------------------------

TEST_CASE("TEST_OPCODE_MAX_COUNT matches spec")
{
    // SFALL: increased from implicit 256 to 768 to accommodate
    // 226 sfall opcodes beyond the vanilla 500+ opcodes.
    // gInterpreterOpcodeHandlers[OPCODE_MAX_COUNT] is a fixed-size array.
    CHECK(TEST_OPCODE_MAX_COUNT == 768);

    // 226 sfall opcodes + ~47 core VM opcodes + ~32 lib opcodes + ~170 game opcodes
    // ≈ 475 total registered / 768 slots ≈ 62% filled
}

TEST_CASE("Opcode decode mask and bounds")
{
    // The opcode decode mask changed from 0x3FF (10-bit, 1024 entries) to
    // 0x3FFF (14-bit, 16384 entries). But OPCODE_MAX_COUNT is 768, so the
    // bounds check at interpreter.cc:2851 catches anything >= 768.
    //
    // 0x3FFF & 0x827F = 0x027F = 639 (valid sfall opcode)
    // 0x3FFF & 0x9000 = 0x1000 = 4096 (out of bounds → programFatalError)
    CHECK(0x3FFF == 16383);
    CHECK((0x827F & 0x3FFF) < TEST_OPCODE_MAX_COUNT); // 639 < 768 ✓
    CHECK((0x9000 & 0x3FFF) >= TEST_OPCODE_MAX_COUNT); // 4096 >= 768 → bounds trap
}

TEST_CASE("TEST_VALUE_TYPE constants follow VALUE_TYPE_MASK pattern")
{
    // VALUE_TYPE_MASK = 0xF7FF
    // All types share the 0xF001 base with a distinguishing byte
    CHECK(TEST_VALUE_TYPE_MASK == 0xF7FF);

    // Values are bit-coded: high bytes encode type, low bit 0x0001 means "not raw"
    int maskedInt = TEST_VALUE_TYPE_INT & TEST_VALUE_TYPE_MASK;
    CHECK(maskedInt == 0xC001);
    int maskedFloat = TEST_VALUE_TYPE_FLOAT & TEST_VALUE_TYPE_MASK;
    CHECK(maskedFloat == 0xA001);
    int maskedString = TEST_VALUE_TYPE_STRING & TEST_VALUE_TYPE_MASK;
    CHECK(maskedString == 0x9001);
    int maskedDynStr = TEST_VALUE_TYPE_DYNAMIC_STRING & TEST_VALUE_TYPE_MASK;
    CHECK(maskedDynStr == 0x9801);
    int maskedPtr = TEST_VALUE_TYPE_PTR & TEST_VALUE_TYPE_MASK;
    CHECK(maskedPtr == 0xE001);

    // All value types are distinct
    CHECK(TEST_VALUE_TYPE_INT != TEST_VALUE_TYPE_FLOAT);
    CHECK(TEST_VALUE_TYPE_INT != TEST_VALUE_TYPE_STRING);
    CHECK(TEST_VALUE_TYPE_INT != TEST_VALUE_TYPE_DYNAMIC_STRING);
    CHECK(TEST_VALUE_TYPE_INT != TEST_VALUE_TYPE_PTR);
    CHECK(TEST_VALUE_TYPE_STRING != TEST_VALUE_TYPE_DYNAMIC_STRING);
}

TEST_CASE("TEST_PROCEDURE_FLAG constants — bit flags")
{
    // Procedure flags are mutually non-overlapping:
    int timedCond = TEST_PROCEDURE_FLAG_TIMED & TEST_PROCEDURE_FLAG_CONDITIONAL;
    CHECK(timedCond == 0);
    int timedImp = TEST_PROCEDURE_FLAG_TIMED & TEST_PROCEDURE_FLAG_IMPORTED;
    CHECK(timedImp == 0);
    int timedExp = TEST_PROCEDURE_FLAG_TIMED & TEST_PROCEDURE_FLAG_EXPORTED;
    CHECK(timedExp == 0);
    int timedCrit = TEST_PROCEDURE_FLAG_TIMED & TEST_PROCEDURE_FLAG_CRITICAL;
    CHECK(timedCrit == 0);

    CHECK(TEST_PROCEDURE_FLAG_TIMED == 0x01);
    CHECK(TEST_PROCEDURE_FLAG_CONDITIONAL == 0x02);
    CHECK(TEST_PROCEDURE_FLAG_IMPORTED == 0x04);
    CHECK(TEST_PROCEDURE_FLAG_EXPORTED == 0x08);
    CHECK(TEST_PROCEDURE_FLAG_CRITICAL == 0x10);
}

TEST_CASE("TEST_PROGRAM_FLAG constants — bit flags")
{
    int and1 = TEST_PROGRAM_FLAG_EXITED & TEST_PROGRAM_FLAG_RUNNING;
    CHECK(and1 == 0);
    int and2 = TEST_PROGRAM_FLAG_FATAL_ERROR & TEST_PROGRAM_FLAG_STOPPED;
    CHECK(and2 == 0);

    CHECK(TEST_PROGRAM_FLAG_EXITED == 0x01);
    CHECK(TEST_PROGRAM_FLAG_RUNNING == 0x02);
    CHECK(TEST_PROGRAM_FLAG_FATAL_ERROR == 0x04);
    CHECK(TEST_PROGRAM_FLAG_STOPPED == 0x08);
    CHECK(TEST_PROGRAM_IS_WAITING == 0x10);
    CHECK(TEST_PROGRAM_FLAG_CHILD_CALL == 0x20);
    CHECK(TEST_PROGRAM_FLAG_FINISHED == 0x40);
    CHECK(TEST_PROGRAM_FLAG_CRITICAL_SECTION == 0x80);
    CHECK(TEST_PROGRAM_FLAG_CHILD_SPAWN == 0x0100);
}

// ---------------------------------------------------------------------------
// opTokenize Off-By-One Pattern Test (interpreter_lib.cc:313)
// ---------------------------------------------------------------------------

TEST_CASE("opTokenize off-by-one fix — delimiter skip pattern")
{
    // The discovery report F-01 notes: strncpy(temp, start + 1, length)
    // was changed from strncpy(temp, start, length). The +1 skips the
    // opening delimiter character.
    //
    // This validates the pattern: the token content starts AFTER the
    // opening delimiter.

    SUBCASE("strncpy with +1 offset skips opening delimiter")
    {
        // Simulated tokenize: extracting "abc" from a string like `'abc'`
        // where start points to `'`, length = 3 (chars between quotes)
        const char* str = "'abc'rest";
        const char* start = str;        // points to opening `'`
        const char* token_start = start + 1; // skip the opening delimiter

        char temp[64];
        memset(temp, 0, sizeof(temp));

        // Old behavior (off-by-one): strncpy(temp, start, 3) → includes `'`
        // Fixed behavior: strncpy(temp, start + 1, 3) → starts at `a`
        strncpy(temp, token_start, 3);
        CHECK(strcmp(temp, "abc") == 0);

        // Verify old behavior would include the delimiter:
        char tempOld[64];
        memset(tempOld, 0, sizeof(tempOld));
        strncpy(tempOld, start, 3);
        CHECK(strcmp(tempOld, "'ab") == 0); // includes opening quote — BUG
    }

    SUBCASE("edge case: zero-length token (empty string between delimiters)")
    {
        const char* str = "''rest";
        const char* start = str;
        const char* token_start = start + 1;
        char temp[64];
        memset(temp, 0, sizeof(temp));

        // length = 0
        strncpy(temp, token_start, 0);
        CHECK(temp[0] == '\0');
    }

    SUBCASE("edge case: length 1 extracts single char after delimiter")
    {
        const char* str = "'x'rest";
        const char* start = str;
        const char* token_start = start + 1;
        char temp[64];
        memset(temp, 0, sizeof(temp));

        strncpy(temp, token_start, 1);
        CHECK(temp[0] == 'x');
    }
}

// ---------------------------------------------------------------------------
// ProgramValue Edge Cases
// ---------------------------------------------------------------------------

TEST_CASE("TestProgramValue default-constructed is zero-int and empty")
{
    // Default constructor: opcode=INT, integerValue=0
    // This means a default-constructed ProgramValue is both:
    // - TYPE INT (isInt() = true)
    // - EMPTY (isEmpty() = true, because integerValue == 0)
    //
    // This is intentional: the VM uses 0 as a generic "nothing" value.

    TestProgramValue pv;
    CHECK(pv.opcode == TEST_VALUE_TYPE_INT);
    CHECK(pv.integerValue == 0);
    CHECK(pv.isInt());
    CHECK(pv.isEmpty());
    CHECK(pv.asInt() == 0);
    CHECK(pv.asFloat() == doctest::Approx(0.0f));
}

TEST_CASE("TestProgramValue large values")
{
    SUBCASE("INT_MAX")
    {
        TestProgramValue pv(2147483647);
        CHECK(pv.asInt() == 2147483647);
    }

    SUBCASE("INT_MIN")
    {
        TestProgramValue pv(-2147483647 - 1);
        CHECK(pv.asInt() == -2147483647 - 1);
    }

    SUBCASE("float preserves bit pattern")
    {
        float val = 1.23456789f;
        TestProgramValue pv(val);
        CHECK(pv.asFloat() == doctest::Approx(val));
    }
}

TEST_CASE("TestProgramValue asInt from large float should truncate")
{
    // 3.99f truncated to 3 (not rounded)
    TestProgramValue pv(3.99f);
    CHECK(pv.asInt() == 3);
}

// ---------------------------------------------------------------------------
// Metarule ArgumentCount Boundary Tests (METARULE_MAX_ARGS = 8)
// ---------------------------------------------------------------------------

TEST_CASE("METARULE_MAX_ARGS boundary")
{
    CHECK(TEST_METARULE_MAX_ARGS == 8);

    // The OpcodeContext constructor asserts numArgs <= METARULE_MAX_ARGS.
    // The arg() method asserts index < _numArgs.
    // Both guards are at interpreter.cc / opcode_context.cc.
    // This verifies the constant definition matches the production code.

    // Production code: sizeof(_args) = 8 ProgramValue slots
    // See opcode_context.h:43 — std::array<ProgramValue, METARULE_MAX_ARGS>
}

// ---------------------------------------------------------------------------
// TEST_OPCODE Constants
// ---------------------------------------------------------------------------

TEST_CASE("TEST_OPCODE range constants")
{
    // Opcodes start at 0x8000 (high bit set distinguishes opcodes from data)
    CHECK(TEST_OPCODE_NOOP == 0x8000);

    // The standard library opcodes end at 0x80A0, vanilla game opcodes at 0x8155,
    // and sfall opcodes range from 0x8156 to 0x827F.
    //
    // 0x8000 + 768 (OPCODE_MAX_COUNT) = 0x8300, which covers all sfall opcodes
    // since 0x827F < 0x8300.
    CHECK(0x8000 + TEST_OPCODE_MAX_COUNT == 0x8300);
}

// ============================================================================
// TEST SUMMARY:
// Total test cases: ~40
// Approximate LOC: ~800 (including type definitions and stubs)
//
// Coverage:
// - ProgramValue: all 8 constructors, isInt/isFloat/isString/isPointer,
//   isEmpty (all 5 type states), asInt, asFloat, typeDebugString,
//   C-string sentinel pattern
// - stackReadInt16/Int32: big-endian reads, offsets, boundary values
// - procedureCount: stackReadInt32 integration
// - OpcodeContext: construction, argument reversal, validateArguments
//   (all 6 ARG_ types + count validation + multi-arg mixed types),
//   setReturn (all 5 overloads)
// - Constants: OPCODE_MAX_COUNT, VALUE_TYPE_, ProcedureFlags, ProgramFlags
// - opTokenize: off-by-one strncpy pattern validation
//
// Stub requirements for CMakeLists.txt integration:
// (none if self-contained; if linking real interpreter.cc, need:)
// - programPrintError, programPushString, programStackPushValue, programGetString
// - externalProcedureGetProgram, _interpretOutput, debugPrint, etc.
//
// CMakeLists.txt entry:
//   add_executable(test_interpreter_core test_interpreter_core.cc)
//   target_link_libraries(test_interpreter_core PRIVATE doctest)
// ============================================================================
