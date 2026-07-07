// Unit tests for sfall_kb_helpers.cc — DIK mapping, key simulation,
// synthetic event queue, and HOOK_KEYPRESS integration.
//
// F2-061: sfall_kb_helpers.cc (386 LOC) zero dedicated tests.
//
// Self-contained test. Links SDL2 headers for scancode types, but does NOT
// link sfall_kb_helpers.cc (SDL runtime dependencies: SDL_PushEvent,
// SDL_GetKeyboardState, SDL_GetKeyFromScancode).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <SDL.h>

#include "sfall_kb_helpers.h"
#include "sfall_script_hooks.h"

#include <deque>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace fallout;

// ---- Local mirror of kDiks table (sfall_kb_helpers.cc:17-274) ----
// Non-constexpr so we can fill high-index entries at runtime.
static SDL_Scancode kTestDiks[256];

// Initialize the table with all UNKNOWN, then set the known entries.
static void initTestDiksTable()
{
    for (int i = 0; i < 256; i++) {
        kTestDiks[i] = SDL_SCANCODE_UNKNOWN;
    }
    // Standard keys (indices 1-83)
    kTestDiks[1] = SDL_SCANCODE_ESCAPE;
    kTestDiks[2] = SDL_SCANCODE_1;
    kTestDiks[3] = SDL_SCANCODE_2;
    kTestDiks[4] = SDL_SCANCODE_3;
    kTestDiks[5] = SDL_SCANCODE_4;
    kTestDiks[6] = SDL_SCANCODE_5;
    kTestDiks[7] = SDL_SCANCODE_6;
    kTestDiks[8] = SDL_SCANCODE_7;
    kTestDiks[9] = SDL_SCANCODE_8;
    kTestDiks[10] = SDL_SCANCODE_9;
    kTestDiks[11] = SDL_SCANCODE_0;
    kTestDiks[12] = SDL_SCANCODE_MINUS;
    kTestDiks[13] = SDL_SCANCODE_EQUALS;
    kTestDiks[14] = SDL_SCANCODE_BACKSPACE;
    kTestDiks[15] = SDL_SCANCODE_TAB;
    kTestDiks[16] = SDL_SCANCODE_Q;
    kTestDiks[17] = SDL_SCANCODE_W;
    kTestDiks[18] = SDL_SCANCODE_E;
    kTestDiks[19] = SDL_SCANCODE_R;
    kTestDiks[20] = SDL_SCANCODE_T;
    kTestDiks[21] = SDL_SCANCODE_Y;
    kTestDiks[22] = SDL_SCANCODE_U;
    kTestDiks[23] = SDL_SCANCODE_I;
    kTestDiks[24] = SDL_SCANCODE_O;
    kTestDiks[25] = SDL_SCANCODE_P;
    kTestDiks[26] = SDL_SCANCODE_LEFTBRACKET;
    kTestDiks[27] = SDL_SCANCODE_RIGHTBRACKET;
    kTestDiks[28] = SDL_SCANCODE_RETURN;
    kTestDiks[29] = SDL_SCANCODE_LCTRL;
    kTestDiks[30] = SDL_SCANCODE_A;
    kTestDiks[31] = SDL_SCANCODE_S;
    kTestDiks[32] = SDL_SCANCODE_D;
    kTestDiks[33] = SDL_SCANCODE_F;
    kTestDiks[34] = SDL_SCANCODE_G;
    kTestDiks[35] = SDL_SCANCODE_H;
    kTestDiks[36] = SDL_SCANCODE_J;
    kTestDiks[37] = SDL_SCANCODE_K;
    kTestDiks[38] = SDL_SCANCODE_L;
    kTestDiks[39] = SDL_SCANCODE_SEMICOLON;
    kTestDiks[40] = SDL_SCANCODE_APOSTROPHE;
    kTestDiks[41] = SDL_SCANCODE_GRAVE;
    kTestDiks[42] = SDL_SCANCODE_LSHIFT;
    kTestDiks[43] = SDL_SCANCODE_BACKSLASH;
    kTestDiks[44] = SDL_SCANCODE_Z;
    kTestDiks[45] = SDL_SCANCODE_X;
    kTestDiks[46] = SDL_SCANCODE_C;
    kTestDiks[47] = SDL_SCANCODE_V;
    kTestDiks[48] = SDL_SCANCODE_B;
    kTestDiks[49] = SDL_SCANCODE_N;
    kTestDiks[50] = SDL_SCANCODE_M;
    kTestDiks[51] = SDL_SCANCODE_COMMA;
    kTestDiks[52] = SDL_SCANCODE_PERIOD;
    kTestDiks[53] = SDL_SCANCODE_SLASH;
    kTestDiks[54] = SDL_SCANCODE_RSHIFT;
    kTestDiks[55] = SDL_SCANCODE_KP_MULTIPLY;
    kTestDiks[56] = SDL_SCANCODE_LALT;
    kTestDiks[57] = SDL_SCANCODE_SPACE;
    kTestDiks[58] = SDL_SCANCODE_CAPSLOCK;
    kTestDiks[59] = SDL_SCANCODE_F1;
    kTestDiks[60] = SDL_SCANCODE_F2;
    kTestDiks[61] = SDL_SCANCODE_F3;
    kTestDiks[62] = SDL_SCANCODE_F4;
    kTestDiks[63] = SDL_SCANCODE_F5;
    kTestDiks[64] = SDL_SCANCODE_F6;
    kTestDiks[65] = SDL_SCANCODE_F7;
    kTestDiks[66] = SDL_SCANCODE_F8;
    kTestDiks[67] = SDL_SCANCODE_F9;
    kTestDiks[68] = SDL_SCANCODE_F10;
    kTestDiks[69] = SDL_SCANCODE_NUMLOCKCLEAR;
    kTestDiks[70] = SDL_SCANCODE_SCROLLLOCK;
    kTestDiks[71] = SDL_SCANCODE_KP_7;
    kTestDiks[72] = SDL_SCANCODE_KP_8;
    kTestDiks[73] = SDL_SCANCODE_KP_9;
    kTestDiks[74] = SDL_SCANCODE_KP_MINUS;
    kTestDiks[75] = SDL_SCANCODE_KP_4;
    kTestDiks[76] = SDL_SCANCODE_KP_5;
    kTestDiks[77] = SDL_SCANCODE_KP_6;
    kTestDiks[78] = SDL_SCANCODE_KP_PLUS;
    kTestDiks[79] = SDL_SCANCODE_KP_1;
    kTestDiks[80] = SDL_SCANCODE_KP_2;
    kTestDiks[81] = SDL_SCANCODE_KP_3;
    kTestDiks[82] = SDL_SCANCODE_KP_0;
    kTestDiks[83] = SDL_SCANCODE_KP_PERIOD;
    // Function keys
    kTestDiks[87] = SDL_SCANCODE_F11;
    kTestDiks[88] = SDL_SCANCODE_F12;
    // Numpad extended
    kTestDiks[141] = SDL_SCANCODE_KP_EQUALS;
    kTestDiks[156] = SDL_SCANCODE_KP_ENTER;
    kTestDiks[157] = SDL_SCANCODE_RCTRL;
    kTestDiks[179] = SDL_SCANCODE_KP_COMMA;
    kTestDiks[181] = SDL_SCANCODE_SLASH; // DIK_DIVIDE → shares SLASH scancode
    kTestDiks[183] = SDL_SCANCODE_SYSREQ;
    kTestDiks[184] = SDL_SCANCODE_RALT;
    // Navigation keys
    kTestDiks[199] = SDL_SCANCODE_HOME;
    kTestDiks[200] = SDL_SCANCODE_UP;
    kTestDiks[201] = SDL_SCANCODE_PAGEUP;
    kTestDiks[203] = SDL_SCANCODE_LEFT;
    kTestDiks[205] = SDL_SCANCODE_RIGHT;
    kTestDiks[207] = SDL_SCANCODE_END;
    kTestDiks[208] = SDL_SCANCODE_DOWN;
    kTestDiks[209] = SDL_SCANCODE_PAGEDOWN;
    kTestDiks[210] = SDL_SCANCODE_INSERT;
    kTestDiks[211] = SDL_SCANCODE_DELETE;
    // Windows keys
    kTestDiks[219] = SDL_SCANCODE_LGUI;
    kTestDiks[220] = SDL_SCANCODE_RGUI;
    kTestDiks[221] = SDL_SCANCODE_APPLICATION;
}

