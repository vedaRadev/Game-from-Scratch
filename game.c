#include <stdint.h>
#include "platform.h"

// Declaring this as extern "C" prevents the C++ compiler name mangling on export
void update_and_render(GameOffscreenBuffer *offscreen_buffer) {
    uint8_t *row = (uint8_t *)offscreen_buffer->memory;
    for (int y = 0; y < offscreen_buffer->height; y++) {
        uint32_t *pixel = (uint32_t *)row;
        for (int x = 0; x < offscreen_buffer->width; x++) {
            *pixel = 0x00FFFF00;
            pixel++;
        }
        row += (offscreen_buffer->width * offscreen_buffer->bytes_per_pixel);
    }
}
