#ifndef CPUID_H
#define CPUID_H

#include <stdint.h>

int cpuid(uint32_t leaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx);

#endif  // CPUID_H
