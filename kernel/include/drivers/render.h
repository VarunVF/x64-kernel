#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>

#include <limine.h>

#define RENDER_CELL_SIZE 8
#define RENDER_DEFAULT_TEXT_COLOR 0x00ffffff
#define RENDER_DEFAULT_CLEAR_COLOR 0x00000077

void render_move_pos(int dx, int dy);
void render_reset_pos();
void render_clear_pos(struct limine_framebuffer* framebuffer, uint32_t clear_color);
void render_clear(struct limine_framebuffer* framebuffer, uint32_t clear_color);
void render_pixel(struct limine_framebuffer* framebuffer, uint32_t pos_x, uint32_t pos_y, uint32_t color);
void render_char(struct limine_framebuffer* framebuffer, char glyph, uint32_t color, uint32_t clear_color);
void render_char_at(struct limine_framebuffer* framebuffer, char glyph, uint32_t cell_x, uint32_t cell_y, uint32_t color, uint32_t clear_color);

#endif  // RENDER_H
