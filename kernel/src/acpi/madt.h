#ifndef MADT_H
#define MADT_H

#include <stdint.h>

#include "acpi.h"

#define MADT_LAPIC_RECORD 0
#define MADT_IOAPIC_RECORD 1
#define MADT_ISO_RECORD 2

// Common header for all MADT records
struct madt_record_header {
    uint8_t entry_type;
    uint8_t record_length;
} __attribute__ ((packed));

// Entry type 0
struct madt_lapic_record {
    struct madt_record_header header;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__ ((packed));

// Entry type 1
struct madt_ioapic_record {
    struct madt_record_header header;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_address;
    uint32_t global_system_interrupt_base;
} __attribute__ ((packed));

// Entry type 2
struct madt_iso_record {
    struct madt_record_header header;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t global_system_interrupt;
    uint16_t flags;
} __attribute__ ((packed));

// Multiple APIC Description Table
struct madt {
    struct acpi_sdt_header header;
    uint32_t local_apic_address;
    uint32_t flags;
    // Variable length interrupt control structures
    uint8_t records[];
} __attribute__ ((packed));

struct madt_parse_result {
    bool parse_successful;
    uint32_t lapic_base;
    uint32_t ioapic_base;
};

struct madt_parse_result madt_parse(const struct madt* madt_ptr);

#endif  // MADT_H
