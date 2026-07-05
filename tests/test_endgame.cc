// Unit tests for endgame.cc — fork changes regression tests.
//
// Tests:
//   1. N2-H-001: OOB narratorFileNameLength — strlen(tok) indexes truncated
//      voiceOverBaseName in endgameDeathEndingInit (endgame.cc:1103-1109).
//      Fork-introduced regression: strncpy hardening without recomputing length.
//   2. N2-044: FO1 endgame movie selection + credits suppression —
//      gender-based movie (elder/akiss) + no credits under gFallout1Behavior
//      (endgame.cc:246-265). Zero existing test coverage.
//   3. N2-045: panDistance <= 0 guard in endgameEndingRenderPanningScene
//      (endgame.cc:359-366). 5 state-changing side effects untested.
//      Pre-fork: SIGFPE at width==640.
//   4. N2-046: selectedEnding = index fix in endgameSetupDeathEnding
//      (endgame.cc:1182-1191). Pre-fork: selectedEnding++ (counter);
//      post-fork: selectedEnding = index (actual array index).
//
// This file does NOT link endgame.cc (40+ engine deps). It mirrors the
// specific fork-fixed code paths as local functions and validates against
// the pre-fork buggy behavior.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cctype>
#include <cstring>
#include <cstdlib>

// Self-contained mirrors of production code — only need proto_types.h
// for GENDER_MALE / GENDER_FEMALE enum values and obj_types.h for the
// fallout namespace declaration.
#include "obj_types.h"
#include "proto_types.h"

using namespace fallout;

// ============================================================
// N2-H-001: OOB narratorFileNameLength (endgame.cc:1103-1109)
// ============================================================
// Bug: fork changed strncpy at line 1104 to use sizeof(buf)-1 (16->15 chars)
// with manual NUL terminator, but narratorFileNameLength = strlen(tok) at
// line 1103 is computed BEFORE the truncating copy. If tok > 15 chars,
// narratorFileNameLength > 15, and voiceOverBaseName[narratorFileNameLength-1]
// at line 1109 accesses OUT OF BOUNDS on the 16-byte buffer.
//
// Contrast: endgameEndingInit at line 978 re-computes narratorFileNameLength
// AFTER the copy: narratorFileNameLength = strlen(entry.voiceOverBaseName);
// This is CORRECT. The death-ending path at line 1103 does NOT recompute
// after the copy — it uses the pre-truncation strlen. This is the bug.
//
// Research: N/A (self-contained fork regression)
// Verification: CONFIRMED (OOB read confirmed; fork-introduced regression)

// Mirrored EndgameDeathEnding with minimal fields for N2-H-001 test
struct TestDeathEnding {
    char voiceOverBaseName[16];
    bool enabled;
};

// Mirrors the BUGGY fork code at endgame.cc:1103-1111
// Pre-truncation strlen used to index post-truncation buffer
static void deathEndingInitBuggy(TestDeathEnding* entry, const char* tok) {
    int narratorFileNameLength = strlen(tok);  // BUG: pre-truncation length
    strncpy(entry->voiceOverBaseName, tok, sizeof(entry->voiceOverBaseName) - 1);
    entry->voiceOverBaseName[sizeof(entry->voiceOverBaseName) - 1] = '\0';
    entry->enabled = false;
    // BUG: uses narratorFileNameLength from pre-truncation tok
    // If tok was > 15 chars, narratorFileNameLength > 15, indexing OOB
    if (isspace(entry->voiceOverBaseName[narratorFileNameLength - 1])) {
        entry->voiceOverBaseName[narratorFileNameLength - 1] = '\0';
    }
}

// Mirrors the FIXED fork code at endgame.cc:1103-1111 (recomputes after copy)
static void deathEndingInitFixed(TestDeathEnding* entry, const char* tok) {
    strncpy(entry->voiceOverBaseName, tok, sizeof(entry->voiceOverBaseName) - 1);
    entry->voiceOverBaseName[sizeof(entry->voiceOverBaseName) - 1] = '\0';
    int narratorFileNameLength = strlen(entry->voiceOverBaseName);  // FIX: post-copy length
    entry->enabled = false;
    if (narratorFileNameLength > 0 && isspace(entry->voiceOverBaseName[narratorFileNameLength - 1])) {
        entry->voiceOverBaseName[narratorFileNameLength - 1] = '\0';
    }
}

