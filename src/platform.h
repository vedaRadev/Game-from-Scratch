// Everything in this file is shared between the following two parts:
// 1. The platform layer which is compiled into an executable.
// 2. The game dll which the platform executable loads dynamically.

// TODO(ryan): should we do this or include guards?
#pragma once

// TODO(ryan): maybe move common types and "#define"s into a separate file? Not sure if
// I'm going to do a unity build or not yet.
#include <stdint.h>
// TODO(ryan): Instead of this, maybe make types b8, b16, b32, b64 etc.
typedef enum { false, true } bool;

// TODO(ryan): If not unity build, move into own file?
// TODO(ryan): This assumes that we're running while hooked up to a debugger and can catch a crash.
// See GEA 3rd p126 for details on a perhaps more-proper assertion implementation. Probably want to
// use the __FILE__ and __LINE__ macros to report when an assertion fails, then use some inline
// assembly that will break to a debugger if one is attached.
#ifdef ASSERTIONS_ENABLED
    #define assert(expr) if (!(expr)) { *(volatile int *)0; }
#else
    #define assert(expr)
#endif

typedef struct GameOffscreenBuffer {
    void *memory;
    int width;
    int height;
    int bytes_per_pixel;
} GameOffscreenBuffer;

// NOTE(ryan): These codes are based on win32 virtual keycodes
// TODO(ryan): add numpad keys?
// TODO(ryan): add home, end, insert, page up/down, keys?
typedef enum GameKey {
    // ARROWS
    GAME_KEY_UP     = 0x26,
    GAME_KEY_DOWN   = 0x27,
    GAME_KEY_LEFT   = 0x28,
    GAME_KEY_RIGHT  = 0x29,

    // NUMERIC
    GAME_KEY_0 = 0x30,
    GAME_KEY_1 = 0x31,
    GAME_KEY_2 = 0x32,
    GAME_KEY_3 = 0x33,
    GAME_KEY_4 = 0x34,
    GAME_KEY_5 = 0x35,
    GAME_KEY_6 = 0x36,
    GAME_KEY_7 = 0x37,
    GAME_KEY_8 = 0x38,
    GAME_KEY_9 = 0x39,

    // ALPHABETIC
    GAME_KEY_A = 0x41,
    GAME_KEY_B = 0x42,
    GAME_KEY_C = 0x43,
    GAME_KEY_D = 0x44,
    GAME_KEY_E = 0x45,
    GAME_KEY_F = 0x46,
    GAME_KEY_G = 0x47,
    GAME_KEY_H = 0x48,
    GAME_KEY_I = 0x49,
    GAME_KEY_J = 0x4A,
    GAME_KEY_K = 0x4B,
    GAME_KEY_L = 0x4C,
    GAME_KEY_M = 0x4D,
    GAME_KEY_N = 0x4E,
    GAME_KEY_O = 0x4F,
    GAME_KEY_P = 0x50,
    GAME_KEY_Q = 0x51,
    GAME_KEY_R = 0x52,
    GAME_KEY_S = 0x53,
    GAME_KEY_T = 0x54,
    GAME_KEY_U = 0x55,
    GAME_KEY_V = 0x56,
    GAME_KEY_W = 0x57,
    GAME_KEY_X = 0x58,
    GAME_KEY_Y = 0x59,
    GAME_KEY_Z = 0x5A,

    // PUNCTUATION
    GAME_KEY_TAB            = 0x09,
    GAME_KEY_SPACE          = 0x20,
    GAME_KEY_SEMICOLON      = 0xBA,
    GAME_KEY_EQUAL          = 0xBB,
    GAME_KEY_COMMA          = 0xBC,
    GAME_KEY_MINUS          = 0xBD,
    GAME_KEY_PERIOD         = 0xBE,
    GAME_KEY_FORWARD_SLASH  = 0xBF,
    GAME_KEY_GRAVE          = 0xC0,
    GAME_KEY_BRACKET_L      = 0xDB,
    GAME_KEY_BACKSLASH      = 0xDC,
    GAME_KEY_BRACKET_R      = 0xDD,
    GAME_KEY_QUOTE          = 0xDE,

    // FUNCTION
    GAME_KEY_F1     = 0x70,
    GAME_KEY_F2     = 0x71,
    GAME_KEY_F3     = 0x72,
    GAME_KEY_F4     = 0x73,
    GAME_KEY_F5     = 0x74,
    GAME_KEY_F6     = 0x75,
    GAME_KEY_F7     = 0x76,
    GAME_KEY_F8     = 0x77,
    GAME_KEY_F9     = 0x78,
    GAME_KEY_F10    = 0x79,
    GAME_KEY_F11    = 0x7A,
    GAME_KEY_F12    = 0x7B,

    // MISC
    GAME_KEY_ESCAPE     = 0x1B,
    GAME_KEY_BACKSPACE  = 0x08,
    GAME_KEY_DELETE     = 0x2E,
    GAME_KEY_CAPS       = 0x14,
    GAME_KEY_SHIFT      = 0x10,
    GAME_KEY_SHIFT_L    = 0xA0,
    GAME_KEY_SHIFT_R    = 0xA1,
    GAME_KEY_ALT        = 0x12,
    GAME_KEY_ALT_L      = 0xA4,
    GAME_KEY_ALT_R      = 0xA5,
    GAME_KEY_CTRL       = 0x11,
    GAME_KEY_CTRL_L     = 0xA2,
    GAME_KEY_CTRL_R     = 0xA3,
    GAME_KEY_ENTER      = 0x0D,
} GameKey;

typedef struct GameInput {
    bool keys_down[256];
    // TODO(ryan): mouse input
} GameInput;

// TODO(ryan): maybe we should split this up into persistent (across frame boundaries) and scratch storage?
typedef struct GameMemory {
    bool is_initialized;
    void *storage;
    size_t storage_size;
} GameMemory;

#ifdef _MSC_VER
    // TODO(ryan): document why we don't have to declare __declspec(dllexport/dllimport) when using MSVC
    // (reminder: we're declaring -EXPORT:func_name to the linker in our build script)
    #define EXPORT
#endif

// TODO(ryan): not sure if I like this approach.
// Here's what I was doing previously:
//      EXPORT void update_and_render(/* args */)
//      typedef void (*GameUpdateAndRender)(/* args */)
// But that means that the prototype for update_and_render will be included in the platform layer,
// which isn't what I wanted (didn't seem to cause issues with MSVC though, so maybe doesn't matter).
#define GAME_UPDATE_AND_RENDER_SIGNATURE(func_name) void func_name(GameOffscreenBuffer *offscreen_buffer, GameInput *input, GameMemory *memory)
#define GAME_UPDATE_AND_RENDER_TYPE(type_name) GAME_UPDATE_AND_RENDER_SIGNATURE((*type_name))
typedef GAME_UPDATE_AND_RENDER_TYPE(GameUpdateAndRender);
