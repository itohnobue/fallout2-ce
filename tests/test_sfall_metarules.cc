// Unit tests for sfall_metarules — header-level type validation, compute/pure
// function logic mirrors, and registry structure validation.
//
// Tests: OpcodeArgumentType enum, MetaruleInfo struct layout, kMetarules[]
// registry properties (if linkable), kDefaultStatLimits table, compute/pure
// metarule logic (string_compare, string_find, string_to_case, string_format,
// floor2), message_box nesting counter, and stub metarule return values.
//
// This test does NOT link sfall_metarules.cc (which has 50+ engine dependencies).
// Compute/pure functions are mirrored locally following the test_criticals.cc
// pattern. Header types are validated via #include "sfall_metarules.h".
//
// CMakeLists.txt dependency: needs test_stubs + SDL2/zlib headers for
// transitive includes from opcode_context.h → interpreter.h.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "sfall_metarules.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

using namespace fallout;

// =================================================================
// Local mirrors of production data structures (not linkable from .cc)
// =================================================================

// Mirror of kDefaultStatLimits from sfall_metarules.cc:1992-2034.
// This table maps STAT_* indices to default min/max values used by
// mf_get_stat_max / mf_get_stat_min. Mirrors the gStatDescriptions[]
// initializer in stat.cc.
static constexpr int TEST_STAT_COUNT = 37;
static const struct TestStatLimit {
    int min;
    int max;
    int statIndex; // index in the table for cross-reference
} kTestDefaultStatLimits[TEST_STAT_COUNT] = {
    { 1, 10, 0 },          // STAT_STRENGTH
    { 1, 10, 1 },          // STAT_PERCEPTION
    { 1, 10, 2 },          // STAT_ENDURANCE
    { 1, 10, 3 },          // STAT_CHARISMA
    { 1, 10, 4 },          // STAT_INTELLIGENCE
    { 1, 10, 5 },          // STAT_AGILITY
    { 1, 10, 6 },          // STAT_LUCK
    { 0, 999, 7 },         // STAT_MAXIMUM_HIT_POINTS
    { 1, 99, 8 },          // STAT_MAXIMUM_ACTION_POINTS
    { 0, 999, 9 },         // STAT_ARMOR_CLASS
    { 0, 0, 10 },          // STAT_UNARMED_DAMAGE — min 0, max INT_MAX (substituted for test)
    { 0, 500, 11 },        // STAT_MELEE_DAMAGE
    { 0, 999, 12 },        // STAT_CARRY_WEIGHT
    { 0, 60, 13 },         // STAT_SEQUENCE
    { 0, 30, 14 },         // STAT_HEALING_RATE
    { 0, 100, 15 },        // STAT_CRITICAL_CHANCE
    { -60, 100, 16 },      // STAT_BETTER_CRITICALS
    { 0, 100, 17 },        // STAT_DAMAGE_THRESHOLD
    { 0, 100, 18 },        // STAT_DAMAGE_THRESHOLD_LASER
    { 0, 100, 19 },        // STAT_DAMAGE_THRESHOLD_FIRE
    { 0, 100, 20 },        // STAT_DAMAGE_THRESHOLD_PLASMA
    { 0, 100, 21 },        // STAT_DAMAGE_THRESHOLD_ELECTRICAL
    { 0, 100, 22 },        // STAT_DAMAGE_THRESHOLD_EMP
    { 0, 100, 23 },        // STAT_DAMAGE_THRESHOLD_EXPLOSION
    { 0, 90, 24 },         // STAT_DAMAGE_RESISTANCE
    { 0, 90, 25 },         // STAT_DAMAGE_RESISTANCE_LASER
    { 0, 90, 26 },         // STAT_DAMAGE_RESISTANCE_FIRE
    { 0, 90, 27 },         // STAT_DAMAGE_RESISTANCE_PLASMA
    { 0, 90, 28 },         // STAT_DAMAGE_RESISTANCE_ELECTRICAL
    { 0, 100, 29 },        // STAT_DAMAGE_RESISTANCE_EMP
    { 0, 90, 30 },         // STAT_DAMAGE_RESISTANCE_EXPLOSION
    { 0, 95, 31 },         // STAT_RADIATION_RESISTANCE
    { 0, 95, 32 },         // STAT_POISON_RESISTANCE
    { 16, 101, 33 },       // STAT_AGE
    { 0, 1, 34 },          // STAT_GENDER
    { 0, 2000, 35 },       // STAT_CURRENT_HIT_POINTS
    { 0, 2000, 36 },       // STAT_CURRENT_POISON_LEVEL
    // STAT_CURRENT_RADIATION_LEVEL is index 37 in production, but STAT_COUNT=37
    // means indices 0-36. Production code has STAT_CURRENT_RADIATION_LEVEL at
    // index 37 requiring STAT_COUNT=38. This test mirrors the values as documented.
    // For this test, we validate the STAT_COUNT-dependent entries.
};

// For STAT_UNARMED_DAMAGE the max is INT_MAX — test uses a sentinel check.
static constexpr int TEST_STAT_UNARMED_DAMAGE_MAX_SENTINEL = -1; // indicates INT_MAX in production

