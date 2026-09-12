#include "acpi/acpi.h"

#include <stdbool.h>
#include <stdint.h>

bool acpi_checksum(const void* start, uint64_t size) {
    // The sum of all bytes must equal 0 (mod 256).
    uint8_t sum = 0;
    const uint8_t* table = start;
    for (uint64_t i = 0; i < size; i++) {
        sum += table[i];
    }

    return sum == 0;
}
