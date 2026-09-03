#include "../cpu.h"

#include <stdint.h>

#include "gdt/gdt.h"
#include "interrupts/idt.h"
#include "../../kprint.h"

void cpu_init(void) {
    gdt_init();
    kprintf("GDT initialised\n");
}

void cpu_setup_interrupts(void) {
    idt_init();
    kprintf("IDT initialised\n");
}

inline void cpu_halt(void) {
    __asm__ volatile ("hlt");
}

inline void cpu_halt_loop(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

inline void cpu_disable_interrupts(void) {
    __asm__ volatile ("cli");
}

inline void cpu_invalidate_tlb_page(virt_addr virt) {
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

inline uint8_t cpu_inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %[src_port], %[dest_reg]" 
        : [dest_reg] "=a"(ret) 
        : [src_port] "Nd"(port) 
    );
    return ret;
}

inline void cpu_outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %[data], %[dest_port]" 
        : // No outputs
        : [data] "a"(value), [dest_port] "Nd"(port) 
    );
}

inline void cpu_io_wait(void) {
    // We can do an IO operation on any unused port
    cpu_outb(0x80, 0);
}

inline uint64_t* cpu_read_frame_ptr(void) {
    uint64_t* rbp;
    __asm__ volatile ("mov %%rbp, %0" : "=r"(rbp));
    return rbp;
}

inline uint64_t* cpu_read_return_addr(uint64_t* frame_ptr) {
    return (uint64_t*)frame_ptr[1];
}
