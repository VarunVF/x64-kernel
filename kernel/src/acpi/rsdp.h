#ifndef RSDP_H
#define RSDP_H

#include <stdbool.h>
#include <stdint.h>

#include "acpi.h"

// Root System Description Pointer, ACPI version 1.0
struct rsdp {
    char signature[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__ ((packed));

// Root System Description Pointer, ACPI version 2.0+
struct xsdp {
    char signature[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t revision;
    uint32_t rsdt_address;      // Deprecated since version 2.0

    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__ ((packed));

struct rsdt {
    struct acpi_sdt_header header;
    // Number of elements = (header.length - sizeof(header)) / 4
    uint32_t pointer_to_other_sdt[];
} __attribute__ ((packed));

struct xsdt {
    struct acpi_sdt_header header;
    // Number of elements = (header.length - sizeof(header)) / 8
    uint64_t pointer_to_other_sdt[];
} __attribute__ ((packed));


struct rsdp_parse_result {
    bool parse_successful;
    void* lapic_base;
};

struct rsdp_parse_result rsdp_parse(const struct rsdp* rsdp_ptr);
struct rsdp_parse_result rsdt_parse(const struct rsdt* rsdt_ptr, bool is_extended);
void* rsdt_find_table(const struct rsdt* rsdt_ptr, const char* signature, bool is_extended);

#endif  // RSDP_H
