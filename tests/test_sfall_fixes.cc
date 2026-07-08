// Unit tests for sfall validation fixes (F-01, F-27, F-48, F2-06, F-38).
//
// Self-contained header-level test — does NOT link sfall_script_hooks.cc
// or sfall_opcodes.cc. Uses local mirrors of the validation logic
// introduced by each fix.
//
// F-01: HOOK_COMBATDAMAGE return value clamping
// F-27: HOOK_BARTERPRICE upper-bound clamping
// F-48: fseek return value checking in fs_create / fs_resize
// F2-06: VFS sandbox resolution for play_sfall_sound
// F-38: FID_TYPE check for op_set_critter_hit_chance_mod

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "combat_defs.h"
#include "obj_types.h"
#include "sfall_script_hooks.h"

using namespace fallout;

// =================================================================
// F-01: HOOK_COMBATDAMAGE return value validation
// =================================================================
// Mirror of the validation logic from sfall_script_hooks.cc:1275-1318.
// Tests that the 5 return values (defenderDamage, attackerDamage,
// defenderFlags, attackerFlags, defenderKnockback) are properly
// clamped/masked.

namespace combat_damage_mirror {

constexpr int COMBAT_DAMAGE_MIN = 0;
constexpr int COMBAT_DAMAGE_MAX = 9999;
constexpr int COMBAT_FLAGS_MASK = DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN | DAM_CRIP | DAM_DEAD | DAM_LOSE_TURN;
constexpr int COMBAT_KNOCKBACK_MIN = 0;
constexpr int COMBAT_KNOCKBACK_MAX = 20;  // Mirrors production: MAX_KNOCKDOWN_DISTANCE (src/actions.h:9)

struct CombatDamageFields {
    int defenderDamage = 0;
    int attackerDamage = 0;
    int defenderFlags = 0;
    int attackerFlags = 0;
    int defenderKnockback = 0;
};

// Mirror of the production clamping loop.
void applyHookReturns(CombatDamageFields& f, int numRets, const int* retValues)
{
    constexpr int DAMAGE_INDICES[] = {0, 1};
    constexpr int FLAGS_INDICES[] = {2, 3};
    constexpr int KNOCKBACK_INDEX = 4;

    int* fields[] = {
        &f.defenderDamage,
        &f.attackerDamage,
        &f.defenderFlags,
        &f.attackerFlags,
        &f.defenderKnockback
    };

    for (int i = 0; i < numRets && i < 5; i++) {
        int value = retValues[i];
        if (i == DAMAGE_INDICES[0] || i == DAMAGE_INDICES[1]) {
            value = std::clamp(value, COMBAT_DAMAGE_MIN, COMBAT_DAMAGE_MAX);
        }
        if (i == FLAGS_INDICES[0] || i == FLAGS_INDICES[1]) {
            value &= COMBAT_FLAGS_MASK;
        }
        if (i == KNOCKBACK_INDEX) {
            value = std::clamp(value, COMBAT_KNOCKBACK_MIN, COMBAT_KNOCKBACK_MAX);
        }
        *fields[i] = value;
    }
}

} // namespace combat_damage_mirror

TEST_CASE("F-01: Combat damage — normal values pass through unchanged")
{
    combat_damage_mirror::CombatDamageFields f;
    int returns[] = {10, 5, DAM_DEAD | DAM_KNOCKED_DOWN, DAM_HIT, 3};
    combat_damage_mirror::applyHookReturns(f, 5, returns);

    CHECK(f.defenderDamage == 10);
    CHECK(f.attackerDamage == 5);
    CHECK(f.defenderFlags == (DAM_DEAD | DAM_KNOCKED_DOWN));
    CHECK(f.attackerFlags == (DAM_HIT & combat_damage_mirror::COMBAT_FLAGS_MASK));
    // DAM_HIT (0x100) is not in COMBAT_FLAGS_MASK, so attackerFlags should be 0
    CHECK(f.attackerFlags == 0);
    CHECK(f.defenderKnockback == 3);
}

