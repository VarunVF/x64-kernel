#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

#include "../limine-protocol/include/limine.h"

void clear(struct limine_framebuffer* framebuffer, uint32_t color);
void put_pixel(struct limine_framebuffer* framebuffer, uint32_t pos_x, uint32_t pos_y, uint32_t color);

void display_str(struct limine_framebuffer* framebuffer, const char* str, uint32_t color);
void display_bin(struct limine_framebuffer* framebuffer, uint64_t number, uint32_t color);
void display_hex(struct limine_framebuffer* framebuffer, uint64_t number, uint32_t color);
void display_dec(struct limine_framebuffer* framebuffer, uint64_t number, uint32_t color);

#endif  // VIDEO_H
