#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>

#include "../limine-protocol/include/limine.h"

void render_clear(struct limine_framebuffer* framebuffer, uint32_t color);
void render_pixel(struct limine_framebuffer* framebuffer, uint32_t pos_x, uint32_t pos_y, uint32_t color);
void render_char(struct limine_framebuffer* framebuffer, char glyph, uint32_t color);
void render_str(struct limine_framebuffer* framebuffer, const char* str, uint32_t color);

#endif  // RENDER_H