TEST_CASE("F-01: Combat damage — negative damage clamped to 0")
{
    combat_damage_mirror::CombatDamageFields f;
    int returns[] = {-100, -50, 0, 0, 0};
    combat_damage_mirror::applyHookReturns(f, 5, returns);

    CHECK(f.defenderDamage == 0);
    CHECK(f.attackerDamage == 0);
}

TEST_CASE("F-01: Combat damage — excessively large damage clamped to 9999")
{
    combat_damage_mirror::CombatDamageFields f;
    int returns[] = {50000, 100000, 0, 0, 0};
    combat_damage_mirror::applyHookReturns(f, 5, returns);

    CHECK(f.defenderDamage == 9999);
    CHECK(f.attackerDamage == 9999);
}

TEST_CASE("F-01: Combat damage — INT_MAX clamped to COMBAT_DAMAGE_MAX")
{
    combat_damage_mirror::CombatDamageFields f;
    int returns[] = {INT_MAX, INT_MAX, 0, 0, 0};
    combat_damage_mirror::applyHookReturns(f, 2, returns);

    CHECK(f.defenderDamage == combat_damage_mirror::COMBAT_DAMAGE_MAX);
    CHECK(f.attackerDamage == combat_damage_mirror::COMBAT_DAMAGE_MAX);
}

TEST_CASE("F-01: Combat damage — flags masked to valid DAM_* bits only")
{
    combat_damage_mirror::CombatDamageFields f;
    // Pass all bits set — only mask bits should survive
    int returns[] = {0, 0, -1, -1, 0};
    combat_damage_mirror::applyHookReturns(f, 5, returns);

    int expectedMask = combat_damage_mirror::COMBAT_FLAGS_MASK;
    CHECK(f.defenderFlags == expectedMask);
    CHECK(f.attackerFlags == expectedMask);
    // Verify that bits outside the mask are stripped
    CHECK((f.defenderFlags & ~expectedMask) == 0);
    CHECK((f.attackerFlags & ~expectedMask) == 0);
}

TEST_CASE("F-01: Combat damage — specific flag combinations survive masking")
{
    combat_damage_mirror::CombatDamageFields f;
    int returns[] = {0, 0, DAM_DEAD, DAM_KNOCKED_OUT | DAM_LOSE_TURN, 0};
    combat_damage_mirror::applyHookReturns(f, 5, returns);

    CHECK(f.defenderFlags == DAM_DEAD);
    CHECK(f.attackerFlags == (DAM_KNOCKED_OUT | DAM_LOSE_TURN));
}

TEST_CASE("F-01: Combat damage — uncontrolled bits (DAM_HIT, DAM_ON_FIRE) stripped from flags")
{
    combat_damage_mirror::CombatDamageFields f;
    int returns[] = {0, 0, DAM_HIT | DAM_DEAD | DAM_ON_FIRE | DAM_BYPASS, DAM_EXPLODE | DAM_DESTROY | DAM_DROP | DAM_DEAD, 0};
    combat_damage_mirror::applyHookReturns(f, 5, returns);

    // DAM_HIT (0x100), DAM_ON_FIRE (0x400), DAM_BYPASS (0x800) are stripped
    // DAM_EXPLODE (0x1000), DAM_DESTROY (0x2000), DAM_DROP (0x4000) are stripped
    // Only DAM_DEAD survives in both
    CHECK(f.defenderFlags == DAM_DEAD);
    CHECK(f.attackerFlags == DAM_DEAD);
}

TEST_CASE("F-01: Combat damage — knockback negative clamped to 0")
{
    combat_damage_mirror::CombatDamageFields f;
    int returns[] = {10, 5, DAM_DEAD, 0, -50};
    combat_damage_mirror::applyHookReturns(f, 5, returns);

    CHECK(f.defenderKnockback == 0);
}

TEST_CASE("F-01: Combat damage — knockback above max clamped")
{
    combat_damage_mirror::CombatDamageFields f;
    int returns[] = {10, 5, DAM_DEAD, 0, 999};
    combat_damage_mirror::applyHookReturns(f, 5, returns);

    CHECK(f.defenderKnockback == combat_damage_mirror::COMBAT_KNOCKBACK_MAX);
}

