#include "util.h"

#include <stdbool.h>
#include <stdint.h>

#include "../macros.h"

extern uint64_t hhdm_offset;

inline virt_addr phys_to_virt(phys_addr phys) {
    return phys + hhdm_offset;
}

inline phys_addr virt_to_phys(virt_addr virt) {
    return virt - hhdm_offset;
}

static inline bool is_power_of_two(int number) {
    return (number > 0) && (number & (number - 1)) == 0;
}

uint64_t align_up(uint64_t value, int alignment) {
    KASSERT(is_power_of_two(alignment));
    return (value + alignment - 1) & ~(alignment - 1);
}
