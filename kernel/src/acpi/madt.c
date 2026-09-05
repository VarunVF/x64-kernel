#include "madt.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../kprint.h"

const struct madt_parse_result madt_failed_result = {
    false,
    0,
    0,
    {{0}}
};

struct madt_parse_result madt_parse(const struct madt* madt_ptr) {
    kprintf("Parsing MADT\n");

    struct madt_parse_result result = madt_failed_result;

    if (!acpi_checksum(madt_ptr, madt_ptr->header.length)) {
        kprintf("MADT checksum is invalid\n");
        return result;
    }
    kprintf("MADT checksum validated\n");

    result.lapic_base = madt_ptr->local_apic_address;

    uint8_t* start = (uint8_t*)madt_ptr->records;
    uint8_t* end = start + madt_ptr->header.length;
    uint8_t* current = start;
    while (current < end) {
        struct madt_record_header* record = (struct madt_record_header*)current;
        
        if (record->record_length == 0) {
            break;
        }

        switch (record->entry_type) {
            case MADT_LAPIC_RECORD: {
                struct madt_lapic_record* lapic = (struct madt_lapic_record*)current;
                if (lapic->flags & 1) {
                    kprintf("Found enabled CPU core: ");
                } else if (lapic->flags & 2) {
                    kprintf("Found disabled CPU core: ");
                } else {
                    kprintf("Found online incapable CPU core: ");
                }
                kprintf("Processor ID %d, APIC ID %d\n", lapic->acpi_processor_id, lapic->apic_id);
                break;
            }
            case MADT_IOAPIC_RECORD: {
                struct madt_ioapic_record* ioapic = (struct madt_ioapic_record*)current;
                result.ioapic_base = ioapic->ioapic_address;
                break;
            }
            case MADT_ISO_RECORD: {
                struct madt_iso_record* iso = (struct madt_iso_record*)current;
                result.overrides[iso->irq_source].global_system_interrupt = iso->global_system_interrupt;
                result.overrides[iso->irq_source].flags = iso->flags;
                result.overrides[iso->irq_source].is_present = true;
                break;
            }
            default: {
                // Other record types
                break;
            }
        }

        // Skip to the next record
        current += record->record_length;
    }

    result.parse_successful = true;
    return result;
}