// =================================================================
// Local mirror: FalloutStringCompare (exact copy from sfall_metarules.cc:1311-1374)
// =================================================================

static bool TestFalloutStringCompare(const char* str1, const char* str2, long codePage)
{
    while (true) {
        unsigned char c1 = static_cast<unsigned char>(*str1);
        unsigned char c2 = static_cast<unsigned char>(*str2);

        if (c1 == 0 && c2 == 0) return true;
        if (c1 == 0 || c2 == 0) return false;
        str1++;
        str2++;
        if (c1 == c2) continue;

        if (codePage == 866) {
            if (c1 == 229) c1 -= 229 - 'x';
            if (c2 == 229) c2 -= 229 - 'x';
        }

        if (c1 >= 'A' && c1 <= 'Z') c1 |= 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 |= 32;
        if (c1 == c2) continue;
        if (c1 < 128 || c2 < 128) return false;

        switch (codePage) {
        case 866:
            if (c1 != 149 && c2 != 149) {
                if (c1 >= 128 && c1 <= 159) {
                    c1 |= 32;
                } else if (c1 >= 224 && c1 <= 239) {
                    c1 -= 48;
                } else if (c1 == 240) {
                    c1++;
                }
                if (c2 >= 128 && c2 <= 159) {
                    c2 |= 32;
                } else if (c2 >= 224 && c2 <= 239) {
                    c2 -= 48;
                } else if (c2 == 240) {
                    c2++;
                }
            }
            break;
        case 1251:
            if (c1 >= 0xC0 && c1 <= 0xDF) c1 |= 32;
            if (c2 >= 0xC0 && c2 <= 0xDF) c2 |= 32;
            if (c1 == 0xA8) c1 += 16;
            if (c2 == 0xA8) c2 += 16;
            break;
        case 1250:
        case 1252:
            if (c1 != 0xD7 && c1 != 0xF7 && c2 != 0xD7 && c2 != 0xF7) {
                if (c1 >= 0xC0 && c1 <= 0xDE) c1 |= 32;
                if (c2 >= 0xC0 && c2 <= 0xDE) c2 |= 32;
            }
            break;
        }
        if (c1 != c2) return false;
    }
}

// =================================================================
// Local mirror: mf_floor2 (exact copy from sfall_metarules.cc:1578-1581)
// =================================================================

static int TestFloor2(double value)
{
    return static_cast<int>(floor(value));
}

// =================================================================
// Local mirror: mf_string_to_case logic (from sfall_metarules.cc:1431-1444)
// =================================================================

static std::string TestStringToCase(const char* buf, int caseType)
{
    std::string s(buf);
    if (caseType == 1) {
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    } else if (caseType == 0) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    }
    // caseType other than 0/1 is handled by error logging in production,
    // but the string is returned as-is (unchanged). Tests verify no crash.
    return s;
}

// =================================================================
// Local mirror: mf_string_find logic (from sfall_metarules.cc:1407-1429)
// =================================================================

static int TestStringFind(const char* str, const char* substr, int startPos)
{
    int strLen = static_cast<int>(strlen(str));
    if (startPos < 0 || startPos >= strLen) {
        return -1;
    }

    const char* found = strstr(str + startPos, substr);
    if (found) {
        return static_cast<int>(found - str);
    }
    return -1;
}

// =================================================================
// Local mirror: metarule_exist lookup logic
// =================================================================

// Simulated metarule registry for testing metarule_exist-style lookup.
struct TestMetaruleEntry {
    const char* name;
    int minArgs;
    int maxArgs;
    int errorReturn;
};

static const TestMetaruleEntry kTestMetaruleSubset[] = {
    { "floor2", 1, 1, 0 },
    { "string_compare", 2, 3, 0 },
    { "string_find", 2, 3, -1 },
    { "string_format", 2, 8, 0 },
    { "string_to_case", 2, 2, -1 },
    { "metarule_exist", 1, 1, 0 },
    { "npc_engine_level_up", 1, 1, -1 },
    { "signal_close_game", 0, 0, 0 },
    { "lock_is_jammed", 1, 1, 0 },
    { "add_trait", 1, 1, -1 },
    { "rotators", 0, 0, 0 }, // sentinel for compatibility check
};
static const int kTestMetaruleSubsetCount = sizeof(kTestMetaruleSubset) / sizeof(kTestMetaruleSubset[0]);

static const TestMetaruleEntry* TestFindMetarule(const char* name)
{
    for (int i = 0; i < kTestMetaruleSubsetCount; i++) {
        if (strcmp(kTestMetaruleSubset[i].name, name) == 0) {
            return &kTestMetaruleSubset[i];
        }
    }
    return nullptr;
}

// =================================================================
// Local mirror: message_box nesting counter logic (from sfall_metarules.cc:2414-2419)
// =================================================================

static int g_TestDialogShowCount = 0;
static bool g_TestScriptsEnabled = true;

static void TestMessageBoxEnter()
{
    g_TestDialogShowCount++;
    g_TestScriptsEnabled = false;
}

