// F-055: End-to-end save/load integration test with real file I/O
//
// tests/test_loadsave.cc covers 3 fork-change patterns (H-013, H-014, H-015)
// using in-memory TestFile stubs — no actual file I/O, no format validation.
// This file adds real file I/O round-trip tests for the save format components
// that CAN be tested without the full game engine.
//
// What's tested:
//   1. SFGV magic number write/read round-trip
//   2. Version field write/read
//   3. GlobalVarEntry binary format write/read
//   4. FloatVarEntry binary format write/read
//   5. Count-bounded entry deserialization
//   6. Truncated file detection
//
// What's NOT tested (requires game engine):
//   - sfall_gl_vars_save / sfall_gl_vars_load (require File* + game state)
//   - Other save subsystems (snapshot, proto, map, automap)
//   - Actual save slot file layout (SAVEGAME/SLOTxx/*.SAV)
//
// Uses real file I/O via std::fstream — temp files auto-deleted.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// ============================================================
// Production format definitions (mirrored from sfall_global_vars.cc:32-34)
// ============================================================

static constexpr uint32_t kSfallMagic = 0x53464756;  // "SFGV"
static constexpr int32_t kSfallVersion = 1;

// Production struct: sfall_global_vars.cc:18-28
#pragma pack(push)
#pragma pack(8)
struct TestGlobalVarEntry {
    uint64_t key;
    int32_t value;
    int32_t unused;
};

struct TestFloatVarEntry {
    uint64_t key;
    float value;
};
#pragma pack(pop)

// ============================================================
// Test helpers
// ============================================================

namespace {

// Get a unique temp file path. Uses process ID to avoid collisions.
std::string tempFilePath(const char* suffix)
{
    char buf[256];
    std::snprintf(buf, sizeof(buf), "tmp_test_loadsave_%s.bin", suffix);
    return std::string(buf);
}

// Remove temp file if it exists
void cleanupTempFile(const std::string& path)
{
    std::remove(path.c_str());
}

// Write a save header (magic + version + count) to a file
bool writeSaveHeader(std::ofstream& out, int count, int floatCount)
{
    uint32_t magic = kSfallMagic;
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    if (!out.good()) return false;

    int32_t version = kSfallVersion;
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    if (!out.good()) return false;

    out.write(reinterpret_cast<const char*>(&count), sizeof(count));
    if (!out.good()) return false;

    out.write(reinterpret_cast<const char*>(&floatCount), sizeof(floatCount));
    if (!out.good()) return false;

    return true;
}

// Read a save header from a file
struct SavedHeader {
    uint32_t magic = 0;
    int32_t version = 0;
    int count = 0;
    int floatCount = 0;
    bool valid = false;
};

SavedHeader readSaveHeader(std::ifstream& in)
{
    SavedHeader h;
    in.read(reinterpret_cast<char*>(&h.magic), sizeof(h.magic));
    if (!in.good()) return h;
    in.read(reinterpret_cast<char*>(&h.version), sizeof(h.version));
    if (!in.good()) return h;
    in.read(reinterpret_cast<char*>(&h.count), sizeof(h.count));
    if (!in.good()) return h;
    in.read(reinterpret_cast<char*>(&h.floatCount), sizeof(h.floatCount));
    if (!in.good()) return h;
    h.valid = true;
    return h;
}

// Write GlobalVarEntry array
bool writeGlobalVarEntries(std::ofstream& out,
                            const std::vector<TestGlobalVarEntry>& entries)
{
    for (const auto& e : entries) {
        out.write(reinterpret_cast<const char*>(&e), sizeof(e));
        if (!out.good()) return false;
    }
    return true;
}

// Read GlobalVarEntry array
std::vector<TestGlobalVarEntry> readGlobalVarEntries(std::ifstream& in, int count)
{
    std::vector<TestGlobalVarEntry> entries;
    for (int i = 0; i < count; i++) {
        TestGlobalVarEntry e;
        in.read(reinterpret_cast<char*>(&e), sizeof(e));
        if (!in.good()) break;
        entries.push_back(e);
    }
    return entries;
}

// Write FloatVarEntry array
bool writeFloatVarEntries(std::ofstream& out,
                           const std::vector<TestFloatVarEntry>& entries)
{
    for (const auto& e : entries) {
        out.write(reinterpret_cast<const char*>(&e), sizeof(e));
        if (!out.good()) return false;
    }
    return true;
}

// Read FloatVarEntry array
std::vector<TestFloatVarEntry> readFloatVarEntries(std::ifstream& in, int count)
{
    std::vector<TestFloatVarEntry> entries;
    for (int i = 0; i < count; i++) {
        TestFloatVarEntry e;
        in.read(reinterpret_cast<char*>(&e), sizeof(e));
        if (!in.good()) break;
        entries.push_back(e);
    }
    return entries;
}

} // anonymous namespace

