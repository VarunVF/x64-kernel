#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

#include "../types.h"

virt_addr phys_to_virt(phys_addr phys);
phys_addr virt_to_phys(virt_addr virt);

// Alignment must be a power of two.
uint64_t align_up(uint64_t value, int alignment);

uint32_t mmio_read32(void* base, uintptr_t offset);
void mmio_write32(void* base, uintptr_t offset, uint32_t value);

#endif  // UTIL_H
