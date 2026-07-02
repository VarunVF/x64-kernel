#include "rsdp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "madt.h"
#include "../kprint.h"
#include "../memory.h"
#include "../serial.h"
#include "../util.h"

const struct rsdp_parse_result rsdp_failed_result = {
    false,
    NULL
};

static bool rsdp_validate_checksum(const struct rsdp* rsdp_ptr, bool is_extended) {
    // For any ACPI version, this checksum should be valid
    if (!acpi_checksum(rsdp_ptr, sizeof(*rsdp_ptr))) {
        return false;
    }

    if (is_extended) {
        // ACPI v2+: We also need to check the extension fields
        const uint8_t* extended_start = (uint8_t*)rsdp_ptr + sizeof(*rsdp_ptr);
        const size_t extended_size = sizeof(struct xsdp) - sizeof(*rsdp_ptr);
        if (!acpi_checksum(extended_start, extended_size)) {
            return false;
        }
    }

    return true;
}

struct rsdp_parse_result rsdp_parse(const struct rsdp* rsdp_ptr) {
    if (memcmp(rsdp_ptr, "RSD PTR ", 8) != 0) {
        kprintf("RSDP signature not found\n");
        return rsdp_failed_result;
    }
    kprintf("RSDP signature found\n");

    bool is_extended = false;
    if (rsdp_ptr->revision == 0) {
        kprintf("ACPI v1.0\n");
        is_extended = false;
    } else if (rsdp_ptr->revision == 2) {
        kprintf("ACPI v2.0+\n");
        is_extended = true;
    } else {
        kprintf("RSDP revision is invalid\n");
        return rsdp_failed_result;
    }

    if (!rsdp_validate_checksum(rsdp_ptr, is_extended)) {
        kprintf("RSDP checksum is invalid\n");
        return rsdp_failed_result;
    }
    kprintf("RSDP checksum validated\n");

    if (is_extended) {
        struct xsdp* xsdp_ptr = (struct xsdp*)rsdp_ptr;
        uint64_t xsdp_virtual_addr = phys_to_virt(xsdp_ptr->xsdt_address);
        return rsdt_parse((struct rsdt*)xsdp_virtual_addr, is_extended);
    } else {
        uint64_t rsdp_virtual_addr = phys_to_virt(rsdp_ptr->rsdt_address);
        return rsdt_parse((struct rsdt*)rsdp_virtual_addr, is_extended);
    }
}

struct rsdp_parse_result rsdt_parse(const struct rsdt* rsdt_ptr, bool is_extended) {
    kprintf("Parsing RSDT/XSDT\n");

    if (!acpi_checksum(rsdt_ptr, rsdt_ptr->header.length)) {
        kprintf("RSDT/XSDT checksum is invalid\n");
        return rsdp_failed_result;
    }
    kprintf("RSDT/XSDT checksum validated\n");

    struct madt* madt_ptr = rsdt_find_table(rsdt_ptr, "APIC", is_extended);
    if (madt_ptr == NULL) {
        kprintf("MADT not found\n");
        return rsdp_failed_result;
    }
    kprintf("MADT found at %p\n", (void*)madt_ptr);
    struct madt_parse_result madt_info = madt_parse(madt_ptr);
    return (struct rsdp_parse_result){
        madt_info.parse_successful,
        madt_info.lapic_base
    };
}

void* rsdt_find_table(const struct rsdt* rsdt_ptr, const char* signature, bool is_extended) {
    struct rsdt* rsdt = (struct rsdt*)rsdt_ptr;
    int entries = 0;
    if (is_extended) {
        entries = (rsdt->header.length - sizeof(rsdt->header)) / 8;
    } else {
        entries = (rsdt->header.length - sizeof(rsdt->header)) / 4;
    }

    for (int i = 0; i < entries; i++) {
        struct acpi_sdt_header* header = (struct acpi_sdt_header*)phys_to_virt(rsdt->pointer_to_other_sdt[i]);
        if (memcmp(header->signature, signature, 4) == 0) {
            return header;
        }
    }

    // Table not found
    return NULL;
}
