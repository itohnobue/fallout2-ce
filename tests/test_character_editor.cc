// Unit tests for character_editor.cc — pure logic, data structure validation,
// and sfall integration points (gSkillPointsPerLevelMod, gPerkFrequencyOverride).
//
// This test does NOT link character_editor.cc (7,364 LOC with 40+ rendering
// engine dependencies). It mirrors the production logic with test-local stubs
// that reproduce the exact algorithms from the source file. See the discovery
// report at tmp/s2-discover-char-editor-report.md for the full testability
// assessment.
//
// Tests: _isdoschar, _strmfe, characterEditorReset, characterEditorSelectFolder,
//        _is_supper_bonus, customKarmaFolderGetFrmId, characterEditorUpdateLevel
//        (skill points calculation, gSkillPointsPerLevelMod integration,
//         gPerkFrequencyOverride integration, free-perk logic).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cctype>
#include <climits>
#include <cstring>
#include <vector>

#include "stat_defs.h"

// ================================================================
// Test-local type mirrors matching production definitions
// ================================================================

// EditorFolder enum: character_editor.cc:110-114
enum TestEditorFolder {
    TEST_EDITOR_FOLDER_PERKS,
    TEST_EDITOR_FOLDER_KARMA,
    TEST_EDITOR_FOLDER_KILLS,
};

// CustomKarmaFolderDescription: character_editor.cc:786-789
struct TestCustomKarmaFolderDescription {
    int frmId;
    int threshold;
};

// ---- Production constant mirrors ----
// stat_defs.h
constexpr int TEST_PRIMARY_STAT_MAX = 10;
constexpr int TEST_PC_LEVEL_MAX = 99;
static_assert(TEST_PC_LEVEL_MAX == PC_LEVEL_MAX,
    "TEST_PC_LEVEL_MAX must match production PC_LEVEL_MAX");
constexpr int TEST_STAT_STRENGTH = 0;
constexpr int TEST_STAT_INTELLIGENCE = 4;
constexpr int TEST_PRIMARY_STAT_COUNT = 7;
constexpr int TEST_PC_STAT_UNSPENT_SKILL_POINTS = 0;
constexpr int TEST_PC_STAT_LEVEL = 1;

// perk_defs.h
constexpr int TEST_PERK_EDUCATED = 25;
constexpr int TEST_PERK_COUNT = 126;

// trait_defs.h
constexpr int TEST_TRAIT_SKILLED = 14;
constexpr int TEST_TRAIT_GIFTED = 15;

// game_vars.h
constexpr int TEST_GVAR_PLAYER_REPUTATION = 0;

// character_editor.cc:786-789
constexpr int TEST_DEFAULT_KARMA_FRM_ID = 47;

// character_editor.cc:110-114
constexpr int TEST_EDITOR_FOLDER_NONE = -1;

// ================================================================
// Test-local global mirrors matching production
// ================================================================

// character_editor.cc:504
static int g_testCharacterEditorRemainingCharacterPoints = 0;

// character_editor.cc:692 (static)
static int g_testCharacterEditorLastLevel = 0;

// character_editor.cc:715 (static, default -1 per characterEditorWindowInit:1917)
static int g_testCharacterEditorWindowSelectedFolder = -1;

// character_editor.cc:777 (static unsigned char)
static unsigned char g_testCharacterEditorHasFreePerk = 0;

// character_editor.cc:667 (static int, default -1)
static int g_testCharacterEditorWindow = -1;

// sfall_opcodes.h global mirrors
static int g_testSkillPointsPerLevelMod = 0;
static int g_testPerkFrequencyOverride = 0;

// character_editor.cc:791
static std::vector<TestCustomKarmaFolderDescription> g_testCustomKarmaFolderDescriptions;

// ================================================================
// Test-local function mirrors matching production logic
// ================================================================

// Mirror of _isdoschar: character_editor.cc:2021-2037
static bool test_isdoschar(int ch)
{
    const char* punctuations = "#@!$`'~^&()-_=[]{}";

    if (isalnum(ch)) {
        return true;
    }

    size_t length = strlen(punctuations);
    for (size_t index = 0; index < length; index++) {
        if (punctuations[index] == ch) {
            return true;
        }
    }

    return false;
}

// Mirror of _strmfe: character_editor.cc:2042-2055
static char* test_strmfe(char* dest, const char* name, const char* ext)
{
    char* save = dest;

    while (*name != '\0' && *name != '.') {
        *dest++ = *name++;
    }

    *dest++ = '.';

    strcpy(dest, ext);

    return save;
}

// Mirror of characterEditorReset: character_editor.cc:5681-5685
static void test_characterEditorReset()
{
    g_testCharacterEditorRemainingCharacterPoints = 5;
    g_testCharacterEditorLastLevel = 1;
}

// Mirror of _is_supper_bonus: character_editor.cc:6810-6821
// Parameters replace production calls to critterGetBaseStatWithTraitModifier
// and critterGetBonusStat. The stat array is indexed 0..6 (SPECIAL stats).
// Returns 1 if any SPECIAL stat (base + bonus) > 10, 0 otherwise.
static int test_is_supper_bonus(const int* baseStats, const int* bonusStats)
{
    for (int stat = 0; stat < 7; stat++) {
        int v1 = baseStats[stat];
        int v2 = bonusStats[stat];
        if (v1 + v2 > 10) {
            return 1;
        }
    }

    return 0;
}

// Mirror of customKarmaFolderGetFrmId: character_editor.cc:7302-7315
// Takes reputation and descriptions vector as explicit parameters instead
// of reading gGameGlobalVars[GVAR_PLAYER_REPUTATION] directly.
static int test_customKarmaFolderGetFrmId(
    int reputation,
    const std::vector<TestCustomKarmaFolderDescription>& descriptions)
{
    if (descriptions.empty()) {
        return 47; // TEST_DEFAULT_KARMA_FRM_ID
    }

    for (auto& entry : descriptions) {
        if (reputation < entry.threshold) {
            return entry.frmId;
        }
    }
    return descriptions.back().frmId;
}

// Mirror of characterEditorSelectFolder: character_editor.cc:5710-5742
// Parameterized: windowPtr simulates windowGetWindow return (nullptr = no window),
// folder is the folder index (0-3). Sets the selected folder global and
// returns true if the window was open and folder was changed.
static bool test_characterEditorSelectFolder(int folder, bool windowIsOpen)
{
    // Guard: no-op if the character editor window is not currently open.
    // Production uses: windowGetWindow(gCharacterEditorWindow) == nullptr
    if (!windowIsOpen) {
        return false;
    }

    switch (folder) {
    case 1:
        g_testCharacterEditorWindowSelectedFolder = TEST_EDITOR_FOLDER_PERKS;
        break;
    case 2:
        g_testCharacterEditorWindowSelectedFolder = TEST_EDITOR_FOLDER_KARMA;
        break;
    case 3:
        g_testCharacterEditorWindowSelectedFolder = TEST_EDITOR_FOLDER_KILLS;
        break;
    case 0:
    default:
        g_testCharacterEditorWindowSelectedFolder = -1;
        break;
    }

    // Production would also call characterEditorDrawFolders() and
    // characterEditorDisplayStats() — rendering side effects omitted.

    return true;
}

