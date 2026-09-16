#include "arch/x86_64/apic.h"

#include <stdbool.h>
#include <stdint.h>

#include "acpi/madt.h"
#include "arch/x86_64/cpuid.h"
#include "memory/util.h"

#define LAPIC_SVR 0x0F0
#define LAPIC_SVR_APIC_ENABLE (1 << 8)

#define IOREGSEL 0x00   // I/O Register select register
#define IOWIN 0x10      // I/O Window register

// Redirection Table entry flags (see Intel 82093AA datasheet)
#define IOAPIC_DELIVERY_FIXED           (0 << 8)
#define IOAPIC_DEST_PHYSICAL            (0 << 11)
#define IOAPIC_POLARITY_ACTIVE_HIGH     (0 << 13)
#define IOAPIC_POLARITY_ACTIVE_LOW      (1 << 13)
#define IOAPIC_TRIGGER_EDGE             (0 << 15)
#define IOAPIC_TRIGGER_LEVEL            (1 << 15)

const uint32_t CPUID_FLAG_MSR = 1 << 5;
const uint32_t CPUID_FEAT_EDX_APIC = 1 << 9;

// Check if CPU has model-specific registers
static bool cpu_has_msr() {
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    return edx & CPUID_FLAG_MSR;
}

bool lapic_is_usable() {
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    return edx & CPUID_FEAT_EDX_APIC;
}

static inline void cpu_read_msr(uint32_t msr, uint32_t* lo, uint32_t* hi) {
    __asm__ volatile ("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

static inline void cpu_write_msr(uint32_t msr, uint32_t lo, uint32_t hi) {
    __asm__ volatile ("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

#define IA32_APIC_BASE 0x1B
#define IA32_APIC_BASE_ENABLE (1 << 11)

void apic_enable() {
    uint32_t lo, hi;
    cpu_read_msr(IA32_APIC_BASE, &lo, &hi);

    if (!(lo & IA32_APIC_BASE_ENABLE)) {
        lo |= IA32_APIC_BASE_ENABLE;
        cpu_write_msr(IA32_APIC_BASE, lo, hi);
    }
}

int lapic_init(void* lapic_regs) {
    if (!cpu_has_msr())
        return 1;
    
    apic_enable();
    if (!lapic_is_usable())
        return 2;

    // Set the Spurious Interrupt Vector Register to start receiving interrupts
    uint32_t svr = mmio_read32(lapic_regs, LAPIC_SVR);
    svr |= LAPIC_SVR_APIC_ENABLE | 0xFF;
    mmio_write32(lapic_regs, LAPIC_SVR, svr);

    return 0;
}

static inline void ioapic_write(void* ioapic_virt_base, uint32_t reg, uint32_t data) {
    mmio_write32(ioapic_virt_base, IOREGSEL, reg);
    mmio_write32(ioapic_virt_base, IOWIN, data);
}

static inline uint32_t ioapic_read(void* ioapic_virt_base, uint32_t reg) {
    mmio_write32(ioapic_virt_base, IOREGSEL, reg);
    return mmio_read32(ioapic_virt_base, IOWIN);
}

void ioapic_set_irq(void* ioapic_regs, struct madt_parse_result* madt_info, uint8_t isa_irq, uint8_t vector, uint8_t dest_lapic_id) {
    uint8_t pin = isa_irq;
    uint16_t iso_flags = 0;

    struct irq_override override = madt_info->overrides[isa_irq];
    if (override.is_present) {
        pin = override.global_system_interrupt;
        iso_flags = override.flags;
    }

    uint32_t reg_low  = 0x10 + (pin * 2);
    uint32_t reg_high = reg_low + 1;

    // From ACPI specification v6.6 section 5.2.12.5:
    // ISO flags bits 1:0  : Polarity
    // ISO flags bits 3:2  : Trigger Mode
    // ISO flags bits 15:4 : Reserved
    uint32_t low_flags = vector | IOAPIC_DELIVERY_FIXED | IOAPIC_DEST_PHYSICAL;
    if ((iso_flags & 0x03) == 0x03) {
        low_flags |= IOAPIC_POLARITY_ACTIVE_LOW;
    } else {
        low_flags |= IOAPIC_POLARITY_ACTIVE_HIGH;
    }
    if ((iso_flags & 0x0C) == 0x0C) {
        low_flags |= IOAPIC_TRIGGER_LEVEL;
    } else {
        low_flags |= IOAPIC_TRIGGER_EDGE;
    }
    
    uint32_t high_flags = ((uint32_t)dest_lapic_id) << 24;

    ioapic_write(ioapic_regs, reg_high, high_flags);
    ioapic_write(ioapic_regs, reg_low, low_flags);
}
