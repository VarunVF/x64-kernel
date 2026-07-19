#ifndef GDT_H
#define GDT_H

#include <stdint.h>

struct gdt_entry {
    uint16_t limit;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t flags;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_pointer {
    uint16_t limit;  // The size of the table in bytes, subtracted by 1.
    uintptr_t base;  // The linear address of the GDT.
} __attribute__((packed));

void gdt_init();
void gdt_set_gate(uint32_t index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

#endif  // GDT_H
