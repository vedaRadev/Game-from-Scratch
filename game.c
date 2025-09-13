#include <stdint.h>
#include "platform.h"

EXPORT GAME_UPDATE_AND_RENDER_SIGNATURE(update_and_render) {
    // TODO(ryan): just test code, do NOT use static storage in game DLL!
    static uint8_t r = 0;
    static uint8_t g = 0;
    static uint8_t b = 0;

    if (input->keys_down[GAME_KEY_R]) r += 10;
    if (input->keys_down[GAME_KEY_G]) g += 10;
    if (input->keys_down[GAME_KEY_B]) b += 10;

    uint8_t *row = (uint8_t *)offscreen_buffer->memory;
    for (int y = 0; y < offscreen_buffer->height; y++) {
        uint32_t *pixel = (uint32_t *)row;
        for (int x = 0; x < offscreen_buffer->width; x++) {
            *pixel = (r << 16) | (g << 8) | b;
            pixel++;
        }
        row += (offscreen_buffer->width * offscreen_buffer->bytes_per_pixel);
    }
}