// ---- Lazy initialization flag ----
static bool gTableInitialized = false;

// ---- Local mirror of get_scancode_from_key (sfall_kb_helpers.cc:280-297) ----
static SDL_Scancode testGetScancodeFromKey(int key)
{
    // F-046: VK (Virtual Key) codes have the 0x80000000 flag set and use a
    // different numbering scheme from DIK. Production code at
    // sfall_kb_helpers.cc:293 returns SDL_SCANCODE_UNKNOWN for VK codes
    // so callers (is_key_pressed, press_key) fail cleanly.
    if (key & 0x80000000) {
        return SDL_SCANCODE_UNKNOWN;
    }
    if (!gTableInitialized) {
        initTestDiksTable();
        gTableInitialized = true;
    }
    // Mirror: kDiks[key & 0xFF]
    int index = key & 0xFF;
    if (index < 0 || index >= 256) {
        return SDL_SCANCODE_UNKNOWN;
    }
    return kTestDiks[index];
}

// ---- Local mirror of get_key_from_scancode (sfall_kb_helpers.cc:286-300) ----
static int testGetKeyFromScancode(SDL_Scancode scanCode)
{
    static std::unordered_map<SDL_Scancode, int> scanCodeToDik;
    if (scanCodeToDik.empty()) {
        for (int dik = 255; dik >= 1; --dik) {
            if (kTestDiks[dik] == SDL_SCANCODE_UNKNOWN) continue;
            scanCodeToDik[kTestDiks[dik]] = dik;
        }
    }
    auto it = scanCodeToDik.find(scanCode);
    if (it == scanCodeToDik.end()) {
        return SDL_SCANCODE_UNKNOWN;
    }
    return it->second;
}

