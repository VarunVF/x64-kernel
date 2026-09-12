#ifndef PS2KBD_H
#define PS2KBD_H

#include <stdbool.h>
#include <stdint.h>

enum ps2kbd_event_type : uint8_t {
    KBD_EVENT_TYPE_PRESS,
    KBD_EVENT_TYPE_RELEASE
};

struct ps2kbd_event {
    uint8_t scancode;
    enum ps2kbd_event_type type;
};

void ps2kbd_init();
bool ps2kbd_has_event();
bool ps2kbd_get_event(struct ps2kbd_event* out_event);
void ps2kbd_send_event(struct ps2kbd_event event);
void ps2kbd_irq();
void ps2kbd_print_debug();

#endif  // PS2KBD_H
