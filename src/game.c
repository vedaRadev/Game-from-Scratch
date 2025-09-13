#include <stdint.h>
#include "platform.h"

EXPORT GAME_UPDATE_AND_RENDER_SIGNATURE(update_and_render) {
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
