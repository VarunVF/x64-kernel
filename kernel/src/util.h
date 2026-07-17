#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

uint64_t phys_to_virt(uint64_t physical);
uint64_t virt_to_phys(uint64_t virt);

int cpuid(uint32_t leaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx);

#endif  // UTIL_H
