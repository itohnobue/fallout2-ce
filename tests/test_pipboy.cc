// Unit tests for pipboy.cc — quest list bounds checking, pagination edge
// cases, and quest location rendering logic.
//
// Self-contained test — does NOT link pipboy.cc (2,887 LOC with 30+ engine
// dependencies). Mirrors the critical data structures and logic functions
// to validate bounds-checking correctness.
//
// Tests cover confirmed findings:
//   M-081: OOB read at index==gQuestsCount (pipboy.cc:1269)
//   N2-026: startIndex underflow to -1 when zero quests (pipboy.cc:1414)
//   N2-027: realIndex OOB via page-offset miscalculation (pipboy.cc:1176,1199)
//
// Research tiers: N/A (no RPU/ET Tu research overlap with engine-internal
// pipboy quest display code per s1-research-* reports).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

// ============================================================
// Mirrored types and constants from pipboy.cc
// ============================================================

namespace fallout {

// PIPBOY_STATUS_QUEST_LINES from pipboy.cc:78
constexpr int PIPBOY_STATUS_QUEST_LINES = 19;

// Mirror of QuestDescription struct from pipboy.cc:168
typedef struct TestQuestDescription {
    int location;
    int description;
    int gvar;
    int displayThreshold;
    int completedThreshold;
} TestQuestDescription;

} // namespace fallout

using namespace fallout;

// ============================================================
// M-081: Mirrored quest location scanning loop
// ============================================================
// Extracted from pipboy.cc:1226-1269 (pipboyWindowQuestList).
// The original function finds the quest at a selectedLocationIndex
// within the quest locations list. It skips quests sharing the same
// location via a nested skip-loop.
//
// The bug (M-081): When the loop exhausts all quests without finding
// a valid matching location, `index` reaches `gQuestsCount`, and
// `gQuestDescriptions[index]` at line 1269 reads OOB.
//
// This test mirrors the scanning logic only (lines 1226-1244, 1269)
// and returns whether the traversal was safe.

struct QuestScanResult {
    int index;          // Final index after loop
    int gQuestsCount;   // Total quest count
    bool found;         // Whether a matching location was found
};

// Mirrors pipboyWindowQuestList lines 1226-1269 (logic core only)
static QuestScanResult testQuestListScan(
    TestQuestDescription* quests,
    int questsCount,
    int selectedLocationIndex,
    int* globalVars)
{
    int index = 0;
    int validQuestLocationCount = 0;

    for (; index < questsCount; index++) {
        TestQuestDescription* quest = &(quests[index]);

        if (quest->displayThreshold <= globalVars[quest->gvar]) {
            // Check if we have reached the selected quest
            if (validQuestLocationCount == selectedLocationIndex - 1) {
                break;
            }

            validQuestLocationCount += 1;

            // Skip quests in the same location (FORK FIX: gQuestsCount-1)
            for (; index < questsCount - 1; index++) {
                if (quests[index].location != quests[index + 1].location) {
                    break;
                }
            }
        }
    }

    // At line 1269: QuestDescription* questDescription = &(gQuestDescriptions[index]);
    // If index == questsCount, this is OOB.
    QuestScanResult result;
    result.index = index;
    result.gQuestsCount = questsCount;
    result.found = (index < questsCount);
    return result;
}

// ============================================================
// M-081: OOB read index==gQuestsCount tests
// ============================================================

TEST_CASE("M-081: quest list scan — no valid quests causes OOB at index==gQuestsCount")
{
    // Finding M-081 | pipboy.cc:1269 | Research: N/A (engine-internal)
    // When all quests are below displayThreshold or no quests exist,
    // the loop at line 1226 runs to completion and index == gQuestsCount.
    // line 1269 then reads gQuestDescriptions[gQuestsCount] — OOB.

    int vars[10] = {0};

    // 2 quests, both below threshold (displayThreshold=5, GVAR=0)
    TestQuestDescription quests[2] = {
        {0, 101, 0, 5, 0},  // threshold=5 > GVAR[0]=0 → filtered out
        {1, 102, 0, 5, 0},  // also filtered
    };

    QuestScanResult r = testQuestListScan(quests, 2, 1, vars);
    // Loop ran to completion: index == 2 == gQuestsCount
    CHECK(r.found == false);
    // This confirms the OOB scenario: line 1269 reads quests[2] (past end)
    CHECK(r.index == r.gQuestsCount);
}

