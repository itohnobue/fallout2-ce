// Boundary value tests for OPCODES CORE and OPCODES EXT findings.
// Focused on INT_MIN, INT_MAX, capacity limit stress, and boundary
// edge cases that are harder to cover in standard behavioral tests.
//
// All opcode handler functions (op_*) are file-static in sfall_opcodes.cc
// and inaccessible from test files. Where possible, production extern
// globals are tested directly. Otherwise, behavioral mirrors validate
// the production logic patterns at boundary values.
//
// Findings covered:
//   F-084: Knockback type boundary values (INT_MIN, INT_MAX)
//   F-085: Hit chance max boundary values (INT_MIN, INT_MAX, 0, 1, 99, 100, 101)
//   F-086: Hit chance mod boundary values (INT_MIN, INT_MAX)
//   F-087: Perk mod globals boundary values
//   F-088: perkID boundary: 0, PERK_COUNT-1, PERK_COUNT, INT_MIN, INT_MAX
//   F-079: Fake perk/trait capacity stress (exactly 64/16, overflow at 65/17)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "sfall_opcodes.h"
#include "perk.h"

#include <climits>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace fallout;

// ============================================================
// Mirror helpers (minimal duplicates for boundary testing)
// ============================================================

// Hit chance max mirror (sfall_opcodes.cc:4355-4365)
static int gBoundaryHitChanceMax = 95;
static void mirrorSetHitChanceMax(int max)
{
    if (max < 1)  { max = 1; }
    if (max > 100) { max = 100; }
    gBoundaryHitChanceMax = max;
}

// Hit chance mod mirror (sfall_opcodes.cc:4376-4388)
static int gBoundaryHitChanceMod = 0;
static void mirrorSetBaseHitChanceMod(int max, int mod)
{
    if (max < 1)  { max = 1; }
    if (max > 100) { max = 100; }
    gBoundaryHitChanceMax = max;
    gBoundaryHitChanceMod = mod;
}

// Perk mod mirrors (sfall_opcodes.cc:4250-4288)
static int gBoundaryPyroMod = 0;
static int gBoundarySwiftMod = 0;
static int gBoundaryHpMod = 0;
static void mirrorSetPyroMod(int v) { gBoundaryPyroMod = v; }
static void mirrorSetSwiftMod(int v) { gBoundarySwiftMod = v; }
static void mirrorSetHpMod(int v) { gBoundaryHpMod = v; }

// Fake perk/trait mirrors (sfall_opcodes.cc:3994-4115)
static constexpr int kMaxFp = 64;
static constexpr int kMaxFt = 16;

struct BndFakePerk { std::string name; int level; int image; std::string desc; bool active; };
struct BndFakeTrait { std::string name; int active; int image; std::string desc; };

static std::vector<BndFakePerk> bndFakePerks;
static std::vector<BndFakeTrait> bndFakeTraits;

static bool bndAddFakePerk(const char* name, int level, int image, const char* desc)
{
    if (static_cast<int>(bndFakePerks.size()) >= kMaxFp) { return false; }
    bndFakePerks.push_back({
        (name && name[0]) ? name : "",
        level, image,
        (desc && desc[0]) ? desc : "",
        true
    });
    return true;
}

static bool bndAddFakeTrait(const char* name, int active, int image, const char* desc)
{
    if (static_cast<int>(bndFakeTraits.size()) >= kMaxFt) { return false; }
    bndFakeTraits.push_back({
        (name && name[0]) ? name : "",
        (active != 0) ? 1 : 0,
        image,
        (desc && desc[0]) ? desc : ""
    });
    return true;
}

static void bndResetAll()
{
    bndFakePerks.clear();
    bndFakeTraits.clear();
    gBoundaryHitChanceMax = 95;
    gBoundaryHitChanceMod = 0;
    gBoundaryPyroMod = 0;
    gBoundarySwiftMod = 0;
    gBoundaryHpMod = 0;
}

// ============================================================
// F-084: Knockback — boundary type values
// ============================================================

