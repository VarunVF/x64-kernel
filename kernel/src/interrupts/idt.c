#include "idt.h"

#include <stdint.h>

#include "../io.h"
#include "../memory.h"
#include "pic.h"

struct idt_entry idt[256];
struct idt_pointer idt_ptr;

extern uint64_t isr_stub_table[];

extern void idt_flush(struct idt_pointer*);

// Give the PIC some time to react to the command.
inline static void outb_and_wait(uint16_t port, uint8_t value) {
    outb(port, value);
    io_wait();
}

static void pic_remap(int offset1, int offset2) {
    outb_and_wait(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);  // Restart Master
    outb_and_wait(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);  // Restart Slave

    outb_and_wait(PIC1_DATA, offset1);  // Master now starts at 0x20 (32)
    outb_and_wait(PIC2_DATA, offset2);  // Slave now starts at 0x28 (40)

    outb_and_wait(PIC1_DATA, 1 << CASCADE_IRQ);  // Setup cascading
    outb_and_wait(PIC2_DATA, 2);

    outb_and_wait(PIC1_DATA, ICW4_8086);
    outb_and_wait(PIC2_DATA, ICW4_8086);
}

// Mask (disable) all PIC lines
static void pic_mask() {
    outb_and_wait(PIC1_DATA, 0xFF);
    outb_and_wait(PIC2_DATA, 0xFF);
}

void idt_init() {
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uintptr_t)&idt;

    memset(&idt, 0, sizeof(idt));

    pic_remap(PIC1_OFFSET, PIC2_OFFSET);
    pic_mask();

    for (int i = 0; i < 48; i++) {
        // 0x08 -> 0000 1000 (GDT code segment)
        // 0x8E -> 1000 1110
        idt_set_gate(i, isr_stub_table[i], 0x08, 0x8E);
    }

    idt_flush(&idt_ptr);
}

void idt_set_gate(uint8_t index, uint64_t base, uint16_t selector, uint8_t flags) {
    idt[index].base_low = base & 0xFFFF;
    idt[index].base_middle = (base >> 16) & 0xFFFF;
    idt[index].base_high = (base >> 32) & 0xFFFFFFFF;
    idt[index].selector = selector;
    idt[index].always_0 = 0;
    idt[index].flags = flags;
    idt[index].reserved = 0;
}
