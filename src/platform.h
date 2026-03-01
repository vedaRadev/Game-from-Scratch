// Everything in this file is shared between the following two parts:
// 1. The platform layer which is compiled into an executable.
// 2. The game dll which the platform executable loads dynamically.

// TODO(mal): should we do this or include guards?
#pragma once

// TODO(mal): maybe move common types and "#define"s into a separate file? Not sure if
// I'm going to do a unity build or not yet.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h> // TODO(mal): Should this be here in the header or the implementations of
                   // platform.h? I only put it here because the assertion macros use it (though
                   // technically ofc that code actually is pasted into the files using the macros).
// TODO(mal): Instead of this, maybe make types b8, b16, b32, b64 etc.
typedef enum { false, true } bool;

// TODO(mal): Have some cross platform printing solution in case I want to get rid of the c
// standard library? Might have to have separate assertion definitions for platform and game...
// Could then add some sort of printing function to the GameMemory struct.

// TODO(mal): Move assertion macros into their own file?
// TODO(mal): This assumes that we're running while hooked up to a debugger and can catch a crash.
// See GEA 3rd p126 for details on a perhaps more-proper assertion implementation. Probably want to
// use the __FILE__ and __LINE__ macros to report when an assertion fails, then use some inline
// assembly that will break to a debugger if one is attached.
#ifdef ASSERTIONS_ENABLED
	#define __CRASH_PROGRAM *(volatile char *)0;
	#define ASSERT_MSG(expr, msg)\
		if (!(expr)) {\
			char *out = msg ? msg : #expr;\
			fprintf(stderr, "Assertion failed (%s +%d): %s\n", __FILE__, __LINE__, out);\
			fflush(stderr);\
			__CRASH_PROGRAM;\
		}
	#define ASSERT(expr) ASSERT_MSG(expr, NULL)
	#define ASSERT_MSG_FMT(expr, msg_fmt, ...)\
		if (!(expr)) {\
			char full_fmt[128] = "Assertion failed (%s +%d): ";\
			strncat(full_fmt, msg_fmt, 128 - strlen(full_fmt));\
			fprintf(stderr, full_fmt, __FILE__, __LINE__, __VA_ARGS__);\
			fflush(stderr);\
			__CRASH_PROGRAM;\
		}
#else
	#define __CRASH_PROGRAM
	#define ASSERT_MSG(expr, msg)
	#define ASSERT(expr)
	#define ASSERT_MSG_FMT(expr, msg_fmt, ...)
#endif

typedef struct GameOffscreenBuffer {
    void *memory;
    int width;
    int height;
    int bytes_per_pixel;
} GameOffscreenBuffer;

// NOTE(mal): These codes are based on win32 virtual keycodes
// TODO(mal): add numpad keys?
// TODO(mal): add home, end, insert, page up/down, keys?
// TODO(mal): Maybe stop basing on win32 key codes? Instead, just have the GameKey values increase
// monotonically from 0 or something? Then I could compress the array we actually send as our
// keyboard input to only contain the necessary elements.
typedef enum GameKey {
	GAME_KEY_UNKNOWN = -1,

	// ARROWS
    GAME_KEY_LEFT  = 0x25,
    GAME_KEY_UP    = 0x26,
    GAME_KEY_RIGHT = 0x27,
    GAME_KEY_DOWN  = 0x28,

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
    GAME_KEY_TAB           = 0x09,
    GAME_KEY_SPACE         = 0x20,
    GAME_KEY_SEMICOLON     = 0xBA,
    GAME_KEY_EQUAL         = 0xBB,
    GAME_KEY_COMMA         = 0xBC,
    GAME_KEY_MINUS         = 0xBD,
    GAME_KEY_PERIOD        = 0xBE,
    GAME_KEY_FORWARD_SLASH = 0xBF,
    GAME_KEY_GRAVE         = 0xC0,
    GAME_KEY_BRACKET_L     = 0xDB,
    GAME_KEY_BACKSLASH     = 0xDC,
    GAME_KEY_BRACKET_R     = 0xDD,
    GAME_KEY_QUOTE         = 0xDE,

    // FUNCTION
    GAME_KEY_F1  = 0x70,
    GAME_KEY_F2  = 0x71,
    GAME_KEY_F3  = 0x72,
    GAME_KEY_F4  = 0x73,
    GAME_KEY_F5  = 0x74,
    GAME_KEY_F6  = 0x75,
    GAME_KEY_F7  = 0x76,
    GAME_KEY_F8  = 0x77,
    GAME_KEY_F9  = 0x78,
    GAME_KEY_F10 = 0x79,
    GAME_KEY_F11 = 0x7A,
    GAME_KEY_F12 = 0x7B,

    // MISC
    GAME_KEY_ESCAPE    = 0x1B,
    GAME_KEY_BACKSPACE = 0x08,
    GAME_KEY_DELETE    = 0x2E,
    GAME_KEY_CAPS      = 0x14,
	// WARNING(mal): Linux xkbcommon does not have a generic shift key!
    GAME_KEY_SHIFT     = 0x10,
    GAME_KEY_SHIFT_L   = 0xA0,
    GAME_KEY_SHIFT_R   = 0xA1,
	// WARNING(mal): Linux xkbcommon does not have a generic alt key!
    GAME_KEY_ALT       = 0x12,
    GAME_KEY_ALT_L     = 0xA4,
    GAME_KEY_ALT_R     = 0xA5,
	// WARNING(mal): Linux xkbcommon does not have a generic ctrl key!
    GAME_KEY_CTRL      = 0x11,
    GAME_KEY_CTRL_L    = 0xA2,
    GAME_KEY_CTRL_R    = 0xA3,
    GAME_KEY_ENTER     = 0x0D,
} GameKey;

