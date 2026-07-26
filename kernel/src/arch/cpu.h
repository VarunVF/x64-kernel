#ifndef CPU_H
#define CPU_H

#include <stdint.h>

#include "../types.h"

// Initialise architecture-specific structures
void cpu_init(void);

// Setup interrupt handlers and enable interrupts
void cpu_setup_interrupts(void);

// Execute a halt instruction
void cpu_halt(void);

// Execute a halt instruction in an infinite loop
void cpu_halt_loop(void);

// Disable interrupts
void cpu_disable_interrupts(void);

// Flush an entry in the TLB
void cpu_invalidate_tlb_page(virt_addr virt);

// Input a byte from a serial port
uint8_t cpu_inb(uint16_t port);

// Output a byte to a serial port
void cpu_outb(uint16_t port, uint8_t value);

// Wait a very small amount of time
void cpu_io_wait(void);

#endif  // CPU_H