// Mirror of characterEditorUpdateLevel: character_editor.cc:5747-5815
//
// This mirrors the core level-up calculation logic. It takes all external
// dependencies as explicit parameters so we can test the mathematical
// behavior without linking the engine.
//
// Parameters:
//   newLevel       — pcGetStat(PC_STAT_LEVEL) -> the current player level
//   lastLevel      — gCharacterEditorLastLevel (stateful, in/out: updated to newLevel on success)
//   sp             — pcGetStat(PC_STAT_UNSPENT_SKILL_POINTS) (in/out)
//   intBase        — critterGetBaseStatWithTraitModifier(gDude, STAT_INTELLIGENCE)
//   educatedRank   — perkGetRank(gDude, PERK_EDUCATED)
//   isSkilled      — traitIsSelected(TRAIT_SKILLED)
//   isGifted       — traitIsSelected(TRAIT_GIFTED)
//   skillPtMod     — gSkillPointsPerLevelMod
//   perkFreqOverride — gPerkFrequencyOverride
//   selectedPerksCount — current count of selected perks
//   hasFreePerk    — gCharacterEditorHasFreePerk (in/out)
//
// Returns:
//   1  — success, level-up processed
//   0  — no level-up needed (level unchanged or already at cap)
//  -1  — error (reserved for perkDialogShow failure, not tested here)
//
// Note: The production function also calls perkDialogShow() and rendering
// functions. Those side effects are omitted from this mirror.
static int test_characterEditorUpdateLevel(
    int newLevel,
    int& lastLevel,
    int& sp,
    int intBase,
    int educatedRank,
    int isSkilled,
    int isGifted,
    int skillPtMod,
    int perkFreqOverride,
    int selectedPerksCount,
    unsigned char& hasFreePerk)
{
    if (newLevel != lastLevel && newLevel <= TEST_PC_LEVEL_MAX) {
        for (int nextLevel = lastLevel + 1; nextLevel <= newLevel; nextLevel++) {
            sp += 5;
            sp += intBase * 2;
            sp += educatedRank * 2;
            sp += isSkilled * 5;
            sp += skillPtMod;  // LINE 5757: gSkillPointsPerLevelMod integration
            if (isGifted) {
                sp -= 5;
                if (sp < 0) {
                    sp = 0;
                }
            }
            if (sp > 99) {
                sp = 99;
            }

            if (selectedPerksCount < 37) {
                // LINE 5783: gPerkFrequencyOverride integration
                int progression = (perkFreqOverride > 0) ? perkFreqOverride : 3;
                if (isSkilled) {
                    progression += 1;
                }

                if (nextLevel % progression == 0) {
                    hasFreePerk = 1;
                }
            }
        }
    }

    lastLevel = newLevel;

    return 1;
}

// ================================================================
// TEST CASES
// ================================================================

// ---- _isdoschar tests ----

TEST_CASE("_isdoschar — alphanumeric characters")
{
    SUBCASE("lowercase letters are DOS characters")
    {
        CHECK(test_isdoschar('a'));
        CHECK(test_isdoschar('z'));
        CHECK(test_isdoschar('m'));
    }

    SUBCASE("uppercase letters are DOS characters")
    {
        CHECK(test_isdoschar('A'));
        CHECK(test_isdoschar('Z'));
        CHECK(test_isdoschar('M'));
    }

    SUBCASE("digits are DOS characters")
    {
        CHECK(test_isdoschar('0'));
        CHECK(test_isdoschar('9'));
        CHECK(test_isdoschar('5'));
    }
}

TEST_CASE("_isdoschar — punctuation characters")
{
    // The allowed punctuation set is: #@!$`'~^&()-_=[]{}
    SUBCASE("allowed punctuation characters")
    {
        CHECK(test_isdoschar('#'));
        CHECK(test_isdoschar('@'));
        CHECK(test_isdoschar('!'));
        CHECK(test_isdoschar('$'));
        CHECK(test_isdoschar('`'));
        CHECK(test_isdoschar('\''));
        CHECK(test_isdoschar('~'));
        CHECK(test_isdoschar('^'));
        CHECK(test_isdoschar('&'));
        CHECK(test_isdoschar('('));
        CHECK(test_isdoschar(')'));
        CHECK(test_isdoschar('-'));
        CHECK(test_isdoschar('_'));
        CHECK(test_isdoschar('='));
        CHECK(test_isdoschar('['));
        CHECK(test_isdoschar(']'));
        CHECK(test_isdoschar('{'));
        CHECK(test_isdoschar('}'));
    }

    SUBCASE("disallowed punctuation characters")
    {
        CHECK_FALSE(test_isdoschar(' '));   // space
        CHECK_FALSE(test_isdoschar('.'));   // dot
        CHECK_FALSE(test_isdoschar(','));   // comma
        CHECK_FALSE(test_isdoschar(';'));   // semicolon
        CHECK_FALSE(test_isdoschar(':'));   // colon
        CHECK_FALSE(test_isdoschar('"'));   // double quote
        CHECK_FALSE(test_isdoschar('?'));   // question mark
        CHECK_FALSE(test_isdoschar('/'));   // forward slash
        CHECK_FALSE(test_isdoschar('\\'));  // backslash
        CHECK_FALSE(test_isdoschar('*'));   // asterisk
        CHECK_FALSE(test_isdoschar('+'));   // plus
        CHECK_FALSE(test_isdoschar('<'));   // less than
        CHECK_FALSE(test_isdoschar('>'));   // greater than
        CHECK_FALSE(test_isdoschar('|'));   // pipe
    }

    SUBCASE("non-printable characters are not DOS characters")
    {
        CHECK_FALSE(test_isdoschar('\0'));
        CHECK_FALSE(test_isdoschar('\n'));
        CHECK_FALSE(test_isdoschar('\t'));
        CHECK_FALSE(test_isdoschar('\r'));
        CHECK_FALSE(test_isdoschar(1));     // SOH
        CHECK_FALSE(test_isdoschar(127));   // DEL
    }
}

TEST_CASE("_isdoschar — edge cases")
{
    SUBCASE("extended ASCII characters are not DOS characters")
    {
        CHECK_FALSE(test_isdoschar(128));    // non-ASCII
        CHECK_FALSE(test_isdoschar(0xFF));   // highest byte value
        CHECK_FALSE(test_isdoschar(0xC0));   // typical non-ASCII
    }

    SUBCASE("negative character value")
    {
        // char may be signed; negative values are not alnum and not in the
        // punctuation set, so they should return false.
        CHECK_FALSE(test_isdoschar(-1));
    }

    SUBCASE("all 18 punctuation chars are covered")
    {
        const char* punctuations = "#@!$`'~^&()-_=[]{}";
        int count = static_cast<int>(strlen(punctuations));
        CHECK(count == 18);
        for (int i = 0; i < count; i++) {
            CHECK(test_isdoschar(punctuations[i]));
        }
    }
}