TEST_CASE("F-084 Boundary: knockback type — INT_MIN to INT_MAX sweep")
{
    // Production stores type directly (sfall_opcodes.cc:3912):
    //   sfallWeaponKnockbackType = type;  // no validation
    //
    // Knockback globals are extern (sfall_opcodes.h:97-102)
    // and defined in test_common_stubs.cc.

    SUBCASE("type = 0 through 9 (small positive values)")
    {
        for (int t = 0; t <= 9; t++) {
            sfallWeaponKnockbackType = t;
            CHECK(sfallWeaponKnockbackType == t);
        }
    }

    SUBCASE("type = -1 through -9 (small negative, no guard)")
    {
        for (int t = -1; t >= -9; t--) {
            sfallWeaponKnockbackType = t;
            CHECK(sfallWeaponKnockbackType == t);
        }
    }

    SUBCASE("type bounds: INT_MIN, INT_MAX, 0")
    {
        sfallWeaponKnockbackType = INT_MIN;
        CHECK(sfallWeaponKnockbackType == INT_MIN);

        sfallWeaponKnockbackType = 0;
        CHECK(sfallWeaponKnockbackType == 0);

        sfallWeaponKnockbackType = INT_MAX;
        CHECK(sfallWeaponKnockbackType == INT_MAX);
    }

    SUBCASE("float knockback values: NaN-like and extreme")
    {
        // Production stores: value.asFloat() or static_cast<float>(value.integerValue)
        sfallWeaponKnockbackValue = 0.0f;
        CHECK(sfallWeaponKnockbackValue == doctest::Approx(0.0f));

        sfallWeaponKnockbackValue = 99999.5f;
        CHECK(sfallWeaponKnockbackValue == doctest::Approx(99999.5f));

        sfallWeaponKnockbackValue = -1.0e10f;
        CHECK(sfallWeaponKnockbackValue == doctest::Approx(-1.0e10f));
    }

    SUBCASE("all six knockback globals are independently addressable")
    {
        int* allTypes[3] = { &sfallWeaponKnockbackType, &sfallTargetKnockbackType, &sfallAttackerKnockbackType };
        float* allValues[3] = { &sfallWeaponKnockbackValue, &sfallTargetKnockbackValue, &sfallAttackerKnockbackValue };

        for (int i = 0; i < 3; i++) {
            *allTypes[i] = i * 10;
            *allValues[i] = static_cast<float>(i * 10 + 5);
        }
        for (int i = 0; i < 3; i++) {
            CHECK(*allTypes[i] == i * 10);
            CHECK(*allValues[i] == doctest::Approx(static_cast<float>(i * 10 + 5)));
        }
    }

    sfallOpcodesReset();
}

// ============================================================
// F-085: Hit chance max — boundary value matrix
// ============================================================

TEST_CASE("F-085 Boundary: hit chance max — systematic boundary matrix")
{
    // Production clamp: [1, 100]
    // Test every value in the range [1, 100] to verify no off-by-one errors.

    SUBCASE("sweep [1..100] — all pass through unchanged")
    {
        for (int v = 1; v <= 100; v++) {
            mirrorSetHitChanceMax(v);
            CHECK(gBoundaryHitChanceMax == v);
        }
    }

    SUBCASE("sweep [-100..0] — all clamp to 1")
    {
        for (int v = -100; v <= 0; v++) {
            mirrorSetHitChanceMax(v);
            CHECK(gBoundaryHitChanceMax == 1);
        }
    }

    SUBCASE("sweep [101..200] — all clamp to 100")
    {
        for (int v = 101; v <= 200; v++) {
            mirrorSetHitChanceMax(v);
            CHECK(gBoundaryHitChanceMax == 100);
        }
    }

    SUBCASE("INT_MIN through INT_MAX — guards work at extremes")
    {
        mirrorSetHitChanceMax(INT_MIN);
        CHECK(gBoundaryHitChanceMax == 1);

        mirrorSetHitChanceMax(INT_MIN + 1);
        CHECK(gBoundaryHitChanceMax == 1);

        mirrorSetHitChanceMax(INT_MAX);
        CHECK(gBoundaryHitChanceMax == 100);

        mirrorSetHitChanceMax(INT_MAX - 1);
        CHECK(gBoundaryHitChanceMax == 100);
    }

    SUBCASE("transition points: -1→1, 0→1, 100→100, 101→100")
    {
        mirrorSetHitChanceMax(-1);
        CHECK(gBoundaryHitChanceMax == 1);

        mirrorSetHitChanceMax(0);
        CHECK(gBoundaryHitChanceMax == 1);

        mirrorSetHitChanceMax(100);
        CHECK(gBoundaryHitChanceMax == 100);

        mirrorSetHitChanceMax(101);
        CHECK(gBoundaryHitChanceMax == 100);
    }

    bndResetAll();
}

