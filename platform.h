// Everything in this file is shared between the following two parts:
// 1. The platform layer which is compiled into an executable.
// 2. The game dll which the platform executable loads dynamically.

// TODO(ryan): should we do this or include guards?
#pragma once

typedef struct GameOffscreenBuffer {
    void *memory;
    int width;
    int height;
    int bytes_per_pixel;
} GameOffscreenBuffer;

#ifdef _MSC_VER
    // TODO(ryan): document why we don't have to declare __declspec(dllexport/dllimport) when using MSVC
    // (reminder: we're declaring -EXPORT:func_name to the linker in our build script)
#   define EXPORT
#endif

// TODO(ryan): not sure if I like this approach.
// Here's what I was doing previously:
//      EXPORT void update_and_render(/* args */)
//      typedef void (*GameUpdateAndRender)(/* args */)
// But that means that the prototype for update_and_render will be included in the platform layer,
// which isn't what I wanted (didn't seem to cause issues with MSVC though, so maybe doesn't matter).
#define GAME_UPDATE_AND_RENDER_SIGNATURE(func_name) void func_name(GameOffscreenBuffer *offscreen_buffer)
#define GAME_UPDATE_AND_RENDER_TYPE(type_name) GAME_UPDATE_AND_RENDER_SIGNATURE((*type_name))
typedef GAME_UPDATE_AND_RENDER_TYPE(GameUpdateAndRender);
