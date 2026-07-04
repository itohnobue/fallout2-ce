// Unit tests for config.cc — INI config data structure operations.
//
// Tests: configInit, configSetString, configGetString, configSetInt,
//        configGetInt, configSetBool, configGetBool, configGetDouble,
//        configFree, ScopedConfig

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "config.h"
#include "dictionary.h"

using namespace fallout;

TEST_CASE("configInit / configFree lifecycle")
{
    Config cfg;
    memset(&cfg, 0, sizeof(cfg));

    SUBCASE("configInit succeeds with valid pointer")
    {
        CHECK(configInit(&cfg));
        CHECK(cfg.isInitialized());
        CHECK(cfg.entriesLength == 0);
        configFree(&cfg);
        CHECK_FALSE(cfg.isInitialized());
    }

    SUBCASE("configInit fails with nullptr")
    {
        CHECK_FALSE(configInit(nullptr));
    }

    SUBCASE("double init is valid (configFree is a no-op on uninitialized)")
    {
        CHECK(configInit(&cfg));
        CHECK(configInit(&cfg)); // already initialized — second call succeeds
        configFree(&cfg);
    }
}

TEST_CASE("configSetString / configGetString")
{
    Config cfg;
    configInit(&cfg);

    SUBCASE("set and get a string value")
    {
        CHECK(configSetString(&cfg, "Section", "Key", "Value"));
        char* val = nullptr;
        CHECK(configGetString(&cfg, "Section", "Key", &val));
        CHECK(val != nullptr);
        CHECK(strcmp(val, "Value") == 0);
    }

    SUBCASE("get non-existent section returns false")
    {
        char* val = nullptr;
        CHECK_FALSE(configGetString(&cfg, "NoSection", "Key", &val));
    }

    SUBCASE("get non-existent key returns false")
    {
        configSetString(&cfg, "Section", "Key", "Value");
        char* val = nullptr;
        CHECK_FALSE(configGetString(&cfg, "Section", "OtherKey", &val));
    }

    SUBCASE("overwrite existing key")
    {
        configSetString(&cfg, "Section", "Key", "First");
        configSetString(&cfg, "Section", "Key", "Second");
        char* val = nullptr;
        CHECK(configGetString(&cfg, "Section", "Key", &val));
        CHECK(strcmp(val, "Second") == 0);
    }

    SUBCASE("null arguments return false")
    {
        CHECK_FALSE(configSetString(nullptr, "S", "K", "V"));
        CHECK_FALSE(configSetString(&cfg, nullptr, "K", "V"));
        CHECK_FALSE(configSetString(&cfg, "S", nullptr, "V"));
        CHECK_FALSE(configSetString(&cfg, "S", "K", nullptr));

        char* val = nullptr;
        CHECK_FALSE(configGetString(nullptr, "S", "K", &val));
        CHECK_FALSE(configGetString(&cfg, nullptr, "K", &val));
        CHECK_FALSE(configGetString(&cfg, "S", nullptr, &val));
        CHECK_FALSE(configGetString(&cfg, "S", "K", nullptr));
    }

    SUBCASE("multiple sections and keys")
    {
        configSetString(&cfg, "Video", "Width", "1920");
        configSetString(&cfg, "Video", "Height", "1080");
        configSetString(&cfg, "Audio", "Volume", "75");

        char* val = nullptr;
        CHECK(configGetString(&cfg, "Video", "Width", &val));
        CHECK(strcmp(val, "1920") == 0);
        CHECK(configGetString(&cfg, "Video", "Height", &val));
        CHECK(strcmp(val, "1080") == 0);
        CHECK(configGetString(&cfg, "Audio", "Volume", &val));
        CHECK(strcmp(val, "75") == 0);
    }

    SUBCASE("default value overload")
    {
        char* val = nullptr;
        const char* defaultVal = "Default";
        // Non-existent key returns defaultValue
        CHECK(configGetString(&cfg, "Section", "Missing", &val, defaultVal));
        CHECK(val == defaultVal); // pointer comparison — returns the default pointer directly

        // Empty string also returns defaultValue
        configSetString(&cfg, "Section", "Empty", "");
        CHECK(configGetString(&cfg, "Section", "Empty", &val, defaultVal));
        CHECK(val == defaultVal);
    }

    configFree(&cfg);
}

