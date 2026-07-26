#include "util.h"

#include <stdint.h>

extern uint64_t hhdm_offset;

inline virt_addr phys_to_virt(phys_addr phys) {
    return phys + hhdm_offset;
}

inline phys_addr virt_to_phys(virt_addr virt) {
    return virt - hhdm_offset;
}