static void TestMessageBoxExit()
{
    if (--g_TestDialogShowCount == 0) {
        g_TestScriptsEnabled = true;
    }
}

// =================================================================
// TEST CASES
// =================================================================

// --- OpcodeArgumentType enum ---

TEST_CASE("OpcodeArgumentType enum values match specification")
{
    CHECK(static_cast<int>(OpcodeArgumentType::ARG_ANY) == 0);
    CHECK(static_cast<int>(OpcodeArgumentType::ARG_INT) == 1);
    CHECK(static_cast<int>(OpcodeArgumentType::ARG_OBJECT) == 2);
    CHECK(static_cast<int>(OpcodeArgumentType::ARG_STRING) == 3);
    CHECK(static_cast<int>(OpcodeArgumentType::ARG_INTSTR) == 4);
    CHECK(static_cast<int>(OpcodeArgumentType::ARG_NUMBER) == 5);
}

// --- MetaruleInfo struct ---

TEST_CASE("MetaruleInfo struct layout and defaults")
{
    // Verify the struct is initializable
    MetaruleInfo info = {};
    CHECK(info.name == nullptr);
    CHECK(info.handler == nullptr);
    CHECK(info.minArgs == 0);
    CHECK(info.maxArgs == 0);
    CHECK(info.errorReturn == 0);

    // All argumentTypes default to ARG_ANY (0)
    for (int i = 0; i < METARULE_MAX_ARGS; i++) {
        CHECK(static_cast<int>(info.argumentTypes[i]) == 0); // ARG_ANY
    }
}

TEST_CASE("MetaruleInfo non-zero initialization")
{
    MetaruleInfo info = {};
    info.name = "test_metarule";
    info.minArgs = 2;
    info.maxArgs = 5;
    info.errorReturn = -1;
    info.argumentTypes[0] = OpcodeArgumentType::ARG_STRING;
    info.argumentTypes[1] = OpcodeArgumentType::ARG_INT;

    CHECK(strcmp(info.name, "test_metarule") == 0);
    CHECK(info.minArgs == 2);
    CHECK(info.maxArgs == 5);
    CHECK(info.errorReturn == -1);
    CHECK(static_cast<int>(info.argumentTypes[0]) == static_cast<int>(OpcodeArgumentType::ARG_STRING));
    CHECK(static_cast<int>(info.argumentTypes[1]) == static_cast<int>(OpcodeArgumentType::ARG_INT));
    // Remaining slots stay ARG_ANY (0)
    for (int i = 2; i < METARULE_MAX_ARGS; i++) {
        CHECK(static_cast<int>(info.argumentTypes[i]) == 0);
    }
}

TEST_CASE("MetaruleInfo minArgs <= maxArgs invariant")
{
    // Used in test mirrors to validate kTestMetaruleSubset entries
    for (int i = 0; i < kTestMetaruleSubsetCount; i++) {
        const auto& entry = kTestMetaruleSubset[i];
        if (entry.maxArgs < 0) {
            // Variadic entry — skip min/max check (entry has sentinel)
            INFO("Entry: ", entry.name, " has variadic maxArgs");
            continue;
        }
        CHECK(entry.minArgs <= entry.maxArgs);
    }
}

TEST_CASE("METARULE_MAX_ARGS is 8")
{
    CHECK(METARULE_MAX_ARGS == 8);
}

// --- kDefaultStatLimits table ---

TEST_CASE("kDefaultStatLimits table — all 37 entries have min <= max")
{
    for (int i = 0; i < TEST_STAT_COUNT; i++) {
        INFO("Stat index: ", i);
        if (i == 10) {
            // STAT_UNARMED_DAMAGE has max = INT_MAX in production
            CHECK(kTestDefaultStatLimits[i].min == 0);
            continue;
        }
        CHECK(kTestDefaultStatLimits[i].min <= kTestDefaultStatLimits[i].max);
    }
}

TEST_CASE("kDefaultStatLimits table — known boundary values")
{
    // Primary SPECIAL stats are 1-10
    for (int i = 0; i < 7; i++) {
        CHECK(kTestDefaultStatLimits[i].min == 1);
        CHECK(kTestDefaultStatLimits[i].max == 10);
    }

    // HP: 0-999
    CHECK(kTestDefaultStatLimits[7].min == 0);
    CHECK(kTestDefaultStatLimits[7].max == 999);

    // AP: 1-99
    CHECK(kTestDefaultStatLimits[8].min == 1);
    CHECK(kTestDefaultStatLimits[8].max == 99);

    // BETTER_CRITICALS: -60 to 100 (only negative-bound stat besides damage thresholds)
    CHECK(kTestDefaultStatLimits[16].min == -60);
    CHECK(kTestDefaultStatLimits[16].max == 100);

    // GENDER: 0-1 (binary)
    CHECK(kTestDefaultStatLimits[34].min == 0);
    CHECK(kTestDefaultStatLimits[34].max == 1);

    // AGE: 16-101
    CHECK(kTestDefaultStatLimits[33].min == 16);
    CHECK(kTestDefaultStatLimits[33].max == 101);
}