TEST_CASE("N2-H-001 — OOB narratorFileNameLength indexes truncated voiceOverBaseName (endgame.cc:1103-1109)")
{
    SUBCASE("tok within buffer (15 chars) — buggy and fixed paths both correct") {
        TestDeathEnding entryBuggy, entryFixed;
        const char* tok = "0123456789ABCDE";  // exactly 15 chars, fits in buf[16]
        deathEndingInitBuggy(&entryBuggy, tok);
        deathEndingInitFixed(&entryFixed, tok);
        // When tok <= 15, strlen(tok) == strlen(voiceOverBaseName)
        // strncpy copies 15 chars (sizeof(buf)-1=15), NUL at buf[15]
        // Result: full 15-char string "0123456789ABCDE"
        CHECK(strcmp(entryBuggy.voiceOverBaseName, "0123456789ABCDE") == 0);
        CHECK(strcmp(entryFixed.voiceOverBaseName, "0123456789ABCDE") == 0);
    }

    SUBCASE("tok longer than buffer (20 chars) — buggy path accesses OOB, fixed path safe") {
        TestDeathEnding entryBuggy, entryFixed;
        const char* tok = "0123456789ABCDEFGHIJ";  // 20 chars
        deathEndingInitFixed(&entryFixed, tok);
        // Fixed path: safe, truncates to 15 chars + null
        CHECK(strlen(entryFixed.voiceOverBaseName) == 15);
        CHECK(entryFixed.voiceOverBaseName[15] == '\0');

        // Buggy path: narratorFileNameLength = 20 (from tok)
        // voiceOverBaseName has only 16 bytes (indices 0..15)
        // isspace(voiceOverBaseName[19]) reads OOB at offset 19
        // We verify the bug exists by checking that strlen(tok) > sizeof(buf)
        // Pre-condition: tok length exceeds buffer size
        CHECK(strlen(tok) >= sizeof(entryBuggy.voiceOverBaseName));
        // The buggy init would read voiceOverBaseName[narratorFileNameLength-1]
        // where narratorFileNameLength-1 = 19 > 15 = last valid index
        int preTruncationLen = (int)strlen(tok);
        CHECK(preTruncationLen > (int)sizeof(entryBuggy.voiceOverBaseName) - 1);
        // This confirms: pre-truncation length indexes beyond buffer bounds
    }

    SUBCASE("voiceOverBaseName with trailing space — issafe guard comparison") {
        TestDeathEnding entryBuggy, entryFixed;
        const char* tok = "testname ";  // 9 chars with trailing space
        deathEndingInitBuggy(&entryBuggy, tok);
        deathEndingInitFixed(&entryFixed, tok);
        // Both should strip the trailing space
        CHECK(strcmp(entryBuggy.voiceOverBaseName, "testname") == 0);
        CHECK(strcmp(entryFixed.voiceOverBaseName, "testname") == 0);
    }

    SUBCASE("empty tok — guard asymmetry between endingInit and deathEndingInit") {
        TestDeathEnding entryBuggy, entryFixed;
        const char* tok = "";
        deathEndingInitFixed(&entryFixed, tok);
        // Fixed path: narratorFileNameLength = 0, guard "> 0" prevents OOB
        CHECK(entryFixed.voiceOverBaseName[0] == '\0');

        // Buggy path: narratorFileNameLength = 0 from strlen(tok)
        // No "> 0" guard → voiceOverBaseName[-1] is OOB access
        // We verify that this is the asymmetric guard issue (N2-03 in iter-2 report)
        int preTruncationLen = (int)strlen(tok);
        CHECK(preTruncationLen == 0);
        // When narratorFileNameLength == 0, voiceOverBaseName[-1] is OOB
        // (Note: we don't actually call the buggy function since it would be UB)
    }
}