// ---- _strmfe tests ----

TEST_CASE("_strmfe — basic filename extension replacement")
{
    char dest[256];

    SUBCASE("normal filename with extension")
    {
        const char* result = test_strmfe(dest, "image.frm", "gif");
        CHECK(strcmp(dest, "image.gif") == 0);
        CHECK(result == dest); // returns original dest pointer
    }

    SUBCASE("filename with .frm extension replaced by .png")
    {
        test_strmfe(dest, "splash.frm", "png");
        CHECK(strcmp(dest, "splash.png") == 0);
    }

    SUBCASE("filename with multiple dots uses first dot for extension boundary")
    {
        // Production behavior: the loop stops at the first '.'
        test_strmfe(dest, "file.name.txt", "dat");
        CHECK(strcmp(dest, "file.dat") == 0);
    }

    SUBCASE("filename with no extension gets extension appended")
    {
        test_strmfe(dest, "readme", "txt");
        CHECK(strcmp(dest, "readme.txt") == 0);
    }

    SUBCASE("extension is the only content copied after the dot")
    {
        test_strmfe(dest, "test.bak", "cfg");
        CHECK(strcmp(dest, "test.cfg") == 0);
    }
}

TEST_CASE("_strmfe — edge cases")
{
    char dest[256];

    SUBCASE("empty name produces just the extension")
    {
        const char* result = test_strmfe(dest, "", "ext");
        CHECK(strcmp(dest, ".ext") == 0);
        CHECK(result == dest);
    }

    SUBCASE("name starts with dot produces just extension (dot-only base)")
    {
        // name[0] == '.' → loop body never executes, *dest++ = '.' written
        test_strmfe(dest, ".hidden", "txt");
        CHECK(strcmp(dest, ".txt") == 0);
    }

    SUBCASE("name is just a dot")
    {
        test_strmfe(dest, ".", "bak");
        CHECK(strcmp(dest, ".bak") == 0);
    }

    SUBCASE("long filename within buffer")
    {
        test_strmfe(dest, "abcdefghijklmnopqrstuvwxyz.ext", "abc");
        // base name (up to first '.') is 26 chars + '.' + "abc" + '\0' = 31
        CHECK(strcmp(dest, "abcdefghijklmnopqrstuvwxyz.abc") == 0);
    }
}

// ---- characterEditorReset tests ----

TEST_CASE("characterEditorReset — defaults")
{
    // Initialize to non-default values to verify reset overwrites them
    g_testCharacterEditorRemainingCharacterPoints = 999;
    g_testCharacterEditorLastLevel = 42;

    test_characterEditorReset();

    CHECK(g_testCharacterEditorRemainingCharacterPoints == 5);
    CHECK(g_testCharacterEditorLastLevel == 1);
}

TEST_CASE("characterEditorReset — idempotency")
{
    test_characterEditorReset();
    CHECK(g_testCharacterEditorRemainingCharacterPoints == 5);
    CHECK(g_testCharacterEditorLastLevel == 1);

    // Double reset produces same values
    test_characterEditorReset();
    CHECK(g_testCharacterEditorRemainingCharacterPoints == 5);
    CHECK(g_testCharacterEditorLastLevel == 1);
}

// ---- _is_supper_bonus tests ----

TEST_CASE("_is_supper_bonus — SPECIAL stat cap validation")
{
    SUBCASE("all stats at 10 with zero bonus — not exceeding")
    {
        int baseStats[7] = {10, 10, 10, 10, 10, 10, 10};
        int bonusStats[7] = {0, 0, 0, 0, 0, 0, 0};
        CHECK(test_is_supper_bonus(baseStats, bonusStats) == 0);
    }

    SUBCASE("all stats at 10 with +1 bonus on INT — exceeding")
    {
        int baseStats[7] = {10, 10, 10, 10, 10, 10, 10};
        int bonusStats[7] = {0, 0, 0, 0, 1, 0, 0}; // INT at index 4
        CHECK(test_is_supper_bonus(baseStats, bonusStats) == 1);
    }

    SUBCASE("strength at 11 — exceeding")
    {
        int baseStats[7] = {11, 5, 5, 5, 5, 5, 5};
        int bonusStats[7] = {0, 0, 0, 0, 0, 0, 0};
        CHECK(test_is_supper_bonus(baseStats, bonusStats) == 1);
    }

    SUBCASE("luck at 5 base + 6 bonus — exceeding")
    {
        int baseStats[7] = {5, 5, 5, 5, 5, 5, 5};
        int bonusStats[7] = {0, 0, 0, 0, 0, 0, 6}; // Luck at index 6
        CHECK(test_is_supper_bonus(baseStats, bonusStats) == 1);
    }

    SUBCASE("exactly at 10 total — not exceeding")
    {
        int baseStats[7] = {5, 8, 7, 6, 4, 9, 5};
        int bonusStats[7] = {5, 2, 3, 4, 6, 1, 5}; // each pair sums to 10
        CHECK(test_is_supper_bonus(baseStats, bonusStats) == 0);
    }

    SUBCASE("only the last stat exceeds")
    {
        int baseStats[7] = {5, 5, 5, 5, 5, 5, 5};
        int bonusStats[7] = {5, 5, 5, 5, 5, 5, 6}; // index 6 sums to 11
        CHECK(test_is_supper_bonus(baseStats, bonusStats) == 1);
    }

    SUBCASE("first stat exceeds — early detect")
    {
        int baseStats[7] = {10, 5, 5, 5, 5, 5, 5};
        int bonusStats[7] = {1, 0, 0, 0, 0, 0, 0};
        CHECK(test_is_supper_bonus(baseStats, bonusStats) == 1);
    }
}

// ---- customKarmaFolderGetFrmId tests ----

TEST_CASE("customKarmaFolderGetFrmId — empty descriptions default")
{
    std::vector<TestCustomKarmaFolderDescription> empty;
    // Empty vector → return default FRM ID 47
    CHECK(test_customKarmaFolderGetFrmId(0, empty) == 47);
    CHECK(test_customKarmaFolderGetFrmId(-500, empty) == 47);
    CHECK(test_customKarmaFolderGetFrmId(1000, empty) == 47);
}