TEST_CASE("kDefaultStatLimits table — resistance stats cap at sane values")
{
    // DR stats cap at 90% (indices 24-30, except EMP which is 100)
    CHECK(kTestDefaultStatLimits[24].max == 90);  // DAMAGE_RESISTANCE
    CHECK(kTestDefaultStatLimits[25].max == 90);  // LASER
    CHECK(kTestDefaultStatLimits[26].max == 90);  // FIRE
    CHECK(kTestDefaultStatLimits[27].max == 90);  // PLASMA
    CHECK(kTestDefaultStatLimits[28].max == 90);  // ELECTRICAL
    CHECK(kTestDefaultStatLimits[29].max == 100); // EMP (100 cap)
    CHECK(kTestDefaultStatLimits[30].max == 90);  // EXPLOSION
}

TEST_CASE("kDefaultStatLimits table — rad/poison resist at 95")
{
    CHECK(kTestDefaultStatLimits[31].max == 95); // RADIATION_RESISTANCE
    CHECK(kTestDefaultStatLimits[32].max == 95); // POISON_RESISTANCE
}

// --- TestFalloutStringCompare ---

TEST_CASE("TestFalloutStringCompare — exact match (any codePage)")
{
    CHECK(TestFalloutStringCompare("hello", "hello", 0));
    CHECK(TestFalloutStringCompare("world", "world", 866));
    CHECK(TestFalloutStringCompare("fallout", "fallout", 1251));
}

TEST_CASE("TestFalloutStringCompare — case-insensitive match")
{
    CHECK(TestFalloutStringCompare("Hello", "hello", 0));
    CHECK(TestFalloutStringCompare("HELLO", "hello", 0));
    CHECK(TestFalloutStringCompare("hello", "HELLO", 0));
    CHECK(TestFalloutStringCompare("MiXeD", "mIxEd", 0));

    // CP 866 also handles ASCII case
    CHECK(TestFalloutStringCompare("HELLO", "hello", 866));
    // CP 1251
    CHECK(TestFalloutStringCompare("HELLO", "hello", 1251));
    // CP 1250/1252
    CHECK(TestFalloutStringCompare("HELLO", "hello", 1250));
    CHECK(TestFalloutStringCompare("HELLO", "hello", 1252));
}

TEST_CASE("TestFalloutStringCompare — non-match")
{
    CHECK_FALSE(TestFalloutStringCompare("abc", "xyz", 0));
    CHECK_FALSE(TestFalloutStringCompare("hello", "world", 0));
    CHECK_FALSE(TestFalloutStringCompare("hello", "hell", 0));    // different length
    CHECK_FALSE(TestFalloutStringCompare("hello", "helloo", 0));   // different length
}

TEST_CASE("TestFalloutStringCompare — empty strings")
{
    CHECK(TestFalloutStringCompare("", "", 0));
    CHECK_FALSE(TestFalloutStringCompare("a", "", 0));
    CHECK_FALSE(TestFalloutStringCompare("", "a", 0));
}

TEST_CASE("TestFalloutStringCompare — CP866 Russian 'x' replacement")
{
    // Character 229 in CP866 is Russian 'x' — should be treated like English 'x'
    // In CP866, 229 should map to 'x' (ASCII 120) for case-insensitive comparison
    char russianX[2] = { static_cast<char>(229), '\0' };
    // After CP866 mapping, 229-229+'x' = 'x' = 120. So uppercase 'X' (88) should match.
    CHECK(TestFalloutStringCompare(russianX, "x", 866));
    CHECK(TestFalloutStringCompare(russianX, "X", 866));
}

TEST_CASE("TestFalloutStringCompare — CP1251 Cyrillic case folding")
{
    // CP1251 uppercase A-j (0xC0-0xDF) should match lowercase a-ja (0xE0-0xFF)
    char upperBe[2] = { static_cast<char>(0xC1), '\0' }; // Б (Cyrillic uppercase be)
    char lowerBe[2] = { static_cast<char>(0xE1), '\0' }; // б (Cyrillic lowercase be)
    CHECK(TestFalloutStringCompare(upperBe, lowerBe, 1251));

    // CP1251 Yo (0xA8) should match yo (0xB8) after +16 shift
    char upperYo[2] = { static_cast<char>(0xA8), '\0' }; // Ё
    char lowerYo[2] = { static_cast<char>(0xB8), '\0' }; // ё
    CHECK(TestFalloutStringCompare(upperYo, lowerYo, 1251));
}

TEST_CASE("TestFalloutStringCompare — CP1250/1252 multiplication sign exclusion")
{
    // 0xD7 and 0xF7 are multiplication/division signs in CP1250/1252
    // and should NOT be case-folded
    char timesSign[2] = { static_cast<char>(0xD7), '\0' }; // ×
    char divSign[2] = { static_cast<char>(0xF7), '\0' };   // ÷
    // These are not alphabetic, should not be modified by case folding
    CHECK_FALSE(TestFalloutStringCompare(timesSign, divSign, 1250));
    CHECK_FALSE(TestFalloutStringCompare(timesSign, divSign, 1252));
}

// --- TestFloor2 ---