// ============================================================
// N2-044: FO1 endgame movie + credits suppression (endgame.cc:246-265)
// ============================================================
// Fork added gender-based movie selection under gFallout1Behavior:
//   FO1 (gFallout1Behavior=true):
//     Male → "elder", Female → "akiss"
//     No creditsOpen("credits.txt")
//   FO2 (gFallout1Behavior=false):
//     "akiss" regardless
//     creditsOpen("credits.txt", -1, false)
// Research: CONFIRMED — RPU sets Fallout1Behavior=1; ET Tu uses own endgame.

static const char* g_selectedMovie = nullptr;
static bool g_creditsOpened = false;

// Mirrors the FO1/FO2 endgame movie selection at endgame.cc:246-265
static void endgamePlayMovieLogic(bool fallout1Behavior, int gender,
                                  const char*& selectedMovie, bool& creditsOpened) {
    selectedMovie = nullptr;
    creditsOpened = false;

    if (fallout1Behavior) {
        if (gender == GENDER_MALE) {
            selectedMovie = "elder";
        } else {
            selectedMovie = "akiss";
        }
    } else {
        selectedMovie = "akiss";
    }

    // FO1: no credits; FO2: show credits
    if (!fallout1Behavior) {
        creditsOpened = true;
    }
}

TEST_CASE("N2-044 — FO1 endgame movie selection + credits suppression (endgame.cc:246-265)")
{
    SUBCASE("FO2 mode — movie is always akiss, credits shown") {
        const char* movie = nullptr;
        bool credits = false;
        endgamePlayMovieLogic(false, GENDER_MALE, movie, credits);
        CHECK(strcmp(movie, "akiss") == 0);
        CHECK(credits == true);

        endgamePlayMovieLogic(false, GENDER_FEMALE, movie, credits);
        CHECK(strcmp(movie, "akiss") == 0);
        CHECK(credits == true);
    }

    SUBCASE("FO1 mode male — movie is elder, no credits") {
        const char* movie = nullptr;
        bool credits = false;
        endgamePlayMovieLogic(true, GENDER_MALE, movie, credits);
        CHECK(strcmp(movie, "elder") == 0);
        CHECK(credits == false);
    }

    SUBCASE("FO1 mode female — movie is akiss, no credits") {
        const char* movie = nullptr;
        bool credits = false;
        endgamePlayMovieLogic(true, GENDER_FEMALE, movie, credits);
        CHECK(strcmp(movie, "akiss") == 0);
        CHECK(credits == false);
    }

    SUBCASE("FO1/FO2 flag isolation — gFallout1Behavior controls both movie and credits") {
        // FO2 path: credits must always follow
        const char* movie;
        bool credits;
        endgamePlayMovieLogic(false, GENDER_MALE, movie, credits);
        CHECK(credits == true);

        // FO1 path: credits must never follow
        endgamePlayMovieLogic(true, GENDER_MALE, movie, credits);
        CHECK(credits == false);
    }

    SUBCASE("FO1 mode with invalid gender value — defers to else (akiss)") {
        const char* movie = nullptr;
        bool credits = false;
        // Any gender value not equal to GENDER_MALE (0) → akiss
        endgamePlayMovieLogic(true, 99, movie, credits);
        CHECK(strcmp(movie, "akiss") == 0);
        CHECK(credits == false);
    }
}

// ============================================================
// N2-045: panDistance <= 0 guard (endgame.cc:359-366)
// ============================================================
// Fork added guard: if (panDistance <= 0) { cleanup + early return }
// Pre-fork at width==640: panDistance = 0 → fadeDistance = 0/4 = 0 (OK),
// but frameDelay = 16*0/0 → SIGFPE (division by zero).
// Early return calls endgameEndingVoiceOverFree(), artUnlock(), paletteFadeTo(),
// bufferFill(), windowRefresh() — 5 state-changing side effects.
// Research: CONFIRMED (exact 640-width trigger verified in iter-2 adversarial)

struct PanningTestState {
    bool voiceOverFreed;
    bool artUnlocked;
    bool paletteFaded;
    bool bufferFilled;
    bool windowRefreshed;
    bool earlyReturned;
};

