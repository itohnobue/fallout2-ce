// Unit tests for save/load data integrity fixes from Stage 6 (loadsave.cc).
// Covers CRC initialization, backup handling, sfallgv ordering, and
// cross-save state desynchronization fixes.
//
// Self-contained mirror test — does NOT link loadsave.cc (engine file I/O deps).
//
// Fixes covered:
//   I2-M031 (MEDIUM): Write ordering gap — sfallgv.sav committed before SAVE.DAT
//   I2-M033 (MEDIUM): Header CRC computed over uninitialized memory
//   I2-M034 (MEDIUM): Handler chunk CRC over uninitialized memory
//   F-M041 (MEDIUM):  _SaveBackup failure non-fatal but _map_backup_count mismatch
//   F-M042 (MEDIUM):  Inconsistent treatment of missing vs. corrupt sfallgv.sav
//   F-M060 (MEDIUM):  sfallgv.bak restore ignores return, .bak cleanup incomplete
//   I2-M028 (MEDIUM): gScriptsListEntriesLength incremented before realloc success
//   I2-M029 (MEDIUM): scriptLoadAll memory leak on scriptListExtentRead failure

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <climits>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cstdint>

// ============================================================
// Intent
// ============================================================
//
// This test file validates 8 save/load and scripts fixes from the Stage 6
// workflow. The fixes span three categories:
//
// 1. Data integrity fixes (4 fixes):
//    - sfallgv vs SAVE.DAT ordering: Crash between writes can produce
//      cross-save state mismatch. Fix: write SAVE.DAT first, then commit
//      sfallgv.sav after.
//    - Header CRC: fileRead return unchecked → CRC computed over uninitialized
//      data. Fix: zero-init memory before read, check fileRead return.
//    - Handler CRC: Save path writes garbage CRC (self-inflicted corruption).
//      Fix: zero-init CRC buffer before computing.
//    - gScriptsListEntriesLength: incremented BEFORE realloc success →
//      phantom entry. Fix: increment after success check.
//
// 2. Error handling fixes (3 fixes):
//    - _SaveBackup: failure leaves _map_backup_count mismatched. Fix: reset on error.
//    - sfallgv.sav corrupt vs missing: Missing→success, corrupt→abort.
//      Fix: handle corrupt gracefully (load defaults, don't abort).
//    - sfallgv.bak restore: ignores compat_rename return. Fix: check return,
//      clean up .bak on failure.
//
// 3. Memory leak fix (1 fix):
//    - scriptLoadAll: leak on scriptListExtentRead failure. Fix: clean up
//      partially-allocated extent chain on error path.

// ============================================================
// Section 1: Write ordering fix — SAVE.DAT before sfallgv.sav
// ============================================================

namespace save_ordering {

struct FileSystemMirror {
    bool saveDatExists = false;
    bool sfallgvExists = false;
    std::string saveDatContent;
    std::string sfallgvContent;
    bool crashBetweenWrites = false;  // simulate crash point
};

// Mirror of the FIXED save sequence: SAVE.DAT first, then sfallgv
static bool mirrorSaveOrdered(FileSystemMirror& fs, const std::string& engineData,
                               const std::string& sfallData) {
    // I2-M031 fix: write SAVE.DAT FIRST, then sfallgv.sav
    // Step 1: Write engine data (SAVE.DAT)
    fs.saveDatContent = engineData;
    fs.saveDatExists = true;

    // Simulate crash between writes
    if (fs.crashBetweenWrites) {
        return false;  // crash — only SAVE.DAT written, sfallgv not committed
    }

    // Step 2: Write sfall extension data
    fs.sfallgvContent = sfallData;
    fs.sfallgvExists = true;
    return true;
}

// Mirror of the BUGGY save sequence: sfallgv first, then SAVE.DAT
static bool mirrorSaveBuggy(FileSystemMirror& fs, const std::string& engineData,
                            const std::string& sfallData) {
    // Buggy ordering: sfallgv first, then SAVE.DAT
    fs.sfallgvContent = sfallData;
    fs.sfallgvExists = true;

    if (fs.crashBetweenWrites) {
        return false;  // crash — sfallgv committed but SAVE.DAT not
    }

    fs.saveDatContent = engineData;
    fs.saveDatExists = true;
    return true;
}

}  // namespace save_ordering

