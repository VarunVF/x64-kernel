#include "render.h"

#include <stdint.h>
#include <stddef.h>

#include "../limine-protocol/include/limine.h"

extern const uint8_t font_data[128][8];

struct position {
    int x;
    int y;
};
static struct position render_pos = {0, 0};

void render_clear(struct limine_framebuffer* framebuffer, uint32_t color) {
    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
    uint32_t *fb_ptr = framebuffer->address;
    for (size_t y = 0; y < framebuffer->height; y++) {
        for (size_t x = 0; x < framebuffer->width; x++) {
            fb_ptr[y * (framebuffer->pitch / 4) + x] = color;
        }
    }
}

void render_pixel(struct limine_framebuffer* framebuffer, uint32_t pos_x, uint32_t pos_y, uint32_t color) {
    // Note: we assume the framebuffer model is RGB with 32-bit pixels.

    if (pos_x >= framebuffer->width || pos_y >= framebuffer->height) {
        return;
    }

    uint32_t* fb_ptr = framebuffer->address;
    fb_ptr[pos_y * (framebuffer->pitch / 4) + pos_x] = color;
}

void render_char(struct limine_framebuffer* framebuffer, char glyph, uint32_t color) {
    uint32_t start_x = render_pos.x * 8;
    uint32_t start_y = render_pos.y * 8;

    if (glyph == '\n') {
        render_pos.x = 0;
        render_pos.y++;
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

void render_str(struct limine_framebuffer* framebuffer, const char* str, uint32_t color) {
    if (str == NULL) {
        return;
    }

    char current;
    while ((current = *str) != '\0') {
        render_char(framebuffer, current, color);
        str++;
    }
}

void render_bin(struct limine_framebuffer* framebuffer, uint64_t number, uint32_t color) {
    const int total_bits = sizeof(number) * 8;
    for (int i = total_bits - 1; i >= 0; i--) {
        int bit = (number >> i) & 1;
        if (bit) {
            render_char(framebuffer, '1', color);
        } else {
            render_char(framebuffer, '0', color);
        }
    }
}

void render_hex(struct limine_framebuffer* framebuffer, uint64_t number, uint32_t color) {
    const int total_bytes = sizeof(number);
    const int hex_digits = total_bytes * 2;
    const char hex_table[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'a', 'b', 'c', 'd', 'e', 'f'
    };
    for (int i = hex_digits - 1; i >= 0; i--) {
        int digit = (number >> i * 4) & 0xf;
        render_char(framebuffer, hex_table[digit], color);
    }
}

void render_dec(struct limine_framebuffer* framebuffer, uint64_t number, uint32_t color) {
    // UINT64_MAX (18446744073709551615UL) has 20 decimal digits,
    // so we need space for at most 20 characters.
    char digits[20] = {0};
    int current_digit = 0;

    // Append the digits from right to left.
    while (number > 0) {
        int digit = number % 10;
        digits[current_digit++] = '0' + digit;
        number /= 10;
    }

    // Print the digits in reverse order.
    while (current_digit--) {
        render_char(framebuffer, digits[current_digit], color);
    }
}
