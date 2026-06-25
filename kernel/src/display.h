#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#include "../limine-protocol/include/limine.h"

void display_clear(struct limine_framebuffer* framebuffer, uint32_t color);
void display_pixel(struct limine_framebuffer* framebuffer, uint32_t pos_x, uint32_t pos_y, uint32_t color);

void display_char(struct limine_framebuffer* framebuffer, char glyph, uint32_t color);
void display_str(struct limine_framebuffer* framebuffer, const char* str, uint32_t color);
void display_bin(struct limine_framebuffer* framebuffer, uint64_t number, uint32_t color);
void display_hex(struct limine_framebuffer* framebuffer, uint64_t number, uint32_t color);
void display_dec(struct limine_framebuffer* framebuffer, uint64_t number, uint32_t color);

#endif  // DISPLAY_H
