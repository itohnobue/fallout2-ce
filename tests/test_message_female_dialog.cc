// Unit tests for sfall's FemaleDialogMsgs directory selection — the real
// messageListGetLocalizedDir (src/message.cc) plus a mirror of the call-site
// fallback used when the female directory is missing.
//
// This test LINKS message.cc (real implementation). Its external deps:
//   fileOpen/fileReadString/fileClose/compat_stricmp/debugPrint/settings —
//     provided by test_stubs (test_common_stubs.cc)
//   internal_* memory helpers, config, gContentConfig — test_sources
//   gFemaleDialogMsgs — sfall_config.cc (test_sources)
//   fileReadChar/fileTell/fileSeek/compat_strupr/randomBetween — local
//     stubs below (test_common_stubs.cc does not define these)
//
// The stubbed fileOpen always returns nullptr, so a messageListLoad lifecycle
// can only exercise the failure path; the dir-missing fallback decision is
// therefore mirrored here using the real dir helper, following the suite's
// local-mirror pattern (e.g. test_misc_ui_config_fixes.cc).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>
#include <string>

#include "db.h"
#include "message.h"
#include "platform_compat.h"
#include "random.h"
#include "sfall_config.h"

using namespace fallout;

// =============================================================
// Local stubs — symbols message.cc references that
// test_common_stubs.cc does not provide. Not exercised by the
// tests below (messageListGetLocalizedDir is pure string logic).
// =============================================================

namespace fallout {

int fileReadChar(File* /*stream*/)
{
    return -1;
}

int fileSeek(File* /*stream*/, long /*offset*/, int /*origin*/)
{
    return 0;
}

long fileTell(File* /*stream*/)
{
    return 0;
}

char* compat_strupr(char* string)
{
    return string;
}

int randomBetween(int /*min*/, int /*max*/)
{
    return 0;
}

} // namespace fallout

// =============================================================
// Helper mirror — the scripts.cc fallback decision.
//
// Mirrors scriptsGetMessageList (scripts.cc:3328-3352): the dialog dir is
// selected via messageListGetLocalizedDir, and when the load from that dir
// fails AND the female dir was selected, the caller resets the list and
// retries from the normal dialog dir. Returns the path that would actually
// be loaded.
// =============================================================

static std::string dialogFallbackPath(bool isFemale, bool femaleLoadSucceeds)
{
    const char* dialogDir = messageListGetLocalizedDir("dialog", false, isFemale);
    std::string path = std::string(dialogDir) + "\\script.msg";
    if (!femaleLoadSucceeds && std::strcmp(dialogDir, "dialog") != 0) {
        // Female dir selected but the file is missing → fall back to normal.
        path = "dialog\\script.msg";
    }
    return path;
}

// =============================================================
// messageListGetLocalizedDir — real implementation
// =============================================================

TEST_CASE("messageListGetLocalizedDir — FemaleDialogMsgs=0 (default)") {
    gFemaleDialogMsgs = 0;

    SUBCASE("female player still uses the normal dirs") {
        CHECK(std::strcmp(messageListGetLocalizedDir("dialog", false, true), "dialog") == 0);
        CHECK(std::strcmp(messageListGetLocalizedDir("cuts", true, true), "cuts") == 0);
    }

    SUBCASE("male player uses the normal dirs") {
        CHECK(std::strcmp(messageListGetLocalizedDir("dialog", false, false), "dialog") == 0);
        CHECK(std::strcmp(messageListGetLocalizedDir("cuts", true, false), "cuts") == 0);
    }
}

TEST_CASE("messageListGetLocalizedDir — FemaleDialogMsgs=1 (dialog_female only)") {
    gFemaleDialogMsgs = 1;

    SUBCASE("female dialog messages move to dialog_female") {
        CHECK(std::strcmp(messageListGetLocalizedDir("dialog", false, true), "dialog_female") == 0);
    }

    SUBCASE("level 1 does not affect cutscene subtitles") {
        CHECK(std::strcmp(messageListGetLocalizedDir("cuts", true, true), "cuts") == 0);
    }

    SUBCASE("male player keeps the normal dialog dir") {
        CHECK(std::strcmp(messageListGetLocalizedDir("dialog", false, false), "dialog") == 0);
    }
}