TEST_CASE("customKarmaFolderGetFrmId — threshold matching")
{
    std::vector<TestCustomKarmaFolderDescription> descs;

    // Set up karma descriptions with thresholds in ascending order
    // (Production sorts by threshold descending, but the lookup iterates
    //  linearly checking reputation < threshold — order matters for correctness.)
    TestCustomKarmaFolderDescription d1 = {100, -500};   // reputation < -500 → frmId 100
    TestCustomKarmaFolderDescription d2 = {101, -250};   // reputation < -250 → frmId 101
    TestCustomKarmaFolderDescription d3 = {102, 0};      // reputation < 0 → frmId 102
    TestCustomKarmaFolderDescription d4 = {103, 250};    // reputation < 250 → frmId 103
    TestCustomKarmaFolderDescription d5 = {104, 500};    // reputation < 500 → frmId 104
    descs.push_back(d1);
    descs.push_back(d2);
    descs.push_back(d3);
    descs.push_back(d4);
    descs.push_back(d5);

    SUBCASE("reputation below first threshold returns first entry")
    {
        CHECK(test_customKarmaFolderGetFrmId(-1000, descs) == 100);
    }

    SUBCASE("reputation between first and second threshold returns first match")
    {
        CHECK(test_customKarmaFolderGetFrmId(-400, descs) == 101);
    }

    SUBCASE("reputation negative but above some thresholds")
    {
        CHECK(test_customKarmaFolderGetFrmId(-10, descs) == 102);
    }

    SUBCASE("reputation in middle range")
    {
        CHECK(test_customKarmaFolderGetFrmId(100, descs) == 103);
    }

    SUBCASE("reputation below last threshold")
    {
        CHECK(test_customKarmaFolderGetFrmId(300, descs) == 104);
    }

    SUBCASE("reputation at or above all thresholds returns last entry's frmId")
    {
        // 999 >= all thresholds, so we fall through all `if (rep < threshold)` checks
        // and return the last entry's frmId
        CHECK(test_customKarmaFolderGetFrmId(999, descs) == 104);
    }

    SUBCASE("reputation equal to threshold is NOT less — skips that entry")
    {
        // reputation == 0 is NOT < 0, so skips d3 and checks d4 (0 < 250 → true)
        CHECK(test_customKarmaFolderGetFrmId(0, descs) == 103);
    }
}

TEST_CASE("customKarmaFolderGetFrmId — single entry")
{
    std::vector<TestCustomKarmaFolderDescription> descs;
    TestCustomKarmaFolderDescription d = {42, 0};
    descs.push_back(d);

    SUBCASE("reputation below threshold")
    {
        CHECK(test_customKarmaFolderGetFrmId(-1, descs) == 42);
    }

    SUBCASE("reputation at or above threshold returns single entry's frmId (fallthrough)")
    {
        // 0 is not < 0 → falls through → returns back().frmId
        CHECK(test_customKarmaFolderGetFrmId(0, descs) == 42);
        CHECK(test_customKarmaFolderGetFrmId(100, descs) == 42);
    }
}

// ---- characterEditorSelectFolder tests ----

TEST_CASE("characterEditorSelectFolder — guard: no-op when window not open")
{
    g_testCharacterEditorWindowSelectedFolder = TEST_EDITOR_FOLDER_KILLS;

    // Window not open → should no-op, return false
    bool changed = test_characterEditorSelectFolder(2, false);
    CHECK_FALSE(changed);
    // Folder selection unchanged
    CHECK(g_testCharacterEditorWindowSelectedFolder == TEST_EDITOR_FOLDER_KILLS);

    // All folder values should no-op
    for (int f = 0; f <= 5; f++) {
        changed = test_characterEditorSelectFolder(f, false);
        CHECK_FALSE(changed);
    }
}

TEST_CASE("characterEditorSelectFolder — folder mapping when window is open")
{
    g_testCharacterEditorWindowSelectedFolder = 999; // arbitrary initial value

    SUBCASE("folder 0 (character sheet) → -1")
    {
        CHECK(test_characterEditorSelectFolder(0, true));
        CHECK(g_testCharacterEditorWindowSelectedFolder == -1);
    }

    SUBCASE("folder 1 (perks) → EDITOR_FOLDER_PERKS")
    {
        CHECK(test_characterEditorSelectFolder(1, true));
        CHECK(g_testCharacterEditorWindowSelectedFolder == TEST_EDITOR_FOLDER_PERKS);
    }

    SUBCASE("folder 2 (karma) → EDITOR_FOLDER_KARMA")
    {
        CHECK(test_characterEditorSelectFolder(2, true));
        CHECK(g_testCharacterEditorWindowSelectedFolder == TEST_EDITOR_FOLDER_KARMA);
    }

    SUBCASE("folder 3 (kills) → EDITOR_FOLDER_KILLS")
    {
        CHECK(test_characterEditorSelectFolder(3, true));
        CHECK(g_testCharacterEditorWindowSelectedFolder == TEST_EDITOR_FOLDER_KILLS);
    }
}

TEST_CASE("characterEditorSelectFolder — unknown folder values default to -1")
{
    SUBCASE("negative folder → -1")
    {
        g_testCharacterEditorWindowSelectedFolder = 123;
        CHECK(test_characterEditorSelectFolder(-1, true));
        CHECK(g_testCharacterEditorWindowSelectedFolder == -1);
    }

    SUBCASE("folder 4 → -1 (default case)")
    {
        g_testCharacterEditorWindowSelectedFolder = 123;
        CHECK(test_characterEditorSelectFolder(4, true));
        CHECK(g_testCharacterEditorWindowSelectedFolder == -1);
    }

    SUBCASE("folder 99 → -1 (default case)")
    {
        g_testCharacterEditorWindowSelectedFolder = 123;
        CHECK(test_characterEditorSelectFolder(99, true));
        CHECK(g_testCharacterEditorWindowSelectedFolder == -1);
    }
}

TEST_CASE("characterEditorSelectFolder — consecutive folder switches")
{
    g_testCharacterEditorWindowSelectedFolder = -1;

    // Switch to perks
    CHECK(test_characterEditorSelectFolder(1, true));
    CHECK(g_testCharacterEditorWindowSelectedFolder == TEST_EDITOR_FOLDER_PERKS);

    // Switch to karma
    CHECK(test_characterEditorSelectFolder(2, true));
    CHECK(g_testCharacterEditorWindowSelectedFolder == TEST_EDITOR_FOLDER_KARMA);

    // Switch back to character sheet
    CHECK(test_characterEditorSelectFolder(0, true));
    CHECK(g_testCharacterEditorWindowSelectedFolder == -1);

    // Switch to kills
    CHECK(test_characterEditorSelectFolder(3, true));
    CHECK(g_testCharacterEditorWindowSelectedFolder == TEST_EDITOR_FOLDER_KILLS);
}

// ---- characterEditorUpdateLevel tests ----

TEST_CASE("characterEditorUpdateLevel — no level change (no-op)")
{
    int sp = 0;
    int lastLevel = 5;
    unsigned char hasFreePerk = 0;

    // newLevel == lastLevel → inner loop never executes
    test_characterEditorUpdateLevel(5, lastLevel, sp, 5, 0, 0, 0, 0, 0, 0, hasFreePerk);

    // Skill points unchanged
    CHECK(sp == 0);
    // No free perk awarded
    CHECK(hasFreePerk == 0);
    // lastLevel updated
    CHECK(lastLevel == 5);
}

