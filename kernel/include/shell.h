#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>

#include <limine.h>

#include "drivers/ps2kbd.h"

struct cell {
    uint8_t character;
};

struct shell_display {
    struct cell* cells;
    const size_t width;
    const size_t height;
    size_t x, y;
};

struct shell_state {
    struct shell_display display;
    struct ps2kbd_state* ps2kbd;
    struct limine_framebuffer* framebuffer;
    uint32_t text_color;
    uint32_t clear_color;
};

typedef void (*shell_fn)(struct shell_state* shell);

struct shell_fn_entry {
    const char* name;
    const shell_fn command;
};

struct shell_init_arg {
    struct ps2kbd_state* ps2kbd;
    struct limine_framebuffer* framebuffer;
};

void shell_run(struct shell_init_arg* arg);

#endif  // SHELL_H
