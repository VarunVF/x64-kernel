#ifndef CPU_H
#define CPU_H

#include <stdint.h>

#include "types.h"

#ifdef __x86_64__
#define CPU_READ_INSTRUCTION_PTR() ({                                   \
        uint64_t _inst_ptr;                                             \
        __asm__ volatile ("leaq 0(%%rip), %0" : "=r"(_inst_ptr));       \
        (void*)_inst_ptr;                                               \
    })
#else
#error "CPU_READ_INSTRUCTION_PTR is not implemented for this platform"
#endif  // __x86_64__

// Initialise architecture-specific structures
void cpu_init(void);

// Setup interrupt handlers and enable interrupts
void cpu_setup_interrupts(void);

// Enable interrupts
void cpu_enable_interrupts(void);

// Disable interrupts
void cpu_disable_interrupts(void);

// Execute a halt instruction
void cpu_halt(void);

// Execute a halt instruction in an infinite loop
void cpu_halt_loop(void);

// Flush an entry in the TLB
void cpu_invalidate_tlb_page(virt_addr virt);

// Input a byte from a serial port
uint8_t cpu_inb(uint16_t port);

// Output a byte to a serial port
void cpu_outb(uint16_t port, uint8_t value);

// Wait a very small amount of time
void cpu_io_wait(void);

// Get the stack frame pointer
uint64_t* cpu_read_frame_ptr(void);

// Get the return address for a specific stack frame
uint64_t* cpu_read_return_addr(uint64_t* frame_ptr);

#endif  // CPU_H