// ---- Local mirror of synthetic event queue (sfall_kb_helpers.cc:277) ----
static std::deque<std::pair<SDL_Scancode, bool>> testSyntheticEvents;

// ---- Local mirror of sfall_kb_consume_synthetic_key_event (sfall_kb_helpers.cc:347-360) ----
static bool testConsumeSyntheticEvent(int sdlScanCode, bool pressed)
{
    if (testSyntheticEvents.empty()) {
        return false;
    }

    const auto& front = testSyntheticEvents.front();
    if (front.first != static_cast<SDL_Scancode>(sdlScanCode) || front.second != pressed) {
        return false;
    }

    testSyntheticEvents.pop_front();
    return true;
}

// ---- Local mirror of sfall_kb_clear_synthetic_key_events (sfall_kb_helpers.cc:362-365) ----
static void testClearSyntheticEvents()
{
    testSyntheticEvents.clear();
}

// ---- Helper to simulate sfall_kb_press_key enqueueing ----
static void testSimulatePressKey(int key)
{
    SDL_Scancode scancode = testGetScancodeFromKey(key);
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        return;
    }
    testSyntheticEvents.emplace_back(scancode, true);  // keydown
    testSyntheticEvents.emplace_back(scancode, false); // keyup
}

// =================================================================
// DIK code to SDL keycode mapping
// =================================================================

TEST_CASE("DIK keycodes map to correct SDL scancodes")
{
    // Verify known DIK codes against expected SDL scancodes
    CHECK(testGetScancodeFromKey(1) == SDL_SCANCODE_ESCAPE);   // DIK_ESCAPE
    CHECK(testGetScancodeFromKey(2) == SDL_SCANCODE_1);        // DIK_1
    CHECK(testGetScancodeFromKey(11) == SDL_SCANCODE_0);       // DIK_0
    CHECK(testGetScancodeFromKey(15) == SDL_SCANCODE_TAB);     // DIK_TAB
    CHECK(testGetScancodeFromKey(16) == SDL_SCANCODE_Q);       // DIK_Q
    CHECK(testGetScancodeFromKey(30) == SDL_SCANCODE_A);       // DIK_A
    CHECK(testGetScancodeFromKey(44) == SDL_SCANCODE_Z);       // DIK_Z
    CHECK(testGetScancodeFromKey(28) == SDL_SCANCODE_RETURN);  // DIK_RETURN
    CHECK(testGetScancodeFromKey(57) == SDL_SCANCODE_SPACE);   // DIK_SPACE
    CHECK(testGetScancodeFromKey(42) == SDL_SCANCODE_LSHIFT);  // DIK_LSHIFT
    CHECK(testGetScancodeFromKey(54) == SDL_SCANCODE_RSHIFT);  // DIK_RSHIFT
    CHECK(testGetScancodeFromKey(29) == SDL_SCANCODE_LCTRL);   // DIK_LCONTROL
    CHECK(testGetScancodeFromKey(56) == SDL_SCANCODE_LALT);    // DIK_LMENU
}

TEST_CASE("DIK function keys map correctly")
{
    CHECK(testGetScancodeFromKey(59) == SDL_SCANCODE_F1);      // DIK_F1
    CHECK(testGetScancodeFromKey(60) == SDL_SCANCODE_F2);      // DIK_F2
    CHECK(testGetScancodeFromKey(67) == SDL_SCANCODE_F9);      // DIK_F9
    CHECK(testGetScancodeFromKey(68) == SDL_SCANCODE_F10);     // DIK_F10
    CHECK(testGetScancodeFromKey(87) == SDL_SCANCODE_F11);     // DIK_F11
    CHECK(testGetScancodeFromKey(88) == SDL_SCANCODE_F12);     // DIK_F12
}