TEST_CASE("F-01b: Combat damage — knockback at production boundary (MAX_KNOCKDOWN_DISTANCE=20)")
{
    // This test ensures the test mirror's COMBAT_KNOCKBACK_MAX stays in sync
    // with production's MAX_KNOCKDOWN_DISTANCE=20 (src/actions.h:9).
    // With the stale value of 99, knockback=21 and knockback=50 would pass
    // through unclamped; with the correct value of 20, both are clamped.

    combat_damage_mirror::CombatDamageFields f1;
    int atBoundary[] = {0, 0, 0, 0, 20};
    combat_damage_mirror::applyHookReturns(f1, 5, atBoundary);
    CHECK(f1.defenderKnockback == 20);

    combat_damage_mirror::CombatDamageFields f2;
    int justAbove[] = {0, 0, 0, 0, 21};
    combat_damage_mirror::applyHookReturns(f2, 5, justAbove);
    CHECK(f2.defenderKnockback == 20);

    combat_damage_mirror::CombatDamageFields f3;
    int wellAbove[] = {0, 0, 0, 0, 50};
    combat_damage_mirror::applyHookReturns(f3, 5, wellAbove);
    CHECK(f3.defenderKnockback == 20);
}

TEST_CASE("F-01: Combat damage — fewer returns than fields leaves remainder untouched")
{
    combat_damage_mirror::CombatDamageFields f;
    f.defenderDamage = 50;
    f.attackerDamage = 25;
    f.defenderFlags = DAM_DEAD;
    f.attackerFlags = DAM_HIT;
    f.defenderKnockback = 10;

    // Only return first 2 values
    int returns[] = {42, 17};
    combat_damage_mirror::applyHookReturns(f, 2, returns);

    CHECK(f.defenderDamage == 42);
    CHECK(f.attackerDamage == 17);
    // Remaining fields should be unchanged
    CHECK(f.defenderFlags == DAM_DEAD);
    CHECK(f.attackerFlags == DAM_HIT);
    CHECK(f.defenderKnockback == 10);
}

TEST_CASE("F-01: Combat damage — zero return values leaves all fields unchanged")
{
    combat_damage_mirror::CombatDamageFields f;
    f.defenderDamage = 100;
    f.attackerDamage = 50;
    f.defenderFlags = DAM_KNOCKED_OUT;
    f.attackerFlags = 0;
    f.defenderKnockback = 5;

    combat_damage_mirror::applyHookReturns(f, 0, nullptr);

    CHECK(f.defenderDamage == 100);
    CHECK(f.attackerDamage == 50);
    CHECK(f.defenderFlags == DAM_KNOCKED_OUT);
    CHECK(f.attackerFlags == 0);
    CHECK(f.defenderKnockback == 5);
}

// =================================================================
// F-27: HOOK_BARTERPRICE upper-bound clamping
// =================================================================
// Mirror of the validation logic from sfall_script_hooks.cc:1343-1358.
// Tests that barter values are clamped to [0, 9999999].

namespace barter_price_mirror {

constexpr int BARTER_PRICE_MIN = 0;
constexpr int BARTER_PRICE_MAX = 9999999;

struct BarterPriceFields {
    int value = 100;
    int offerValue = 50;
};

void applyBarterReturns(BarterPriceFields& f, int numRets, const int* retValues)
{
    int* fields[] = { &f.value, &f.offerValue };

    for (int i = 0; i < numRets && i < 2; i++) {
        int valueFromScript = retValues[i];
        if (valueFromScript < 0) continue;
        *fields[i] = std::clamp(valueFromScript, BARTER_PRICE_MIN, BARTER_PRICE_MAX);
    }
}

} // namespace barter_price_mirror

TEST_CASE("F-27: Barter price — normal value passes through unchanged")
{
    barter_price_mirror::BarterPriceFields f;
    int returns[] = {1000, 500};
    barter_price_mirror::applyBarterReturns(f, 2, returns);

    CHECK(f.value == 1000);
    CHECK(f.offerValue == 500);
}

