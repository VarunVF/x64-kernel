#ifndef SHELL_H
#define SHELL_H

#include "drivers/ps2kbd.h"

#include <limine.h>

void shell_run(struct ps2kbd_state* ps2kbd, struct limine_framebuffer* framebuffer, uint32_t clear_color);

#endif  // SHELL_H