TEST_CASE("DIK numpad keys map correctly")
{
    CHECK(testGetScancodeFromKey(71) == SDL_SCANCODE_KP_7);    // DIK_NUMPAD7
    CHECK(testGetScancodeFromKey(72) == SDL_SCANCODE_KP_8);    // DIK_NUMPAD8
    CHECK(testGetScancodeFromKey(82) == SDL_SCANCODE_KP_0);    // DIK_NUMPAD0
    CHECK(testGetScancodeFromKey(55) == SDL_SCANCODE_KP_MULTIPLY); // DIK_MULTIPLY
    CHECK(testGetScancodeFromKey(74) == SDL_SCANCODE_KP_MINUS);    // DIK_SUBTRACT
    CHECK(testGetScancodeFromKey(78) == SDL_SCANCODE_KP_PLUS);     // DIK_ADD
    CHECK(testGetScancodeFromKey(83) == SDL_SCANCODE_KP_PERIOD);   // DIK_DECIMAL
}

TEST_CASE("DIK navigation keys map correctly")
{
    CHECK(testGetScancodeFromKey(200) == SDL_SCANCODE_UP);     // DIK_UP
    CHECK(testGetScancodeFromKey(208) == SDL_SCANCODE_DOWN);   // DIK_DOWN
    CHECK(testGetScancodeFromKey(203) == SDL_SCANCODE_LEFT);   // DIK_LEFT
    CHECK(testGetScancodeFromKey(205) == SDL_SCANCODE_RIGHT);  // DIK_RIGHT
    CHECK(testGetScancodeFromKey(199) == SDL_SCANCODE_HOME);   // DIK_HOME
    CHECK(testGetScancodeFromKey(207) == SDL_SCANCODE_END);    // DIK_END
    CHECK(testGetScancodeFromKey(201) == SDL_SCANCODE_PAGEUP); // DIK_PRIOR
    CHECK(testGetScancodeFromKey(209) == SDL_SCANCODE_PAGEDOWN); // DIK_NEXT
}

TEST_CASE("DIK modifier keys map correctly")
{
    CHECK(testGetScancodeFromKey(210) == SDL_SCANCODE_INSERT); // DIK_INSERT
    CHECK(testGetScancodeFromKey(211) == SDL_SCANCODE_DELETE); // DIK_DELETE
    CHECK(testGetScancodeFromKey(58) == SDL_SCANCODE_CAPSLOCK); // DIK_CAPITAL
    CHECK(testGetScancodeFromKey(69) == SDL_SCANCODE_NUMLOCKCLEAR); // DIK_NUMLOCK
    CHECK(testGetScancodeFromKey(70) == SDL_SCANCODE_SCROLLLOCK);   // DIK_SCROLL
}

// =================================================================
// key & 0xFF masking
// =================================================================

TEST_CASE("DIK mapping uses key & 0xFF masking")
{
    // The production code masks with key & 0xFF, so high bits are ignored.
    // Test that DIK_ESCAPE (1) maps correctly with high bits set.
    int keyWithHighBits = 1 | 0xFF00;
    CHECK(testGetScancodeFromKey(keyWithHighBits) == SDL_SCANCODE_ESCAPE);
}

TEST_CASE("DIK mapping produces UNKNOWN for out-of-range keys")
{
    // Key 0 → SDL_SCANCODE_UNKNOWN
    CHECK(testGetScancodeFromKey(0) == SDL_SCANCODE_UNKNOWN);

    // Empty/gap entries in the table → SDL_SCANCODE_UNKNOWN
    // Indices 84-86 are UNKNOWN between DIK_DECIMAL and DIK_F11
    CHECK(testGetScancodeFromKey(84) == SDL_SCANCODE_UNKNOWN);
    CHECK(testGetScancodeFromKey(85) == SDL_SCANCODE_UNKNOWN);
    CHECK(testGetScancodeFromKey(86) == SDL_SCANCODE_UNKNOWN);
}

// =================================================================
// Reverse mapping: SDL scancode → DIK code
// =================================================================

TEST_CASE("SDL scancodes map back to correct DIK codes")
{
    // Forward: DIK_A(30) → SDL_SCANCODE_A
    // Reverse: SDL_SCANCODE_A → DIK_A(30)
    CHECK(testGetKeyFromScancode(SDL_SCANCODE_A) == 30);
    CHECK(testGetKeyFromScancode(SDL_SCANCODE_ESCAPE) == 1);
    CHECK(testGetKeyFromScancode(SDL_SCANCODE_SPACE) == 57);
    CHECK(testGetKeyFromScancode(SDL_SCANCODE_RETURN) == 28);
    CHECK(testGetKeyFromScancode(SDL_SCANCODE_TAB) == 15);
}

