#ifndef IDT_H
#define IDT_H

#include <stdint.h>

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t always_0;
    uint8_t flags;
    uint16_t base_middle;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_pointer {
    uint16_t limit;
    uintptr_t base;
} __attribute__((packed));

struct interrupt_registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t interrupt_number, error_code;
    // Pushed onto stack by CPU:
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

void idt_init();
void idt_set_gate(uint8_t index, uint64_t base, uint16_t selector, uint8_t flags);

#endif  // IDT_H
