#include "arch/x86_64/cpuid.h"

#include <stdint.h>

static inline void real_cpuid(uint32_t leaf, uint32_t sub_leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile (
        "cpuid"
        : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
        : "a" (leaf), "c" (sub_leaf)
    );
}

static inline uint32_t cpuid_max(uint32_t leaf) {
    uint32_t eax, ebx, ecx, edx;
    real_cpuid(leaf, 0, &eax, &ebx, &ecx, &edx);
    return eax;
}

int cpuid(uint32_t leaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    uint32_t max_leaf = cpuid_max(leaf & 0x80000000);

    if (max_leaf == 0 || max_leaf < leaf) {
        return 0;
    }

    real_cpuid(leaf, 0, eax, ebx, ecx, edx);
    return 1;
}