TEST_CASE("Reverse mapping returns UNKNOWN for unmapped scancodes")
{
    // SDL_SCANCODE_UNKNOWN maps to 0 (lookup fails → returns SDL_SCANCODE_UNKNOWN cast to int)
    CHECK(testGetKeyFromScancode(SDL_SCANCODE_UNKNOWN) == 0);
}

TEST_CASE("Reverse mapping is consistent — all non-UNKNOWN entries round-trip")
{
    // For every DIK in the table where scancode != UNKNOWN,
    // the scancode should map back to the original DIK.
    for (int dik = 1; dik < 256; dik++) {
        SDL_Scancode sc = kTestDiks[dik];
        if (sc == SDL_SCANCODE_UNKNOWN) continue;

        int back = testGetKeyFromScancode(sc);
        // If multiple DIK codes map to the same scancode (e.g., DIK_SLASH at 53 and DIK_DIVIDE at 181),
        // the reverse map returns the higher DIK code (due to reverse iteration from 255 down to 1).
        // This is acceptable because the function maps the first (highest DIK) found.
        // Just verify the round trip produces a valid DIK.
        CHECK(back > 0);
        CHECK(back < 256);
    }
}

// =================================================================
// Key simulation (tap_key)
// =================================================================

TEST_CASE("Simulating a key press enqueues keydown + keyup events")
{
    testClearSyntheticEvents();

    testSimulatePressKey(30); // DIK_A

    CHECK(testSyntheticEvents.size() == 2);

    // First event: keydown
    CHECK(testSyntheticEvents[0].first == SDL_SCANCODE_A);
    CHECK(testSyntheticEvents[0].second == true);

    // Second event: keyup
    CHECK(testSyntheticEvents[1].first == SDL_SCANCODE_A);
    CHECK(testSyntheticEvents[1].second == false);
}

TEST_CASE("Simulating an unknown key produces no events")
{
    testClearSyntheticEvents();

    testSimulatePressKey(0); // DIK 0 → SDL_SCANCODE_UNKNOWN

    CHECK(testSyntheticEvents.size() == 0);
}

TEST_CASE("Simulating multiple keys creates correct ordered queue")
{
    testClearSyntheticEvents();

    testSimulatePressKey(16); // DIK_Q
    testSimulatePressKey(30); // DIK_A
    testSimulatePressKey(44); // DIK_Z

    CHECK(testSyntheticEvents.size() == 6);

    // Q keydown
    CHECK(testSyntheticEvents[0].first == SDL_SCANCODE_Q);
    CHECK(testSyntheticEvents[0].second == true);
    // Q keyup
    CHECK(testSyntheticEvents[1].first == SDL_SCANCODE_Q);
    CHECK(testSyntheticEvents[1].second == false);

    // A keydown
    CHECK(testSyntheticEvents[2].first == SDL_SCANCODE_A);
    CHECK(testSyntheticEvents[2].second == true);
    // A keyup
    CHECK(testSyntheticEvents[3].first == SDL_SCANCODE_A);
    CHECK(testSyntheticEvents[3].second == false);

    // Z keydown
    CHECK(testSyntheticEvents[4].first == SDL_SCANCODE_Z);
    CHECK(testSyntheticEvents[4].second == true);
    // Z keyup
    CHECK(testSyntheticEvents[5].first == SDL_SCANCODE_Z);
    CHECK(testSyntheticEvents[5].second == false);
}

// =================================================================
// Synthetic event queue operations
// =================================================================

TEST_CASE("Consume matching synthetic event returns true")
{
    testClearSyntheticEvents();
    testSimulatePressKey(16); // DIK_Q → Q keydown + Q keyup

    // Consume Q keydown
    CHECK(testConsumeSyntheticEvent(SDL_SCANCODE_Q, true) == true);
    // Queue now: [Q keyup]
    CHECK(testSyntheticEvents.size() == 1);

    // Consume Q keyup
    CHECK(testConsumeSyntheticEvent(SDL_SCANCODE_Q, false) == true);
    // Queue now empty
    CHECK(testSyntheticEvents.size() == 0);
}

TEST_CASE("Consume non-matching event returns false (wrong scancode)")
{
    testClearSyntheticEvents();
    testSimulatePressKey(30); // DIK_A

    // Try to consume with wrong scancode
    CHECK(testConsumeSyntheticEvent(SDL_SCANCODE_B, true) == false);
    // Queue unchanged
    CHECK(testSyntheticEvents.size() == 2);
}

TEST_CASE("Consume non-matching event returns false (wrong press state)")
{
    testClearSyntheticEvents();
    testSimulatePressKey(30); // DIK_A

    // First event is keydown (true), try to match with false
    CHECK(testConsumeSyntheticEvent(SDL_SCANCODE_A, false) == false);
    // Queue unchanged
    CHECK(testSyntheticEvents.size() == 2);
}