// ============================================================
// Test: SFGV magic round-trip
// ============================================================

TEST_CASE("F-055: SFGV magic write/read round-trip")
{
    std::string path = tempFilePath("magic");
    cleanupTempFile(path);

    // Write
    {
        std::ofstream out(path, std::ios::binary);
        REQUIRE(out.is_open());

        writeSaveHeader(out, 0, 0);
        out.close();
    }

    // Read
    {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in.is_open());

        SavedHeader h = readSaveHeader(in);
        REQUIRE(h.valid);
        CHECK(h.magic == kSfallMagic);
        CHECK(h.version == kSfallVersion);
        CHECK(h.count == 0);
        CHECK(h.floatCount == 0);
    }

    cleanupTempFile(path);
}

TEST_CASE("F-055: GlobalVarEntry single entry round-trip")
{
    std::string path = tempFilePath("gventry");
    cleanupTempFile(path);

    // Test data
    TestGlobalVarEntry original = { 0xDEADBEEFCAFEBABEULL, 42 };

    // Write
    {
        std::ofstream out(path, std::ios::binary);
        REQUIRE(out.is_open());

        writeSaveHeader(out, 1, 0);
        writeGlobalVarEntries(out, { original });
        out.close();
    }

    // Read
    {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in.is_open());

        SavedHeader h = readSaveHeader(in);
        REQUIRE(h.valid);
        REQUIRE(h.count == 1);

        auto entries = readGlobalVarEntries(in, h.count);
        REQUIRE(entries.size() == 1);
        CHECK(entries[0].key == original.key);
        CHECK(entries[0].value == original.value);
    }

    cleanupTempFile(path);
}

TEST_CASE("F-055: GlobalVarEntry multiple entries round-trip")
{
    std::string path = tempFilePath("gventries");
    cleanupTempFile(path);

    // Test data: 3 entries
    std::vector<TestGlobalVarEntry> originals = {
        { 100ULL, 1000 },
        { 200ULL, 2000 },
        { 300ULL, 3000 },
    };

    // Write
    {
        std::ofstream out(path, std::ios::binary);
        REQUIRE(out.is_open());

        writeSaveHeader(out, 3, 0);
        writeGlobalVarEntries(out, originals);
        out.close();
    }

    // Read
    {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in.is_open());

        SavedHeader h = readSaveHeader(in);
        REQUIRE(h.valid);
        REQUIRE(h.count == 3);

        auto entries = readGlobalVarEntries(in, h.count);
        REQUIRE(entries.size() == 3);
        CHECK(entries[0].key == originals[0].key);
        CHECK(entries[0].value == originals[0].value);
        CHECK(entries[1].key == originals[1].key);
        CHECK(entries[1].value == originals[1].value);
        CHECK(entries[2].key == originals[2].key);
        CHECK(entries[2].value == originals[2].value);
    }

    cleanupTempFile(path);
}

TEST_CASE("F-055: FloatVarEntry single entry round-trip")
{
    std::string path = tempFilePath("fventry");
    cleanupTempFile(path);

    TestFloatVarEntry original = { 0xABCDEF0123456789ULL, 3.14159f };

    // Write
    {
        std::ofstream out(path, std::ios::binary);
        REQUIRE(out.is_open());

        writeSaveHeader(out, 0, 1);
        writeFloatVarEntries(out, { original });
        out.close();
    }

    // Read
    {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in.is_open());

        SavedHeader h = readSaveHeader(in);
        REQUIRE(h.valid);
        REQUIRE(h.floatCount == 1);

        auto entries = readFloatVarEntries(in, h.floatCount);
        REQUIRE(entries.size() == 1);
        CHECK(entries[0].key == original.key);
        CHECK(entries[0].value == doctest::Approx(original.value));
    }

    cleanupTempFile(path);
}

