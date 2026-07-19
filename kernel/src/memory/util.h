#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

uint64_t phys_to_virt(uint64_t phys);
uint64_t virt_to_phys(uint64_t virt);

#endif  // UTIL_H