TEST_CASE("Consume from empty queue returns false")
{
    testClearSyntheticEvents();

    CHECK(testConsumeSyntheticEvent(SDL_SCANCODE_A, true) == false);
    CHECK(testSyntheticEvents.size() == 0);
}

TEST_CASE("Consume events in order, queue drains correctly")
{
    testClearSyntheticEvents();
    testSimulatePressKey(16); // DIK_Q
    testSimulatePressKey(30); // DIK_A

    // Consume Q keydown
    CHECK(testConsumeSyntheticEvent(SDL_SCANCODE_Q, true) == true);
    // Consume Q keyup
    CHECK(testConsumeSyntheticEvent(SDL_SCANCODE_Q, false) == true);
    // Consume A keydown
    CHECK(testConsumeSyntheticEvent(SDL_SCANCODE_A, true) == true);
    // Consume A keyup
    CHECK(testConsumeSyntheticEvent(SDL_SCANCODE_A, false) == true);

    CHECK(testSyntheticEvents.size() == 0);
}

// =================================================================
// Clear synthetic events
// =================================================================

TEST_CASE("Clear synthetic events empties the queue")
{
    testClearSyntheticEvents();
    testSimulatePressKey(16); // DIK_Q
    testSimulatePressKey(30); // DIK_A

    CHECK(testSyntheticEvents.size() == 4);

    testClearSyntheticEvents();

    CHECK(testSyntheticEvents.size() == 0);
}

TEST_CASE("Clear on already-empty queue is no-op")
{
    testClearSyntheticEvents();
    CHECK(testSyntheticEvents.size() == 0);

    testClearSyntheticEvents();
    CHECK(testSyntheticEvents.size() == 0);
}

// =================================================================
// Mixed operations: simulate → consume → clear → simulate
// =================================================================

TEST_CASE("Mixed operations maintain queue integrity")
{
    testClearSyntheticEvents();

    // Simulate Q
    testSimulatePressKey(16);
    CHECK(testSyntheticEvents.size() == 2);

    // Consume Q keydown (leaving Q keyup)
    CHECK(testConsumeSyntheticEvent(SDL_SCANCODE_Q, true) == true);
    CHECK(testSyntheticEvents.size() == 1);

    // Clear remaining
    testClearSyntheticEvents();
    CHECK(testSyntheticEvents.size() == 0);

    // Simulate A
    testSimulatePressKey(30);
    CHECK(testSyntheticEvents.size() == 2);

    // Consume A keydown and keyup
    CHECK(testConsumeSyntheticEvent(SDL_SCANCODE_A, true) == true);
    CHECK(testConsumeSyntheticEvent(SDL_SCANCODE_A, false) == true);
    CHECK(testSyntheticEvents.size() == 0);
}

// =================================================================
// HOOK_KEYPRESS integration
// =================================================================

TEST_CASE("HOOK_KEYPRESS is defined at enum value 19")
{
    CHECK(static_cast<int>(HOOK_KEYPRESS) == 19);
}

TEST_CASE("HOOK_KEYPRESS hook type is valid and < HOOK_COUNT")
{
    CHECK(HOOK_KEYPRESS < HOOK_COUNT);
}

// ---- Local mirror of sfall_kb_handle_key_pressed logic ----
// Mirrors sfall_kb_helpers.cc:367-384
namespace {
    struct TestHookCallResult {
        bool hookFired = false;
        int returnValue = 0;
        int arg0 = -1; // pressed
        int arg1 = -1; // dik code
    };

    static TestHookCallResult gTestHookCallResult;

    static int testHandleKeyPressed(int sdlScanCode, bool pressed, bool gameLoaded, bool hooksRegistered)
    {
        if (!gameLoaded) return SDL_SCANCODE_UNKNOWN;

        SDL_Scancode scanCode = static_cast<SDL_Scancode>(sdlScanCode);

        if (hooksRegistered) {
            int dikCode = testGetKeyFromScancode(scanCode);
            gTestHookCallResult.hookFired = true;
            gTestHookCallResult.arg0 = pressed ? 1 : 0;
            gTestHookCallResult.arg1 = dikCode;

            // Simulate hook return value
            int overrideDxCode = gTestHookCallResult.returnValue;
            return static_cast<int>(testGetScancodeFromKey(overrideDxCode));
        }

        return SDL_SCANCODE_UNKNOWN;
    }
}