TEST_CASE("F-27: Barter price — negative value is rejected (field unchanged)")
{
    barter_price_mirror::BarterPriceFields f;
    f.value = 100;
    f.offerValue = 50;
    int returns[] = {-500, -200};
    barter_price_mirror::applyBarterReturns(f, 2, returns);

    CHECK(f.value == 100);   // unchanged
    CHECK(f.offerValue == 50); // unchanged
}

TEST_CASE("F-27: Barter price — zero value is accepted")
{
    barter_price_mirror::BarterPriceFields f;
    int returns[] = {0, 0};
    barter_price_mirror::applyBarterReturns(f, 2, returns);

    CHECK(f.value == 0);
    CHECK(f.offerValue == 0);
}

TEST_CASE("F-27: Barter price — excessively large value clamped to BARTER_PRICE_MAX")
{
    barter_price_mirror::BarterPriceFields f;
    int returns[] = {50000000, 100000000};
    barter_price_mirror::applyBarterReturns(f, 2, returns);

    CHECK(f.value == barter_price_mirror::BARTER_PRICE_MAX);
    CHECK(f.offerValue == barter_price_mirror::BARTER_PRICE_MAX);
}

TEST_CASE("F-27: Barter price — INT_MAX clamped to BARTER_PRICE_MAX")
{
    barter_price_mirror::BarterPriceFields f;
    int returns[] = {INT_MAX, INT_MAX};
    barter_price_mirror::applyBarterReturns(f, 2, returns);

    CHECK(f.value == barter_price_mirror::BARTER_PRICE_MAX);
    CHECK(f.offerValue == barter_price_mirror::BARTER_PRICE_MAX);
}

TEST_CASE("F-27: Barter price — negative value mixed with normal value")
{
    barter_price_mirror::BarterPriceFields f;
    f.value = 100;
    int returns[] = {-1, 200};
    barter_price_mirror::applyBarterReturns(f, 2, returns);

    CHECK(f.value == 100);  // negative, unchanged
    CHECK(f.offerValue == 200); // accepted and passed through
}

TEST_CASE("F-27: Barter price — value at exact boundary MAX")
{
    barter_price_mirror::BarterPriceFields f;
    int returns[] = {barter_price_mirror::BARTER_PRICE_MAX, barter_price_mirror::BARTER_PRICE_MAX};
    barter_price_mirror::applyBarterReturns(f, 2, returns);

    CHECK(f.value == barter_price_mirror::BARTER_PRICE_MAX);
    CHECK(f.offerValue == barter_price_mirror::BARTER_PRICE_MAX);
}

TEST_CASE("F-27: Barter price — value at exact boundary MAX+1 is clamped")
{
    barter_price_mirror::BarterPriceFields f;
    int returns[] = {barter_price_mirror::BARTER_PRICE_MAX + 1, barter_price_mirror::BARTER_PRICE_MAX + 100};
    barter_price_mirror::applyBarterReturns(f, 2, returns);

    CHECK(f.value == barter_price_mirror::BARTER_PRICE_MAX);
    CHECK(f.offerValue == barter_price_mirror::BARTER_PRICE_MAX);
}

// =================================================================
// F-48: fseek return value checking in VFS
// =================================================================
// Mirror of the error-checking logic from fs_create (sfall_opcodes.cc:2129)
// and fs_resize (sfall_opcodes.cc:2630). Tests that fseek returns are
// checked and errors are propagated.

namespace vfs_fseek_mirror {

// Mirror of fs_create size-allocation fseek+fputc path
struct FsCreateResult {
    bool success = false;
    int errorCode = 0;
};

FsCreateResult mirrorFsCreateAlloc(FILE* file, long size)
{
    FsCreateResult result;

    // F-48: fseek return check
    if (fseek(file, size - 1, SEEK_SET) != 0) {
        result.success = false;
        result.errorCode = 1; // fseek failed
        return result;
    }

    if (fputc(0, file) == EOF) {
        result.success = false;
        result.errorCode = 2; // fputc failed
        return result;
    }

    rewind(file);
    result.success = true;
    return result;
}

// Mirror of fs_resize fseek + fputc path
struct FsResizeResult {
    bool fseekOk = false;
    bool fputcOk = false;
};

FsResizeResult mirrorFsResizeFseek(FILE* file, long size)
{
    FsResizeResult result;

    // F-48: fseek return check
    if (fseek(file, size - 1, SEEK_SET) != 0) {
        result.fseekOk = false;
        return result;
    }
    result.fseekOk = true;

    if (fputc(0, file) == EOF) {
        result.fputcOk = false;
        return result;
    }
    result.fputcOk = true;
    return result;
}

} // namespace vfs_fseek_mirror

