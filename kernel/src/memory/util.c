#include "util.h"

#include <stdint.h>

extern uint64_t hhdm_offset;

inline uint64_t phys_to_virt(uint64_t phys) {
    return phys + hhdm_offset;
}

inline uint64_t virt_to_phys(uint64_t virt) {
    return virt - hhdm_offset;
}
