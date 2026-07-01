#include "madt.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../serial.h"
#include "../util.h"

const struct madt_parse_result madt_failed_result = {
    false,
    NULL
};


struct madt_parse_result madt_parse(const struct madt* madt_ptr) {
    serial_write_str(SERIAL_COM1, "Parsing MADT\n");

    if (!acpi_checksum(madt_ptr, madt_ptr->header.length)) {
        serial_write_str(SERIAL_COM1, "MADT checksum is invalid\n");
        return madt_failed_result;
    }
    serial_write_str(SERIAL_COM1, "MADT checksum validated\n");

    uint64_t local_apic_addr = phys_to_virt(madt_ptr->local_apic_address);
    serial_write_str(SERIAL_COM1, "Local APIC address: ");
    serial_write_hex(SERIAL_COM1, local_apic_addr);
    serial_write_str(SERIAL_COM1, "\n");
    
    uint8_t* start = (uint8_t*)madt_ptr->records;
    uint8_t* end = start + madt_ptr->header.length;
    uint8_t* current = start;
    while (current < end) {
        struct madt_record_header* record = (struct madt_record_header*)current;
        
        if (record->record_length == 0) {
            break;
        }

        // Different record types
        struct madt_lapic_record* lapic = (struct madt_lapic_record*)current;
        struct madt_ioapic_record* ioapic = (struct madt_ioapic_record*)current;
        struct madt_iso_record* iso = (struct madt_iso_record*)current;
        switch (record->entry_type) {
            case MADT_LAPIC_RECORD:
                if (lapic->flags & 1) {
                    serial_write_str(SERIAL_COM1, "Found enabled CPU core: ");
                } else if (lapic->flags & 2) {
                    serial_write_str(SERIAL_COM1, "Found disabled CPU core: ");
                } else {
                    serial_write_str(SERIAL_COM1, "Found online incapable CPU core: ");
                }
                serial_write_str(SERIAL_COM1, "Processor ID ");
                serial_write_dec(SERIAL_COM1, lapic->acpi_processor_id);
                serial_write_str(SERIAL_COM1, ", APIC ID ");
                serial_write_dec(SERIAL_COM1, lapic->apic_id);
                serial_write_str(SERIAL_COM1, "\n");
                break;
            case MADT_IOAPIC_RECORD:
                serial_write_str(SERIAL_COM1, "Found I/O APIC: ");
                serial_write_str(SERIAL_COM1, "ID ");
                serial_write_dec(SERIAL_COM1, ioapic->ioapic_id);
                serial_write_str(SERIAL_COM1, ", Physical address ");
                serial_write_hex(SERIAL_COM1, ioapic->ioapic_address);
                serial_write_str(SERIAL_COM1, "\n");
                break;
            case MADT_ISO_RECORD:
                serial_write_str(SERIAL_COM1, "Found IRQ Override: ");
                serial_write_str(SERIAL_COM1, "ISA IRQ ");
                serial_write_dec(SERIAL_COM1, iso->irq_source);
                serial_write_str(SERIAL_COM1, " maps to GSI ");
                serial_write_dec(SERIAL_COM1, iso->global_system_interrupt);
                serial_write_str(SERIAL_COM1, "\n");
                break;
            default:
                // Other record types
                break;
        }

        // Skip to the next record
        current += record->record_length;
    }

    return (struct madt_parse_result){
        true,
        (void*)local_apic_addr
    };
}
