#ifndef GDT_H
#define GDT_H

#include <stdint.h>

#define GDT_KERNEL_CS 0x08
#define GDT_KERNEL_SS 0x10
#define GDT_USER_CS 0x1B
#define GDT_USER_SS 0x23

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
