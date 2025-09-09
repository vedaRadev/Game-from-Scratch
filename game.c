#include <stdint.h>
#include "platform.h"

EXPORT GAME_UPDATE_AND_RENDER_SIGNATURE(update_and_render) {
    const uint32_t color = 0x003090FF;
    uint8_t *row = (uint8_t *)offscreen_buffer->memory;
    for (int y = 0; y < offscreen_buffer->height; y++) {
        uint32_t *pixel = (uint32_t *)row;
        for (int x = 0; x < offscreen_buffer->width; x++) {
            *pixel = color;
            pixel++;
        }
        row += (offscreen_buffer->width * offscreen_buffer->bytes_per_pixel);
    }
}
