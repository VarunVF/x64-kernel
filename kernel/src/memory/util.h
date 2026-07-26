#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

#include "../types.h"

virt_addr phys_to_virt(phys_addr phys);
phys_addr virt_to_phys(virt_addr virt);

#endif  // UTIL_H