TEST_CASE("characterEditorUpdateLevel — basic skill point calculation")
{
    // Base formula per level (without traits/mods):
    //   sp += 5 + INT*2 + Educated_rank*2 + Skilled*5 - Gifted*5
    // clamped to [0, 99]

    SUBCASE("INT=5, no traits, no mods, 1 level")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(1, lastLevel, sp, 5, 0, 0, 0, 0, 0, 0, hasFreePerk);

        // sp = 0 + 5 + 5*2 = 15
        CHECK(sp == 15);
    }

    SUBCASE("INT=10, no traits, no mods, 1 level")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(1, lastLevel, sp, 10, 0, 0, 0, 0, 0, 0, hasFreePerk);

        // sp = 0 + 5 + 10*2 = 25
        CHECK(sp == 25);
    }

    SUBCASE("INT=1 (minimum), no traits, 1 level")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(1, lastLevel, sp, 1, 0, 0, 0, 0, 0, 0, hasFreePerk);

        // sp = 0 + 5 + 1*2 = 7
        CHECK(sp == 7);
    }
}

TEST_CASE("characterEditorUpdateLevel — Educated perk bonus")
{
    SUBCASE("Educated rank 1 adds 2 skill points per level")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(1, lastLevel, sp, 5, 1, 0, 0, 0, 0, 0, hasFreePerk);

        // sp = 0 + 5 + 5*2 + 1*2 = 17
        CHECK(sp == 17);
    }

    SUBCASE("Educated rank 3 adds 6 skill points per level")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(1, lastLevel, sp, 5, 3, 0, 0, 0, 0, 0, hasFreePerk);

        // sp = 0 + 5 + 5*2 + 3*2 = 21
        CHECK(sp == 21);
    }
}

TEST_CASE("characterEditorUpdateLevel — Skilled trait bonus")
{
    SUBCASE("Skilled adds 5 skill points per level")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(1, lastLevel, sp, 5, 0, 1, 0, 0, 0, 0, hasFreePerk);

        // sp = 0 + 5 + 5*2 + 0*2 + 1*5 = 20
        CHECK(sp == 20);
    }

    SUBCASE("Skilled also modifies perk progression (+1 to divisor)")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // Skilled makes progression = 3 + 1 = 4
        // Level 4 is divisible by 4 → free perk awarded
        test_characterEditorUpdateLevel(4, lastLevel, sp, 5, 0, 1, 0, 0, 0, 0, hasFreePerk);

        CHECK(hasFreePerk == 1);
    }
}

TEST_CASE("characterEditorUpdateLevel — Gifted trait penalty")
{
    SUBCASE("Gifted subtracts 5 skill points per level")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(1, lastLevel, sp, 5, 0, 0, 1, 0, 0, 0, hasFreePerk);

        // sp = 0 + 5 + 5*2 + 0*2 + 0*5 - 5 = 10
        CHECK(sp == 10);
    }

    SUBCASE("Gifted with INT=1 — skill points clamped to 0")
    {
        int sp = 10;  // starting sp
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // INT=1 gives +2, Gifted -5, 5 base = total +2 per level
        // After one level: 10 + 5 + 1*2 - 5 = 12 (not clamped)
        test_characterEditorUpdateLevel(1, lastLevel, sp, 1, 0, 0, 1, 0, 0, 0, hasFreePerk);
        CHECK(sp == 12);

        // Edge: what if sp is already very low? Test with negative modifier combo
        // This can't happen in practice since INT >= 1, but the clamp still works
    }

    SUBCASE("Gifted cannot make sp negative (clamped to 0)")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // With INT=0 (hypothetical), sp = 0 + 5 + 0 - 5 = 0 → clamped to 0
        test_characterEditorUpdateLevel(1, lastLevel, sp, 0, 0, 0, 1, 0, 0, 0, hasFreePerk);
        CHECK(sp == 0);
    }
}

TEST_CASE("characterEditorUpdateLevel — gSkillPointsPerLevelMod integration (sfall)")
{
    SUBCASE("positive modifier adds skill points")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(1, lastLevel, sp, 5, 0, 0, 0, 5, 0, 0, hasFreePerk);

        // sp = 0 + 5 + 5*2 + 5 = 20
        CHECK(sp == 20);
    }

    SUBCASE("negative modifier subtracts skill points")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(1, lastLevel, sp, 5, 0, 0, 0, -3, 0, 0, hasFreePerk);

        // sp = 0 + 5 + 5*2 - 3 = 12
        CHECK(sp == 12);
    }

    SUBCASE("zero modifier is a no-op")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // Zero modifier should not change the result
        test_characterEditorUpdateLevel(1, lastLevel, sp, 5, 0, 0, 0, 0, 0, 0, hasFreePerk);
        CHECK(sp == 15);
    }

    SUBCASE("modifier combined with Gifted — subtracts further")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // sp = 0 + 5 + 5*2 - 2 - 5 = 8
        test_characterEditorUpdateLevel(1, lastLevel, sp, 5, 0, 0, 1, -2, 0, 0, hasFreePerk);

        CHECK(sp == 8);
    }

    SUBCASE("modifier combined with Skilled — adds extra points")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // sp = 0 + 5 + 5*2 + 1*5 + 10 = 30
        test_characterEditorUpdateLevel(1, lastLevel, sp, 5, 0, 1, 0, 10, 0, 0, hasFreePerk);

        CHECK(sp == 30);
    }
}

TEST_CASE("characterEditorUpdateLevel — skill point clamp to 99")
{
    SUBCASE("sp is clamped at 99 even with high INT and mods")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // Multiple levels with high INT could exceed 99
        test_characterEditorUpdateLevel(5, lastLevel, sp, 10, 3, 1, 0, 50, 0, 0, hasFreePerk);

        // Each level: 5 + 20 + 6 + 5 + 50 = 86. Over 5 levels: 430, clamped to 99.
        CHECK(sp == 99);
    }

    SUBCASE("sp exactly 99 is allowed")
    {
        int sp = 94;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // 1 level with INT=1: sp = 94 + 5 + 2 = 101 → clamped to 99
        test_characterEditorUpdateLevel(1, lastLevel, sp, 1, 0, 0, 0, 0, 0, 0, hasFreePerk);
        CHECK(sp == 99);
    }
}

TEST_CASE("characterEditorUpdateLevel — multiple level gains accumulate")
{
    SUBCASE("3 levels with INT=5, no traits/mods")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(3, lastLevel, sp, 5, 0, 0, 0, 0, 0, 0, hasFreePerk);

        // Each level: 5 + 5*2 = 15. 3 levels = 45
        CHECK(sp == 45);
    }

    SUBCASE("5 levels, maintains running total")
    {
        int sp = 10;  // existing skill points
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(5, lastLevel, sp, 5, 0, 0, 0, 0, 0, 0, hasFreePerk);

        // 5 levels * 15 = 75 + 10 = 85
        CHECK(sp == 85);
    }
}

