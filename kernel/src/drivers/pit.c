#include "drivers/pit.h"

#include <stdint.h>

#include "cpu.h"

static uint64_t timer_ticks = 0;

void pit_init(uint32_t frequency_hz) {
    uint32_t divisor = 1193182 / frequency_hz;

    cpu_outb(0x43, 0x36);
    cpu_outb(0x40, (uint8_t)(divisor & 0xFF));
    cpu_outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint64_t pit_get_ticks() {
    return timer_ticks;
}

void pit_irq() {
    timer_ticks++;
}
