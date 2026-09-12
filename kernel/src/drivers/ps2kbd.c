#include "drivers/ps2kbd.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cpu.h"
#include "kprint.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64

#define PS2_STATUS_OUTPUT_FULL (1 << 0)
#define PS2_STATUS_INPUT_FULL (1 << 1)

#define MAX_QUEUED_EVENTS 128

static struct ps2kbd_event kbd_event_queue[MAX_QUEUED_EVENTS] = {0};
static int first = 0;
static int last = 0;
static int size = 0;

static void ps2_flush_output_buffer() {
    while ((cpu_inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL)) {
        cpu_inb(PS2_DATA);
    }
}

static bool ps2kbd_is_event_queue_full() {
    return size == MAX_QUEUED_EVENTS;
}

void ps2kbd_init() {
    ps2_flush_output_buffer();
}

bool ps2kbd_has_event() {
    return size > 0;
}

size_t ps2kbd_event_count() {
    return size;
}
bool ps2kbd_get_event(struct ps2kbd_event* out_event) {
    if (!ps2kbd_has_event())
        return false;

    *out_event = kbd_event_queue[first];
    first = (first + 1) % MAX_QUEUED_EVENTS;
    size--;
    return true;
}

void ps2kbd_send_event(struct ps2kbd_event event) {
    if (!ps2kbd_is_event_queue_full()) {
        kbd_event_queue[last] = event;
        last = (last + 1) % MAX_QUEUED_EVENTS;
        size++;
    }
}

void ps2kbd_irq() {
    uint8_t scancode = cpu_inb(0x60) & 0x7F;
    uint8_t press = cpu_inb(0x60) & 0x80;

    ps2kbd_send_event((struct ps2kbd_event){
        scancode,
        press == 0 ? KBD_EVENT_TYPE_PRESS : KBD_EVENT_TYPE_RELEASE
    });
}

void ps2kbd_print_debug() {
    kprintf("Left: %d, right: %d, %d events in queue\n", first, last, size);

    for (int i = 0; i < size; i++) {
        int index = (first + i) % MAX_QUEUED_EVENTS;
        struct ps2kbd_event event = kbd_event_queue[index];
        kprintf("Event #%d: scancode=%d type=%d\n", i, event.scancode, event.type);
    }
}