TEST_CASE("M-081: quest list scan — empty quest list")
{
    // Finding M-081 | pipboy.cc:1269 | Research: N/A
    // gQuestsCount == 0 → loop never executes, index stays 0.
    // But line 1269 still accesses gQuestDescriptions[0], which could
    // be uninitialized (gQuestDescriptions may be nullptr when count is 0).

    int vars[1] = {0};
    QuestScanResult r = testQuestListScan(nullptr, 0, 1, vars);
    CHECK(r.found == false);
    CHECK(r.index == 0);
    // With gQuestsCount==0, accessing quests[0] is OOB
}

TEST_CASE("M-081: quest list scan — valid quest found (normal path)")
{
    // Finding M-081 | pipboy.cc:1269 | Research: N/A
    // Normal case: quest at threshold, index stays valid.

    int vars[10] = {0};
    vars[0] = 10; // GVAR threshold exceeded

    TestQuestDescription quests[1] = {
        {0, 201, 0, 5, 0},  // threshold=5 <= GVAR[0]=10 → valid
    };

    QuestScanResult r = testQuestListScan(quests, 1, 1, vars);
    // Should find the quest and break at validQuestLocationCount == 0
    CHECK(r.found == true);
    CHECK(r.index == 0);
    CHECK(r.index < r.gQuestsCount);
}

TEST_CASE("M-081: quest list scan — single quest with same-location skip (FORK FIX)")
{
    // Finding M-081 | pipboy.cc:1269 (related: fork fix at :1239) | Research: N/A
    // The fork fix changed `index < gQuestsCount` to `index < gQuestsCount - 1`
    // in the inner skip loop. Without the fix, when the last element is reached
    // in the skip loop, gQuestDescriptions[index+1] reads OOB.
    // This test verifies the fixed logic doesn't OOB on the inner loop.

    int vars[10] = {0};
    vars[0] = 10;

    TestQuestDescription quests[3] = {
        {0, 301, 0, 5, 0},  // valid, location 0
        {0, 302, 0, 5, 0},  // same location → should be skipped
        {0, 303, 0, 5, 0},  // same location → should be skipped
    };

    // Ask for selectedLocationIndex=2 (second location group)
    QuestScanResult r = testQuestListScan(quests, 3, 2, vars);
    // After the skip loop, index should be 3 (== gQuestsCount), none found
    // because all quests are in the same location group (only 1 location group)
    CHECK(r.index == 3);
    CHECK(r.index == r.gQuestsCount);
    // This is the OOB scenario — no second location group exists
}

// ============================================================
// N2-026: Mirrored pagination startIndex/endIndex calculation
// ============================================================
// Extracted from pipboy.cc:1408-1418 (pipboyWindowRenderQuestLocationList).
//
// The bug (N2-026): When gPipboyQuestLocationsCount == 0, the guard at
// line 1414 `if (startIndex >= gPipboyQuestLocationsCount)` is true
// (0 >= 0), setting startIndex = 0 - 1 = -1. The display loop at
// line 1424 then accesses gPipboyQuestLocations[-1] (OOB).
//
// Additionally, gPipboyWindowQuestsCurrentPageCount = endIndex - startIndex
// becomes 0 - (-1) = 1 (wrong; should be 0).

struct PaginationResult {
    int startIndex;
    int endIndex;
    int currentPageCount;
};

// Mirrors pipboyWindowRenderQuestLocationList lines 1407-1421
static PaginationResult testPaginationCalc(
    int locationsCount,
    int viewPage)
{
    const int maxPerPage = PIPBOY_STATUS_QUEST_LINES; // 19
    int totalPages = (locationsCount + maxPerPage - 1) / maxPerPage;
    if (totalPages < 1) totalPages = 1; // ceil division yields 0 when count=0

    int startIndex = viewPage * maxPerPage;
    int endIndex = startIndex + maxPerPage;

    // Line 1414-1415: Buggy guard (N2-026)
    if (startIndex >= locationsCount) {
        startIndex = locationsCount - 1; // underflows to -1 when count=0
    }
    // Line 1417-1418
    if (endIndex > locationsCount) {
        endIndex = locationsCount;
    }

    int currentPageCount = endIndex - startIndex;

    PaginationResult r;
    r.startIndex = startIndex;
    r.endIndex = endIndex;
    r.currentPageCount = currentPageCount;
    return r;
}