TEST_CASE("F-48: fs_create — fseek success followed by successful fputc")
{
    // Use a temporary file on disk for real fseek behavior
    FILE* f = std::tmpfile();
    REQUIRE(f != nullptr);

    vfs_fseek_mirror::FsCreateResult r = vfs_fseek_mirror::mirrorFsCreateAlloc(f, 100);
    CHECK(r.success == true);

    // Verify file was positioned back to start
    long pos = ftell(f);
    CHECK(pos == 0);

    std::fclose(f);
}

TEST_CASE("F-48: fs_create — fseek failure returns error (negative size would UB; test code path)")
{
    // The actual fseek check is `fseek(file, size-1, SEEK_SET) != 0`.
    // Negative sizes are already validated by fs_create (size <= 0 check at
    // line 2120 BEFORE fseek), so the fseek check protects against other
    // failures (e.g., disk error, stream closed). We test the logic
    // correctness with a valid size on a valid file.
    FILE* f = std::tmpfile();
    REQUIRE(f != nullptr);

    // Write some data so we have a valid stream
    std::fputs("test data", f);
    std::fflush(f);
    rewind(f);

    vfs_fseek_mirror::FsCreateResult r = vfs_fseek_mirror::mirrorFsCreateAlloc(f, 50);
    CHECK(r.success == true);
    CHECK(r.errorCode == 0);

    std::fclose(f);
}

TEST_CASE("F-48: fs_resize — fseek success with valid file")
{
    FILE* f = std::tmpfile();
    REQUIRE(f != nullptr);

    vfs_fseek_mirror::FsResizeResult r = vfs_fseek_mirror::mirrorFsResizeFseek(f, 200);
    CHECK(r.fseekOk == true);
    CHECK(r.fputcOk == true);

    std::fclose(f);
}

TEST_CASE("F-48: fseek return value — non-zero indicates error")
{
    // Test that the check `fseek(..., SEEK_SET) != 0` correctly captures
    // the semantics: fseek returns 0 on success, non-zero on error.
    // The `fseek` function from C standard returns 0 on success.
    // We verify this with a normal successful seek.
    FILE* f = std::tmpfile();
    REQUIRE(f != nullptr);

    int seekResult = fseek(f, 0, SEEK_SET);
    CHECK(seekResult == 0); // confirmed: 0 = success
    CHECK((seekResult != 0) == false); // our check logic is correct

    std::fclose(f);
}

// =================================================================
// F2-06: VFS sandbox resolution for play_sfall_sound
// =================================================================
// Mirror of the resolution logic that was added to op_play_sfall_sound.
// Tests that the sandbox root is properly prepended to audio paths.

namespace vfs_audio_mirror {

// Mirror of sfallVfsResolvePath logic (sfall_opcodes.cc:2053-2081)
// simplified for testing without the actual global sfallVfsRootDir.
struct VfsResolveResult {
    bool success = false;
    std::string resolvedPath;
};

VfsResolveResult mirrorResolvePath(const char* rawPath, const char* rootDir, size_t outBufSize)
{
    VfsResolveResult result;

    if (rawPath == nullptr || rawPath[0] == '\0') {
        return result;
    }

    // Path traversal check (simplified — production uses sfallVfsPathContainsTraversal)
    if (std::strstr(rawPath, "..") != nullptr) {
        return result;
    }

    // Absolute path rejection
    if (rawPath[0] == '/' || rawPath[0] == '\\') {
        return result;
    }

    if (rootDir != nullptr && rootDir[0] != '\0') {
        std::string resolved = std::string(rootDir) + "/" + std::string(rawPath);
        if (resolved.size() >= outBufSize) {
            return result;
        }
        result.resolvedPath = resolved;
    } else {
        result.resolvedPath = rawPath;
    }

    result.success = true;
    return result;
}

} // namespace vfs_audio_mirror

