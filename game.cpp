#include <stdint.h>
#include "platform.h"

// Declaring this as extern "C" prevents the C++ compiler name mangling on export
extern "C" GAME_UPDATE_AND_RENDER(update_and_render) {
    uint8_t *row = (uint8_t *)offscreen_buffer->memory;
    for (int y = 0; y < offscreen_buffer->height; y++) {
        uint32_t *pixel = (uint32_t *)row;
        for (int x = 0; x < offscreen_buffer->width; x++) {
            *pixel = 0x00FF0000;
            pixel++;
        }
        row += (offscreen_buffer->width * offscreen_buffer->bytes_per_pixel);
    }
}