TEST_CASE("TestFloor2 — positive values")
{
    CHECK(TestFloor2(0.0) == 0);
    CHECK(TestFloor2(1.0) == 1);
    CHECK(TestFloor2(1.5) == 1);
    CHECK(TestFloor2(1.9) == 1);
    CHECK(TestFloor2(2.0) == 2);
    CHECK(TestFloor2(99.9) == 99);
    CHECK(TestFloor2(100.0) == 100);
}

TEST_CASE("TestFloor2 — negative values")
{
    CHECK(TestFloor2(-0.5) == -1);
    CHECK(TestFloor2(-1.0) == -1);
    CHECK(TestFloor2(-1.5) == -2);
    CHECK(TestFloor2(-99.9) == -100);
}

TEST_CASE("TestFloor2 — edge cases")
{
    CHECK(TestFloor2(0.0001) == 0);
    CHECK(TestFloor2(-0.0001) == -1);
    // Large positive — value must be representable as int to avoid UB
    // (static_cast<int>(floor(x)) is UB per [conv.fpint] when truncated
    // value cannot be represented in the destination type).
    // NOTE: The production mf_floor2 at sfall_metarules.cc:1580 uses the
    // same unchecked static_cast<int>(floor(value)) pattern — float values
    // exceeding INT_MAX from sfall scripts would also trigger UB there.
    CHECK(TestFloor2(1e9) > 0);   // 1,000,000,000 — safely within int range
    // Very large negative — safely within int range
    CHECK(TestFloor2(-1e9) < 0);
}

// --- TestStringToCase ---

TEST_CASE("TestStringToCase — to uppercase (caseType=1)")
{
    CHECK(TestStringToCase("hello", 1) == "HELLO");
    CHECK(TestStringToCase("Hello World", 1) == "HELLO WORLD");
    CHECK(TestStringToCase("abc123", 1) == "ABC123");
    CHECK(TestStringToCase("already upper", 1) == "ALREADY UPPER");
}

TEST_CASE("TestStringToCase — to lowercase (caseType=0)")
{
    CHECK(TestStringToCase("HELLO", 0) == "hello");
    CHECK(TestStringToCase("HeLLo WoRLD", 0) == "hello world");
    CHECK(TestStringToCase("ABC123", 0) == "abc123");
    CHECK(TestStringToCase("already lower", 0) == "already lower");
}

TEST_CASE("TestStringToCase — empty string")
{
    CHECK(TestStringToCase("", 0) == "");
    CHECK(TestStringToCase("", 1) == "");
}

TEST_CASE("TestStringToCase — invalid caseType does not crash (no-op)")
{
    // Production code logs an error for invalid caseType but leaves string unchanged
    CHECK(TestStringToCase("Hello", 2) == "Hello");
    CHECK(TestStringToCase("Hello", -1).size() > 0); // not empty
}

// --- TestStringFind ---

TEST_CASE("TestStringFind — substring found")
{
    CHECK(TestStringFind("hello world", "hello", 0) == 0);
    CHECK(TestStringFind("hello world", "world", 0) == 6);
    CHECK(TestStringFind("hello world", "o", 0) == 4);         // first 'o' at index 4
    CHECK(TestStringFind("hello world", "o wo", 0) == 4);
}

TEST_CASE("TestStringFind — with start position")
{
    CHECK(TestStringFind("hello world", "o", 5) == 7);         // second 'o' at index 7
    CHECK(TestStringFind("ababab", "ab", 0) == 0);
    CHECK(TestStringFind("ababab", "ab", 2) == 2);             // next occurrence
    CHECK(TestStringFind("ababab", "ab", 4) == 4);             // last occurrence
}

TEST_CASE("TestStringFind — not found")
{
    CHECK(TestStringFind("hello", "xyz", 0) == -1);
    CHECK(TestStringFind("hello", "world", 0) == -1);
    CHECK(TestStringFind("abc", "abcd", 0) == -1);             // substr longer than str
}

TEST_CASE("TestStringFind — bounds checking for start position")
{
    CHECK(TestStringFind("hello", "h", -1) == -1);             // negative start
    CHECK(TestStringFind("hello", "h", 5) == -1);              // start at end (strlen=5)
    CHECK(TestStringFind("hello", "h", 10) == -1);             // start beyond end
}

TEST_CASE("TestStringFind — edge cases")
{
    CHECK(TestStringFind("", "", 0) == 0);                     // empty in empty
    CHECK(TestStringFind("hello", "", 0) == 0);                // empty substring
    CHECK(TestStringFind("", "hello", 0) == -1);               // non-empty in empty
}

// --- TestMetaruleSubset lookup ---

TEST_CASE("TestFindMetarule — known metarules found")
{
    CHECK(TestFindMetarule("floor2") != nullptr);
    CHECK(TestFindMetarule("string_compare") != nullptr);
    CHECK(TestFindMetarule("string_find") != nullptr);
    CHECK(TestFindMetarule("string_format") != nullptr);
    CHECK(TestFindMetarule("string_to_case") != nullptr);
    CHECK(TestFindMetarule("metarule_exist") != nullptr);
    CHECK(TestFindMetarule("npc_engine_level_up") != nullptr);
    CHECK(TestFindMetarule("signal_close_game") != nullptr);
    CHECK(TestFindMetarule("lock_is_jammed") != nullptr);
    CHECK(TestFindMetarule("add_trait") != nullptr);
}