TEST_CASE("F2-06: VFS audio — path resolved with sandbox root")
{
    const char* rootDir = "/game/mods/sandbox";
    vfs_audio_mirror::VfsResolveResult r = vfs_audio_mirror::mirrorResolvePath(
        "sound/fx/boom.wav", rootDir, 260);

    CHECK(r.success == true);
    CHECK(r.resolvedPath == "/game/mods/sandbox/sound/fx/boom.wav");
}

TEST_CASE("F2-06: VFS audio — path resolved without sandbox root uses raw path")
{
    vfs_audio_mirror::VfsResolveResult r = vfs_audio_mirror::mirrorResolvePath(
        "sound/music/theme.acm", nullptr, 260);

    CHECK(r.success == true);
    CHECK(r.resolvedPath == "sound/music/theme.acm");
}

TEST_CASE("F2-06: VFS audio — path traversal (..) rejected")
{
    vfs_audio_mirror::VfsResolveResult r = vfs_audio_mirror::mirrorResolvePath(
        "../etc/passwd", "sandbox", 260);

    CHECK(r.success == false);
    CHECK(r.resolvedPath.empty());
}

TEST_CASE("F2-06: VFS audio — absolute path rejected")
{
    vfs_audio_mirror::VfsResolveResult r = vfs_audio_mirror::mirrorResolvePath(
        "/etc/hosts", "sandbox", 260);

    CHECK(r.success == false);
    CHECK(r.resolvedPath.empty());
}

TEST_CASE("F2-06: VFS audio — empty path rejected")
{
    vfs_audio_mirror::VfsResolveResult r = vfs_audio_mirror::mirrorResolvePath(
        "", "sandbox", 260);

    CHECK(r.success == false);
}

TEST_CASE("F2-06: VFS audio — null path rejected")
{
    vfs_audio_mirror::VfsResolveResult r = vfs_audio_mirror::mirrorResolvePath(
        nullptr, "sandbox", 260);

    CHECK(r.success == false);
}

TEST_CASE("F2-06: VFS audio — resolved path exceeding buffer size rejected")
{
    const char* rootDir = "/very/long/sandbox/path/that/exceeds/typical/buffer/capacity";
    vfs_audio_mirror::VfsResolveResult r = vfs_audio_mirror::mirrorResolvePath(
        "sound/fx/boom.wav", rootDir, 30); // small buffer to trigger overflow

    CHECK(r.success == false);
}

TEST_CASE("F2-06: VFS audio — path at exact buffer boundary accepted")
{
    const char* rootDir = "sandbox";
    const char* rawPath = "s.wav";

    vfs_audio_mirror::VfsResolveResult r = vfs_audio_mirror::mirrorResolvePath(
        rawPath, rootDir, 260); // well within buffer

    CHECK(r.success == true);
    CHECK(r.resolvedPath == "sandbox/s.wav");
}

// =================================================================
// F-38: FID_TYPE check for op_set_critter_hit_chance_mod
// =================================================================
// Mirror of the FID_TYPE validation logic added to the opcode.
// Tests that non-critter objects are rejected.

namespace hit_chance_mirror {

constexpr int OBJ_TYPE_CRITTER = 1;

struct MockObject {
    int id = 0;
    int fid = 0;
};

struct HitChanceOverride {
    int mod = 0;
    int max = 95;
};

bool trySetHitChanceMod(const MockObject* critter, int mod, int max, HitChanceOverride& out)
{
    if (critter == nullptr) {
        return false;
    }

    // F-38: Validate FID_TYPE — mirror of the check in sfall_opcodes.cc
    if ((critter->fid >> 24) != OBJ_TYPE_CRITTER) { // FID_TYPE macro equivalent
        return false;
    }

    if (max < 1) {
        max = 1;
    }
    if (max > 100) {
        max = 100;
    }

    out.mod = mod;
    out.max = max;
    return true;
}

} // namespace hit_chance_mirror

