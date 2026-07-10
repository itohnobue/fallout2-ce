#include "sfall_kb_helpers.h"

#include <SDL.h>

#include "game.h"
#include "sfall_script_hooks.h"
#include "svga.h"

#include <deque>
#include <unordered_map>

namespace fallout {

constexpr size_t DIK_MAP_COUNT = 256;

// Maps DirectInput DIK constants to SDL scancodes.
static constexpr SDL_Scancode kDiks[DIK_MAP_COUNT] = {
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_ESCAPE, // DIK_ESCAPE
    SDL_SCANCODE_1, // DIK_1
    SDL_SCANCODE_2, // DIK_2
    SDL_SCANCODE_3, // DIK_3
    SDL_SCANCODE_4, // DIK_4
    SDL_SCANCODE_5, // DIK_5
    SDL_SCANCODE_6, // DIK_6
    SDL_SCANCODE_7, // DIK_7
    SDL_SCANCODE_8, // DIK_8
    SDL_SCANCODE_9, // DIK_9
    SDL_SCANCODE_0, // DIK_0
    SDL_SCANCODE_MINUS, // DIK_MINUS
    SDL_SCANCODE_EQUALS, // DIK_EQUALS
    SDL_SCANCODE_BACKSPACE, // DIK_BACK
    SDL_SCANCODE_TAB, // DIK_TAB
    SDL_SCANCODE_Q, // DIK_Q
    SDL_SCANCODE_W, // DIK_W
    SDL_SCANCODE_E, // DIK_E
    SDL_SCANCODE_R, // DIK_R
    SDL_SCANCODE_T, // DIK_T
    SDL_SCANCODE_Y, // DIK_Y
    SDL_SCANCODE_U, // DIK_U
    SDL_SCANCODE_I, // DIK_I
    SDL_SCANCODE_O, // DIK_O
    SDL_SCANCODE_P, // DIK_P
    SDL_SCANCODE_LEFTBRACKET, // DIK_LBRACKET
    SDL_SCANCODE_RIGHTBRACKET, // DIK_RBRACKET
    SDL_SCANCODE_RETURN, // DIK_RETURN
    SDL_SCANCODE_LCTRL, // DIK_LCONTROL
    SDL_SCANCODE_A, // DIK_A
    SDL_SCANCODE_S, // DIK_S
    SDL_SCANCODE_D, // DIK_D
    SDL_SCANCODE_F, // DIK_F
    SDL_SCANCODE_G, // DIK_G
    SDL_SCANCODE_H, // DIK_H
    SDL_SCANCODE_J, // DIK_J
    SDL_SCANCODE_K, // DIK_K
    SDL_SCANCODE_L, // DIK_L
    SDL_SCANCODE_SEMICOLON, // DIK_SEMICOLON
    SDL_SCANCODE_APOSTROPHE, // DIK_APOSTROPHE
    SDL_SCANCODE_GRAVE, // DIK_GRAVE
    SDL_SCANCODE_LSHIFT, // DIK_LSHIFT
    SDL_SCANCODE_BACKSLASH, // DIK_BACKSLASH
    SDL_SCANCODE_Z, // DIK_Z
    SDL_SCANCODE_X, // DIK_X
    SDL_SCANCODE_C, // DIK_C
    SDL_SCANCODE_V, // DIK_V
    SDL_SCANCODE_B, // DIK_B
    SDL_SCANCODE_N, // DIK_N
    SDL_SCANCODE_M, // DIK_M
    SDL_SCANCODE_COMMA, // DIK_COMMA
    SDL_SCANCODE_PERIOD, // DIK_PERIOD
    SDL_SCANCODE_SLASH, // DIK_SLASH
    SDL_SCANCODE_RSHIFT, // DIK_RSHIFT
    SDL_SCANCODE_KP_MULTIPLY, // DIK_MULTIPLY
    SDL_SCANCODE_LALT, // DIK_LMENU
    SDL_SCANCODE_SPACE, // DIK_SPACE
    SDL_SCANCODE_CAPSLOCK, // DIK_CAPITAL
    SDL_SCANCODE_F1, // DIK_F1
    SDL_SCANCODE_F2, // DIK_F2
    SDL_SCANCODE_F3, // DIK_F3
    SDL_SCANCODE_F4, // DIK_F4
    SDL_SCANCODE_F5, // DIK_F5
    SDL_SCANCODE_F6, // DIK_F6
    SDL_SCANCODE_F7, // DIK_F7
    SDL_SCANCODE_F8, // DIK_F8
    SDL_SCANCODE_F9, // DIK_F9
    SDL_SCANCODE_F10, // DIK_F10
    SDL_SCANCODE_NUMLOCKCLEAR, // DIK_NUMLOCK
    SDL_SCANCODE_SCROLLLOCK, // DIK_SCROLL
    SDL_SCANCODE_KP_7, // DIK_NUMPAD7
    SDL_SCANCODE_KP_8, // DIK_NUMPAD8
    SDL_SCANCODE_KP_9, // DIK_NUMPAD9
    SDL_SCANCODE_KP_MINUS, // DIK_SUBTRACT
    SDL_SCANCODE_KP_4, // DIK_NUMPAD4
    SDL_SCANCODE_KP_5, // DIK_NUMPAD5
    SDL_SCANCODE_KP_6, // DIK_NUMPAD6
    SDL_SCANCODE_KP_PLUS, // DIK_ADD
    SDL_SCANCODE_KP_1, // DIK_NUMPAD1
    SDL_SCANCODE_KP_2, // DIK_NUMPAD2
    SDL_SCANCODE_KP_3, // DIK_NUMPAD3
    SDL_SCANCODE_KP_0, // DIK_NUMPAD0
    SDL_SCANCODE_KP_PERIOD, // DIK_DECIMAL
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_F11, // DIK_F11
    SDL_SCANCODE_F12, // DIK_F12
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_KP_EQUALS, // DIK_NUMPADEQUALS
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN, // DIK_AT
    SDL_SCANCODE_UNKNOWN, // DIK_COLON
    SDL_SCANCODE_UNKNOWN, // DIK_UNDERLINE
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN, // DIK_STOP
    SDL_SCANCODE_UNKNOWN, // DIK_AX
    SDL_SCANCODE_UNKNOWN, // DIK_UNLABELED
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_KP_ENTER, // DIK_NUMPADENTER
    SDL_SCANCODE_RCTRL, // DIK_RCONTROL
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_KP_COMMA, // DIK_NUMPADCOMMA
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_KP_DIVIDE, // DIK_DIVIDE
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_SYSREQ, // DIK_SYSRQ
    SDL_SCANCODE_RALT, // DIK_RMENU
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_HOME, // DIK_HOME
    SDL_SCANCODE_UP, // DIK_UP
    SDL_SCANCODE_PAGEUP, // DIK_PRIOR
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_LEFT, // DIK_LEFT
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_RIGHT, // DIK_RIGHT
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_END, // DIK_END
    SDL_SCANCODE_DOWN, // DIK_DOWN
    SDL_SCANCODE_PAGEDOWN, // DIK_NEXT
    SDL_SCANCODE_INSERT, // DIK_INSERT
    SDL_SCANCODE_DELETE, // DIK_DELETE
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_LGUI, // DIK_LWIN
    SDL_SCANCODE_RGUI, // DIK_RWIN
    SDL_SCANCODE_APPLICATION, // DIK_APPS
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN,
};

static std::unordered_map<SDL_Scancode, int> kScanCodeToDik;
static std::deque<std::pair<SDL_Scancode, bool>> syntheticKeyEvents;

// F-8 (FIX): VK→SDL scancode mapping table.
// Windows Virtual Key codes use a sparse numbering scheme (0x01-0xAF).
// This 256-entry flat table maps VK codes (with the 0x80000000 flag stripped)
// to SDL scancodes. Entries not mapped remain SDL_SCANCODE_UNKNOWN.
// References: Windows VK_ constants → SDL scancode equivalents.
static constexpr SDL_Scancode kVkToSdl[256] = {
    // 0x00-0x0F
    SDL_SCANCODE_UNKNOWN, // 0x00 (unused)
    SDL_SCANCODE_UNKNOWN, // 0x01 VK_LBUTTON
    SDL_SCANCODE_UNKNOWN, // 0x02 VK_RBUTTON
    SDL_SCANCODE_CANCEL,  // 0x03 VK_CANCEL
    SDL_SCANCODE_UNKNOWN, // 0x04 VK_MBUTTON
    SDL_SCANCODE_UNKNOWN, // 0x05 VK_XBUTTON1
    SDL_SCANCODE_UNKNOWN, // 0x06 VK_XBUTTON2
    SDL_SCANCODE_UNKNOWN, // 0x07
    SDL_SCANCODE_BACKSPACE, // 0x08 VK_BACK
    SDL_SCANCODE_TAB,      // 0x09 VK_TAB
    SDL_SCANCODE_UNKNOWN,  // 0x0A
    SDL_SCANCODE_UNKNOWN,  // 0x0B
    SDL_SCANCODE_CLEAR,    // 0x0C VK_CLEAR
    SDL_SCANCODE_RETURN,   // 0x0D VK_RETURN
    SDL_SCANCODE_UNKNOWN,  // 0x0E
    SDL_SCANCODE_UNKNOWN,  // 0x0F
    // 0x10-0x1F
    SDL_SCANCODE_LSHIFT,   // 0x10 VK_SHIFT
    SDL_SCANCODE_LCTRL,    // 0x11 VK_CONTROL
    SDL_SCANCODE_LALT,     // 0x12 VK_MENU
    SDL_SCANCODE_PAUSE,    // 0x13 VK_PAUSE
    SDL_SCANCODE_CAPSLOCK, // 0x14 VK_CAPITAL
    SDL_SCANCODE_UNKNOWN,  // 0x15 VK_KANA
    SDL_SCANCODE_UNKNOWN,  // 0x16
    SDL_SCANCODE_UNKNOWN,  // 0x17 VK_JUNJA
    SDL_SCANCODE_UNKNOWN,  // 0x18 VK_FINAL
    SDL_SCANCODE_UNKNOWN,  // 0x19 VK_HANJA
    SDL_SCANCODE_UNKNOWN,  // 0x1A
    SDL_SCANCODE_ESCAPE,   // 0x1B VK_ESCAPE
    SDL_SCANCODE_UNKNOWN,  // 0x1C VK_CONVERT
    SDL_SCANCODE_UNKNOWN,  // 0x1D VK_NONCONVERT
    SDL_SCANCODE_UNKNOWN,  // 0x1E VK_ACCEPT
    SDL_SCANCODE_UNKNOWN,  // 0x1F VK_MODECHANGE
    // 0x20-0x2F
    SDL_SCANCODE_SPACE,    // 0x20 VK_SPACE
    SDL_SCANCODE_PAGEUP,   // 0x21 VK_PRIOR
    SDL_SCANCODE_PAGEDOWN, // 0x22 VK_NEXT
    SDL_SCANCODE_END,      // 0x23 VK_END
    SDL_SCANCODE_HOME,     // 0x24 VK_HOME
    SDL_SCANCODE_LEFT,     // 0x25 VK_LEFT
    SDL_SCANCODE_UP,       // 0x26 VK_UP
    SDL_SCANCODE_RIGHT,    // 0x27 VK_RIGHT
    SDL_SCANCODE_DOWN,     // 0x28 VK_DOWN
    SDL_SCANCODE_UNKNOWN,  // 0x29 VK_SELECT
    SDL_SCANCODE_UNKNOWN,  // 0x2A VK_PRINT
    SDL_SCANCODE_UNKNOWN,  // 0x2B VK_EXECUTE
    SDL_SCANCODE_PRINTSCREEN, // 0x2C VK_SNAPSHOT
    SDL_SCANCODE_INSERT,   // 0x2D VK_INSERT
    SDL_SCANCODE_DELETE,   // 0x2E VK_DELETE
    SDL_SCANCODE_UNKNOWN,  // 0x2F VK_HELP
    // 0x30-0x39: VK_0 through VK_9 (ASCII digits)
    SDL_SCANCODE_0, // 0x30 VK_0
    SDL_SCANCODE_1, // 0x31 VK_1
    SDL_SCANCODE_2, // 0x32 VK_2
    SDL_SCANCODE_3, // 0x33 VK_3
    SDL_SCANCODE_4, // 0x34 VK_4
    SDL_SCANCODE_5, // 0x35 VK_5
    SDL_SCANCODE_6, // 0x36 VK_6
    SDL_SCANCODE_7, // 0x37 VK_7
    SDL_SCANCODE_8, // 0x38 VK_8
    SDL_SCANCODE_9, // 0x39 VK_9
    // 0x3A-0x40
    SDL_SCANCODE_UNKNOWN, // 0x3A
    SDL_SCANCODE_UNKNOWN, // 0x3B
    SDL_SCANCODE_UNKNOWN, // 0x3C
    SDL_SCANCODE_UNKNOWN, // 0x3D
    SDL_SCANCODE_UNKNOWN, // 0x3E
    SDL_SCANCODE_UNKNOWN, // 0x3F
    SDL_SCANCODE_UNKNOWN, // 0x40
    // 0x41-0x5A: VK_A through VK_Z (ASCII uppercase letters)
    SDL_SCANCODE_A, // 0x41 VK_A
    SDL_SCANCODE_B, // 0x42 VK_B
    SDL_SCANCODE_C, // 0x43 VK_C
    SDL_SCANCODE_D, // 0x44 VK_D
    SDL_SCANCODE_E, // 0x45 VK_E
    SDL_SCANCODE_F, // 0x46 VK_F
    SDL_SCANCODE_G, // 0x47 VK_G
    SDL_SCANCODE_H, // 0x48 VK_H
    SDL_SCANCODE_I, // 0x49 VK_I
    SDL_SCANCODE_J, // 0x4A VK_J
    SDL_SCANCODE_K, // 0x4B VK_K
    SDL_SCANCODE_L, // 0x4C VK_L
    SDL_SCANCODE_M, // 0x4D VK_M
    SDL_SCANCODE_N, // 0x4E VK_N
    SDL_SCANCODE_O, // 0x4F VK_O
    SDL_SCANCODE_P, // 0x50 VK_P
    SDL_SCANCODE_Q, // 0x51 VK_Q
    SDL_SCANCODE_R, // 0x52 VK_R
    SDL_SCANCODE_S, // 0x53 VK_S
    SDL_SCANCODE_T, // 0x54 VK_T
    SDL_SCANCODE_U, // 0x55 VK_U
    SDL_SCANCODE_V, // 0x56 VK_V
    SDL_SCANCODE_W, // 0x57 VK_W
    SDL_SCANCODE_X, // 0x58 VK_X
    SDL_SCANCODE_Y, // 0x59 VK_Y
    SDL_SCANCODE_Z, // 0x5A VK_Z
    // 0x5B-0x5F
    SDL_SCANCODE_LGUI,     // 0x5B VK_LWIN
    SDL_SCANCODE_RGUI,     // 0x5C VK_RWIN
    SDL_SCANCODE_APPLICATION, // 0x5D VK_APPS
    SDL_SCANCODE_UNKNOWN,  // 0x5E
    SDL_SCANCODE_UNKNOWN,  // 0x5F VK_SLEEP
    // 0x60-0x6F: VK_NUMPAD0 through VK_DIVIDE
    SDL_SCANCODE_KP_0,       // 0x60 VK_NUMPAD0
    SDL_SCANCODE_KP_1,       // 0x61 VK_NUMPAD1
    SDL_SCANCODE_KP_2,       // 0x62 VK_NUMPAD2
    SDL_SCANCODE_KP_3,       // 0x63 VK_NUMPAD3
    SDL_SCANCODE_KP_4,       // 0x64 VK_NUMPAD4
    SDL_SCANCODE_KP_5,       // 0x65 VK_NUMPAD5
    SDL_SCANCODE_KP_6,       // 0x66 VK_NUMPAD6
    SDL_SCANCODE_KP_7,       // 0x67 VK_NUMPAD7
    SDL_SCANCODE_KP_8,       // 0x68 VK_NUMPAD8
    SDL_SCANCODE_KP_9,       // 0x69 VK_NUMPAD9
    SDL_SCANCODE_KP_MULTIPLY, // 0x6A VK_MULTIPLY
    SDL_SCANCODE_KP_PLUS,    // 0x6B VK_ADD
    SDL_SCANCODE_UNKNOWN,    // 0x6C VK_SEPARATOR
    SDL_SCANCODE_KP_MINUS,   // 0x6D VK_SUBTRACT
    SDL_SCANCODE_KP_PERIOD,  // 0x6E VK_DECIMAL
    SDL_SCANCODE_KP_DIVIDE,  // 0x6F VK_DIVIDE
    // 0x70-0x7B: VK_F1 through VK_F12
    SDL_SCANCODE_F1,  // 0x70 VK_F1
    SDL_SCANCODE_F2,  // 0x71 VK_F2
    SDL_SCANCODE_F3,  // 0x72 VK_F3
    SDL_SCANCODE_F4,  // 0x73 VK_F4
    SDL_SCANCODE_F5,  // 0x74 VK_F5
    SDL_SCANCODE_F6,  // 0x75 VK_F6
    SDL_SCANCODE_F7,  // 0x76 VK_F7
    SDL_SCANCODE_F8,  // 0x77 VK_F8
    SDL_SCANCODE_F9,  // 0x78 VK_F9
    SDL_SCANCODE_F10, // 0x79 VK_F10
    SDL_SCANCODE_F11, // 0x7A VK_F11
    SDL_SCANCODE_F12, // 0x7B VK_F12
    // 0x7C-0x8F
    SDL_SCANCODE_UNKNOWN, // 0x7C
    SDL_SCANCODE_UNKNOWN, // 0x7D
    SDL_SCANCODE_UNKNOWN, // 0x7E
    SDL_SCANCODE_UNKNOWN, // 0x7F
    SDL_SCANCODE_UNKNOWN, // 0x80
    SDL_SCANCODE_UNKNOWN, // 0x81
    SDL_SCANCODE_UNKNOWN, // 0x82
    SDL_SCANCODE_UNKNOWN, // 0x83
    SDL_SCANCODE_UNKNOWN, // 0x84
    SDL_SCANCODE_UNKNOWN, // 0x85
    SDL_SCANCODE_UNKNOWN, // 0x86
    SDL_SCANCODE_UNKNOWN, // 0x87
    SDL_SCANCODE_UNKNOWN, // 0x88
    SDL_SCANCODE_UNKNOWN, // 0x89
    SDL_SCANCODE_UNKNOWN, // 0x8A
    SDL_SCANCODE_UNKNOWN, // 0x8B
    SDL_SCANCODE_UNKNOWN, // 0x8C
    SDL_SCANCODE_UNKNOWN, // 0x8D
    SDL_SCANCODE_UNKNOWN, // 0x8E
    SDL_SCANCODE_UNKNOWN, // 0x8F
    // 0x90-0x9F
    SDL_SCANCODE_NUMLOCKCLEAR, // 0x90 VK_NUMLOCK
    SDL_SCANCODE_SCROLLLOCK,   // 0x91 VK_SCROLL
    SDL_SCANCODE_UNKNOWN, // 0x92
    SDL_SCANCODE_UNKNOWN, // 0x93
    SDL_SCANCODE_UNKNOWN, // 0x94
    SDL_SCANCODE_UNKNOWN, // 0x95
    SDL_SCANCODE_UNKNOWN, // 0x96
    SDL_SCANCODE_UNKNOWN, // 0x97
    SDL_SCANCODE_UNKNOWN, // 0x98
    SDL_SCANCODE_UNKNOWN, // 0x99
    SDL_SCANCODE_UNKNOWN, // 0x9A
    SDL_SCANCODE_UNKNOWN, // 0x9B
    SDL_SCANCODE_UNKNOWN, // 0x9C
    SDL_SCANCODE_UNKNOWN, // 0x9D
    SDL_SCANCODE_UNKNOWN, // 0x9E
    SDL_SCANCODE_UNKNOWN, // 0x9F
    // 0xA0-0xA5: VK_LSHIFT through VK_RMENU (modifier extended keys)
    SDL_SCANCODE_LSHIFT, // 0xA0 VK_LSHIFT
    SDL_SCANCODE_RSHIFT, // 0xA1 VK_RSHIFT
    SDL_SCANCODE_LCTRL,  // 0xA2 VK_LCONTROL
    SDL_SCANCODE_RCTRL,  // 0xA3 VK_RCONTROL
    SDL_SCANCODE_LALT,   // 0xA4 VK_LMENU
    SDL_SCANCODE_RALT,   // 0xA5 VK_RMENU
    // 0xA6-0xAF: Browser/media keys — no SDL equivalents in standard scancode set
    SDL_SCANCODE_UNKNOWN, // 0xA6 VK_BROWSER_BACK
    SDL_SCANCODE_UNKNOWN, // 0xA7 VK_BROWSER_FORWARD
    SDL_SCANCODE_UNKNOWN, // 0xA8 VK_BROWSER_REFRESH
    SDL_SCANCODE_UNKNOWN, // 0xA9 VK_BROWSER_STOP
    SDL_SCANCODE_UNKNOWN, // 0xAA VK_BROWSER_SEARCH
    SDL_SCANCODE_UNKNOWN, // 0xAB VK_BROWSER_FAVORITES
    SDL_SCANCODE_UNKNOWN, // 0xAC VK_BROWSER_HOME
    SDL_SCANCODE_AUDIOMUTE,  // 0xAD VK_VOLUME_MUTE
    SDL_SCANCODE_VOLUMEDOWN, // 0xAE VK_VOLUME_DOWN
    SDL_SCANCODE_VOLUMEUP,   // 0xAF VK_VOLUME_UP
    // 0xB0-0xFF: remaining slots
    SDL_SCANCODE_UNKNOWN, // 0xB0 VK_MEDIA_NEXT_TRACK
    SDL_SCANCODE_UNKNOWN, // 0xB1 VK_MEDIA_PREV_TRACK
    SDL_SCANCODE_UNKNOWN, // 0xB2 VK_MEDIA_STOP
    SDL_SCANCODE_UNKNOWN, // 0xB3 VK_MEDIA_PLAY_PAUSE
    SDL_SCANCODE_UNKNOWN, // 0xB4 VK_LAUNCH_MAIL
    SDL_SCANCODE_UNKNOWN, // 0xB5 VK_LAUNCH_MEDIA_SELECT
    SDL_SCANCODE_UNKNOWN, // 0xB6 VK_LAUNCH_APP1
    SDL_SCANCODE_UNKNOWN, // 0xB7 VK_LAUNCH_APP2
    SDL_SCANCODE_UNKNOWN, // 0xB8
    SDL_SCANCODE_UNKNOWN, // 0xB9
    // 0xBA-0xBF: OEM keys
    SDL_SCANCODE_SEMICOLON,    // 0xBA VK_OEM_1 (;:)
    SDL_SCANCODE_EQUALS,       // 0xBB VK_OEM_PLUS (=+)
    SDL_SCANCODE_COMMA,        // 0xBC VK_OEM_COMMA (,<)
    SDL_SCANCODE_MINUS,        // 0xBD VK_OEM_MINUS (-_)
    SDL_SCANCODE_PERIOD,       // 0xBE VK_OEM_PERIOD (.>)
    SDL_SCANCODE_SLASH,        // 0xBF VK_OEM_2 (/?)
    SDL_SCANCODE_GRAVE,        // 0xC0 VK_OEM_3 (`~)
    SDL_SCANCODE_UNKNOWN, // 0xC1
    SDL_SCANCODE_UNKNOWN, // 0xC2
    SDL_SCANCODE_UNKNOWN, // 0xC3
    SDL_SCANCODE_UNKNOWN, // 0xC4
    SDL_SCANCODE_UNKNOWN, // 0xC5
    SDL_SCANCODE_UNKNOWN, // 0xC6
    SDL_SCANCODE_UNKNOWN, // 0xC7
    SDL_SCANCODE_UNKNOWN, // 0xC8
    SDL_SCANCODE_UNKNOWN, // 0xC9
    SDL_SCANCODE_UNKNOWN, // 0xCA
    SDL_SCANCODE_UNKNOWN, // 0xCB
    SDL_SCANCODE_UNKNOWN, // 0xCC
    SDL_SCANCODE_UNKNOWN, // 0xCD
    SDL_SCANCODE_UNKNOWN, // 0xCE
    SDL_SCANCODE_UNKNOWN, // 0xCF
    SDL_SCANCODE_UNKNOWN, // 0xD0
    SDL_SCANCODE_UNKNOWN, // 0xD1
    SDL_SCANCODE_UNKNOWN, // 0xD2
    SDL_SCANCODE_UNKNOWN, // 0xD3
    SDL_SCANCODE_UNKNOWN, // 0xD4
    SDL_SCANCODE_UNKNOWN, // 0xD5
    SDL_SCANCODE_UNKNOWN, // 0xD6
    SDL_SCANCODE_UNKNOWN, // 0xD7
    SDL_SCANCODE_UNKNOWN, // 0xD8
    SDL_SCANCODE_UNKNOWN, // 0xD9
    SDL_SCANCODE_UNKNOWN, // 0xDA
    SDL_SCANCODE_LEFTBRACKET,  // 0xDB VK_OEM_4 ([{)
    SDL_SCANCODE_BACKSLASH,    // 0xDC VK_OEM_5 (\|)
    SDL_SCANCODE_RIGHTBRACKET, // 0xDD VK_OEM_6 (]})
    SDL_SCANCODE_APOSTROPHE,   // 0xDE VK_OEM_7 ('")
    SDL_SCANCODE_UNKNOWN, // 0xDF VK_OEM_8
    SDL_SCANCODE_UNKNOWN, // 0xE0
    SDL_SCANCODE_UNKNOWN, // 0xE1
    SDL_SCANCODE_NONUSBACKSLASH, // 0xE2 VK_OEM_102 (<> on non-US)
    SDL_SCANCODE_UNKNOWN, // 0xE3
    SDL_SCANCODE_UNKNOWN, // 0xE4
    SDL_SCANCODE_UNKNOWN, // 0xE5
    SDL_SCANCODE_UNKNOWN, // 0xE6
    SDL_SCANCODE_UNKNOWN, // 0xE7
    SDL_SCANCODE_UNKNOWN, // 0xE8
    SDL_SCANCODE_UNKNOWN, // 0xE9
    SDL_SCANCODE_UNKNOWN, // 0xEA
    SDL_SCANCODE_UNKNOWN, // 0xEB
    SDL_SCANCODE_UNKNOWN, // 0xEC
    SDL_SCANCODE_UNKNOWN, // 0xED
    SDL_SCANCODE_UNKNOWN, // 0xEE
    SDL_SCANCODE_UNKNOWN, // 0xEF
    SDL_SCANCODE_UNKNOWN, // 0xF0
    SDL_SCANCODE_UNKNOWN, // 0xF1
    SDL_SCANCODE_UNKNOWN, // 0xF2
    SDL_SCANCODE_UNKNOWN, // 0xF3
    SDL_SCANCODE_UNKNOWN, // 0xF4
    SDL_SCANCODE_UNKNOWN, // 0xF5
    SDL_SCANCODE_UNKNOWN, // 0xF6
    SDL_SCANCODE_UNKNOWN, // 0xF7
    SDL_SCANCODE_UNKNOWN, // 0xF8
    SDL_SCANCODE_UNKNOWN, // 0xF9
    SDL_SCANCODE_UNKNOWN, // 0xFA
    SDL_SCANCODE_UNKNOWN, // 0xFB
    SDL_SCANCODE_UNKNOWN, // 0xFC
    SDL_SCANCODE_UNKNOWN, // 0xFD
    SDL_SCANCODE_UNKNOWN, // 0xFE
    SDL_SCANCODE_UNKNOWN, // 0xFF
};

// UF-H-050: Reverse lookup table — maps SDL_Scancode → VK code.
// Built lazily from kVkToSdl on first use (see get_vk_from_scancode()).
// When a scancode maps to multiple VK codes (kVkToSdl has duplicate
// scancodes, e.g. VK_SHIFT/VK_LSHIFT both → SDL_SCANCODE_LSHIFT),
// the LAST mapping found is used — the higher-index VK code (e.g.
// VK_LSHIFT 0xA0) preserves the left/right modifier distinction
// that the generic VK code (e.g. VK_SHIFT 0x10) loses.
static std::unordered_map<SDL_Scancode, int> kSdlScancodeToVk;

/// Returns the VK (Virtual Key) code corresponding to the given SDL scancode,
/// or -1 if no mapping exists.  Populates kSdlScancodeToVk on first call.
static int get_vk_from_scancode(SDL_Scancode sc)
{
    if (kSdlScancodeToVk.empty()) {
        for (int vk = 0; vk < 256; ++vk) {
            SDL_Scancode mapped = kVkToSdl[vk];
            if (mapped != SDL_SCANCODE_UNKNOWN) {
                // Last-wins: higher-index VK codes (e.g. VK_LSHIFT 0xA0)
                // overwrite generic ones (e.g. VK_SHIFT 0x10) to preserve
                // left/right modifier distinction.
                kSdlScancodeToVk[mapped] = vk;
            }
        }
    }
    auto it = kSdlScancodeToVk.find(sc);
    return (it != kSdlScancodeToVk.end()) ? it->second : -1;
}

/// Converts an SDL_Keycode to the corresponding VK (Virtual Key) code
/// for HOOK_KEYPRESS arg2 compatibility.
/// Steps: SDL_Keycode → SDL_Scancode (via SDL) → VK code (via reverse table).
/// Returns the VK code, or -1 if no mapping exists.
static int sdl_keycode_to_vk(SDL_Keycode keycode)
{
    SDL_Scancode sc = SDL_GetScancodeFromKey(keycode);
    if (sc == SDL_SCANCODE_UNKNOWN) {
        return -1;
    }
    return get_vk_from_scancode(sc);
}

// Translates Sfall key code to SDL scancode.
//
// VK (Virtual Key) codes have the 0x80000000 flag set and use a different
// numbering scheme from DIK.  DIK (DirectInput Key) codes in the 0-255 range
// are translated via the kDiks lookup table.
//
// F-8 (FIX): VK codes are now mapped via the kVkToSdl 256-entry table.
// Previously they returned SDL_SCANCODE_UNKNOWN unconditionally, making
// key_pressed_vk() completely inoperable.
static SDL_Scancode get_scancode_from_key(int key)
{
    if (key & 0x80000000) {
        // VK (Virtual Key) code — strip the flag and look up in VK table.
        return kVkToSdl[key & 0xFF];
    }
    return kDiks[key & 0xFF];
}

// Translates SDL scancode into DIK key constant, used by sfall.
static int get_key_from_scancode(SDL_Scancode scanCode)
{
    if (kScanCodeToDik.empty()) {
        for (int dik = DIK_MAP_COUNT - 1; dik >= 1; --dik) {
            if (kDiks[dik] == SDL_SCANCODE_UNKNOWN) continue;

            kScanCodeToDik[kDiks[dik]] = dik;
        }
    }
    auto dikIt = kScanCodeToDik.find(scanCode);
    if (dikIt == kScanCodeToDik.end()) {
        return SDL_SCANCODE_UNKNOWN;
    }
    return dikIt->second;
}

bool sfall_kb_is_key_pressed(int key)
{
    // F-8 (FIX): VK codes are now supported via the kVkToSdl mapping table.
    // The 0x80000000 flag triggers VK→SDL translation in get_scancode_from_key().
    SDL_Scancode scancode = get_scancode_from_key(key);
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        return false;
    }