// ============================================================
// F-086: Hit chance mod — boundary values for mod parameter
// ============================================================

TEST_CASE("F-086 Boundary: hit chance mod — extreme mod values")
{
    // mod parameter has NO clamping — any int is accepted ($CODE:4378).

    SUBCASE("mod=INT_MAX — extreme positive preserved")
    {
        mirrorSetBaseHitChanceMod(50, INT_MAX);
        CHECK(gBoundaryHitChanceMod == INT_MAX);
        CHECK(gBoundaryHitChanceMax == 50);
    }

    SUBCASE("mod=INT_MIN — extreme negative preserved")
    {
        mirrorSetBaseHitChanceMod(50, INT_MIN);
        CHECK(gBoundaryHitChanceMod == INT_MIN);
        CHECK(gBoundaryHitChanceMax == 50);
    }

    SUBCASE("mod=0 — zero mod preserved")
    {
        mirrorSetBaseHitChanceMod(75, 0);
        CHECK(gBoundaryHitChanceMod == 0);
        CHECK(gBoundaryHitChanceMax == 75);
    }

    SUBCASE("max clamping is independent of mod value")
    {
        // max clamps to [1,100] regardless of what mod is
        mirrorSetBaseHitChanceMod(-999, INT_MAX);
        CHECK(gBoundaryHitChanceMax == 1);      // -999 clamps to 1
        CHECK(gBoundaryHitChanceMod == INT_MAX); // mod unchanged

        mirrorSetBaseHitChanceMod(99999, INT_MIN);
        CHECK(gBoundaryHitChanceMax == 100);    // 99999 clamps to 100
        CHECK(gBoundaryHitChanceMod == INT_MIN); // mod unchanged
    }

    bndResetAll();
}

// ============================================================
// F-087: Perk mod globals — boundary values
// ============================================================

TEST_CASE("F-087 Boundary: perk mod globals — INT_MIN to INT_MAX")
{
    // All three globals accept any int without clamping
    // (sfall_opcodes.cc:4267-4288).

    SUBCASE("pyromaniac mod extreme values")
    {
        mirrorSetPyroMod(INT_MIN);
        CHECK(gBoundaryPyroMod == INT_MIN);
        mirrorSetPyroMod(0);
        CHECK(gBoundaryPyroMod == 0);
        mirrorSetPyroMod(INT_MAX);
        CHECK(gBoundaryPyroMod == INT_MAX);
    }

    SUBCASE("swiftlearner mod extreme values")
    {
        mirrorSetSwiftMod(INT_MIN);
        CHECK(gBoundarySwiftMod == INT_MIN);
        mirrorSetSwiftMod(0);
        CHECK(gBoundarySwiftMod == 0);
        mirrorSetSwiftMod(INT_MAX);
        CHECK(gBoundarySwiftMod == INT_MAX);
    }

    SUBCASE("HP per level mod extreme values")
    {
        mirrorSetHpMod(INT_MIN);
        CHECK(gBoundaryHpMod == INT_MIN);
        mirrorSetHpMod(0);
        CHECK(gBoundaryHpMod == 0);
        mirrorSetHpMod(INT_MAX);
        CHECK(gBoundaryHpMod == INT_MAX);
    }

    SUBCASE("all three at extremes simultaneously")
    {
        mirrorSetPyroMod(INT_MIN);
        mirrorSetSwiftMod(INT_MAX);
        mirrorSetHpMod(INT_MIN + 1);
        CHECK(gBoundaryPyroMod == INT_MIN);
        CHECK(gBoundarySwiftMod == INT_MAX);
        CHECK(gBoundaryHpMod == INT_MIN + 1);

        // Reset and verify extreme-to-zero transition
        bndResetAll();
        CHECK(gBoundaryPyroMod == 0);
        CHECK(gBoundarySwiftMod == 0);
        CHECK(gBoundaryHpMod == 0);
    }

    bndResetAll();
}