// Mirrors the FIXED version (correct guard)
static PaginationResult testPaginationCalcFixed(
    int locationsCount,
    int viewPage)
{
    const int maxPerPage = PIPBOY_STATUS_QUEST_LINES;
    int totalPages = (locationsCount + maxPerPage - 1) / maxPerPage;
    if (totalPages < 1) totalPages = 1;

    int startIndex = viewPage * maxPerPage;
    int endIndex = startIndex + maxPerPage;

    // Fixed: guard startIndex from underflowing below 0
    if (startIndex >= locationsCount) {
        if (locationsCount > 0) {
            startIndex = locationsCount - 1;
        } else {
            startIndex = 0; // no underflow when count is 0
            endIndex = 0;
        }
    }
    if (endIndex > locationsCount) {
        endIndex = locationsCount;
    }

    int currentPageCount = endIndex - startIndex;

    PaginationResult r;
    r.startIndex = startIndex;
    r.endIndex = endIndex;
    r.currentPageCount = currentPageCount;
    return r;
}

TEST_CASE("N2-026: startIndex underflow — zero quest locations")
{
    // Finding N2-026 | pipboy.cc:1414 | Research: N/A
    // When gPipboyQuestLocationsCount == 0, startIndex underflows to -1.
    // This is pre-existing, not fork-introduced.

    int locationsCount = 0;
    int viewPage = 0;

    PaginationResult buggy = testPaginationCalc(locationsCount, viewPage);
    // BUG: startIndex == -1
    CHECK(buggy.startIndex == -1);
    // BUG: currentPageCount == 1 when it should be 0
    CHECK(buggy.currentPageCount == 1);

    PaginationResult fixed = testPaginationCalcFixed(locationsCount, viewPage);
    // Fixed: startIndex stays 0
    CHECK(fixed.startIndex == 0);
    CHECK(fixed.endIndex == 0);
    CHECK(fixed.currentPageCount == 0);
}

TEST_CASE("N2-026: startIndex underflow — negative test (normal path)")
{
    // Finding N2-026 | pipboy.cc:1414 | Research: N/A
    // With normal data, startIndex stays valid.

    int locationsCount = 5;
    int viewPage = 0;

    PaginationResult r = testPaginationCalc(locationsCount, viewPage);
    CHECK(r.startIndex == 0);
    CHECK(r.endIndex == 5);
    CHECK(r.currentPageCount == 5);
}

TEST_CASE("N2-026: startIndex underflow — last page boundary")
{
    // Finding N2-026 | pipboy.cc:1414 | Research: N/A
    // View page beyond the last valid page: startIndex >= locationCount

    int locationsCount = 5;
    int viewPage = 1; // startIndex = 19, which is >= 5

    PaginationResult r = testPaginationCalc(locationsCount, viewPage);
    // Guard correct for non-zero count: clamp to last index
    CHECK(r.startIndex == 4); // locationsCount - 1
    CHECK(r.endIndex == 5);
    CHECK(r.currentPageCount == 1);
}

// ============================================================
// N2-027: Mirrored realIndex page-offset calculation
// ============================================================
// Extracted from pipboy.cc:1176, 1199-1200.
//
// The bug (N2-027): realIndex = (_view_page_quest * PIPBOY_STATUS_QUEST_LINES)
// + userInput. The guard at line 1199 checks only userInput <=
// gPipboyQuestLocationsCount, NOT realIndex. When _view_page_quest > 0
// and userInput is at the display limit, realIndex can far exceed
// gPipboyQuestLocationsCount, triggering the OOB at line 1269 (M-081).

struct RealIndexResult {
    int realIndex;
    int userInput;
    int locationsCount;
    int viewPage;
    bool guardPasses; // Would the line-1199 guard pass?
};

static RealIndexResult testRealIndexGuard(
    int userInput,
    int locationsCount,
    int viewPage)
{
    RealIndexResult r;
    r.userInput = userInput;
    r.locationsCount = locationsCount;
    r.viewPage = viewPage;
    r.realIndex = (viewPage * PIPBOY_STATUS_QUEST_LINES) + userInput;
    // Line 1199 guard: only checks userInput, not realIndex
    r.guardPasses = (userInput <= locationsCount);
    return r;
}

TEST_CASE("N2-027: realIndex OOB — page-offset miscalculation")
{
    // Finding N2-027 | pipboy.cc:1176,1199 | Research: N/A
    // With gPipboyQuestLocationsCount=5, page=1, userInput=5:
    // realIndex = 1*19 + 5 = 24, but the guard only checks userInput (5 <= 5 passes)
    // → massive OOB when passed to pipboyWindowQuestList(24)

    RealIndexResult r = testRealIndexGuard(5, 5, 1);
    CHECK(r.guardPasses == true);  // userInput 5 <= locationsCount 5
    CHECK(r.realIndex == 24);       // realIndex is FAR beyond bounds
    CHECK(r.realIndex > r.locationsCount);
}

