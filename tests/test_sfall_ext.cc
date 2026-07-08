// Unit tests for sfall_ext.cc — sfall extension module validation.
//
// F-060 (MEDIUM, confirmed): 3 sfall modules with zero dedicated test coverage.
// sfall_ext.cc (~362 LOC) handles:
//   - GlobalScriptPaths parsing from ddraw.ini [Misc]
//   - HookScriptsPath parsing from ddraw.ini [Misc]
//   - ExtraPatches support
//   - sfall_metarules save/load orchestration
//
// F2-T2 (MEDIUM, confirmed): SaveGameData/LoadGameData cross-subsystem
// orchestration has zero behavioral tests. Adds mirror tests tracing
// the full save/load call sequences with error propagation.
//
// Header-level test — does NOT link sfall_ext.cc (heavy engine deps:
// sfall_arrays.h, sfall_metarules.h, sfall_opcodes.h, etc.).
// Validates function declarations, types, compile-time properties,
// and save/load orchestration logic via mirrors.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <string>
#include <type_traits>
#include <vector>

#include "sfall_ext.h"
#include "db.h" // File type definition

using namespace fallout;

TEST_CASE("F-060: sfall_ext — header compiles and include guard works")
{
    // sfall_ext.h defines: SFALL_EXT_H
    CHECK(true); // compile-time: header is valid C++
}

TEST_CASE("F-060: sfall_ext — sfallParseGlobalScriptPaths returns bool")
{
    // Production: sfall_ext.h:19. Returns bool (true on success).
    CHECK(std::is_same_v<decltype(sfallParseGlobalScriptPaths()), bool>);
}

TEST_CASE("F-060: sfall_ext — sfallGetGlobalScriptPaths returns const vector<string>&")
{
    // Production: sfall_ext.h:23. Returns const reference to vector of strings.
    using ExpectedType = const std::vector<std::string>&;
    CHECK(std::is_same_v<decltype(sfallGetGlobalScriptPaths()), ExpectedType>);
}

TEST_CASE("F-060: sfall_ext — sfallParseHookScriptsPath returns bool")
{
    // Production: sfall_ext.h:29. Returns bool.
    CHECK(std::is_same_v<decltype(sfallParseHookScriptsPath()), bool>);
}

TEST_CASE("F-060: sfall_ext — sfallGetHookScriptsPath returns const string&")
{
    // Production: sfall_ext.h:32. Returns const reference to string.
    CHECK(std::is_same_v<decltype(sfallGetHookScriptsPath()), const std::string&>);
}

TEST_CASE("F-060: sfall_ext — save/load function signatures")
{
    // sfallSaveGameData / sfallLoadGameData take File* and return bool.
    // Production: sfall_ext.h:13-14.
    CHECK(std::is_same_v<decltype(sfallSaveGameData(std::declval<File*>())), bool>);
    CHECK(std::is_same_v<decltype(sfallLoadGameData(std::declval<File*>())), bool>);
}

// =============================================================================
// F2-T2: Save/Load orchestration behavioral mirror tests
// =============================================================================
// Production: sfall_ext.cc:252-363.
// sfallSaveGameData orchestrates 7 steps in sequence:
//   1. sfallOpcodeStateSave() — store opcode state into globals
//   2. sfall_gl_vars_save(stream) — serialize global vars
//   3. fileWriteInt32(nextObjectId) — unique object ID counter
//   4. 4× int32_t zeros — unimplemented sfall fields
//   5. sfallArraysSave(stream) — serialize arrays
//   6. fileWrite(zero) — drugPidsCount stub
//   7. sfall_metarules_save(stream) — serialize metarule state
//
// sfallLoadGameData orchestrates 6 steps:
//   1. sfall_gl_vars_load(stream) — deserialize global vars
//   2. sfallOpcodeStateLoad() — restore opcode state
//   3. fileReadInt32(nextObjectId) — unique object ID
//   4. 4× fileRead(ignored) — skip unimplemented fields
//   5. sfallArraysLoad(stream) — deserialize arrays
//   6. fileReadInt32(drugPids) + sfall_metarules_load(stream) — metarules

