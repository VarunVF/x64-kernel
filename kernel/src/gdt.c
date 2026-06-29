#include "gdt.h"

extern void gdt_flush(struct gdt_pointer*);

struct gdt_entry gdt[5];
struct gdt_pointer gdt_ptr;

void gdt_init() {
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base = (uintptr_t)&gdt;

    // Notes for long mode:
    // - The base and limit values are ignored.
    // - The long-mode code flag (L) in gran is set for code segments, and clear for data segments.
    gdt_set_gate(0, 0, 0, 0x00, 0x00);  // Null descriptor
    gdt_set_gate(1, 0, 0, 0x9A, 0x20);  // Kernel code
    gdt_set_gate(2, 0, 0, 0x92, 0x00);  // Kernel data
    gdt_set_gate(3, 0, 0, 0xFA, 0x20);  // User code
    gdt_set_gate(4, 0, 0, 0xF2, 0x00);  // User data

    gdt_flush(&gdt_ptr);
}

void gdt_set_gate(uint32_t index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[index].base_low = base & 0xFFFF;
    gdt[index].base_middle = (base >> 16) & 0xFF;
    gdt[index].base_high = (base >> 24) & 0xFF;

    gdt[index].limit = limit & 0xFFFF;
    gdt[index].flags = (limit >> 16) & 0x0F;

    gdt[index].flags |= gran & 0xF0;
    gdt[index].access = access;
}