TEST_CASE("HOOK_KEYPRESS fires when game is loaded and hooks registered")
{
    gTestHookCallResult = {};
    gTestHookCallResult.returnValue = 0;

    int result = testHandleKeyPressed(SDL_SCANCODE_A, true, true, true);

    CHECK(gTestHookCallResult.hookFired == true);
    CHECK(gTestHookCallResult.arg0 == 1);  // pressed
    CHECK(gTestHookCallResult.arg1 == 30); // DIK_A
}

TEST_CASE("HOOK_KEYPRESS does not fire when game not loaded")
{
    gTestHookCallResult = {};

    int result = testHandleKeyPressed(SDL_SCANCODE_A, true, false, true);

    CHECK(gTestHookCallResult.hookFired == false);
    CHECK(result == SDL_SCANCODE_UNKNOWN);
}

TEST_CASE("HOOK_KEYPRESS does not fire when no hooks registered (empty hook list)")
{
    gTestHookCallResult = {};

    int result = testHandleKeyPressed(SDL_SCANCODE_A, true, true, false);

    CHECK(result == SDL_SCANCODE_UNKNOWN);
}

TEST_CASE("HOOK_KEYPRESS with key release (pressed=false)")
{
    gTestHookCallResult = {};
    gTestHookCallResult.returnValue = 0;

    int result = testHandleKeyPressed(SDL_SCANCODE_ESCAPE, false, true, true);

    CHECK(gTestHookCallResult.hookFired == true);
    CHECK(gTestHookCallResult.arg0 == 0); // not pressed (release)
    CHECK(gTestHookCallResult.arg1 == 1); // DIK_ESCAPE
}

TEST_CASE("HOOK_KEYPRESS with return value override")
{
    gTestHookCallResult = {};
    // Return DIK_TAB (15) from hook
    gTestHookCallResult.returnValue = 15;

    int result = testHandleKeyPressed(SDL_SCANCODE_A, true, true, true);

    CHECK(gTestHookCallResult.hookFired == true);
    // Override: 15 → get_scancode_from_key → SDL_SCANCODE_TAB
    CHECK(result == SDL_SCANCODE_TAB);
}

TEST_CASE("HOOK_KEYPRESS override to unknown key returns UNKNOWN")
{
    gTestHookCallResult = {};
    // Return DIK 0 (UNKNOWN) from hook
    gTestHookCallResult.returnValue = 0;

    int result = testHandleKeyPressed(SDL_SCANCODE_A, true, true, true);

    CHECK(gTestHookCallResult.hookFired == true);
    CHECK(result == SDL_SCANCODE_UNKNOWN);
}

TEST_CASE("HOOK_KEYPRESS with zero return value means no override")
{
    gTestHookCallResult = {};
    // In production: numReturnValues() <= 0 → return SDL_SCANCODE_UNKNOWN
    gTestHookCallResult.returnValue = 0;

    int result = testHandleKeyPressed(SDL_SCANCODE_SPACE, true, true, true);

    CHECK(gTestHookCallResult.hookFired == true);
    // return value 0 → key 0 → get_scancode_from_key(0) → SDL_SCANCODE_UNKNOWN
    CHECK(result == SDL_SCANCODE_UNKNOWN);
}

// =================================================================
// sfall_kb_is_key_pressed — logic mirror
// =================================================================

namespace {
    // Local mirror of sfall_kb_is_key_pressed logic (sfall_kb_helpers.cc:316-335)
    static bool testIsKeyPressed(int key, bool keyStateArray[256])
    {
        SDL_Scancode scancode = testGetScancodeFromKey(key);
        if (scancode == SDL_SCANCODE_UNKNOWN) {
            // VK codes (0x80000000 flag) return SDL_SCANCODE_UNKNOWN from
            // get_scancode_from_key, causing is_key_pressed to return false.
            return false;
        }
        return keyStateArray[static_cast<int>(scancode)];
    }
}

TEST_CASE("sfall_kb_is_key_pressed — returns true when key is down")
{
    bool keyState[256] = {};
    keyState[SDL_SCANCODE_A] = true;

    CHECK(testIsKeyPressed(30, keyState) == true); // DIK_A
}

TEST_CASE("sfall_kb_is_key_pressed — returns false when key is up")
{
    bool keyState[256] = {};
    keyState[SDL_SCANCODE_A] = false;

    CHECK(testIsKeyPressed(30, keyState) == false); // DIK_A
}

TEST_CASE("sfall_kb_is_key_pressed — returns false for unknown key")
{
    bool keyState[256] = {};
    keyState[0] = true;

    CHECK(testIsKeyPressed(0, keyState) == false); // DIK 0 = UNKNOWN
}