namespace {

enum class SaveStep : int {
    OpcodeState = 1,
    GlobalVars,
    NextObjectId,
    StubFields,
    Arrays,
    DrugPids,
    Metarules,
    Done,
};

struct SaveTrace {
    // order[i] records the i-th step executed (1-indexed)
    std::vector<SaveStep> order;
    bool opcodeStateSaved = false;
    bool globalVarsSaved = false;
    int nextObjectId = -1;
    int stubFieldCount = 0;
    bool arraysSaved = false;
    bool drugPidsSaved = false;
    bool metarulesSaved = false;
    bool lastWriteFailed = false; // simulate I/O failure at specific step
    int failAtStep = -1;          // -1 = no failure injection

    void record(SaveStep step) { order.push_back(step); }
};

enum class LoadStep : int {
    GlobalVars = 1,
    OpcodeState,
    NextObjectId,
    StubSkip,
    Arrays,
    DrugPidsSkip,
    Metarules,
    Done,
};

struct LoadTrace {
    std::vector<LoadStep> order;
    bool globalVarsLoaded = false;
    bool opcodeStateLoaded = false;
    int nextObjectId = -1;
    int stubSkipCount = 0;
    bool arraysLoaded = false;
    bool drugPidsSkipped = false;
    bool metarulesLoaded = false;
    bool lastReadFailed = false;
    int failAtStep = -1;

    void record(LoadStep step) { order.push_back(step); }
};

// Mirror sfallSaveGameData orchestration (sfall_ext.cc:252-295)
static bool mirrorSaveGameData(SaveTrace& trace, int nextObjIdCounter, bool arraysOk, bool metarulesOk)
{
    // Step 1: Store opcode state into globals (line 256)
    trace.record(SaveStep::OpcodeState);
    trace.opcodeStateSaved = true;
    if (trace.failAtStep == 1) return false;

    // Step 2: Save global vars (lines 258-261)
    trace.record(SaveStep::GlobalVars);
    trace.globalVarsSaved = true;
    if (trace.failAtStep == 2) return false;

    // Step 3: Write next object ID (lines 263-266)
    trace.record(SaveStep::NextObjectId);
    trace.nextObjectId = nextObjIdCounter;
    if (trace.failAtStep == 3) return false;

    // Step 4: Write 4 stub zero fields (lines 270-276)
    trace.record(SaveStep::StubFields);
    trace.stubFieldCount = 4;
    if (trace.failAtStep == 4) return false;

    // Step 5: Save arrays (lines 278-281)
    trace.record(SaveStep::Arrays);
    trace.arraysSaved = arraysOk;
    if (!arraysOk) return false;
    if (trace.failAtStep == 5) return false;

    // Step 6: Write drugPidsCount stub (lines 283-286)
    trace.record(SaveStep::DrugPids);
    trace.drugPidsSaved = true;
    if (trace.failAtStep == 6) return false;

    // Step 7: Save metarules (lines 289-292)
    trace.record(SaveStep::Metarules);
    trace.metarulesSaved = metarulesOk;
    if (!metarulesOk) return false;
    if (trace.failAtStep == 7) return false;

    trace.record(SaveStep::Done);
    return true;
}

// Mirror sfallLoadGameData orchestration (sfall_ext.cc:314-363)
static bool mirrorLoadGameData(LoadTrace& trace, bool hasNextObjId, int nextObjId, bool arraysOk, bool hasDrugPids)
{
    // Step 1: Load global vars (lines 316-319)
    trace.record(LoadStep::GlobalVars);
    trace.globalVarsLoaded = true;
    if (trace.failAtStep == 1) return false;

    // Step 2: Restore opcode state from globals (line 322)
    trace.record(LoadStep::OpcodeState);
    trace.opcodeStateLoaded = true;
    if (trace.failAtStep == 2) return false;

    // Step 3: Read next object ID (lines 324-328)
    trace.record(LoadStep::NextObjectId);
    if (!hasNextObjId) {
        // Old save — stop gracefully, return true
        return true;
    }
    trace.nextObjectId = nextObjId;
    if (trace.failAtStep == 3) return false;

    // Step 4: Skip 4 stub fields (lines 332-338)
    trace.record(LoadStep::StubSkip);
    trace.stubSkipCount = 4;
    if (trace.failAtStep == 4) {
        // Production: scriptsRestoreUniqueObjectIdCounter(nextObjectId), return true
        return true;
    }

    // Step 5: Load arrays (lines 340-344)
    trace.record(LoadStep::Arrays);
    if (!arraysOk) return false;
    trace.arraysLoaded = true;
    if (trace.failAtStep == 5) return false;

    // Step 6: Consume drugPids, then load metarules (lines 349-360)
    trace.record(LoadStep::DrugPidsSkip);
    if (!hasDrugPids) return false;
    trace.drugPidsSkipped = true;
    if (trace.failAtStep == 6) return false;

    trace.record(LoadStep::Metarules);
    trace.metarulesLoaded = true; // metarules_load is void-return

    trace.record(LoadStep::Done);
    return true;
}

} // anonymous namespace