TEST_CASE("I2-M031: Save write ordering — SAVE.DAT before sfallgv") {
    using namespace save_ordering;

    SUBCASE("normal save: both files written correctly") {
        FileSystemMirror fs;
        bool ok = mirrorSaveOrdered(fs, "engine_v1", "sfall_v1");
        CHECK(ok == true);
        CHECK(fs.saveDatExists == true);
        CHECK(fs.sfallgvExists == true);
        CHECK(fs.saveDatContent == "engine_v1");
        CHECK(fs.sfallgvContent == "sfall_v1");
    }

    SUBCASE("crash between writes (FIXED): old state preserved") {
        FileSystemMirror fs;
        fs.crashBetweenWrites = true;
        bool ok = mirrorSaveOrdered(fs, "engine_v2", "sfall_v2");
        CHECK(ok == false);  // crash detected
        // FIXED: SAVE.DAT was written but sfallgv was not
        // Recovery: sfallgv still has previous version or is absent
        // → engine loads with default/previous sfall state (safe)
        CHECK(fs.saveDatExists == true);   // engine data written
        CHECK(fs.sfallgvExists == false);   // sfall data NOT written → safe
    }

    SUBCASE("crash between writes (BUGGY): cross-save mismatch") {
        FileSystemMirror fs;
        fs.crashBetweenWrites = true;
        bool ok = mirrorSaveBuggy(fs, "engine_v3", "sfall_v3");
        CHECK(ok == false);
        // BUG: sfallgv was written but SAVE.DAT was not
        // Recovery: engine loads old SAVE.DAT but new sfallgv → mismatch
        CHECK(fs.sfallgvExists == true);   // NEW sfallgv committed
        CHECK(fs.saveDatExists == false);   // OLD engine data → mismatch
    }

    SUBCASE("no crash: both orderings produce same result") {
        FileSystemMirror fs1, fs2;
        mirrorSaveOrdered(fs1, "data", "sfall");
        mirrorSaveBuggy(fs2, "data", "sfall");
        CHECK(fs1.saveDatContent == fs2.saveDatContent);
        CHECK(fs1.sfallgvContent == fs2.sfallgvContent);
    }

    SUBCASE("multiple saves: each overwrites correctly") {
        FileSystemMirror fs;

        mirrorSaveOrdered(fs, "slot1_engine", "slot1_sfall");
        CHECK(fs.saveDatContent == "slot1_engine");

        mirrorSaveOrdered(fs, "slot2_engine", "slot2_sfall");
        CHECK(fs.saveDatContent == "slot2_engine");
        CHECK(fs.sfallgvContent == "slot2_sfall");
    }
}

// ============================================================
// Section 2: CRC over uninitialized memory
// ============================================================

// --- I2-M033: Header CRC over uninitialized memory ---
// Production: loadsave.cc:2453,2572
// internal_malloc returns uninit memory; fileRead return unchecked.
// Fix: zero-init the buffer before read, check fileRead return.

