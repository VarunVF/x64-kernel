#ifndef APIC_H
#define APIC_H

#include <stdbool.h>
#include <stdint.h>

#include "../../../acpi/madt.h"

// Check if CPU supports APIC and Local APIC has not been disabled in MSRs
bool lapic_is_usable();

// Initialise the LAPIC. Returns zero on success
int lapic_init(void* lapic_regs);

// Map an ISA IRQ to the IOAPIC, applying any ACPI MADT overrides, and configure its vector and target LAPIC
void ioapic_set_irq(void* ioapic_regs, struct madt_parse_result* madt_info, uint8_t isa_irq, uint8_t vector, uint8_t dest_lapic_id);

#endif  // APIC_H
