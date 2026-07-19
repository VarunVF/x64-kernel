#include "serial.h"

#include "arch/cpu.h"

int serial_init(uint16_t port) {
    cpu_outb(port + 1, 0x00);  // Disable all interrupts
    cpu_outb(port + 3, 0x80);  // Enable DLAB (set baud rate divisor)
    cpu_outb(port + 0, 0x03);  // Set divisor to 3 (lo byte) 38400 baud
    cpu_outb(port + 1, 0x00);  //                  (hi byte)
    cpu_outb(port + 3, 0x03);  // 8 bits, no parity, one stop bit
    cpu_outb(port + 2, 0xC7);  // Enable FIFO, clear them, with 14-byte threshold
    cpu_outb(port + 4, 0x0B);  // IRQs enabled, RTS/DSR set
    cpu_outb(port + 4, 0x1E);  // Set in loopback mode, test the serial chip
    cpu_outb(port + 0, 0xAE);  // Test serial chip (send byte 0xAE and check if serial returns same byte)

    // Check if serial is faulty (i.e: not same byte as sent)
    if (cpu_inb(port + 0) != 0xAE) {
        return 1;
    }

    // If serial is not faulty set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    cpu_outb(port + 4, 0x0F);
    return 0;
}

static int is_transmit_empty(uint16_t port) {
    return cpu_inb(port + 5) & 0x20;
}

void serial_write_char(uint16_t port, uint8_t value) {
    while (is_transmit_empty(port) == 0) {
        // Wait until it's safe to output
    }

    cpu_outb(port, value);
}
