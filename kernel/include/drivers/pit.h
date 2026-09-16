#ifndef PIT_H
#define PIT_H

#include <stdint.h>

void pit_init(uint8_t vector, uint32_t frequency_hz);
uint64_t pit_get_ticks();
void pit_irq(void* context);

#endif  // PIT_H
