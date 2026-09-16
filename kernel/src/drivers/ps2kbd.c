#include "drivers/ps2kbd.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/interrupt_handler.h"
#include "cpu.h"
#include "kprint.h"
#include "kstring.h"
#include "memory/kmalloc.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64

#define PS2_STATUS_OUTPUT_FULL (1 << 0)
#define PS2_STATUS_INPUT_FULL (1 << 1)

#define MAX_QUEUED_EVENTS 128

struct ps2kbd_state {
    struct ps2kbd_event kbd_event_queue[MAX_QUEUED_EVENTS];
    int first;
    int last;
    int size;
};

static void ps2_flush_output_buffer() {
    while ((cpu_inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL)) {
        cpu_inb(PS2_DATA);
    }
}

static bool ps2kbd_is_event_queue_full(struct ps2kbd_state* ps2kbd) {
    return ps2kbd->size == MAX_QUEUED_EVENTS;
}

struct ps2kbd_state* ps2kbd_init(uint8_t vector) {
    ps2_flush_output_buffer();
    
    struct ps2kbd_state* ps2kbd = (struct ps2kbd_state*)kmalloc(sizeof(struct ps2kbd_state));
    memset(ps2kbd, 0, sizeof(struct ps2kbd_state));
    interrupt_set_handler(vector, ps2kbd_irq, ps2kbd);

    return ps2kbd;
}

void ps2kbd_free(struct ps2kbd_state* ps2kbd, uint8_t vector) {
    interrupt_set_handler(vector, NULL, NULL);
    kfree(ps2kbd);
}

bool ps2kbd_has_event(struct ps2kbd_state* ps2kbd) {
    return ps2kbd->size > 0;
}

size_t ps2kbd_event_count(struct ps2kbd_state* ps2kbd) {
    return ps2kbd->size;
}
bool ps2kbd_get_event(struct ps2kbd_state* ps2kbd, struct ps2kbd_event* out_event) {
    if (!ps2kbd_has_event(ps2kbd))
        return false;

    *out_event = ps2kbd->kbd_event_queue[ps2kbd->first];
    ps2kbd->first = (ps2kbd->first + 1) % MAX_QUEUED_EVENTS;
    ps2kbd->size--;
    return true;
}

void ps2kbd_send_event(struct ps2kbd_state* ps2kbd, struct ps2kbd_event event) {
    if (!ps2kbd_is_event_queue_full(ps2kbd)) {
        ps2kbd->kbd_event_queue[ps2kbd->last] = event;
        ps2kbd->last = (ps2kbd->last + 1) % MAX_QUEUED_EVENTS;
        ps2kbd->size++;
    }
}

void ps2kbd_irq(void* context) {
    uint8_t scancode = cpu_inb(0x60) & 0x7F;
    uint8_t press = cpu_inb(0x60) & 0x80;

    ps2kbd_send_event(context, (struct ps2kbd_event){
        scancode,
        press == 0 ? KBD_EVENT_TYPE_PRESS : KBD_EVENT_TYPE_RELEASE
    });
}

void ps2kbd_print_debug(struct ps2kbd_state* ps2kbd) {
    kprintf("Left: %d, right: %d, %d events in queue\n", ps2kbd->first, ps2kbd->last, ps2kbd->size);

    for (int i = 0; i < ps2kbd->size; i++) {
        int index = (ps2kbd->first + i) % MAX_QUEUED_EVENTS;
        struct ps2kbd_event event = ps2kbd->kbd_event_queue[index];
        kprintf("Event #%d: scancode=%d type=%d\n", i, event.scancode, event.type);
    }
}