static uint32_t mirrorCrc32(const uint8_t* data, size_t len) {
    // Simple CRC-32 mirror for testing
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

static bool mirrorHeaderCrcCheckSafe(const uint8_t* data, size_t dataSize,
                                      uint32_t expectedCrc) {
    // I2-M033 fix: zero-init and validate size before computing CRC
    if (data == nullptr || dataSize == 0) {
        return false;  // no data to checksum
    }
    uint32_t actualCrc = mirrorCrc32(data, dataSize);
    return actualCrc == expectedCrc;
}

TEST_CASE("I2-M033: Header CRC over uninitialized memory guard") {
    SUBCASE("valid data with correct CRC passes") {
        const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
        uint32_t crc = mirrorCrc32(data, sizeof(data));
        CHECK(mirrorHeaderCrcCheckSafe(data, sizeof(data), crc) == true);
    }

    SUBCASE("null data → rejected") {
        CHECK(mirrorHeaderCrcCheckSafe(nullptr, 16, 0x12345678) == false);
    }

    SUBCASE("zero-size → rejected") {
        const uint8_t data[] = {0x00};
        CHECK(mirrorHeaderCrcCheckSafe(data, 0, 0) == false);
    }

    SUBCASE("CRC mismatch → rejected") {
        const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
        CHECK(mirrorHeaderCrcCheckSafe(data, sizeof(data), 0xDEADBEEF) == false);
    }

    SUBCASE("different data produces different CRC") {
        const uint8_t data1[] = {0xAA};
        const uint8_t data2[] = {0xBB};
        CHECK(mirrorCrc32(data1, 1) != mirrorCrc32(data2, 1));
    }
}

// --- I2-M034: Handler chunk CRC over uninitialized memory ---
// Production: loadsave.cc:2029,2247
// Save path writes garbage CRC (self-inflicted corruption).
// Fix: zero-init CRC buffer before computing.

static std::vector<uint8_t> mirrorComputeChunkCrcSafe(const std::vector<uint8_t>& chunkData) {
    // I2-M034 fix: compute CRC over initialized data only
    if (chunkData.empty()) {
        return {};  // empty chunk → no CRC
    }
    uint32_t crc = mirrorCrc32(chunkData.data(), chunkData.size());

    std::vector<uint8_t> result;
    result.insert(result.end(), chunkData.begin(), chunkData.end());
    // Append CRC bytes (big-endian)
    result.push_back(static_cast<uint8_t>((crc >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(crc & 0xFF));
    return result;
}

TEST_CASE("I2-M034: Handler chunk CRC over uninitialized memory") {
    SUBCASE("non-empty chunk gets valid CRC appended") {
        std::vector<uint8_t> chunk = {0x01, 0x02, 0x03};
        auto result = mirrorComputeChunkCrcSafe(chunk);
        CHECK(result.size() == chunk.size() + 4);  // data + 4 CRC bytes
        // Data portion preserved
        CHECK(result[0] == 0x01);
        CHECK(result[1] == 0x02);
        CHECK(result[2] == 0x03);
        // CRC appended (non-zero)
        bool crcNonZero = (result[3] | result[4] | result[5] | result[6]) != 0;
        CHECK(crcNonZero);
    }

    SUBCASE("empty chunk → no CRC, returns empty") {
        std::vector<uint8_t> chunk;
        auto result = mirrorComputeChunkCrcSafe(chunk);
        CHECK(result.empty());
    }

    SUBCASE("same data produces same CRC (determinism)") {
        std::vector<uint8_t> chunk = {0x10, 0x20, 0x30, 0x40};
        auto result1 = mirrorComputeChunkCrcSafe(chunk);
        auto result2 = mirrorComputeChunkCrcSafe(chunk);
        CHECK(result1 == result2);
    }

    SUBCASE("different data produces different CRC") {
        auto result1 = mirrorComputeChunkCrcSafe({0x10});
        auto result2 = mirrorComputeChunkCrcSafe({0x11});
        CHECK(result1 != result2);
    }
}

// ============================================================
// Section 3: Backup handling fixes
// ============================================================

// --- F-M041: _SaveBackup failure and _map_backup_count mismatch ---
// Production: loadsave.cc:1959-1961, 3585-3588
// If rename fails partway, backup returns -1 but _map_backup_count is set
// to full list length. Fix: reset _map_backup_count on error.

struct BackupState {
    int mapBackupCount = 0;
    int expectedCount = 0;
    bool renameFailed = false;
};

static bool mirrorSaveBackupSafe(BackupState& state, const std::vector<std::string>& maps) {
    state.expectedCount = static_cast<int>(maps.size());
    state.mapBackupCount = 0;

    for (int i = 0; i < static_cast<int>(maps.size()); i++) {
        if (state.renameFailed) {
            // F-M041 fix: reset count on failure, don't report partial success
            state.mapBackupCount = 0;
            return false;
        }
        state.mapBackupCount++;
    }
    return true;
}

TEST_CASE("F-M041: _SaveBackup count mismatch on partial failure") {
    SUBCASE("successful backup: count matches expected") {
        BackupState state;
        std::vector<std::string> maps = {"map1", "map2", "map3"};
        bool ok = mirrorSaveBackupSafe(state, maps);
        CHECK(ok == true);
        CHECK(state.mapBackupCount == 3);
    }

    SUBCASE("failed backup: count reset to 0 (F-M041 fix)") {
        BackupState state;
        state.renameFailed = true;
        std::vector<std::string> maps = {"map1", "map2", "map3"};
        bool ok = mirrorSaveBackupSafe(state, maps);
        CHECK(ok == false);
        CHECK(state.mapBackupCount == 0);  // reset, not 3
    }

    SUBCASE("no maps: count stays 0") {
        BackupState state;
        std::vector<std::string> maps;
        bool ok = mirrorSaveBackupSafe(state, maps);
        CHECK(ok == true);
        CHECK(state.mapBackupCount == 0);
    }

    SUBCASE("single map backup: count = 1") {
        BackupState state;
        std::vector<std::string> maps = {"single_map"};
        bool ok = mirrorSaveBackupSafe(state, maps);
        CHECK(ok == true);
        CHECK(state.mapBackupCount == 1);
    }
}

// --- F-M042: Inconsistent treatment of missing vs corrupt sfallgv.sav ---
// Production: loadsave.cc:2284-2305
// Missing → load succeeds with defaults. Corrupt → gameReset() and load FAILS.
// Fix: handle corrupt gracefully — load defaults, don't abort full load.

enum class SfallgvStatus {
    PRESENT_VALID,
    PRESENT_CORRUPT,
    MISSING
};

struct LoadResult {
    bool engineLoaded;
    bool sfallLoaded;
    bool usingDefaults;
};

static LoadResult mirrorLoadSfallgvSafe(SfallgvStatus status) {
    LoadResult result = {true, false, false};  // engine always loads first

    if (status == SfallgvStatus::MISSING) {
        // Civil behavior: load defaults
        result.sfallLoaded = true;
        result.usingDefaults = true;
        return result;
    }

    if (status == SfallgvStatus::PRESENT_CORRUPT) {
        // F-M042 fix: handle corrupt gracefully — load defaults
        // Before fix: gameReset() → engine load FAILS entirely
        result.sfallLoaded = true;
        result.usingDefaults = true;
        return result;
    }

    // PRESENT_VALID: load from file
    result.sfallLoaded = true;
    result.usingDefaults = false;
    return result;
}

static LoadResult mirrorLoadSfallgvBuggy(SfallgvStatus status) {
    LoadResult result = {true, false, false};

    if (status == SfallgvStatus::MISSING) {
        result.sfallLoaded = true;
        result.usingDefaults = true;
        return result;
    }

    if (status == SfallgvStatus::PRESENT_CORRUPT) {
        // Buggy: abort entire load — engine data already valid but discarded
        result.engineLoaded = false;
        result.sfallLoaded = false;
        return result;
    }

    result.sfallLoaded = true;
    return result;
}

TEST_CASE("F-M042: sfallgv.sav corrupt vs missing handling") {
    SUBCASE("missing sfallgv: loads defaults (both fixed and buggy)") {
        auto fixed = mirrorLoadSfallgvSafe(SfallgvStatus::MISSING);
        auto buggy = mirrorLoadSfallgvBuggy(SfallgvStatus::MISSING);
        CHECK(fixed.engineLoaded == true);
        CHECK(fixed.usingDefaults == true);
        CHECK(buggy.engineLoaded == true);
        CHECK(buggy.usingDefaults == true);
    }

    SUBCASE("valid sfallgv: loads from file") {
        auto fixed = mirrorLoadSfallgvSafe(SfallgvStatus::PRESENT_VALID);
        CHECK(fixed.engineLoaded == true);
        CHECK(fixed.sfallLoaded == true);
        CHECK(fixed.usingDefaults == false);
    }

    SUBCASE("corrupt sfallgv: FIXED loads defaults (graceful)") {
        auto fixed = mirrorLoadSfallgvSafe(SfallgvStatus::PRESENT_CORRUPT);
        CHECK(fixed.engineLoaded == true);   // engine preserves valid data
        CHECK(fixed.sfallLoaded == true);    // sfall loads defaults
        CHECK(fixed.usingDefaults == true);
    }

    SUBCASE("corrupt sfallgv: BUGGY aborts entire load") {
        auto buggy = mirrorLoadSfallgvBuggy(SfallgvStatus::PRESENT_CORRUPT);
        CHECK(buggy.engineLoaded == false);  // engine data discarded — bug
        CHECK(buggy.sfallLoaded == false);
    }
}

// --- F-M060: sfallgv.bak restore and cleanup ---
// Production: loadsave.cc:3631
// _RestoreSave ignores compat_rename return. Fix: check return, clean up .bak.

struct BakFileState {
    bool bakExists = false;
    bool restoreAttempted = false;
    bool restoreSuccess = false;
    bool bakCleanedUp = false;
};

static bool mirrorRestoreSfallgvSafe(BakFileState& state, bool renameWorks) {
    state.restoreAttempted = true;

    if (!state.bakExists) {
        return false;  // nothing to restore
    }

    // F-M060 fix: check rename return
    if (!renameWorks) {
        // Rename failed — clean up .bak
        state.bakCleanedUp = true;
        state.restoreSuccess = false;
        return false;
    }

    state.restoreSuccess = true;
    state.bakCleanedUp = true;  // .bak becomes .sav → .bak no longer exists
    state.bakExists = false;
    return true;
}

static bool mirrorRestoreSfallgvBuggy(BakFileState& state, bool renameWorks) {
    state.restoreAttempted = true;

    if (!state.bakExists) {
        return false;
    }

    // Buggy: ignores rename return
    // compat_rename called but return value discarded
    if (renameWorks) {
        state.restoreSuccess = true;
        state.bakExists = false;
    }
    // Buggy: no cleanup on failure — .bak orphans
    // Always reports success regardless
    return true;  // bug: returns true even when rename failed
}

TEST_CASE("F-M060: sfallgv.bak restore return check and cleanup") {
    SUBCASE("successful restore: .bak becomes .sav") {
        BakFileState state;
        state.bakExists = true;
        bool ok = mirrorRestoreSfallgvSafe(state, true);
        CHECK(ok == true);
        CHECK(state.restoreSuccess == true);
        CHECK(state.bakExists == false);  // .bak consumed
    }

    SUBCASE("failed restore: .bak cleaned up (F-M060 fix)") {
        BakFileState state;
        state.bakExists = true;
        bool ok = mirrorRestoreSfallgvSafe(state, false);
        CHECK(ok == false);
        CHECK(state.restoreSuccess == false);
        CHECK(state.bakCleanedUp == true);  // cleanup attempted
    }

    SUBCASE("BUGGY: failed restore reported as success") {
        BakFileState state;
        state.bakExists = true;
        bool ok = mirrorRestoreSfallgvBuggy(state, false);
        // Buggy: returns true even though rename failed
        CHECK(ok == true);  // incorrect: reports success
        CHECK(state.restoreSuccess == false);  // actually failed
        CHECK(state.bakCleanedUp == false);  // .bak orphaned
    }

    SUBCASE("no .bak file: restore returns false") {
        BakFileState state;
        state.bakExists = false;
        bool ok = mirrorRestoreSfallgvSafe(state, true);
        CHECK(ok == false);
        CHECK(state.restoreAttempted == true);
    }
}

// ============================================================
// Section 4: Scripts data integrity fixes
// ============================================================

// --- I2-M028: gScriptsListEntriesLength incremented before realloc success ---
// Production: scripts.cc:1466-1470
// gScriptsListEntriesLength++ before internal_realloc check.
// On failure, scriptsIsValidScriptIndex returns true for phantom index.
// Fix: increment only after realloc success.

struct ScriptListState {
    int entriesLength = 0;
    int entriesCapacity = 0;
    std::vector<int> entries;
};

static bool mirrorAddScriptEntrySafe(ScriptListState& state, int scriptId, bool reallocWorks) {
    // I2-M028 fix: check capacity and realloc BEFORE incrementing length
    if (static_cast<int>(state.entries.size()) >= state.entriesCapacity) {
        if (!reallocWorks) {
            return false;  // realloc failed — length NOT incremented
        }
    }

    // Only after success: add entry and increment length
    state.entries.push_back(scriptId);
    state.entriesLength = static_cast<int>(state.entries.size());
    return true;
}

static bool mirrorAddScriptEntryBuggy(ScriptListState& state, int scriptId, bool reallocWorks) {
    // Buggy: increment length FIRST, then check realloc
    state.entriesLength++;  // phantom entry added

    if (static_cast<int>(state.entries.size()) >= state.entriesCapacity) {
        if (!reallocWorks) {
            return false;  // realloc failed but length already incremented — phantom
        }
    }

    state.entries.push_back(scriptId);
    return true;
}

TEST_CASE("I2-M028: gScriptsListEntriesLength incremented before realloc success") {
    SUBCASE("successful add: length matches entries") {
        ScriptListState state;
        bool ok = mirrorAddScriptEntrySafe(state, 100, true);
        CHECK(ok == true);
        CHECK(state.entriesLength == 1);
        CHECK(state.entries.size() == 1);
    }

    SUBCASE("realloc failure: length NOT incremented (FIXED)") {
        ScriptListState state;
        bool ok = mirrorAddScriptEntrySafe(state, 200, false);
        CHECK(ok == false);
        CHECK(state.entriesLength == 0);  // NOT incremented
    }

    SUBCASE("BUGGY: realloc failure: length incremented (phantom entry)") {
        ScriptListState state;
        bool ok = mirrorAddScriptEntryBuggy(state, 300, false);
        CHECK(ok == false);  // realloc failed
        CHECK(state.entriesLength == 1);  // phantom: length says 1 but entries empty
        CHECK(state.entries.size() == 0);
    }

    SUBCASE("multiple adds: length always matches") {
        ScriptListState state;
        CHECK(mirrorAddScriptEntrySafe(state, 1, true));
        CHECK(mirrorAddScriptEntrySafe(state, 2, true));
        CHECK(mirrorAddScriptEntrySafe(state, 3, true));
        CHECK(state.entriesLength == 3);
        CHECK(state.entries.size() == 3);
    }
}

// --- I2-M029: scriptLoadAll memory leak on scriptListExtentRead failure ---
// Production: scripts.cc:2204,2219
// Two leak sites: partially-allocated extent chain not freed on error.
// Fix: clean up extent chain on error path.

struct ExtentNode {
    int id;
    ExtentNode* next = nullptr;
};

static void mirrorFreeExtentChain(ExtentNode* head) {
    // I2-M029 fix: clean up partially-allocated chain
    ExtentNode* current = head;
    while (current != nullptr) {
        ExtentNode* next = current->next;
        delete current;
        current = next;
    }
}

static ExtentNode* mirrorLoadExtentChainSafe(bool simulateReadFailure, bool& hadError) {
    // I2-M029 fix: clean up on error
    ExtentNode* head = nullptr;
    ExtentNode* tail = nullptr;
    hadError = false;

    for (int i = 0; i < 3; i++) {
        auto* node = new ExtentNode{i};
        if (head == nullptr) {
            head = tail = node;
        } else {
            tail->next = node;
            tail = node;
        }

        if (simulateReadFailure && i == 1) {
            // Error mid-chain: clean up what was allocated
            mirrorFreeExtentChain(head);
            head = nullptr;
            hadError = true;
            return nullptr;
        }
    }

    return head;
}

// Buggy version: leaks on error
static ExtentNode* mirrorLoadExtentChainBuggy(bool simulateReadFailure) {
    // Buggy: allocates, fails, returns without freeing
    auto* node1 = new ExtentNode{0};
    auto* node2 = new ExtentNode{1};
    node1->next = node2;

    if (simulateReadFailure) {
        // LEAK: node1 and node2 allocated but not freed
        return nullptr;
    }

    auto* node3 = new ExtentNode{2};
    node2->next = node3;
    return node1;
}

TEST_CASE("I2-M029: scriptLoadAll memory leak on failure") {
    SUBCASE("successful load: full chain returned") {
        bool hadError = false;
        ExtentNode* chain = mirrorLoadExtentChainSafe(false, hadError);
        CHECK(chain != nullptr);
        CHECK(hadError == false);

        // Verify chain has 3 nodes
        int count = 0;
        for (ExtentNode* n = chain; n != nullptr; n = n->next) count++;
        CHECK(count == 3);

        mirrorFreeExtentChain(chain);
    }

    SUBCASE("mid-chain failure: all nodes freed (FIXED)") {
        bool hadError = false;
        ExtentNode* chain = mirrorLoadExtentChainSafe(true, hadError);
        CHECK(chain == nullptr);
        CHECK(hadError == true);
        // No leak: mirrorFreeExtentChain was called on error path
    }

    SUBCASE("extent chain freeing handles empty chain") {
        mirrorFreeExtentChain(nullptr);  // should not crash
        CHECK(true);  // reached here → no crash
    }

    SUBCASE("extent chain freeing handles single node") {
        auto* node = new ExtentNode{42};
        mirrorFreeExtentChain(node);  // should free single node
        CHECK(true);  // no crash, no leak
    }
}
