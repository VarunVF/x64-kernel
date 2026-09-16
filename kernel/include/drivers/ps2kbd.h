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

struct ps2kbd_state;

struct ps2kbd_state* ps2kbd_init(uint8_t vector);
void ps2kbd_free(struct ps2kbd_state* ps2kbd, uint8_t vector);
bool ps2kbd_has_event(struct ps2kbd_state* ps2kbd);
bool ps2kbd_get_event(struct ps2kbd_state* ps2kbd, struct ps2kbd_event* out_event);
void ps2kbd_send_event(struct ps2kbd_state* ps2kbd, struct ps2kbd_event event);
void ps2kbd_irq(void* context);
void ps2kbd_print_debug(struct ps2kbd_state* ps2kbd);

#endif  // PS2KBD_H
