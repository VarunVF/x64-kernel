#include "drivers/pit.h"

#include <stddef.h>
#include <stdint.h>

#include "cpu.h"
#include "arch/x86_64/interrupt_handler.h"

static uint64_t timer_ticks = 0;

void pit_init(uint8_t vector, uint32_t frequency_hz) {
    uint32_t divisor = 1193182 / frequency_hz;

    cpu_outb(0x43, 0x36);
    cpu_outb(0x40, (uint8_t)(divisor & 0xFF));
    cpu_outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));

    interrupt_set_handler(vector, pit_irq, NULL);
}

uint64_t pit_get_ticks() {
    return timer_ticks;
}

void pit_irq(void* context) {
    (void)context;
    timer_ticks++;
}