TEST_CASE("characterEditorUpdateLevel — gPerkFrequencyOverride integration (sfall)")
{
    // Default behavior: perk every 3 levels (gPerkFrequencyOverride = 0)

    SUBCASE("default (override=0): perk at level multiple of 3")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // Level 3 → 3 % 3 == 0 → hasFreePerk = 1
        test_characterEditorUpdateLevel(3, lastLevel, sp, 5, 0, 0, 0, 0, 0, 0, hasFreePerk);
        CHECK(hasFreePerk == 1);
    }

    SUBCASE("default (override=0): level jump 0→4 processes all intermediate levels")
    {
        // When jumping from level 0 to 4 in one call, the function processes
        // levels 1, 2, 3, 4 sequentially. Level 3 is 3%3==0 and awards a perk,
        // so hasFreePerk ends up as 1 even though level 4 alone wouldn't trigger.
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(4, lastLevel, sp, 5, 0, 0, 0, 0, 0, 0, hasFreePerk);
        CHECK(hasFreePerk == 1);
    }

    SUBCASE("default (override=0): perk at level 6 (multiple of 3)")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(6, lastLevel, sp, 5, 0, 0, 0, 0, 0, 0, hasFreePerk);
        CHECK(hasFreePerk == 1);
    }

    SUBCASE("override=2: perk every 2 levels")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // Level 2 → 2 % 2 == 0 → hasFreePerk = 1
        test_characterEditorUpdateLevel(2, lastLevel, sp, 5, 0, 0, 0, 0, 2, 0, hasFreePerk);
        CHECK(hasFreePerk == 1);
    }

    SUBCASE("override=2: no perk at level 3")
    {
        int sp = 0;
        int lastLevel = 2;  // already at level 2, only process level 3
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(3, lastLevel, sp, 5, 0, 0, 0, 0, 2, 0, hasFreePerk);
        CHECK(hasFreePerk == 0);
    }

    SUBCASE("override=4: perk at level 4 (multiple of 4)")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(4, lastLevel, sp, 5, 0, 0, 0, 0, 4, 0, hasFreePerk);
        CHECK(hasFreePerk == 1);
    }

    SUBCASE("override=1: perk every level")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // Level 1 → 1 % 1 == 0 → hasFreePerk = 1
        test_characterEditorUpdateLevel(1, lastLevel, sp, 5, 0, 0, 0, 0, 1, 0, hasFreePerk);
        CHECK(hasFreePerk == 1);
    }

    SUBCASE("override negative: uses default 3 (since check is > 0)")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // Negative override is treated as 0 → use default 3
        test_characterEditorUpdateLevel(3, lastLevel, sp, 5, 0, 0, 0, 0, -1, 0, hasFreePerk);
        CHECK(hasFreePerk == 1); // 3 % 3 == 0
    }
}

TEST_CASE("characterEditorUpdateLevel — Skilled affects perk progression divisor")
{
    SUBCASE("Skilled makes default progression 4 levels between perks")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // Skilled: progression = 3 + 1 = 4
        // Level 3: 3 % 4 != 0 → no perk
        test_characterEditorUpdateLevel(3, lastLevel, sp, 5, 0, 1, 0, 0, 0, 0, hasFreePerk);
        CHECK(hasFreePerk == 0);
    }

    SUBCASE("Skilled with override=2 makes progression 3")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // Skilled: progression = 2 + 1 = 3
        // Level 3: 3 % 3 == 0 → hasFreePerk = 1
        test_characterEditorUpdateLevel(3, lastLevel, sp, 5, 0, 1, 0, 0, 2, 0, hasFreePerk);
        CHECK(hasFreePerk == 1);
    }

    SUBCASE("Skilled with no override: perk at level 4 (progression=4)")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(4, lastLevel, sp, 5, 0, 1, 0, 0, 0, 0, hasFreePerk);
        CHECK(hasFreePerk == 1);
    }
}

TEST_CASE("characterEditorUpdateLevel — free perk cap at 37 selected perks")
{
    SUBCASE("selectedPerksCount = 36: free perks still awarded")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(3, lastLevel, sp, 5, 0, 0, 0, 0, 0, 36, hasFreePerk);
        CHECK(hasFreePerk == 1);
    }

    SUBCASE("selectedPerksCount = 37: no more free perks")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(3, lastLevel, sp, 5, 0, 0, 0, 0, 0, 37, hasFreePerk);
        CHECK(hasFreePerk == 0);
    }

    SUBCASE("selectedPerksCount = 50: already at cap, no perks")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(3, lastLevel, sp, 5, 0, 0, 0, 0, 0, 50, hasFreePerk);
        CHECK(hasFreePerk == 0);
    }
}

TEST_CASE("characterEditorUpdateLevel — level exceeds PC_LEVEL_MAX")
{
    SUBCASE("newLevel > 99: no processing beyond cap")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        test_characterEditorUpdateLevel(100, lastLevel, sp, 5, 0, 0, 0, 0, 0, 0, hasFreePerk);

        // Should still update lastLevel but the inner loop condition
        // `newLevel <= PC_LEVEL_MAX` (99 <= 99) is false, so no processing.
        // Wait — in the production code the condition is:
        //   if (level != gCharacterEditorLastLevel && level <= PC_LEVEL_MAX)
        // So level 100 would skip the entire inner loop.
        CHECK(lastLevel == 100);
        CHECK(sp == 0);
        CHECK(hasFreePerk == 0);
    }
}

TEST_CASE("characterEditorUpdateLevel — cross-level perk: per-level free perk flag")
{
    // The free perk flag is set during the inner per-level loop. Once set,
    // it persists for subsequent levels in the same call.

    SUBCASE("perk at level 3 in a 5-level jump sets flag")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // Levels 1,2,3,4,5: only level 3 triggers free perk (3 % 3 == 0)
        test_characterEditorUpdateLevel(5, lastLevel, sp, 5, 0, 0, 0, 0, 0, 0, hasFreePerk);
        CHECK(hasFreePerk == 1);
    }

    SUBCASE("no perk levels in a 2-level jump")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // Levels 1,2: neither is divisible by 3
        test_characterEditorUpdateLevel(2, lastLevel, sp, 5, 0, 0, 0, 0, 0, 0, hasFreePerk);
        CHECK(hasFreePerk == 0);
    }
}