// TODO(mal): Eventually slam this down to just be bitflags in an n-bit value?
typedef struct ButtonState {
	char is_down;  // Is the key down this update tick?
	char was_down; // Was the key down last update tick?
} ButtonState;

#define NUM_GAME_KEYS 256
typedef struct GameInput {
    ButtonState keys[NUM_GAME_KEYS];
    // TODO(mal): mouse input
} GameInput;

// NOTE(mal): For real file loading functions we'd want something nonblocking
// and to protect against data loss (note taken from Casey Muratori's "Handmade
// Hero")
#define DEBUG_PLATFORM_READ_ENTIRE_FILE_PARAMS (char *file_path)
char *debug_platform_read_entire_file DEBUG_PLATFORM_READ_ENTIRE_FILE_PARAMS;
typedef char *(*DEBUG_PlatformReadEntireFileFunction) DEBUG_PLATFORM_READ_ENTIRE_FILE_PARAMS;

#define DEBUG_PLATFORM_FREE_ENTIRE_FILE_PARAMS (char *file_data, size_t file_len)
void debug_platform_free_entire_file DEBUG_PLATFORM_FREE_ENTIRE_FILE_PARAMS;
typedef void (*DEBUG_PlatformFreeEntireFileFunction) DEBUG_PLATFORM_FREE_ENTIRE_FILE_PARAMS;

typedef struct GameMemory {
	DEBUG_PlatformReadEntireFileFunction debug_platform_read_entire_file;
	DEBUG_PlatformFreeEntireFileFunction debug_platform_free_entire_file;

	// TODO(mal): maybe we should split this up into persistent (across frame boundaries) and scratch storage?
    void *storage;
    size_t storage_size;
} GameMemory;

#if defined(_MSC_VER)
	// NOTE(mal): If using -EXPORT in build script, don't have to specify __declspec(dllexport/import) here
    #define EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
	#define EXPORT __attribute__((visibility("default")))
#else
	#define EXPORT
	#pragma warning Unknown dynamic link import/export semantics
#endif

#define GAME_INIT_PARAMS (GameMemory *memory, int initial_width, int initial_height)
EXPORT void game_init GAME_INIT_PARAMS;
typedef void (*GameInitFunction) GAME_INIT_PARAMS;

#define GAME_RENDER_PARAMS (GameMemory *memory, GameOffscreenBuffer *offscreen_buffer)
EXPORT void game_render GAME_RENDER_PARAMS;
typedef void (*GameRenderFunction) GAME_RENDER_PARAMS;

#define GAME_UPDATE_PARAMS (GameMemory *memory, GameInput *input)
EXPORT void game_update GAME_UPDATE_PARAMS;
typedef void (*GameUpdateFunction) GAME_UPDATE_PARAMS;

// // NOTE(mal): Just leaving the old way of doing this here in case I want to go back to it
// #define GAME_UPDATE_AND_RENDER_SIGNATURE(func_name) void func_name(GameOffscreenBuffer *offscreen_buffer, GameInput *input, GameMemory *memory)
// #define GAME_UPDATE_AND_RENDER_TYPE(type_name) GAME_UPDATE_AND_RENDER_SIGNATURE((*type_name))
// typedef GAME_UPDATE_AND_RENDER_TYPE(GameUpdateAndRender);