    const Uint8* state = SDL_GetKeyboardState(nullptr);
    return state[scancode] != 0;
}

void sfall_kb_press_key(int key)
{
    SDL_Scancode scancode = get_scancode_from_key(key);
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        return;
    }

    SDL_Event event;
    SDL_zero(event);

    event.type = SDL_KEYDOWN;
    event.key.timestamp = SDL_GetTicks();
    event.key.windowID = gSdlWindow != nullptr ? SDL_GetWindowID(gSdlWindow) : 0;
    event.key.state = SDL_PRESSED;
    event.key.repeat = 0;
    event.key.keysym.scancode = scancode;
    event.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
    event.key.keysym.mod = SDL_GetModState();
    if (SDL_PushEvent(&event) == 1) {
        syntheticKeyEvents.emplace_back(scancode, true);
    }

    event.type = SDL_KEYUP;
    event.key.timestamp = SDL_GetTicks();
    event.key.state = SDL_RELEASED;
    if (SDL_PushEvent(&event) == 1) {
        syntheticKeyEvents.emplace_back(scancode, false);
    }
}

bool sfall_kb_consume_synthetic_key_event(int sdlScanCode, bool pressed)
{
    if (syntheticKeyEvents.empty()) {
        return false;
    }

    const auto& [expectedScanCode, expectedPressed] = syntheticKeyEvents.front();
    if (expectedScanCode != static_cast<SDL_Scancode>(sdlScanCode) || expectedPressed != pressed) {
        return false;
    }

    syntheticKeyEvents.pop_front();
    return true;
}

