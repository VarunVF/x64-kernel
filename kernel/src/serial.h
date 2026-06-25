#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

#define SERIAL_COM1 0x3F8

// TODO: combine this and video.h to a common interface

// Initialise a serial port. Returns zero on success
int serial_init(uint16_t port);

// Write data to a serial port
void serial_write_char(uint16_t port, uint8_t value);
void serial_write_str(uint16_t port, const char* message);
void serial_write_bin(uint16_t port, uint64_t number);
void serial_write_hex(uint16_t port, uint64_t number);
void serial_write_dec(uint16_t port, uint64_t number);

#endif  // SERIAL_H
