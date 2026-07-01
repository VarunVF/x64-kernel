#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>
#include <stdbool.h>

// Advanced Configuration and Power Interface (ACPI)

// ACPI System Description Table header
struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemid[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__ ((packed));

bool acpi_checksum(const void* start, uint64_t size);

#endif  // ACPI_H