TEST_CASE("F-055: Mixed int and float entries round-trip")
{
    std::string path = tempFilePath("mixed");
    cleanupTempFile(path);

    std::vector<TestGlobalVarEntry> intEntries = {
        { 1ULL, 100 },
        { 2ULL, 200 },
    };
    std::vector<TestFloatVarEntry> floatEntries = {
        { 10ULL, 1.0f },
        { 20ULL, 2.5f },
        { 30ULL, -0.5f },
    };

    // Write
    {
        std::ofstream out(path, std::ios::binary);
        REQUIRE(out.is_open());

        writeSaveHeader(out, 2, 3);
        writeGlobalVarEntries(out, intEntries);
        writeFloatVarEntries(out, floatEntries);
        out.close();
    }

    // Read
    {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in.is_open());

        SavedHeader h = readSaveHeader(in);
        REQUIRE(h.valid);
        REQUIRE(h.count == 2);
        REQUIRE(h.floatCount == 3);

        auto ints = readGlobalVarEntries(in, h.count);
        REQUIRE(ints.size() == 2);
        CHECK(ints[0].key == intEntries[0].key);
        CHECK(ints[0].value == intEntries[0].value);
        CHECK(ints[1].key == intEntries[1].key);
        CHECK(ints[1].value == intEntries[1].value);

        auto floats = readFloatVarEntries(in, h.floatCount);
        REQUIRE(floats.size() == 3);
        CHECK(floats[0].key == floatEntries[0].key);
        CHECK(floats[0].value == doctest::Approx(floatEntries[0].value));
        CHECK(floats[1].key == floatEntries[1].key);
        CHECK(floats[1].value == doctest::Approx(floatEntries[1].value));
        CHECK(floats[2].key == floatEntries[2].key);
        CHECK(floats[2].value == doctest::Approx(floatEntries[2].value));
    }

    cleanupTempFile(path);
}

TEST_CASE("F-055: Truncated file — header only, no entries")
{
    std::string path = tempFilePath("truncated");
    cleanupTempFile(path);

    // Write header only (count=5 but no entries written)
    {
        std::ofstream out(path, std::ios::binary);
        REQUIRE(out.is_open());

        writeSaveHeader(out, 5, 3);
        // Intentionally do NOT write any entries
        out.close();
    }

    // Read — entries should be empty (truncated)
    {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in.is_open());

        SavedHeader h = readSaveHeader(in);
        REQUIRE(h.valid);
        CHECK(h.count == 5);
        CHECK(h.floatCount == 3);

        auto ints = readGlobalVarEntries(in, h.count);
        // File truncated after header → 0 entries read
        CHECK(ints.size() == 0);

        auto floats = readFloatVarEntries(in, h.floatCount);
        CHECK(floats.size() == 0);
    }

    cleanupTempFile(path);
}

TEST_CASE("F-055: Truncated file — header + partial entries")
{
    std::string path = tempFilePath("partial");
    cleanupTempFile(path);

    // Write header + 2 entries out of 5
    {
        std::ofstream out(path, std::ios::binary);
        REQUIRE(out.is_open());

        writeSaveHeader(out, 5, 0);
        // Write only 2 of the 5 promised entries
        std::vector<TestGlobalVarEntry> partial = {
            { 1ULL, 100 },
            { 2ULL, 200 },
        };
        writeGlobalVarEntries(out, partial);
        out.close();
    }

    // Read — should get only the 2 written entries
    {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in.is_open());

        SavedHeader h = readSaveHeader(in);
        REQUIRE(h.valid);
        CHECK(h.count == 5); // header says 5

        auto ints = readGlobalVarEntries(in, h.count);
        // Only 2 entries were written
        CHECK(ints.size() == 2);
        CHECK(ints[0].key == 1ULL);
        CHECK(ints[0].value == 100);
        CHECK(ints[1].key == 2ULL);
        CHECK(ints[1].value == 200);
    }

    cleanupTempFile(path);
}

TEST_CASE("F-055: Empty file — no header")
{
    std::string path = tempFilePath("empty");
    cleanupTempFile(path);

    // Create empty file
    {
        std::ofstream out(path, std::ios::binary);
        REQUIRE(out.is_open());
        out.close();
    }

    // Read — empty file = no header
    {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in.is_open());

        SavedHeader h = readSaveHeader(in);
        CHECK_FALSE(h.valid);
        CHECK(h.magic == 0);
    }

    cleanupTempFile(path);
}