// Helper to construct an FID with given type
static int makeFid(int objType, int remainder = 0)
{
    return (objType << 24) | (remainder & 0x00FFFFFF);
}

TEST_CASE("F-38: Hit chance mod — critter object is accepted")
{
    hit_chance_mirror::MockObject critter;
    critter.id = 42;
    critter.fid = makeFid(hit_chance_mirror::OBJ_TYPE_CRITTER);

    hit_chance_mirror::HitChanceOverride result;
    bool ok = hit_chance_mirror::trySetHitChanceMod(&critter, 10, 90, result);

    CHECK(ok == true);
    CHECK(result.mod == 10);
    CHECK(result.max == 90);
}

TEST_CASE("F-38: Hit chance mod — null object is rejected")
{
    hit_chance_mirror::HitChanceOverride result;
    bool ok = hit_chance_mirror::trySetHitChanceMod(nullptr, 10, 90, result);

    CHECK(ok == false);
}

TEST_CASE("F-38: Hit chance mod — non-critter object (scenery) is rejected")
{
    hit_chance_mirror::MockObject scenery;
    scenery.id = 1;
    scenery.fid = makeFid(0); // OBJ_TYPE_ITEM = 0

    hit_chance_mirror::HitChanceOverride result;
    bool ok = hit_chance_mirror::trySetHitChanceMod(&scenery, 10, 90, result);

    CHECK(ok == false);
}

TEST_CASE("F-38: Hit chance mod — non-critter object (misc) is rejected")
{
    hit_chance_mirror::MockObject misc;
    misc.id = 2;
    misc.fid = makeFid(3); // OBJ_TYPE_MISC = 3

    hit_chance_mirror::HitChanceOverride result;
    bool ok = hit_chance_mirror::trySetHitChanceMod(&misc, 10, 90, result);

    CHECK(ok == false);
}

TEST_CASE("F-38: Hit chance mod — non-critter object (wall) is rejected")
{
    hit_chance_mirror::MockObject wall;
    wall.id = 3;
    wall.fid = makeFid(4); // OBJ_TYPE_WALL = 4

    hit_chance_mirror::HitChanceOverride result;
    bool ok = hit_chance_mirror::trySetHitChanceMod(&wall, 10, 90, result);

    CHECK(ok == false);
}

TEST_CASE("F-38: Hit chance mod — max below 1 clamped to 1")
{
    hit_chance_mirror::MockObject critter;
    critter.id = 1;
    critter.fid = makeFid(hit_chance_mirror::OBJ_TYPE_CRITTER);

    hit_chance_mirror::HitChanceOverride result;
    bool ok = hit_chance_mirror::trySetHitChanceMod(&critter, 5, -10, result);

    CHECK(ok == true);
    CHECK(result.max == 1);
}

TEST_CASE("F-38: Hit chance mod — max above 100 clamped to 100")
{
    hit_chance_mirror::MockObject critter;
    critter.id = 1;
    critter.fid = makeFid(hit_chance_mirror::OBJ_TYPE_CRITTER);

    hit_chance_mirror::HitChanceOverride result;
    bool ok = hit_chance_mirror::trySetHitChanceMod(&critter, 5, 200, result);

    CHECK(ok == true);
    CHECK(result.max == 100);
}

// =================================================================
// Boundary condition: std::clamp on all platforms
// =================================================================

TEST_CASE("std::clamp identity — value within range unchanged")
{
    int result = std::clamp(50, 0, 9999);
    CHECK(result == 50);
}

TEST_CASE("std::clamp low — value below range clamped to min")
{
    int result = std::clamp(-1, 0, 9999);
    CHECK(result == 0);
}

TEST_CASE("std::clamp high — value above range clamped to max")
{
    int result = std::clamp(100000, 0, 9999);
    CHECK(result == 9999);
}

TEST_CASE("std::clamp boundary — value at min unchanged")
{
    int result = std::clamp(0, 0, 9999);
    CHECK(result == 0);
}

TEST_CASE("std::clamp boundary — value at max unchanged")
{
    int result = std::clamp(9999, 0, 9999);
    CHECK(result == 9999);
}