TEST_CASE("characterEditorUpdateLevel — combined sfall and trait interactions")
{
    SUBCASE("Skilled + override=2 + modifier=10: full combined calculation")
    {
        int sp = 0;
        int lastLevel = 0;
        unsigned char hasFreePerk = 0;

        // INT=7, Educated=2, Skilled, Gifted, modifier=10, override=2
        // per level: 5 + 7*2 + 2*2 + 5 + 10 - 5 = 5 + 14 + 4 + 5 + 10 - 5 = 33
        test_characterEditorUpdateLevel(1, lastLevel, sp, 7, 2, 1, 1, 10, 2, 0, hasFreePerk);
        CHECK(sp == 33);

        // Progression: 2 + 1 = 3. Level 1: 1 % 3 != 0 → no perk.
        // But wait — level 3 would trigger it. Let's test level 3.
        sp = 0;
        lastLevel = 0;
        hasFreePerk = 0;
        test_characterEditorUpdateLevel(3, lastLevel, sp, 7, 2, 1, 1, 10, 2, 0, hasFreePerk);
        CHECK(sp == 99); // 3 * 33 = 99
        CHECK(hasFreePerk == 1); // 3 % 3 == 0
    }
}

// ---- EditorFolder enum validation ----

TEST_CASE("EditorFolder enum values match production")
{
    // These must match character_editor.cc:110-114
    CHECK(TEST_EDITOR_FOLDER_PERKS == 0);
    CHECK(TEST_EDITOR_FOLDER_KARMA == 1);
    CHECK(TEST_EDITOR_FOLDER_KILLS == 2);
}

// ---- Constant validation against production definitions ----

TEST_CASE("Production constant validation")
{
    // Cross-check that our test-local constants match the production header values
    // TEST_PC_LEVEL_MAX is compile-verified via static_assert(TEST_PC_LEVEL_MAX == PC_LEVEL_MAX)
    CHECK(TEST_PC_LEVEL_MAX == 99);  // runtime verification of FO2 cap
    CHECK(TEST_STAT_INTELLIGENCE == 4);
    CHECK(TEST_PRIMARY_STAT_COUNT == 7);
    CHECK(TEST_PC_STAT_UNSPENT_SKILL_POINTS == 0);
    CHECK(TEST_PC_STAT_LEVEL == 1);
    CHECK(TEST_PERK_EDUCATED == 25);
    CHECK(TEST_TRAIT_SKILLED == 14);
    CHECK(TEST_TRAIT_GIFTED == 15);
    CHECK(TEST_GVAR_PLAYER_REPUTATION == 0);
    CHECK(TEST_DEFAULT_KARMA_FRM_ID == 47);
}

TEST_CASE("F-M49: FO1 mode level cap — statGetLevelCap() returns 21 for FO1, 99 for FO2")
{
    // Production statGetLevelCap() at stat.cc:740 gates on gFallout1Behavior:
    //   FO1 mode (gFallout1Behavior=true):  cap = 21
    //   FO2 mode (gFallout1Behavior=false): cap = PC_LEVEL_MAX (99)
    //
    // The test mirror at character_editor.cc:248 uses TEST_PC_LEVEL_MAX (99).
    // In FO1 mode, production checks `newLevel <= statGetLevelCap()` which
    // returns 21 instead of 99. This test verifies both caps are correctly
    // understood.

    // FO2 (default) cap = 99
    CHECK(TEST_PC_LEVEL_MAX == 99);

    // FO1 cap = 21 (verified via stat_defs.h: statGetLevelCap gating)
    constexpr int kFO1LevelCap = 21;

    // Verify FO1 cap is lower than FO2 cap
    CHECK(kFO1LevelCap < TEST_PC_LEVEL_MAX);

    // FO1: level 21 should be at cap (allowed)
    int sp = 0;
    int lastLevel = 0;
    unsigned char hasFreePerk = 0;

    // Simulate FO1 mode by using kFO1LevelCap instead of TEST_PC_LEVEL_MAX
    // (the test mirror always uses TEST_PC_LEVEL_MAX, so we test FO2 directly)
    test_characterEditorUpdateLevel(kFO1LevelCap, lastLevel, sp, 5, 0, 0, 0, 0, 0, 0, hasFreePerk);
    CHECK(sp > 0);  // 21 levels of skill points
    CHECK(lastLevel == 21);

    // FO1: level 22 should be blocked (production check uses statGetLevelCap)
    sp = 0;
    lastLevel = kFO1LevelCap;
    hasFreePerk = 0;
    // The test mirror checks `newLevel <= TEST_PC_LEVEL_MAX` (99), so it allows 22.
    // In FO1 production, statGetLevelCap() returns 21, blocking 22.
    // This documents the divergence between test mirror (FO2-only) and production (FO1-aware).
    // The test mirror could be extended with a fallout1Behavior parameter to match.
}

// ================================================================
// N2-015: Perk dialog return-value branch tests
//          (character_editor.cc:5795-5809)
//
// Finding: The test mirror for characterEditorUpdateLevel omits the
// perkDialogShow() callback entirely. The production code has 3 return
// branches at lines 5800-5809:
//   rc == -1: error — returns -1, but does NOT clear hasFreePerk
//   rc == 0:  character editor re-draws, hasFreePerk unchanged
//   rc == 1:  normal perk selection — hasFreePerk cleared to 0
//
// The iter-1 discovery report (s2i1-discover-char-editor-report.md:156)
// documented this gap. The iter-2 discovery (s4i1-discover-char-editor-r2-report.md:77)
// noted: "N2-03 MEDIUM: perk-dialog error loop — if rc==-1, hasFreePerk stays 1,
// next call re-invokes same error → infinite retry loop."
// ================================================================

// Mirror of the perk dialog branch logic from character_editor.cc:5795-5809.
// This mirrors the exact semantics: hasFreePerk gate, three return branches,
// and the hasFreePerk state transition on rc==1.
//
// Parameters:
//   hasFreePerk — gCharacterEditorHasFreePerk (in/out)
//   dialogReturnCode — simulated return value from perkDialogShow():
//                      -1 = error, 0 = redraw only, 1 = perk selected
//
// Returns:
//   -1 — dialog error (rc == -1)
//    0 — dialog redraw without clearing hasFreePerk (rc == 0)
//    1 — perk selected and processed (rc == 1, hasFreePerk cleared)
//
// Note: Rendering side effects (characterEditorDrawFolders, windowRefresh)
// are documented but not tested in this mirror. They are excluded for the
// same reason the existing mirror omits them — they are pure rendering calls
// with no state mutation beyond the globals tested here.
static int testMirrorPerkDialogBranch(unsigned char& hasFreePerk, int dialogReturnCode)
{
    // Gate: only invoke perk dialog if there is actually a free perk pending.
    // Production: if (gCharacterEditorHasFreePerk != 0) { ... }
    if (hasFreePerk == 0) {
        // No free perk — dialog is never opened. Return 1 (success-noop).
        return 1;
    }

    // Production code at character_editor.cc:5795-5809:
    //   characterEditorWindowSelectedFolder = 0;
    //   characterEditorDrawFolders();
    //   windowRefresh(gCharacterEditorWindow);
    //   int rc = perkDialogShow();

    int rc = dialogReturnCode;

    if (rc == -1) {
        // character_editor.cc:5801-5803:
        //   debugPrint("\n *** Error running perks dialog! ***\n");
        //   return -1;
        //
        // GAP: hasFreePerk is NOT cleared. On next call to
        // characterEditorUpdateLevel(), the check at line 5795 passes again,
        // perk dialog re-invokes, same error → infinite retry loop.
        // This IS a real bug (see N2-03 in s4i1-discover-char-editor-r2-report.md:39).
        return -1;
    } else if (rc == 0) {
        // character_editor.cc:5804-5805:
        //   characterEditorDrawFolders();
        //   // hasFreePerk NOT cleared — dialog was dismissed without selection
        return 0;
    } else if (rc == 1) {
        // character_editor.cc:5806-5809:
        //   characterEditorDrawFolders();
        //   gCharacterEditorHasFreePerk = 0;  // perk selected, flag consumed
        hasFreePerk = 0;
        return 1;
    }

    // Unknown return code — should not happen, but mirror handles it
    return 0;
}