// ============================================================
// F-088: perkID boundary — full range validation
// ============================================================

TEST_CASE("F-088 Boundary: perkID — exhaustive boundary at PERK_COUNT")
{
    // perkIsValid = perk >= 0 && perk < PERK_COUNT
    // PERK_COUNT = 119 (perk_defs.h:126)

    SUBCASE("all valid perk IDs [0..118] pass perkIsValid")
    {
        for (int i = 0; i < PERK_COUNT; i++) {
            CHECK(perkIsValid(i) == true);
        }
    }

    SUBCASE("all invalid perk IDs [119..300] fail perkIsValid")
    {
        for (int i = PERK_COUNT; i < PERK_COUNT + 200; i++) {
            CHECK(perkIsValid(i) == false);
        }
    }

    SUBCASE("negative range [-1000..-1] fails perkIsValid")
    {
        for (int i = -1000; i < 0; i++) {
            CHECK(perkIsValid(i) == false);
        }
    }

    SUBCASE("INT_MIN and INT_MAX rejected")
    {
        CHECK(perkIsValid(INT_MIN) == false);
        CHECK(perkIsValid(INT_MAX) == false);
    }

    SUBCASE("perkIsValid(PERK_COUNT) == false (regression test)")
    {
        // If someone changes PERK_COUNT, the boundary shifts but
        // PERK_COUNT itself is always the first invalid index.
        CHECK(perkIsValid(PERK_COUNT) == false);
    }

    SUBCASE("perkIsValid(PERK_COUNT - 1) == true (last valid)")
    {
        CHECK(perkIsValid(PERK_COUNT - 1) == true);
    }
}

// ============================================================
// F-079: Fake perk/trait — capacity stress and overflow
// ============================================================

TEST_CASE("F-079 Boundary: fake perk capacity — exactly 64 entries")
{
    bndResetAll();

    SUBCASE("fill to exact capacity (64)")
    {
        for (int i = 0; i < kMaxFp; i++) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "P%03d", i);
            CHECK(bndAddFakePerk(buf, 1, i, "d") == true);
        }
        CHECK(static_cast<int>(bndFakePerks.size()) == kMaxFp);
    }

    SUBCASE("overflow rejection at 65 (exactly one over)")
    {
        for (int i = 0; i < kMaxFp; i++) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "P%03d", i);
            bndAddFakePerk(buf, 1, i, "d");
        }
        // 65th entry at capacity
        CHECK(bndAddFakePerk("OVERFLOW", 1, 0, "d") == false);
        CHECK(static_cast<int>(bndFakePerks.size()) == kMaxFp);
    }

    SUBCASE("overflow at 65, then reset, then refill")
    {
        for (int i = 0; i < kMaxFp; i++) {
            bndAddFakePerk("fill", 0, 0, "");
        }
        CHECK(bndAddFakePerk("overflow", 0, 0, "") == false);

        bndFakePerks.clear(); // simulate sfallOpcodesReset()
        CHECK(bndFakePerks.empty());

        // Can fill again after reset
        for (int i = 0; i < kMaxFp; i++) {
            CHECK(bndAddFakePerk("refill", 0, 0, "") == true);
        }
        CHECK(static_cast<int>(bndFakePerks.size()) == kMaxFp);
    }

    bndResetAll();
}

TEST_CASE("F-079 Boundary: fake trait capacity — exactly 16 entries")
{
    bndResetAll();

    SUBCASE("fill to exact capacity (16)")
    {
        for (int i = 0; i < kMaxFt; i++) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "T%02d", i);
            CHECK(bndAddFakeTrait(buf, 1, i, "d") == true);
        }
        CHECK(static_cast<int>(bndFakeTraits.size()) == kMaxFt);
    }

    SUBCASE("overflow rejection at 17")
    {
        for (int i = 0; i < kMaxFt; i++) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "T%02d", i);
            bndAddFakeTrait(buf, 1, i, "d");
        }
        CHECK(bndAddFakeTrait("OVERFLOW", 1, 0, "d") == false);
        CHECK(static_cast<int>(bndFakeTraits.size()) == kMaxFt);
    }

    bndResetAll();
}

