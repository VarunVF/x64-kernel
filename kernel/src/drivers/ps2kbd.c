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
#define READ_TIMEOUT 100000
#define RESET_TIMEOUT 5000000

struct ps2kbd_state {
    struct ps2kbd_event kbd_event_queue[MAX_QUEUED_EVENTS];
    int first;
    int last;
    int size;
};

// Wait until controller is ready to accept bytes
static void ps2_wait_before_write() {
    while (cpu_inb(PS2_STATUS) & PS2_STATUS_INPUT_FULL) {
        cpu_io_wait();
    }
}

// Wait until controller has data to be read.
// Returns 0 on success
static int ps2_wait_before_read(int timeout_loops) {
    for (int i = 0; i < timeout_loops; i++) {
        if (cpu_inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) {
            return 0;
        }
        cpu_io_wait();
    }

    return 1;
}

static void ps2_flush_output_buffer() {
    while ((cpu_inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL)) {
        cpu_inb(PS2_DATA);
    }
}

static int ps2_init() {
    // Determine if controller exists...

    // Disable devices
    ps2_wait_before_write();
    cpu_outb(PS2_COMMAND, 0xAD);
    ps2_wait_before_write();
    cpu_outb(PS2_COMMAND, 0xA7);

    // Flush output now that devices are disabled
    ps2_flush_output_buffer();

    // Set the Controller Configuration Byte
    ps2_wait_before_write();
    cpu_outb(PS2_COMMAND, 0x20);
    if (ps2_wait_before_read(READ_TIMEOUT) != 0) {
        return 1;
    }
    uint8_t config = cpu_inb(PS2_DATA);

    config &= ~(1 << 0);    // Disable IRQ
    config &= ~(1 << 1);    // Disable IRQ
    config &= ~(1 << 6);    // Disable translation
    
    ps2_wait_before_write();
    cpu_outb(PS2_COMMAND, 0x60);
    ps2_wait_before_write();
    cpu_outb(PS2_DATA, config);

    // Perform controller self test...
    ps2_wait_before_write();
    cpu_outb(PS2_COMMAND, 0xAA);
    if (ps2_wait_before_read(READ_TIMEOUT) != 0
     || cpu_inb(PS2_DATA) != 0x55) {
        kprintf("PS/2 controller self-test failed\n");
        return 2;
    }

    // Controller may have reset during self-test, so restore the config byte
    ps2_wait_before_write();
    cpu_outb(PS2_COMMAND, 0x60);
    ps2_wait_before_write();
    cpu_outb(PS2_DATA, config);

    // Determine if there are 2 channels...

    // Perform interface tests...
    ps2_wait_before_write();
    cpu_outb(PS2_COMMAND, 0xAB);
    if (ps2_wait_before_read(READ_TIMEOUT) != 0
     || cpu_inb(PS2_DATA) != 0x00) {
        kprintf("PS/2 Port 1 interface test failed.\n");
        return 3;
    }

    // Enable devices
    ps2_wait_before_write();
    cpu_outb(PS2_COMMAND, 0xAE);

    // Reset devices
    ps2_wait_before_write();
    cpu_outb(PS2_DATA, 0xFF);

    if (ps2_wait_before_read(RESET_TIMEOUT) != 0) {
        kprintf("PS/2 reset timed out waiting for ACK (0xFA)\n");
        return 4;
    }
    uint8_t ack = cpu_inb(PS2_DATA);
    if (ack != 0xFA) {
        kprintf("PS/2 reset unexpected byte: 0x%x (expected 0xFA)\n", ack);
        return 5;
    }

    if (ps2_wait_before_read(RESET_TIMEOUT) != 0) {
        kprintf("PS/2 reset timed out waiting for BAT (0xAA)\n");
        return 6;
    }
    uint8_t bat = cpu_inb(PS2_DATA);
    if (bat != 0xAA) {
        kprintf("PS/2 reset unexpected byte: 0x%x (expected 0xAA)\n", bat);
        return 7;
    }

    // Re-enable IRQs and translation
    ps2_wait_before_write();
    cpu_outb(PS2_COMMAND, 0x20);
    if (ps2_wait_before_read(READ_TIMEOUT) != 0) return -1;
    config = cpu_inb(PS2_DATA);

    config |= (1 << 0);  // Enable port 1 IRQ
    config |= (1 << 6);  // Enable translation (to scancode set 1)

    ps2_wait_before_write();
    cpu_outb(PS2_COMMAND, 0x60);
    ps2_wait_before_write();
    cpu_outb(PS2_DATA, config);

    return 0;
}

struct ps2kbd_state* ps2kbd_init(uint8_t vector) {
    int status = ps2_init();
    if (status != 0) {
        kprintf("PS/2 initialisation failed (error %d)\n", status);
        return NULL;
    }

    kprintf("PS/2 controller and keyboard OK!\n");

    struct ps2kbd_state* ps2kbd = kmalloc(sizeof(struct ps2kbd_state));
    if (ps2kbd == NULL) {
        return NULL;
    }

    memset(ps2kbd, 0, sizeof(*ps2kbd));
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

static bool ps2kbd_is_event_queue_full(struct ps2kbd_state* ps2kbd) {
    return ps2kbd->size == MAX_QUEUED_EVENTS;
}

void ps2kbd_send_event(struct ps2kbd_state* ps2kbd, struct ps2kbd_event event) {
    if (!ps2kbd_is_event_queue_full(ps2kbd)) {
        ps2kbd->kbd_event_queue[ps2kbd->last] = event;
        ps2kbd->last = (ps2kbd->last + 1) % MAX_QUEUED_EVENTS;
        ps2kbd->size++;
    }
}

struct cpu_status_t* ps2kbd_irq(struct cpu_status_t* regs, void* context) {
    uint8_t scancode = cpu_inb(0x60) & 0x7F;
    uint8_t press = cpu_inb(0x60) & 0x80;

    ps2kbd_send_event(context, (struct ps2kbd_event){
        scancode,
        press == 0 ? KBD_EVENT_TYPE_PRESS : KBD_EVENT_TYPE_RELEASE
    });

    return regs;
}

void ps2kbd_print_debug(struct ps2kbd_state* ps2kbd) {
    kprintf("Left: %d, right: %d, %d events in queue\n", ps2kbd->first, ps2kbd->last, ps2kbd->size);

    for (int i = 0; i < ps2kbd->size; i++) {
        int index = (ps2kbd->first + i) % MAX_QUEUED_EVENTS;
        struct ps2kbd_event event = ps2kbd->kbd_event_queue[index];
        kprintf("Event #%d: scancode=%d type=%d\n", i, event.scancode, event.type);
    }
}