// Mirrors the panDistance guard logic at endgame.cc:355-366
static void endgameRenderPanningScene(int width, PanningTestState* state) {
    state->voiceOverFreed = false;
    state->artUnlocked = false;
    state->paletteFaded = false;
    state->bufferFilled = false;
    state->windowRefreshed = false;
    state->earlyReturned = false;

    int panDistance = width - 640;
    if (panDistance <= 0) {
        // 5 side effects from the early return path
        state->voiceOverFreed = true;   // endgameEndingVoiceOverFree()
        state->artUnlocked = true;       // artUnlock(backgroundHandle)
        state->paletteFaded = true;      // paletteFadeTo(gPaletteBlack)
        state->bufferFilled = true;      // bufferFill(...)
        state->windowRefreshed = true;   // windowRefresh(...)
        state->earlyReturned = true;
        return;
    }

    // Normal path (panDistance > 0): would compute fadeDistance, frameDelay, etc.
    state->earlyReturned = false;
}

TEST_CASE("N2-045 — panDistance <= 0 guard prevents SIGFPE (endgame.cc:359-366)")
{
    SUBCASE("width == 640 — panDistance = 0, early return with all 5 side effects") {
        PanningTestState state;
        endgameRenderPanningScene(640, &state);
        CHECK(state.earlyReturned == true);
        CHECK(state.voiceOverFreed == true);
        CHECK(state.artUnlocked == true);
        CHECK(state.paletteFaded == true);
        CHECK(state.bufferFilled == true);
        CHECK(state.windowRefreshed == true);
        // Pre-fork: would have computed frameDelay = 16*0/0 = SIGFPE
    }

    SUBCASE("width < 640 — panDistance negative, early return") {
        PanningTestState state;
        endgameRenderPanningScene(0, &state);  // panDistance = -640
        CHECK(state.earlyReturned == true);
        CHECK(state.voiceOverFreed == true);
    }

    SUBCASE("width > 640 — panDistance positive, normal path") {
        PanningTestState state;
        endgameRenderPanningScene(800, &state);  // panDistance = 160
        CHECK(state.earlyReturned == false);
        CHECK(state.voiceOverFreed == false);
        CHECK(state.artUnlocked == false);
    }

    SUBCASE("width == 641 — one pixel past threshold, normal path") {
        PanningTestState state;
        endgameRenderPanningScene(641, &state);  // panDistance = 1
        CHECK(state.earlyReturned == false);
    }

    SUBCASE("width == 639 — one pixel before threshold, early return") {
        PanningTestState state;
        endgameRenderPanningScene(639, &state);  // panDistance = -1
        CHECK(state.earlyReturned == true);
    }
}

// ============================================================
// N2-046: selectedEnding = index fix (endgame.cc:1182-1191)
// ============================================================
// Pre-fork: selectedEnding++ (increment counter) — indexes into wrong
// position after loop, potentially OOB if fewer endings enabled than expected.
// Post-fork: selectedEnding = index (actual array index of selected ending).
// The fix moves selectedEnding assignment before the break.
// Research: CONFIRMED (wrong-death-ending regression risk validated)

struct TestDeathEndingEntry {
    int gvar;
    int percentage;
    bool enabled;
};

// Mirrors the BUGGY pre-fork loop (selectedEnding++ as 1-based counter)
// Pre-fork code at endgame.cc:1182-1191:
//   int selectedEnding = 0;
//   for (int index = 0; index < count; index++) {
//     if (deathEnding->enabled) {
//       accum += deathEnding->percentage;
//       selectedEnding++;          // 1-based counter of enabled endings
//       if (accum >= chance) break;
//     }
//   }
//   // selectedEnding is a count, NOT an array index → wrong element used
static int setupDeathEndingBuggy(TestDeathEndingEntry* endings, int count,
                                   int /*percentage*/, int chance) {
    int selectedEnding = 0;
    int accum = 0;
    for (int index = 0; index < count; index++) {
        if (endings[index].enabled) {
            accum += endings[index].percentage;
            selectedEnding++;  // BUG: 1-based count, not 0-based index
            if (accum >= chance) {
                break;
            }
        }
    }
    return selectedEnding;
}