TEST_CASE("messageListGetLocalizedDir — FemaleDialogMsgs=2 (dialog_female + cuts_female)") {
    gFemaleDialogMsgs = 2;

    SUBCASE("female dialog messages move to dialog_female") {
        CHECK(std::strcmp(messageListGetLocalizedDir("dialog", false, true), "dialog_female") == 0);
    }

    SUBCASE("female cutscene subtitles move to cuts_female") {
        CHECK(std::strcmp(messageListGetLocalizedDir("cuts", true, true), "cuts_female") == 0);
    }

    SUBCASE("male player keeps both normal dirs") {
        CHECK(std::strcmp(messageListGetLocalizedDir("dialog", false, false), "dialog") == 0);
        CHECK(std::strcmp(messageListGetLocalizedDir("cuts", true, false), "cuts") == 0);
    }
}

TEST_CASE("messageListGetLocalizedDir — unrelated base dirs pass through") {
    gFemaleDialogMsgs = 2;

    CHECK(std::strcmp(messageListGetLocalizedDir("game", false, true), "game") == 0);
    CHECK(std::strcmp(messageListGetLocalizedDir("misc", false, true), "misc") == 0);
    CHECK(std::strcmp(messageListGetLocalizedDir("cuts", false, true), "cuts") == 0);

    gFemaleDialogMsgs = 0;
}

TEST_CASE("messageListGetLocalizedDir — null base dir") {
    gFemaleDialogMsgs = 2;
    CHECK(messageListGetLocalizedDir(nullptr, false, true) == nullptr);
    gFemaleDialogMsgs = 0;
}

// =============================================================
// Dir-missing fallback — mirror of scripts.cc + endgame.cc/main.cc
// decision (female dir selected → load fails → normal dir used)
// =============================================================

TEST_CASE("dir-missing fallback — female dir selected but file absent") {
    gFemaleDialogMsgs = 1;

    SUBCASE("female + level1 + missing dialog_female file → normal dialog dir") {
        CHECK(dialogFallbackPath(true, false) == "dialog\\script.msg");
    }

    SUBCASE("female + level1 + present dialog_female file → dialog_female dir") {
        CHECK(dialogFallbackPath(true, true) == "dialog_female\\script.msg");
    }

    SUBCASE("male + level1 → normal dialog dir regardless of load result") {
        CHECK(dialogFallbackPath(false, false) == "dialog\\script.msg");
        CHECK(dialogFallbackPath(false, true) == "dialog\\script.msg");
    }

    SUBCASE("feature disabled (level 0) → normal dialog dir") {
        gFemaleDialogMsgs = 0;
        CHECK(dialogFallbackPath(true, false) == "dialog\\script.msg");
    }

    gFemaleDialogMsgs = 0;
}

// =============================================================
// Message list lifecycle — real messageListLoad failure path.
//
// The stubbed fileOpen always returns nullptr, so a load from either dir
// fails cleanly: the female-dir attempt returns false and the list is left
// empty, which is the precondition the scripts.cc fallback (messageListFree +
// messageListInit + retry) relies on.
// =============================================================

TEST_CASE("messageListLoad — failure path leaves a clean list for the fallback retry") {
    MessageList list;
    REQUIRE(messageListInit(&list));

    gFemaleDialogMsgs = 1;
    // Female dialog file cannot be opened (fileOpen stub → nullptr) →
    // messageListLoad returns false without populating the list.
    CHECK_FALSE(messageListLoad(&list, "dialog_female\\script.msg"));
    CHECK(list.entries_num == 0);
    CHECK(list.entries == nullptr);

    // The fallback reset (messageListFree + messageListInit) is safe after a
    // failed female-dir load and leaves the list reusable for the retry.
    messageListFree(&list);
    messageListInit(&list);
    CHECK(list.entries_num == 0);

    gFemaleDialogMsgs = 0;
    messageListFree(&list);
}
