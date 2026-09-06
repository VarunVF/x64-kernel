#include "ps2kbd.h"

#include <stdint.h>

#include "../arch/cpu.h"
#include "../kprint.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64

#define PS2_STATUS_OUTPUT_FULL (1 << 0)
#define PS2_STATUS_INPUT_FULL (1 << 1)


static void ps2_flush_output_buffer() {
    while ((cpu_inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL)) {
        cpu_inb(PS2_DATA);
    }
}

void ps2kbd_init() {
    ps2_flush_output_buffer();
}

void ps2kbd_input() {
    uint8_t scancode = cpu_inb(0x60) & 0x7F;
    uint8_t press = cpu_inb(0x60) & 0x80;

    kprintf("Scancode: %d, pressed: %d\n", scancode, press);
}