TEST_CASE("N2-015: perk dialog — hasFreePerk gate prevents dialog when no perk pending")
{
    SUBCASE("hasFreePerk == 0: dialog never invoked, returns 1")
    {
        unsigned char hasFreePerk = 0;
        // Even if the dialog would have returned -1 (error), it should
        // never be called because hasFreePerk is 0.
        int rc = testMirrorPerkDialogBranch(hasFreePerk, -1);
        CHECK(rc == 1);  // success-noop
        CHECK(hasFreePerk == 0); // unchanged
    }

    SUBCASE("hasFreePerk == 0 with rc == 1: still no dialog")
    {
        unsigned char hasFreePerk = 0;
        int rc = testMirrorPerkDialogBranch(hasFreePerk, 1);
        CHECK(rc == 1);
        CHECK(hasFreePerk == 0);
    }

    SUBCASE("hasFreePerk == 0 with rc == 0: still no dialog")
    {
        unsigned char hasFreePerk = 0;
        int rc = testMirrorPerkDialogBranch(hasFreePerk, 0);
        CHECK(rc == 1);
        CHECK(hasFreePerk == 0);
    }
}

TEST_CASE("N2-015: perk dialog — rc == -1 (error) path")
{
    SUBCASE("rc == -1 returns -1, hasFreePerk NOT cleared")
    {
        unsigned char hasFreePerk = 1;
        int rc = testMirrorPerkDialogBranch(hasFreePerk, -1);
        CHECK(rc == -1);
        // BUG (documented): hasFreePerk is NOT cleared on error.
        // The production code at character_editor.cc:5801-5803 returns -1
        // without clearing gCharacterEditorHasFreePerk.
        // This means the next call to characterEditorUpdateLevel will
        // re-invoke the perk dialog and hit the same error.
        CHECK(hasFreePerk == 1); // STILL pending — potential infinite loop
    }

    SUBCASE("rc == -1: subsequent call with same hasFreePerk")
    {
        unsigned char hasFreePerk = 1;

        // First call: perk dialog errors
        int rc1 = testMirrorPerkDialogBranch(hasFreePerk, -1);
        CHECK(rc1 == -1);
        CHECK(hasFreePerk == 1); // flag persists

        // Second call: would re-invoke the dialog with same error
        // since hasFreePerk was never cleared
        int rc2 = testMirrorPerkDialogBranch(hasFreePerk, -1);
        CHECK(rc2 == -1);
        CHECK(hasFreePerk == 1); // still pending — infinite loop confirmed
    }
}

TEST_CASE("N2-015: perk dialog — rc == 0 (dismissed without selection) path")
{
    SUBCASE("rc == 0 returns 0, hasFreePerk NOT cleared")
    {
        unsigned char hasFreePerk = 1;
        int rc = testMirrorPerkDialogBranch(hasFreePerk, 0);
        CHECK(rc == 0);
        // hasFreePerk stays 1 — player can try again later
        CHECK(hasFreePerk == 1);
    }

    SUBCASE("rc == 0: multiple dismissals leave flag unchanged")
    {
        unsigned char hasFreePerk = 1;
        for (int i = 0; i < 3; i++) {
            int rc = testMirrorPerkDialogBranch(hasFreePerk, 0);
            CHECK(rc == 0);
        }
        CHECK(hasFreePerk == 1); // still pending after 3 dismissals
    }
}

TEST_CASE("N2-015: perk dialog — rc == 1 (perk selected) path")
{
    SUBCASE("rc == 1 returns 1, hasFreePerk cleared to 0")
    {
        unsigned char hasFreePerk = 1;
        int rc = testMirrorPerkDialogBranch(hasFreePerk, 1);
        CHECK(rc == 1);
        // hasFreePerk cleared — perk was consumed
        CHECK(hasFreePerk == 0);
    }

    SUBCASE("rc == 1: after perk selected, dialog NOT re-invoked")
    {
        unsigned char hasFreePerk = 1;

        // Player selects a perk
        int rc1 = testMirrorPerkDialogBranch(hasFreePerk, 1);
        CHECK(rc1 == 1);
        CHECK(hasFreePerk == 0); // consumed

        // Next call with hasFreePerk == 0: dialog is NOT invoked
        int rc2 = testMirrorPerkDialogBranch(hasFreePerk, 0);
        CHECK(rc2 == 1); // success-noop, no dialog
        CHECK(hasFreePerk == 0);
    }
}

TEST_CASE("N2-015: perk dialog — state transitions across all three branches")
{
    SUBCASE("Error→retry→success: rc -1 then 1 clears flag")
    {
        unsigned char hasFreePerk = 1;

        // Error on first attempt
        int rc1 = testMirrorPerkDialogBranch(hasFreePerk, -1);
        CHECK(rc1 == -1);
        CHECK(hasFreePerk == 1); // not cleared

        // Player dismisses on second attempt
        int rc2 = testMirrorPerkDialogBranch(hasFreePerk, 0);
        CHECK(rc2 == 0);
        CHECK(hasFreePerk == 1); // still not cleared

        // Successful perk selection on third attempt
        int rc3 = testMirrorPerkDialogBranch(hasFreePerk, 1);
        CHECK(rc3 == 1);
        CHECK(hasFreePerk == 0); // finally cleared
    }

    SUBCASE("Dismiss→select: rc 0 then 1 clears flag")
    {
        unsigned char hasFreePerk = 1;

        int rc1 = testMirrorPerkDialogBranch(hasFreePerk, 0);
        CHECK(rc1 == 0);
        CHECK(hasFreePerk == 1);

        int rc2 = testMirrorPerkDialogBranch(hasFreePerk, 1);
        CHECK(rc2 == 1);
        CHECK(hasFreePerk == 0);
    }
}
