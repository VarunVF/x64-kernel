#include "madt.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../kprint.h"
#include "../util.h"

const struct madt_parse_result madt_failed_result = {
    false,
    NULL
};

struct madt_parse_result madt_parse(const struct madt* madt_ptr) {
    kprintf("Parsing MADT\n");

    if (!acpi_checksum(madt_ptr, madt_ptr->header.length)) {
        kprintf("MADT checksum is invalid\n");
        return madt_failed_result;
    }
    kprintf("MADT checksum validated\n");

    uint64_t local_apic_addr = phys_to_virt(madt_ptr->local_apic_address);
    kprintf("Local APIC address: %p\n", (void*)local_apic_addr);
    
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
                kprintf("Processor ID %d, APIC ID %d\n",
                    lapic->acpi_processor_id, lapic->apic_id);
                    break;
                }
            case MADT_IOAPIC_RECORD: {
                struct madt_ioapic_record* ioapic = (struct madt_ioapic_record*)current;
                kprintf("Found I/O APIC: ID %d, Physical address %p\n",
                    ioapic->ioapic_id, (void*)(uint64_t)ioapic->ioapic_address);
                    break;
                }
            case MADT_ISO_RECORD: {
                struct madt_iso_record* iso = (struct madt_iso_record*)current;
                kprintf("Found IRQ Override: ISA IRQ %d maps to GSI %d\n",
                    iso->irq_source, iso->global_system_interrupt);
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

    return (struct madt_parse_result){
        true,
        (void*)local_apic_addr
    };
}
