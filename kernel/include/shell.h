#ifndef SHELL_H
#define SHELL_H

#include "drivers/ps2kbd.h"

#include <limine.h>

struct shell_context {
    struct ps2kbd_state* ps2kbd;
    struct limine_framebuffer* framebuffer;
    uint32_t clear_color;
};

void shell_run(struct shell_context* arg);

#endif  // SHELL_H