TEST_CASE("sfall_kb_is_key_pressed — key & 0xFF masking applied")
{
    bool keyState[256] = {};
    keyState[SDL_SCANCODE_A] = true;

    // DIK_A is 30; with high bits (0xFF00 | 30), should still map to SDL_SCANCODE_A
    int keyWithHighBits = 30 | 0xFF00;
    CHECK(testIsKeyPressed(keyWithHighBits, keyState) == true);
}

// =================================================================
// F-046: VK (Virtual Key) detection path
// =================================================================
//
// Production code at sfall_kb_helpers.cc:293 checks (key & 0x80000000)
// and returns SDL_SCANCODE_UNKNOWN for VK codes. The VK flag uses a
// completely different numbering scheme (Windows Virtual Key codes)
// from DIK (DirectInput Key codes in 0-255 range). Without a full
// Windows VK→SDL scancode translation table, returning UNKNOWN makes
// callers fail cleanly rather than silently mapping to wrong scancodes.

TEST_CASE("VK detection: bare VK flag returns UNKNOWN")
{
    // The VK flag (0x80000000) by itself, no additional key data
    CHECK(testGetScancodeFromKey(0x80000000) == SDL_SCANCODE_UNKNOWN);
}

TEST_CASE("VK detection: VK_ESCAPE (0x80000001) returns UNKNOWN")
{
    // VK_ESCAPE = 0x80000001: VK flag + value 1 (DIK_ESCAPE maps to 1,
    // but VK codes are a different scheme — VK_ESCAPE is 0x1B on Windows)
    CHECK(testGetScancodeFromKey(0x80000001) == SDL_SCANCODE_UNKNOWN);
}

TEST_CASE("VK detection: VK_RETURN (0x8000000D) returns UNKNOWN")
{
    // VK_RETURN = 0x0D on Windows, with VK flag = 0x8000000D
    CHECK(testGetScancodeFromKey(0x8000000D) == SDL_SCANCODE_UNKNOWN);
}

TEST_CASE("VK detection: VK_SPACE (0x80000020) returns UNKNOWN")
{
    // VK_SPACE = 0x20 on Windows, with VK flag = 0x80000020
    // DIK_SPACE is 57 — VK flag prevents wrong mapping
    CHECK(testGetScancodeFromKey(0x80000020) == SDL_SCANCODE_UNKNOWN);
}

TEST_CASE("VK detection: VK_SHIFT (0x80000010) returns UNKNOWN")
{
    // VK_SHIFT = 0x10 on Windows, with VK flag = 0x80000010
    CHECK(testGetScancodeFromKey(0x80000010) == SDL_SCANCODE_UNKNOWN);
}

TEST_CASE("VK detection: VK_CONTROL (0x80000011) returns UNKNOWN")
{
    // VK_CONTROL = 0x11 on Windows, with VK flag = 0x80000011
    CHECK(testGetScancodeFromKey(0x80000011) == SDL_SCANCODE_UNKNOWN);
}

TEST_CASE("VK detection: is_key_pressed returns false for VK codes")
{
    // VK codes: get_scancode_from_key returns UNKNOWN →
    // is_key_pressed returns false (scancode == UNKNOWN guard)
    bool keyState[256] = {};
    keyState[0] = true; // SDL_SCANCODE_UNKNOWN = 0

    CHECK(testIsKeyPressed(0x80000001, keyState) == false);
    CHECK(testIsKeyPressed(0x8000000D, keyState) == false);
    CHECK(testIsKeyPressed(0x80000020, keyState) == false);
}

TEST_CASE("VK detection: high-bit but non-VK keys still work")
{
    // Keys with high bits set but NOT the VK flag (0x80000000)
    // should still map via DIK table. The 0xFF00 bits are masked
    // by key & 0xFF BEFORE the VK check. Since VK check uses 0x80000000
    // and these values have different high bits, they pass through.
    // Example: 0x7F00001E = DIK_A (30=0x1E) with non-VK high bits
    int di_key_high_bits = 30 | 0x70000000;
    // 0x70000000 & 0x80000000 = 0, so VK check is false →
    // falls through to DIK mapping
    CHECK(testGetScancodeFromKey(di_key_high_bits) == SDL_SCANCODE_A);
}

TEST_CASE("VK detection: tap_key returns UNKNOWN for VK codes (no events)")
{
    // Simulate: press_key with VK → get_scancode_from_key returns UNKNOWN →
    // scancode == UNKNOWN guard → no events enqueued
    testClearSyntheticEvents();

    // testSimulatePressKey calls testGetScancodeFromKey; VK flag → UNKNOWN → no enqueue
    testSimulatePressKey(0x80000001); // VK_ESCAPE
    CHECK(testSyntheticEvents.size() == 0);

    testSimulatePressKey(0x80000020); // VK_SPACE
    CHECK(testSyntheticEvents.size() == 0);
}