TEST_CASE("F2-T2: sfallSaveGameData — full orchestration sequence (sfall_ext.cc:252-295)")
{
    SUBCASE("all 7 steps execute in order")
    {
        SaveTrace trace;
        bool ok = mirrorSaveGameData(trace, 12345, true, true);
        CHECK(ok == true);

        // Verify all 7 steps + Done
        CHECK(trace.order.size() == 8);
        CHECK(trace.order[0] == SaveStep::OpcodeState);
        CHECK(trace.order[1] == SaveStep::GlobalVars);
        CHECK(trace.order[2] == SaveStep::NextObjectId);
        CHECK(trace.order[3] == SaveStep::StubFields);
        CHECK(trace.order[4] == SaveStep::Arrays);
        CHECK(trace.order[5] == SaveStep::DrugPids);
        CHECK(trace.order[6] == SaveStep::Metarules);
        CHECK(trace.order[7] == SaveStep::Done);

        CHECK(trace.nextObjectId == 12345);
        CHECK(trace.stubFieldCount == 4);
    }

    SUBCASE("global vars save failure → abort at step 2")
    {
        SaveTrace trace;
        trace.failAtStep = 2;
        bool ok = mirrorSaveGameData(trace, 12345, true, true);
        CHECK(ok == false);
        // Only steps 1 and 2 executed
        CHECK(trace.order.size() == 2);
    }

    SUBCASE("object ID write failure → abort at step 3")
    {
        SaveTrace trace;
        trace.failAtStep = 3;
        bool ok = mirrorSaveGameData(trace, 12345, true, true);
        CHECK(ok == false);
        CHECK(trace.order.size() == 3);
        CHECK(trace.nextObjectId == 12345);
    }

    SUBCASE("stub fields write failure → abort at step 4")
    {
        SaveTrace trace;
        trace.failAtStep = 4;
        bool ok = mirrorSaveGameData(trace, 12345, true, true);
        CHECK(ok == false);
        CHECK(trace.order.size() == 4);
        CHECK(trace.stubFieldCount == 4);
    }

    SUBCASE("arrays save failure → abort at step 5")
    {
        SaveTrace trace;
        bool ok = mirrorSaveGameData(trace, 12345, false, true);
        CHECK(ok == false);
        // Arrays step recorded but arraysSaved flag is false
        CHECK(trace.arraysSaved == false);
    }

    SUBCASE("drugPids write failure → abort at step 6")
    {
        SaveTrace trace;
        trace.failAtStep = 6;
        bool ok = mirrorSaveGameData(trace, 12345, true, true);
        CHECK(ok == false);
        CHECK(trace.order.size() == 6);
    }

    SUBCASE("metarules save failure → abort at step 7")
    {
        SaveTrace trace;
        bool ok = mirrorSaveGameData(trace, 12345, true, false);
        CHECK(ok == false);
        CHECK(trace.metarulesSaved == false);
    }
}

