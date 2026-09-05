#include "pit.h"

#include "../../cpu.h"

void pit_init(uint32_t frequency_hz) {
    uint32_t divisor = 1193182 / frequency_hz;

    cpu_outb(0x43, 0x36);
    cpu_outb(0x40, (uint8_t)(divisor & 0xFF));
    cpu_outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}