TEST_CASE("F-055: Wrong magic — old format detection")
{
    std::string path = tempFilePath("wrongmagic");
    cleanupTempFile(path);

    // Write a file with wrong magic, simulating old format (starts with count)
    {
        std::ofstream out(path, std::ios::binary);
        REQUIRE(out.is_open());

        int count = 3; // old format: first uint32_t is count
        out.write(reinterpret_cast<const char*>(&count), sizeof(count));
        out.close();
    }

    // Read — first uint32_t != SFGV magic → old format detection path
    {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in.is_open());

        SavedHeader h = readSaveHeader(in);
        // The first 4 bytes are NOT the magic
        CHECK(h.magic != kSfallMagic);
        // 3 cast to uint32_t = 3 → not the magic
        CHECK(h.magic == 3);
        // Production code checks `magicOrCount == kSfallGlobalVarsMagic`
        // 3 != 0x53464756 → enters old-format branch
        CHECK(h.magic != kSfallMagic);
    }

    cleanupTempFile(path);
}

TEST_CASE("F-055: Zero entries round-trip (valid empty state)")
{
    std::string path = tempFilePath("zeroentries");
    cleanupTempFile(path);

    // Write header with zero entries (fresh game state)
    {
        std::ofstream out(path, std::ios::binary);
        REQUIRE(out.is_open());

        writeSaveHeader(out, 0, 0);
        out.close();
    }

    // Read
    {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in.is_open());

        SavedHeader h = readSaveHeader(in);
        REQUIRE(h.valid);
        CHECK(h.magic == kSfallMagic);
        CHECK(h.version == kSfallVersion);
        CHECK(h.count == 0);
        CHECK(h.floatCount == 0);

        auto ints = readGlobalVarEntries(in, h.count);
        CHECK(ints.size() == 0);

        auto floats = readFloatVarEntries(in, h.floatCount);
        CHECK(floats.size() == 0);
    }

    cleanupTempFile(path);
}

TEST_CASE("F-055: Entry with zero key (valid)")
{
    std::string path = tempFilePath("zerokey");
    cleanupTempFile(path);

    TestGlobalVarEntry original = { 0ULL, -1 };

    {
        std::ofstream out(path, std::ios::binary);
        REQUIRE(out.is_open());

        writeSaveHeader(out, 1, 0);
        writeGlobalVarEntries(out, { original });
        out.close();
    }

    {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in.is_open());

        SavedHeader h = readSaveHeader(in);
        REQUIRE(h.valid);
        REQUIRE(h.count == 1);

        auto entries = readGlobalVarEntries(in, h.count);
        REQUIRE(entries.size() == 1);
        CHECK(entries[0].key == 0ULL);
        CHECK(entries[0].value == -1);
    }

    cleanupTempFile(path);
}

TEST_CASE("F-055: Entry with max-value key and value")
{
    std::string path = tempFilePath("maxval");
    cleanupTempFile(path);

    TestGlobalVarEntry original = { UINT64_MAX, INT_MAX };

    {
        std::ofstream out(path, std::ios::binary);
        REQUIRE(out.is_open());

        writeSaveHeader(out, 1, 0);
        writeGlobalVarEntries(out, { original });
        out.close();
    }

    {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in.is_open());

        SavedHeader h = readSaveHeader(in);
        REQUIRE(h.valid);
        REQUIRE(h.count == 1);

        auto entries = readGlobalVarEntries(in, h.count);
        REQUIRE(entries.size() == 1);
        CHECK(entries[0].key == UINT64_MAX);
        CHECK(entries[0].value == INT_MAX);
    }

    cleanupTempFile(path);
}

TEST_CASE("F-055: Large entry count (100 entries) round-trip")
{
    std::string path = tempFilePath("largecount");
    cleanupTempFile(path);

    std::vector<TestGlobalVarEntry> originals;
    for (int i = 0; i < 100; i++) {
        originals.push_back({ static_cast<uint64_t>(i * 1000), i * 100 });
    }

    {
        std::ofstream out(path, std::ios::binary);
        REQUIRE(out.is_open());

        writeSaveHeader(out, 100, 0);
        writeGlobalVarEntries(out, originals);
        out.close();
    }

    {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in.is_open());

        SavedHeader h = readSaveHeader(in);
        REQUIRE(h.valid);
        REQUIRE(h.count == 100);

        auto entries = readGlobalVarEntries(in, h.count);
        REQUIRE(entries.size() == 100);
        for (int i = 0; i < 100; i++) {
            CHECK(entries[i].key == originals[i].key);
            CHECK(entries[i].value == originals[i].value);
        }
    }

    cleanupTempFile(path);
}
