// Everything in this file is shared between the following two parts:
// 1. The platform layer which is compiled into an executable.
// 2. The game dll which the platform executable loads dynamically.

// TODO(ryan): should we do this or include guards?
#pragma once

struct GameOffscreenBuffer {
    void *memory;
    int width;
    int height;
    int bytes_per_pixel;
};

#define GAME_UPDATE_AND_RENDER(name) void name(GameOffscreenBuffer *offscreen_buffer)
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRender);
