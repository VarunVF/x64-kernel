#ifndef PIT_H
#define PIT_H

#include <stdint.h>

#include "arch/x86_64/idt.h"

void pit_init(uint8_t vector, uint32_t frequency_hz);
uint64_t pit_get_ticks();
struct cpu_status_t* pit_irq(struct cpu_status_t* regs, void* context);

#endif  // PIT_H