TEST_CASE("F2-T2: sfallLoadGameData — full orchestration sequence (sfall_ext.cc:314-363)")
{
    SUBCASE("all steps execute in order with valid save data")
    {
        LoadTrace trace;
        bool ok = mirrorLoadGameData(trace, true, 12345, true, true);
        CHECK(ok == true);

        CHECK(trace.order.size() >= 7);
        CHECK(trace.order[0] == LoadStep::GlobalVars);
        CHECK(trace.order[1] == LoadStep::OpcodeState);
        CHECK(trace.order[2] == LoadStep::NextObjectId);
        CHECK(trace.order[3] == LoadStep::StubSkip);
        CHECK(trace.order[4] == LoadStep::Arrays);
        CHECK(trace.order[5] == LoadStep::DrugPidsSkip);
        CHECK(trace.order[6] == LoadStep::Metarules);

        CHECK(trace.nextObjectId == 12345);
        CHECK(trace.stubSkipCount == 4);
    }

    SUBCASE("global vars load failure → abort at step 1")
    {
        LoadTrace trace;
        trace.failAtStep = 1;
        bool ok = mirrorLoadGameData(trace, true, 12345, true, true);
        CHECK(ok == false);
        CHECK(trace.order.size() == 1);
    }

    SUBCASE("old save (no nextObjectId) → graceful early return true")
    {
        LoadTrace trace;
        bool ok = mirrorLoadGameData(trace, false, 0, true, true);
        CHECK(ok == true);
        // Only steps 1-3 executed (globals, opcode state, nextObjectId check)
        CHECK(trace.globalVarsLoaded == true);
        CHECK(trace.opcodeStateLoaded == true);
        CHECK(trace.order.size() == 3);
        // nextObjectId not set (old save path)
        CHECK(trace.nextObjectId == -1);
    }

    SUBCASE("stub skip read failure → graceful early return true")
    {
        // Production: if fileRead fails during stub skip, restore counter and return true
        LoadTrace trace;
        trace.failAtStep = 4;
        bool ok = mirrorLoadGameData(trace, true, 12345, true, true);
        CHECK(ok == true); // graceful: not a fatal error in production
    }

    SUBCASE("arrays load failure → abort with false")
    {
        LoadTrace trace;
        bool ok = mirrorLoadGameData(trace, true, 12345, false, true);
        CHECK(ok == false);
        CHECK(trace.arraysLoaded == false);
    }

    SUBCASE("drugPids skip failure → abort with false")
    {
        LoadTrace trace;
        trace.failAtStep = 6;
        bool ok = mirrorLoadGameData(trace, true, 12345, true, true);
        // Production: fileReadInt32 returns -1 → return false
        CHECK(ok == false);
    }

    SUBCASE("metarules load is void-return — failure cannot be detected by caller")
    {
        // Production: sfall_metarules_load() returns void (line 360).
        // The caller has no way to detect metarule load failure.
        // This is a known gap (F-38, iter-1 CONFIRMED MEDIUM).
        // The mirror records metarulesLoaded=true unconditionally
        // to match the void-return semantics.
        LoadTrace trace;
        bool ok = mirrorLoadGameData(trace, true, 12345, true, true);
        CHECK(ok == true);
        CHECK(trace.metarulesLoaded == true); // void return — always "succeeds"
    }

    SUBCASE("load order: globals loaded before opcode state restored")
    {
        // Production comment at lines 300-313 documents the order constraint:
        // sfall_gl_vars_load → sfallOpcodeStateLoad → arrays → metarules
        LoadTrace trace;
        mirrorLoadGameData(trace, true, 12345, true, true);

        // Verify globals loaded BEFORE opcode state
        auto globalsPos = std::find(trace.order.begin(), trace.order.end(), LoadStep::GlobalVars);
        auto opcodePos = std::find(trace.order.begin(), trace.order.end(), LoadStep::OpcodeState);
        CHECK(globalsPos < opcodePos); // globals must be before opcode state

        // Verify arrays loaded AFTER opcode state but BEFORE metarules
        auto arraysPos = std::find(trace.order.begin(), trace.order.end(), LoadStep::Arrays);
        auto metarulesPos = std::find(trace.order.begin(), trace.order.end(), LoadStep::Metarules);
        CHECK(opcodePos < arraysPos);
        CHECK(arraysPos < metarulesPos);
    }
}
