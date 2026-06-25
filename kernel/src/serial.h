#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

// Initialise a serial port. Returns zero on success
int init_serial(uint16_t port);

// Write data to a serial port
void write_serial(uint16_t port, uint8_t value);

#endif  // SERIAL_H