TEST_CASE("N2-027: realIndex OOB — page 2 extreme")
{
    // Finding N2-027 | pipboy.cc:1176,1199 | Research: N/A
    // page 2, userInput at count limit → realIndex even larger

    RealIndexResult r = testRealIndexGuard(5, 5, 2);
    // guard: 5 <= 5 → passes
    CHECK(r.guardPasses == true);
    // realIndex = 2*19 + 5 = 43
    CHECK(r.realIndex == 43);
    CHECK(r.realIndex > r.locationsCount);
}

TEST_CASE("N2-027: realIndex OOB — mitigated on page 0")
{
    // Finding N2-027 | pipboy.cc:1176,1199 | Research: N/A
    // On page 0 with userInput in bounds, realIndex IS in bounds.
    // This is the normal (non-bug) path and explains why the bug
    // wasn't obvious in single-page testing.

    RealIndexResult r = testRealIndexGuard(3, 10, 0);
    CHECK(r.guardPasses == true);
    CHECK(r.realIndex == 3);
    CHECK(r.realIndex <= r.locationsCount);
}

TEST_CASE("N2-027: realIndex OOB — userInput at boundary with page > 0")
{
    // Finding N2-027 | pipboy.cc:1176,1199 | Research: N/A
    // Even when userInput == locationsCount (the guard boundary),
    // realIndex is OOB on any non-zero page.

    for (int page = 1; page <= 3; page++) {
        RealIndexResult r = testRealIndexGuard(5, 5, page);
        CHECK(r.guardPasses == true);  // guard ALWAYS passes for userInput <= count
        CHECK(r.realIndex > r.locationsCount);
    }
}

// ============================================================
// Cross-finding integration: N2-026 + M-081 combo
// ============================================================

TEST_CASE("Integration: N2-026 zero-location + M-081 OOB chain")
{
    // When gQuestsCount==0, N2-026 causes startIndex=-1 in the rendering
    // function. If that function then accesses gPipboyQuestLocations[-1],
    // it's OOB. Independently, M-081 means the quest list scan also
    // OOBs. Both occur in the same zero-quest scenario.
    //
    // This test documents the full crash chain.

    // Simulate gQuestsCount == 0 → gPipboyQuestLocationsCount == 0
    // (the building loop at pipboy.cc:1389 adds nothing)
    int locationsCount = 0;

    // Step 1: N2-026 — pagination underflows
    PaginationResult pagination = testPaginationCalc(locationsCount, 0);
    CHECK(pagination.startIndex == -1);
    // loop at 1424: for (int i = -1; i < 0; i++) ... 1 iteration
    // accesses gPipboyQuestLocations[-1] = OOB

    // Step 2: M-081 — quest list scan with OOB
    int vars[10] = {0};
    TestQuestDescription quests[1] = {};
    QuestScanResult scan = testQuestListScan(quests, 0, 1, vars);
    CHECK(scan.index == scan.gQuestsCount);
    // Accesses quests[0] when count is 0 = OOB

    // Both paths produce OOB reads — the zero-quest scenario is
    // doubly broken. Fixed pagination avoids Step 1 but Step 2
    // (line 1269) still needs a guard.
}

// ============================================================
// Regression: fork fix verification for inner skip loop
// ============================================================

TEST_CASE("Fork fix regression: gQuestsCount-1 prevents inner-loop OOB")
{
    // The fork fix at pipboy.cc:1239 changed the inner skip-loop
    // condition from `index < gQuestsCount` to `index < gQuestsCount - 1`.
    // Without this fix, the loop at the last element would access
    // gQuestDescriptions[gQuestsCount] (one past the end) via index+1.
    //
    // This test verifies that the fixed logic:
    // - Never accesses index+1 when index is the last element
    // - Still correctly skips adjacent same-location quests

    int vars[10] = {0};
    vars[0] = 10;

    TestQuestDescription quests[5] = {
        {0, 401, 0, 5, 0},  // location 0
        {0, 402, 0, 5, 0},  // location 0 → skipped
        {1, 403, 0, 5, 0},  // location 1
        {1, 404, 0, 5, 0},  // location 1 → skipped
        {2, 405, 0, 5, 0},  // location 2 (LAST element)
    };

    // Select location group 3 (index 2 = the last quest)
    QuestScanResult r = testQuestListScan(quests, 5, 3, vars);

    // Should find the quest at index 4 (last element, location 2)
    CHECK(r.found == true);
    CHECK(r.index == 4);
    // index == 4 < gQuestsCount == 5 → safe
    // In the inner skip loop at index 4: condition is 4 < 4 (false) → no OOB
    CHECK(r.index < r.gQuestsCount);

    // Verify no OOB: the fix ensures index+1 (5) is never accessed
    // when index == 4 (the last valid element)
}