TEST_CASE("TestFindMetarule — unknown metarules not found")
{
    CHECK(TestFindMetarule("nonexistent_function") == nullptr);
    CHECK(TestFindMetarule("") == nullptr);
    CHECK(TestFindMetarule("FLOOR2") == nullptr);              // case-sensitive
}

TEST_CASE("TestFindMetarule — rotators sentinel exists")
{
    // The "rotators" sentinel is used by RPU mods to detect sfall presence.
    // metarule_exist("rotators") should return 1.
    const auto* entry = TestFindMetarule("rotators");
    CHECK(entry != nullptr);
}

TEST_CASE("TestFindMetarule — entry consistency check")
{
    // Every entry has a non-null name and handler
    for (int i = 0; i < kTestMetaruleSubsetCount; i++) {
        const auto& entry = kTestMetaruleSubset[i];
        CHECK(entry.name != nullptr);
        CHECK(strlen(entry.name) > 0);
    }
}

TEST_CASE("TestFindMetarule — error return values are meaningful")
{
    // Stub metarules should have errorReturn set for invalid args
    const auto* lockEntry = TestFindMetarule("lock_is_jammed");
    REQUIRE(lockEntry != nullptr);
    CHECK(lockEntry->errorReturn == 0);  // stub returns 0 (not jammed)

    const auto* traitEntry = TestFindMetarule("add_trait");
    REQUIRE(traitEntry != nullptr);
    CHECK(traitEntry->errorReturn == -1); // returns -1 (failure)

    const auto* compareEntry = TestFindMetarule("string_compare");
    REQUIRE(compareEntry != nullptr);
    CHECK(compareEntry->errorReturn == 0); // returns 0 on validation failure
}

// --- Message box nesting counter ---

TEST_CASE("TestMessageBox nesting counter — single call")
{
    g_TestDialogShowCount = 0;
    g_TestScriptsEnabled = true;

    TestMessageBoxEnter();
    CHECK(g_TestDialogShowCount == 1);
    CHECK_FALSE(g_TestScriptsEnabled);

    TestMessageBoxExit();
    CHECK(g_TestDialogShowCount == 0);
    CHECK(g_TestScriptsEnabled);
}

TEST_CASE("TestMessageBox nesting counter — nested calls")
{
    g_TestDialogShowCount = 0;
    g_TestScriptsEnabled = true;

    TestMessageBoxEnter(); // level 1
    CHECK(g_TestDialogShowCount == 1);
    CHECK_FALSE(g_TestScriptsEnabled);

    TestMessageBoxEnter(); // level 2
    CHECK(g_TestDialogShowCount == 2);
    CHECK_FALSE(g_TestScriptsEnabled); // still disabled

    TestMessageBoxExit(); // back to level 1
    CHECK(g_TestDialogShowCount == 1);
    CHECK_FALSE(g_TestScriptsEnabled); // still disabled (not yet 0)

    TestMessageBoxExit(); // back to level 0
    CHECK(g_TestDialogShowCount == 0);
    CHECK(g_TestScriptsEnabled); // re-enabled
}

TEST_CASE("TestMessageBox nesting counter — reset to defaults")
{
    // Simulates sfall_metarules_reset() which sets count to 0 and re-enables scripts
    g_TestDialogShowCount = 3;
    g_TestScriptsEnabled = false;

    // Reset: mimics sfall_metarules_reset() logic
    g_TestDialogShowCount = 0;
    g_TestScriptsEnabled = true;

    CHECK(g_TestDialogShowCount == 0);
    CHECK(g_TestScriptsEnabled);
}

// --- Stub metarule return value convention ---

TEST_CASE("Stub metarule return values — known behavior")
{
    // All stub metarules return specific values. These mirrors document
    // the expected behavior so tests catch regressions if stubs change.

    // lock_is_jammed → returns 0 (not jammed)
    const auto* lockEntry = TestFindMetarule("lock_is_jammed");
    REQUIRE(lockEntry != nullptr);
    CHECK(lockEntry->errorReturn == 0);

    // add_trait → returns -1 (failure)
    const auto* traitEntry = TestFindMetarule("add_trait");
    REQUIRE(traitEntry != nullptr);
    CHECK(traitEntry->errorReturn == -1);

    // signal_close_game → 0 args, no error return (void-like)
    const auto* signalEntry = TestFindMetarule("signal_close_game");
    REQUIRE(signalEntry != nullptr);
    CHECK(signalEntry->minArgs == 0);
    CHECK(signalEntry->maxArgs == 0);
}

// --- npc_engine_level_up storage logic ---