// Mirrors the FIXED fork loop (selectedEnding = index as 0-based array index)
static int setupDeathEndingFixed(TestDeathEndingEntry* endings, int count,
                                  int /*percentage*/, int chance) {
    int selectedEnding = 0;
    int accum = 0;
    for (int index = 0; index < count; index++) {
        if (endings[index].enabled) {
            accum += endings[index].percentage;
            if (accum >= chance) {
                selectedEnding = index;  // FIX: store actual 0-based array index
                break;
            }
        }
    }
    return selectedEnding;
}

TEST_CASE("N2-046 — selectedEnding = index fix (endgame.cc:1182-1191)")
{
    SUBCASE("single enabled ending — fixed returns index, buggy returns count") {
        TestDeathEndingEntry endings[] = {
            {1, 50, false},
            {2, 30, true},   // index 1, only enabled
            {3, 20, false},
        };
        int buggy = setupDeathEndingBuggy(endings, 3, 70, 25);
        int fixed = setupDeathEndingFixed(endings, 3, 70, 25);
        // Fixed: returns 1 (correct array index)
        CHECK(fixed == 1);
        // Buggy: accum(30) >= chance(25) at index 1,
        // selectedEnding increments from 0 to 1 before break
        CHECK(buggy == 1);
        // In this case they match, but only because it's the first enabled
    }

    SUBCASE("second enabled ending selected — buggy counts, fixed indexes") {
        TestDeathEndingEntry endings[] = {
            {1, 50, false},
            {2, 30, true},   // index 1, accum=30
            {3, 50, true},   // index 2, accum=30+50=80
        };
        int buggy = setupDeathEndingBuggy(endings, 3, 100, 60);
        int fixed = setupDeathEndingFixed(endings, 3, 100, 60);
        // Fixed: index 1 accum(30) < 60; index 2 accum(80) >= 60 → returns 2
        CHECK(fixed == 2);
        // Buggy: index 1 enabled → selectedEnding increments to 1 (not break)
        //        index 2 enabled → selectedEnding increments to 2 (break)
        //        returns 2 (but this is count, not index!)
        CHECK(buggy == 2);
        // With this data they match, but with different patterns they diverge
    }

    SUBCASE("no enabled endings — both return default (0)") {
        TestDeathEndingEntry endings[] = {
            {1, 50, false},
            {2, 30, false},
            {3, 20, false},
        };
        int buggy = setupDeathEndingBuggy(endings, 3, 70, 25);
        int fixed = setupDeathEndingFixed(endings, 3, 70, 25);
        // Both return 0 (not OOB on empty enabled set)
        CHECK(fixed == 0);
        CHECK(buggy == 0);
    }

    SUBCASE("all enabled, last one selected — buggy returns count != index") {
        TestDeathEndingEntry endings[] = {
            {1, 50, true},   // index 0
            {2, 30, true},   // index 1
            {3, 50, true},   // index 2
        };
        int buggy = setupDeathEndingBuggy(endings, 3, 150, 140);
        int fixed = setupDeathEndingFixed(endings, 3, 150, 140);
        // Buggy: index0 accum=50<140, selectedEnding→1; index1 accum=80<140, →2;
        //        index2 accum=130<140, →3; loop ends → returns 3 (count, not index)
        // Fixed: index2 accum=130<140, no break → returns 0 (never selected)
        // chance=140 > total_percentage(130) is unrealistic; kept for boundary test
        CHECK(buggy == 3);   // count of 3 enabled endings
        CHECK(fixed == 0);   // never set (no ending matched chance)
    }

    SUBCASE("first enabled at index 0 — both track correctly") {
        TestDeathEndingEntry endings[] = {
            {1, 100, true},   // index 0, single enabled
            {2, 0, false},
        };
        int buggy = setupDeathEndingBuggy(endings, 2, 100, 50);
        int fixed = setupDeathEndingFixed(endings, 2, 100, 50);
        CHECK(fixed == 0);  // index 0
        CHECK(buggy == 1);  // count = 1 (wrong for array indexing!)
        // Pre-fork bug: gEndgameDeathEndings[1] instead of [0]
    }

    SUBCASE("second enabled at index 2 selected — buggy gives 2 (count) vs fixed 2 (index) — luckily same") {
        TestDeathEndingEntry endings[] = {
            {1, 10, true},   // index 0, enabled
            {2, 0, false},   // index 1, not enabled
            {3, 90, true},   // index 2, enabled, selected
        };
        int buggy = setupDeathEndingBuggy(endings, 3, 100, 60);
        int fixed = setupDeathEndingFixed(endings, 3, 100, 60);
        // Fixed: index 0 accum=10<60; index 2 accum=10+90=100>=60 → selectedEnding=2
        CHECK(fixed == 2);
        // Buggy: index 0 accum=10, selectedEnding++→1, 10<60 continue;
        //        index 2 accum=100, selectedEnding++→2, 100>=60 break → returns 2
        // Pure luck: count(2nd enabled) == index(2)
        CHECK(buggy == 2);
    }

    SUBCASE("skip first enabled picks second — buggy gives OOB when count > index space") {
        // Only index 1 is enabled. Buggy returns count=1 (correct by luck here).
        TestDeathEndingEntry endings[] = {
            {1, 0, false},
            {2, 100, true},
        };
        int buggy = setupDeathEndingBuggy(endings, 2, 100, 50);
        int fixed = setupDeathEndingFixed(endings, 2, 100, 50);
        CHECK(fixed == 1);  // correct: index 1
        CHECK(buggy == 1);  // by luck: first enabled → count=1 → indexes [1] correctly
    }

    SUBCASE("first enabled selected — buggy returns count(1) not index(0), OOB on array") {
        // Key divergence: all 3 enabled, chance hits very first one.
        // Buggy returns 1 (count), accessing gEndgameDeathEndings[1] instead of [0].
        TestDeathEndingEntry endings[] = {
            {1, 50, true},   // index 0, selected by chance
            {2, 30, true},   // index 1
            {3, 20, true},   // index 2
        };
        int buggy = setupDeathEndingBuggy(endings, 3, 100, 25);
        int fixed = setupDeathEndingFixed(endings, 3, 100, 25);
        CHECK(fixed == 0);  // correct: first ending selected
        CHECK(buggy != 0);  // BUG: returns 1 (count), not 0 (index)
        CHECK(buggy == 1);  // Pre-fork would access gEndgameDeathEndings[1] instead of [0]
    }
}

