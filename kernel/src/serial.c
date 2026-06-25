#include "serial.h"

// Input a byte from a serial port
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %[src_port], %[dest_reg]" 
        : [dest_reg] "=a"(ret) 
        : [src_port] "Nd"(port) 
    );
    return ret;
}

// Output a byte to a serial port
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %[data], %[dest_port]" 
        : // No outputs
        : [data] "a"(value), [dest_port] "Nd"(port) 
    );
}


int serial_init(uint16_t port) {
    outb(port + 1, 0x00);  // Disable all interrupts
    outb(port + 3, 0x80);  // Enable DLAB (set baud rate divisor)
    outb(port + 0, 0x03);  // Set divisor to 3 (lo byte) 38400 baud
    outb(port + 1, 0x00);  //                  (hi byte)
    outb(port + 3, 0x03);  // 8 bits, no parity, one stop bit
    outb(port + 2, 0xC7);  // Enable FIFO, clear them, with 14-byte threshold
    outb(port + 4, 0x0B);  // IRQs enabled, RTS/DSR set
    outb(port + 4, 0x1E);  // Set in loopback mode, test the serial chip
    outb(port + 0, 0xAE);  // Test serial chip (send byte 0xAE and check if serial returns same byte)

    // Check if serial is faulty (i.e: not same byte as sent)
    if (inb(port + 0) != 0xAE) {
        return 1;
    }

    // If serial is not faulty set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    outb(port + 4, 0x0F);
    return 0;
}

static int is_transmit_empty(uint16_t port) {
    return inb(port + 5) & 0x20;
}

void serial_write_char(uint16_t port, uint8_t value) {
    while (is_transmit_empty(port) == 0) {
        // Wait until it's safe to output
    }

    outb(port, value);
}

void serial_write_str(uint16_t port, const char* message) {
    for (int i = 0; message[i] != '\0'; i++) {
        serial_write_char(port, message[i]);
    }
}

void serial_write_bin(uint16_t port, uint64_t number) {
    const int total_bits = sizeof(number) * 8;
    for (int i = total_bits - 1; i >= 0; i--) {
        int bit = (number >> i) & 1;
        if (bit) {
            serial_write_char(port, '1');
        } else {
            serial_write_char(port, '0');
        }
    }
}

void serial_write_hex(uint16_t port, uint64_t number) {
    const int total_bytes = sizeof(number);
    const int hex_digits = total_bytes * 2;
    const char* hex_table = "0123456789abcdef";
    for (int i = hex_digits - 1; i >= 0; i--) {
        int digit = (number >> (i * 4)) & 0xf;
        serial_write_char(port, hex_table[digit]);
    }
}

void serial_write_dec(uint16_t port, uint64_t number) {
    // UINT64_MAX (18446744073709551615UL) has 20 decimal digits,
    // so we need space for at most 20 characters.
    char digits[20] = {0};
    int current_digit = 0;

    // Append the digits from right to left.
    while (number > 0) {
        int digit = number % 10;
        digits[current_digit++] = '0' + digit;
        number /= 10;
    }

    // Print the digits in reverse order.
    while (current_digit--) {
        serial_write_char(port, digits[current_digit]);
    }
}