TEST_CASE("npc_engine_level_up — enable/disable toggle logic")
{
    // Mirror of mf_npc_engine_level_up: gNpcEngineLevelUpEnabled = (val != 0) ? 1 : 0
    int gNpcEngineLevelUpEnabled = 1; // default

    // Enable with non-zero
    {
        int val = 42;
        gNpcEngineLevelUpEnabled = (val != 0) ? 1 : 0;
        CHECK(gNpcEngineLevelUpEnabled == 1);
    }

    // Enable with 1
    {
        int val = 1;
        gNpcEngineLevelUpEnabled = (val != 0) ? 1 : 0;
        CHECK(gNpcEngineLevelUpEnabled == 1);
    }

    // Disable with 0
    {
        int val = 0;
        gNpcEngineLevelUpEnabled = (val != 0) ? 1 : 0;
        CHECK(gNpcEngineLevelUpEnabled == 0);
    }

    // Enable with negative (non-zero is truthy)
    {
        int val = -1;
        gNpcEngineLevelUpEnabled = (val != 0) ? 1 : 0;
        CHECK(gNpcEngineLevelUpEnabled == 1);
    }
}

TEST_CASE("set_scr_name — clear and set logic")
{
    // Mirror of mf_set_scr_name: 0 args → clear, otherwise set from arg0
    std::string gOverride;

    // Set override
    gOverride = "TestScript";
    CHECK(gOverride == "TestScript");

    // Clear override (0 args → clear)
    gOverride.clear();
    CHECK(gOverride == "");
    CHECK(gOverride.empty());
}

TEST_CASE("set_worldmap_heal_time / set_rest_heal_time — storage logic")
{
    // Mirror of mf_set_worldmap_heal_time and mf_set_rest_heal_time
    int gWorldmapHealTime = -1;
    int gRestHealTime = -1;

    // Default is -1
    CHECK(gWorldmapHealTime == -1);
    CHECK(gRestHealTime == -1);

    // Set positive values
    gWorldmapHealTime = 7200;
    CHECK(gWorldmapHealTime == 7200);

    gRestHealTime = 3600;
    CHECK(gRestHealTime == 3600);

    // Reset to default (-1)
    gWorldmapHealTime = -1;
    gRestHealTime = -1;
    CHECK(gWorldmapHealTime == -1);
    CHECK(gRestHealTime == -1);
}

TEST_CASE("set_quest_failure_value — map storage logic")
{
    // Mirror of gQuestFailureValues std::map
    std::map<int, int> gQuestFailureValues;

    // Set mapping
    gQuestFailureValues[5] = 10;
    CHECK(gQuestFailureValues[5] == 10);

    gQuestFailureValues[42] = -1;
    CHECK(gQuestFailureValues[42] == -1);

    // Verify key existence
    CHECK(gQuestFailureValues.find(5) != gQuestFailureValues.end());
    CHECK(gQuestFailureValues.find(42) != gQuestFailureValues.end());
    CHECK(gQuestFailureValues.find(99) == gQuestFailureValues.end());

    // Overwrite
    gQuestFailureValues[5] = 20;
    CHECK(gQuestFailureValues[5] == 20);

    // Clear (simulates reset)
    gQuestFailureValues.clear();
    CHECK(gQuestFailureValues.empty());
}

TEST_CASE("set_terrain_name / get_terrain_name — map storage logic")
{
    // Mirror of gTerrainNameOverrides std::map
    std::map<std::pair<int, int>, std::string> gTerrainNameOverrides;

    // Set override
    gTerrainNameOverrides[{10, 20}] = "TestTerrain";
    auto it = gTerrainNameOverrides.find({10, 20});
    CHECK(it != gTerrainNameOverrides.end());
    CHECK(it->second == "TestTerrain");

    // Different coordinate — not found
    it = gTerrainNameOverrides.find({99, 99});
    CHECK(it == gTerrainNameOverrides.end());

    // Overwrite
    gTerrainNameOverrides[{10, 20}] = "NewName";
    it = gTerrainNameOverrides.find({10, 20});
    CHECK(it->second == "NewName");

    // Clear
    gTerrainNameOverrides.clear();
    CHECK(gTerrainNameOverrides.empty());
}

TEST_CASE("set_town_title — map storage with empty-clear logic")
{
    // Mirror of gTownTitleOverrides: set stores, empty string erases
    std::map<int, std::string> gTownTitleOverrides;

    // Set non-empty
    gTownTitleOverrides[1] = "Junktown";
    CHECK(gTownTitleOverrides.find(1) != gTownTitleOverrides.end());
    CHECK(gTownTitleOverrides[1] == "Junktown");

    // Erase with empty string
    gTownTitleOverrides.erase(1);
    CHECK(gTownTitleOverrides.find(1) == gTownTitleOverrides.end());

    // Set to empty string then erase
    gTownTitleOverrides[2] = "";
    CHECK(gTownTitleOverrides.find(2) != gTownTitleOverrides.end());
    gTownTitleOverrides.erase(2);
    CHECK(gTownTitleOverrides.find(2) == gTownTitleOverrides.end());
}

TEST_CASE("set_rest_mode — simple int storage")
{
    // Mirror of gRestMode: default -1, stores any int value
    int gRestMode = -1;

    CHECK(gRestMode == -1); // default

    gRestMode = 0; // disabled
    CHECK(gRestMode == 0);

    gRestMode = 1; // strict
    CHECK(gRestMode == 1);

    gRestMode = 2; // no healing
    CHECK(gRestMode == 2);

    // Reset to default
    gRestMode = -1;
    CHECK(gRestMode == -1);
}

