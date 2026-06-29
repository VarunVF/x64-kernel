#ifndef IO_H
#define IO_H

#include <stdint.h>

// Input a byte from a serial port
uint8_t inb(uint16_t port);

// Output a byte to a serial port
void outb(uint16_t port, uint8_t value);

// Wait a very small amount of time
void io_wait(void);

#endif  // IO_H
