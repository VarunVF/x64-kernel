#include "render.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../limine-protocol/include/limine.h"
#include "kstring.h"

extern const uint8_t font_data[128][8];

struct position {
    int x;
    int y;
};
static struct position render_pos = {0, 0};
static bool is_after_newline = true;

// Note: we assume the framebuffer model is RGB with 32-bit pixels.

static inline uint32_t* pixel_addr(struct limine_framebuffer* framebuffer, uint64_t x, uint64_t y) {
    uint8_t* pixel_byte_addr = (uint8_t*)framebuffer->address + (y * framebuffer->pitch) + (x * framebuffer->bpp / 8);
    return (uint32_t*)pixel_byte_addr;
}

void render_clear(struct limine_framebuffer* framebuffer, uint32_t clear_color) {
    for (uint64_t y = 0; y < framebuffer->height; y++) {
        for (uint64_t x = 0; x < framebuffer->width; x++) {
            *pixel_addr(framebuffer, x, y) = clear_color;
        }
    }
}

void render_pixel(struct limine_framebuffer* framebuffer, uint32_t x, uint32_t y, uint32_t color) {
    if (x >= framebuffer->width || y >= framebuffer->height) {
        return;
    }

    *pixel_addr(framebuffer, x, y) = color;
}

static inline bool render_should_scroll(struct limine_framebuffer* framebuffer) {
    return (uint64_t)render_pos.y >= framebuffer->height / 8;
}

static void render_scroll(struct limine_framebuffer* framebuffer) {
    const size_t line_size = framebuffer->pitch * 8;
    const size_t fb_size = framebuffer->pitch * framebuffer->height;
    const uint8_t* second_line_start = (uint8_t*)framebuffer->address + line_size;
    memmove(framebuffer->address, second_line_start, fb_size - line_size);
}

static void render_clear_line(struct limine_framebuffer* framebuffer, int line, uint32_t clear_color) {
    uint64_t start_y = line * 8;
    for (uint64_t y = start_y; y < start_y + 8; y++) {
        for (uint64_t x = 0; x < framebuffer->width; x++) {
            *pixel_addr(framebuffer, x, y) = clear_color;
        }
    }
}

void render_char(struct limine_framebuffer* framebuffer, char glyph, uint32_t color, uint32_t clear_color) {
    uint32_t start_x = render_pos.x * 8;
    uint32_t start_y = render_pos.y * 8;

    if (is_after_newline) {
        render_clear_line(framebuffer, render_pos.y, clear_color);
        is_after_newline = false;
    }

    if (glyph == '\n') {
        render_pos.x = 0;
        render_pos.y++;

        if (render_should_scroll(framebuffer)) {
            render_scroll(framebuffer);
            render_clear_line(framebuffer, render_pos.y - 1, clear_color);
            render_pos.y -= 1;
        }

        is_after_newline = true;
        return;
    }

    uint8_t glyph_index = (uint8_t)glyph;
    if (glyph_index > 127) {
        // Replace invalid character with '?'
        glyph_index = (uint8_t)'?';
    }

    const uint8_t* glyph_data = font_data[glyph_index];
    
    for (uint8_t l = 0; l < 8; l++) {
        uint32_t current_y = start_y + l;
        
        for (uint8_t i = 0; i < 8; i++) {
            if ((glyph_data[l] & (1 << i))) {
                uint32_t current_x = start_x + i;
                render_pixel(framebuffer, current_x, current_y, color);
            }
        }
    }

    render_pos.x++;
}