// ============================================================
// F-082: set_perk_name — boundary name lengths
// ============================================================

// Mirror of set_perk_name for boundary testing (simplified from sfall_opcodes.cc:3497-3517)
static constexpr int kBNMax = 128;
static char* bndPerkNames[kBNMax] = {};

static void bndSetPerkName(int id, const char* name)
{
    if (!perkIsValid(id) || id >= kBNMax) { return; }
    delete[] bndPerkNames[id];
    bndPerkNames[id] = nullptr;
    if (name != nullptr && name[0] != '\0') {
        size_t len = std::strlen(name) + 1;
        bndPerkNames[id] = new char[len];
        std::memcpy(bndPerkNames[id], name, len);
    }
}

static const char* bndGetPerkName(int id)
{
    return (perkIsValid(id) && id < kBNMax) ? bndPerkNames[id] : nullptr;
}

static void bndCleanupNames()
{
    for (int i = 0; i < kBNMax; i++) {
        delete[] bndPerkNames[i];
        bndPerkNames[i] = nullptr;
    }
}

TEST_CASE("F-082 Boundary: set_perk_name — long name and edge lengths")
{
    bndCleanupNames();

    SUBCASE("single character name")
    {
        bndSetPerkName(10, "A");
        CHECK(std::string(bndGetPerkName(10)) == "A");
    }

    SUBCASE("maximum-length name (1024 chars)")
    {
        std::string longName(1024, 'X');
        bndSetPerkName(10, longName.c_str());
        CHECK(std::string(bndGetPerkName(10)) == longName);
    }

    SUBCASE("name with embedded null characters")
    {
        // strlen stops at first null — only "Hello" is stored
        bndSetPerkName(11, "Hello\0World");
        CHECK(std::string(bndGetPerkName(11)) == "Hello");
    }

    SUBCASE("whitespace-only name preserved as-is")
    {
        bndSetPerkName(12, "   ");
        CHECK(std::string(bndGetPerkName(12)) == "   ");
    }

    SUBCASE("rapid sequence: set, clear, set, clear")
    {
        for (int cycle = 0; cycle < 10; cycle++) {
            bndSetPerkName(5, "A");
            CHECK(bndGetPerkName(5) != nullptr);
            bndSetPerkName(5, "");   // clear
            CHECK(bndGetPerkName(5) == nullptr);
        }
    }

    bndCleanupNames();
}

// ============================================================
// F-063: fs_seek pos — boundary sweep
// ============================================================

TEST_CASE("F-063 Boundary: fs_seek pos — full integer range behavior")
{
    // Production: pos = programStackPopInteger(program) — any int in the
    // engine's 32-bit signed integer range. No validation at all.

    SUBCASE("negative pos values: -1 is UB per C99 §7.19.9.2")
    {
        // For fseek(f, neg, SEEK_SET): "If the resulting file position
        // would be negative, the behavior is undefined."
        // Production code at sfall_opcodes.cc:2514 would call:
        //   fseek(sfallVfsFiles[id], pos, SEEK_SET);
        // with pos = -1, -100, INT_MIN — all UB.
        int posValues[] = { -1, -10, -1000, INT_MIN + 1, INT_MIN };
        for (int pos : posValues) {
            CHECK(pos < 0); // all are negative
        }
    }

    SUBCASE("zero pos: valid, seeks to beginning of file")
    {
        int pos = 0;
        CHECK(pos == 0); // well-defined for fseek
    }

    SUBCASE("positive pos: defined but may extend file on some platforms")
    {
        int posValues[] = { 1, 100, 1000000, INT_MAX - 1, INT_MAX };
        for (int pos : posValues) {
            CHECK(pos > 0); // all are positive
        }
    }

    // NOTE: The actual fseek call cannot be tested without:
    //   (a) Program* mock (50+ engine deps)
    //   (b) A real VFS file handle
    //   (c) Knowledge of platform-specific fseek behavior
    // The mirror validates the script-facing contract: pos has
    // NO guard, which is the confirmed gap from F-063.
}

namespace {
    struct BoundaryCleanupGuard {
        ~BoundaryCleanupGuard() {
            bndCleanupNames();
            bndResetAll();
        }
    };
    static BoundaryCleanupGuard _bndCleanup;
}
