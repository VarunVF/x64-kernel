#include "video.h"

#include <stdint.h>
#include <stddef.h>

#include "../limine-protocol/include/limine.h"

extern const uint8_t font_data[128][8];

struct position {
    int x;
    int y;
};
static struct position display_pos = {0, 0};

void clear(struct limine_framebuffer* framebuffer, uint32_t color) {
    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
    uint32_t *fb_ptr = framebuffer->address;
    for (size_t y = 0; y < framebuffer->height; y++) {
        for (size_t x = 0; x < framebuffer->width; x++) {
            fb_ptr[y * (framebuffer->pitch / 4) + x] = color;
        }
    }
}

void put_pixel(struct limine_framebuffer* framebuffer, int pos_x, int pos_y, uint32_t color) {
    // Note: we assume the framebuffer model is RGB with 32-bit pixels.

    if (pos_x >= framebuffer->width || pos_y >= framebuffer->height) {
        return;
    }

    uint32_t* fb_ptr = framebuffer->address;
    fb_ptr[pos_y * (framebuffer->pitch / 4) + pos_x] = color;
}

void display_char(struct limine_framebuffer* framebuffer, char glyph, uint32_t color) {
    uint32_t start_x = display_pos.x * 8;
    uint32_t start_y = display_pos.y * 8;

    const uint8_t* glyph_data = font_data[(uint8_t)glyph];

    for (uint8_t l = 0; l < 8; l++) {
        uint32_t current_y = start_y + l;
        
        for (uint8_t i = 0; i < 8; i++) {
            if ((glyph_data[l] & (1 << i))) {
                uint32_t current_x = start_x + i;
                put_pixel(framebuffer, current_x, current_y, color);
            }
        }
    }

    display_pos.x++;
}

void display_str(struct limine_framebuffer* framebuffer, const char* str, uint32_t color) {
    if (str == NULL) {
        return;
    }

    char current;
    while ((current = *str) != '\0') {
        if (current == '\n') {
            display_pos.x = 0;
            display_pos.y++;
        } else {
            display_char(framebuffer, current, color);
        }

        str++;
    }
}
