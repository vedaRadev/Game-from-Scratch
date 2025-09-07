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
#   define EXPORT __declspec(dllexport)  
#endif

EXPORT void update_and_render(GameOffscreenBuffer *);
typedef void (*GameUpdateAndRender)(GameOffscreenBuffer *);
