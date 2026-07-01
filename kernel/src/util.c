#include "util.h"

#include <stdint.h>

extern uint64_t hhdm_offset;

inline uint64_t phys_to_virt(uint64_t physical) {
    return physical + hhdm_offset;
}
