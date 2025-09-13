#include <stdint.h>
#include "platform.h"

typedef enum ThingToRender {
    TTR_1,
    TTR_2,
    TTR_3,
} ThingToRender;

typedef struct GameState {
    ThingToRender selection;
} GameState;

void render_thing_1(GameOffscreenBuffer *offscreen_buffer) {
    const uint32_t RED   = 0x00FF0000;
    const uint32_t BLACK = 0;
    // TODO(ryan): just test code, do NOT use static storage in game DLL!
    uint8_t *row = (uint8_t *)offscreen_buffer->memory;
    for (int y = 0; y < offscreen_buffer->height; y++) {
        uint32_t *pixel = (uint32_t *)row;
        for (int x = 0; x < offscreen_buffer->width; x++) {
            if (
                (x == offscreen_buffer->width * 0.25 || x == offscreen_buffer->width * 0.75)
                || (y == offscreen_buffer->height * 0.25 || y == offscreen_buffer->height * 0.75)
            ) {
                *pixel = RED;
            } else {
                *pixel = BLACK;
            }
            pixel++;
        }
        row += (offscreen_buffer->width * offscreen_buffer->bytes_per_pixel);
    }
}

void render_thing_2(GameOffscreenBuffer *offscreen_buffer) {
    const size_t num_pixels = offscreen_buffer->width * offscreen_buffer->height;
    uint32_t *pixel = offscreen_buffer->memory;
    for (int i = 0; i < num_pixels; i++) {
        *pixel = 0x00FF0000;
        pixel++;
    }
}

void render_thing_3(GameOffscreenBuffer *offscreen_buffer) {
    uint8_t *row = (uint8_t *)offscreen_buffer->memory;
    for (int y = 0; y < offscreen_buffer->height; y++) {
        uint32_t *pixel = (uint32_t *)row;
        for (int x = 0; x < offscreen_buffer->width; x++) {
            uint8_t red = (uint8_t)x;
            uint8_t green = (uint8_t)y;
            uint8_t blue = (uint8_t)(x * y);
            *pixel = (red << 16) | (green << 8) | blue;
            pixel++;
        }
        row += (offscreen_buffer->width * offscreen_buffer->bytes_per_pixel);
    }
}

EXPORT GAME_UPDATE_AND_RENDER_SIGNATURE(update_and_render) {
    GameState *game_state = (GameState *)memory->storage;
    if (!memory->is_initialized) {
        memory->is_initialized = true;

        game_state->selection = TTR_1;
    }

    if (input->keys_down[GAME_KEY_1]) {
        game_state->selection = TTR_1;
    } else if (input->keys_down[GAME_KEY_2]) {
        game_state->selection = TTR_2;
    } else if (input->keys_down[GAME_KEY_3]) {
        game_state->selection = TTR_3;
    }

    switch (game_state->selection) {
        case TTR_1: render_thing_1(offscreen_buffer); break;
        case TTR_2: render_thing_2(offscreen_buffer); break;
        case TTR_3: render_thing_3(offscreen_buffer); break;
    }
}