TEST_CASE("configSetInt / configGetInt")
{
    Config cfg;
    configInit(&cfg);

    SUBCASE("set and get integer values")
    {
        CHECK(configSetInt(&cfg, "Misc", "Speed", 42));
        int val = 0;
        CHECK(configGetInt(&cfg, "Misc", "Speed", &val));
        CHECK(val == 42);
    }

    SUBCASE("get non-existent int returns false")
    {
        int val = 0;
        CHECK_FALSE(configGetInt(&cfg, "Misc", "Nope", &val));
    }

    SUBCASE("get int with default value")
    {
        int val = 0;
        CHECK(configGetInt(&cfg, "Misc", "Missing", &val, 99));
        CHECK(val == 99);
    }

    SUBCASE("set negative integers")
    {
        configSetInt(&cfg, "Misc", "Offset", -5);
        int val = 0;
        CHECK(configGetInt(&cfg, "Misc", "Offset", &val));
        CHECK(val == -5);
    }

    configFree(&cfg);
}

TEST_CASE("configSetBool / configGetBool")
{
    Config cfg;
    configInit(&cfg);

    SUBCASE("set and get boolean values")
    {
        configSetBool(&cfg, "Flags", "Enabled", true);
        configSetBool(&cfg, "Flags", "Debug", false);

        bool val = false;
        CHECK(configGetBool(&cfg, "Flags", "Enabled", &val));
        CHECK(val == true);
        CHECK(configGetBool(&cfg, "Flags", "Debug", &val));
        CHECK(val == false);
    }

    SUBCASE("get bool with default")
    {
        bool val = false;
        CHECK(configGetBool(&cfg, "Flags", "Missing", &val, true));
        CHECK(val == true);
    }

    configFree(&cfg);
}

TEST_CASE("ScopedConfig RAII")
{
    SUBCASE("default-constructed ScopedConfig is initialized")
    {
        ScopedConfig sc;
        CHECK(sc.isInitialized());
        CHECK(sc.get() != nullptr);
        // Setting a value works
        CHECK(configSetString(sc.get(), "Test", "Key", "Val"));
    }
    // ScopedConfig destructor calls configFree.

    SUBCASE("operator bool reflects config state")
    {
        ScopedConfig sc;
        CHECK(static_cast<bool>(sc)); // initialized but empty is still valid
    }
}

TEST_CASE("configParseCommandLineArguments")
{
    Config cfg;
    configInit(&cfg);

    SUBCASE("parses [Section]key=value format")
    {
        // NOTE: configParseCommandLineArguments modifies argv strings in-place
        // (writes null terminators at bracket positions). Use mutable buffers.
        char arg1[] = "[Video]Width=1920";
        char arg2[] = "[Video]Height=1080";
        char arg3[] = "not-a-config-arg";
        char* argv[] = { arg1, arg2, arg3 };
        CHECK(configParseCommandLineArguments(&cfg, 3, argv));

        char* val = nullptr;
        CHECK(configGetString(&cfg, "Video", "Width", &val));
        CHECK(strcmp(val, "1920") == 0);
        CHECK(configGetString(&cfg, "Video", "Height", &val));
        CHECK(strcmp(val, "1080") == 0);
    }

    SUBCASE("null config returns false")
    {
        char arg1[] = "[S]K=V";
        char* argv[] = { arg1 };
        CHECK_FALSE(configParseCommandLineArguments(nullptr, 1, argv));
    }

    SUBCASE("invalid format is silently ignored")
    {
        char arg1[] = "plain-text";
        char* argv[] = { arg1 };
        CHECK(configParseCommandLineArguments(&cfg, 1, argv));
        CHECK(cfg.entriesLength == 0); // nothing added
    }

    configFree(&cfg);
}

TEST_CASE("configGetDouble")
{
    Config cfg;
    configInit(&cfg);

    configSetString(&cfg, "Physics", "Gravity", "9.81");
    double val = 0.0;
    CHECK(configGetDouble(&cfg, "Physics", "Gravity", &val));
    CHECK(val == doctest::Approx(9.81));

    configFree(&cfg);
}