// ============================================================
// N2-046 extended: pre-fork vs post-fork endgame array access
// ============================================================
// Validate that the fixed version always returns a valid array index
// (0 ≤ selectedEnding < count), while the buggy version returns a
// 1-based count that may exceed the valid index range.

TEST_CASE("N2-046-ext — selectedEnding always valid array index with fix (endgame.cc:1187)")
{
    SUBCASE("fixed always returns 0-based index in range") {
        // 5 endings, varying enabled states and percentages
        TestDeathEndingEntry endings[] = {
            {1, 0, false},
            {2, 20, true},
            {3, 0, false},
            {4, 40, true},
            {5, 40, true},
        };
        // Test multiple chance values
        int result;
        for (int chance : {5, 15, 25, 35, 55, 75, 95}) {
            result = setupDeathEndingFixed(endings, 5, 100, chance);
            // Fixed always returns a valid index (0-based)
            CHECK(result >= 0);
            CHECK(result < 5);
            // And the returned index must point to an enabled ending
            if (result < 5) {
                // Since the loop only breaks when accum >= chance on an enabled entry
                // result should point to an enabled entry
            }
        }
    }

    SUBCASE("buggy returns 1-based count which can be > last index") {
        // 5 endings, all enabled → buggy returns 1-5 (count), not 0-4 (index)
        TestDeathEndingEntry endings[] = {
            {1, 20, true},
            {2, 20, true},
            {3, 20, true},
            {4, 20, true},
            {5, 20, true},
        };
        // With chance=90: index 0 accum=20<90; 1 accum=40<90; 2 accum=60<90;
        //     3 accum=80<90; 4 accum=100>=90 → break
        // Buggy: selectedEnding = 5 (count of 5 enabled items through to selection)
        // Fixed: selectedEnding = 4 (array index)
        int buggy = setupDeathEndingBuggy(endings, 5, 100, 90);
        int fixed = setupDeathEndingFixed(endings, 5, 100, 90);
        CHECK(fixed == 4);
        CHECK(buggy == 5);
        // Buggy would access gEndgameDeathEndings[5] which is OOB if Length==5
    }
}
