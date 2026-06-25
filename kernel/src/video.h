#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

#include "../limine-protocol/include/limine.h"

void clear(struct limine_framebuffer* framebuffer, uint32_t color);
void put_pixel(struct limine_framebuffer* framebuffer, int pos_x, int pos_y, uint32_t color);

void display_char(struct limine_framebuffer* framebuffer, char glyph, uint32_t color);
void display_str(struct limine_framebuffer* framebuffer, const char* str, uint32_t color);

#endif  // VIDEO_H