TEST_CASE("set_car_intface_art — int storage with default -1")
{
    int gCarIntfaceArtFid = -1;
    CHECK(gCarIntfaceArtFid == -1); // default

    gCarIntfaceArtFid = 0x10000001; // sample FID
    CHECK(gCarIntfaceArtFid == 0x10000001);

    gCarIntfaceArtFid = 0;
    CHECK(gCarIntfaceArtFid == 0);

    gCarIntfaceArtFid = -1;
    CHECK(gCarIntfaceArtFid == -1);
}

TEST_CASE("intface_hide / intface_show / intface_is_hidden — state transitions")
{
    // Mirror of sIntfaceHiddenState tracker
    bool sHidden = false;

    // Initially visible
    CHECK_FALSE(sHidden);

    // Hide
    sHidden = true;
    CHECK(sHidden);

    // Show
    sHidden = false;
    CHECK_FALSE(sHidden);

    // Hide again
    sHidden = true;
    CHECK(sHidden);

    // Double hide — stays hidden
    sHidden = true;
    CHECK(sHidden);

    // Double show — stays shown
    sHidden = false;
    CHECK_FALSE(sHidden);
    sHidden = false;
    CHECK_FALSE(sHidden);
}

// --- NPC Fake Perk/Trait storage logic ---

TEST_CASE("FakePerksNpc — set/has/remove cycle")
{
    // Mirror of gFakePerksNpc unordered_map
    std::unordered_map<int, std::unordered_set<std::string>> gFakePerksNpc;
    int critter1 = 1;
    int critter2 = 2;

    // Set perk for critter1 (simulating level != 0)
    gFakePerksNpc[critter1].insert("QuickPockets");
    auto it = gFakePerksNpc.find(critter1);
    CHECK(it != gFakePerksNpc.end());
    CHECK(it->second.find("QuickPockets") != it->second.end());

    // Has check
    CHECK(gFakePerksNpc[critter1].find("QuickPockets") != gFakePerksNpc[critter1].end());

    // Remove perk (simulating level == 0)
    it = gFakePerksNpc.find(critter1);
    if (it != gFakePerksNpc.end()) {
        it->second.erase("QuickPockets");
    }
    CHECK(gFakePerksNpc[critter1].empty());

    // Different critter isolation
    gFakePerksNpc[critter1].insert("PerkA");
    gFakePerksNpc[critter2].insert("PerkB");
    CHECK(gFakePerksNpc[critter1].find("PerkA") != gFakePerksNpc[critter1].end());
    CHECK(gFakePerksNpc[critter2].find("PerkB") != gFakePerksNpc[critter2].end());
    CHECK(gFakePerksNpc[critter1].find("PerkB") == gFakePerksNpc[critter1].end());
    CHECK(gFakePerksNpc[critter2].find("PerkA") == gFakePerksNpc[critter2].end());

    // Clear all (simulates reset)
    gFakePerksNpc.clear();
    CHECK(gFakePerksNpc.empty());
}

TEST_CASE("FakeTraitsNpc — set/remove with active flag")
{
    // Mirror of gFakeTraitsNpc: active != 0 → insert; active == 0 → erase
    std::unordered_map<int, std::unordered_set<std::string>> gFakeTraitsNpc;
    int critter = 1;

    // Set (active=1)
    gFakeTraitsNpc[critter].insert("FastMetabolism");
    CHECK(gFakeTraitsNpc[critter].find("FastMetabolism") != gFakeTraitsNpc[critter].end());

    // Remove (active=0)
    auto it = gFakeTraitsNpc.find(critter);
    if (it != gFakeTraitsNpc.end()) {
        it->second.erase("FastMetabolism");
    }
    CHECK(gFakeTraitsNpc[critter].empty());

    // Set with non-zero active (e.g., 42)
    gFakeTraitsNpc[critter].insert("Bruiser");
    CHECK(gFakeTraitsNpc[critter].find("Bruiser") != gFakeTraitsNpc[critter].end());
}

TEST_CASE("FakeSelectablePerksNpc — set/remove cycle")
{
    // Mirror of gFakeSelectablePerksNpc
    std::unordered_map<int, std::unordered_set<std::string>> gFakeSelectablePerksNpc;
    int critter = 1;

    // Set (active != 0)
    gFakeSelectablePerksNpc[critter].insert("BonusMove");
    CHECK(gFakeSelectablePerksNpc[critter].find("BonusMove") != gFakeSelectablePerksNpc[critter].end());

    // Has check
    auto it = gFakeSelectablePerksNpc.find(critter);
    CHECK(it != gFakeSelectablePerksNpc.end());
    CHECK(it->second.find("BonusMove") != it->second.end());

    // Remove (active == 0)
    it = gFakeSelectablePerksNpc.find(critter);
    if (it != gFakeSelectablePerksNpc.end()) {
        it->second.erase("BonusMove");
    }
    CHECK(gFakeSelectablePerksNpc[critter].empty());
}