void sfall_kb_clear_synthetic_key_events()
{
    syntheticKeyEvents.clear();
}

int sfall_kb_handle_key_pressed(int sdlScanCode, bool pressed, SDL_Keycode keysym)
{
    if (!gGameLoaded) return SDL_SCANCODE_UNKNOWN;

    SDL_Scancode scanCode = static_cast<SDL_Scancode>(sdlScanCode);
    // F-03 (FIXED): HOOK_KEYPRESS argument order per sfall convention:
    //   arg0 = pressed state (1=pressed, 0=released)
    //   arg1 = DIK keyCode
    //   arg2 = VK_ code (converted from SDL_Keycode via reverse kVkToSdl)
    // Et tu's TMA handler checks arg0 for pressed/not-pressed and arg1 for
    // DIK_ESCAPE; the old order (dikCode, pressed) caused every key to
    // spuriously match DIK_ESCAPE because arg0 was the key code.
    // UF-H-050 (FIXED): arg2 now carries a VK_ Virtual Key code converted
    // from the raw SDL_Keycode via sdl_keycode_to_vk().  Previously arg2
    // was the raw SDL_Keycode, which diverges from VK_ for letter keys
    // (SDLK_F=102 vs VK_F=70) and function keys (SDLK_F1=0x4000003A vs
    // VK_F1=0x70).  Scripts comparing arg2 against VK_ constants now match.
    int dikCode = get_key_from_scancode(scanCode);
    int vkCode = sdl_keycode_to_vk(keysym);
    ScriptHookCall hook(HOOK_KEYPRESS, 1, { pressed ? 1 : 0, dikCode, vkCode });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return SDL_SCANCODE_UNKNOWN;
    }

    int overrideDxCode = hook.getReturnValueAt(0).asInt();
    // F-27: ret0=1 means "block/consume the key" per sfall spec.
    // Return -1 as a sentinel to signal to the caller that the key
    // should be fully consumed (no further processing).
    // Other values remap to the corresponding DIK scancode as before.
    if (overrideDxCode == 1) {
        return -1;
    }
    return get_scancode_from_key(overrideDxCode);
}

} // namespace fallout
