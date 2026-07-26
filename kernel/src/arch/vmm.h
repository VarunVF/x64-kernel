#ifndef VMM_H
#define VMM_H

#include <stdint.h>

#include "../types.h"

#define VMM_PAGE_SIZE PMM_FRAME_SIZE

// Virtual memory management

typedef void (*vmm_switch_fn)(void);

// Initialise the virtual memory manager. Returns zero on success
int vmm_init();

// Switch to a new stack, apply the new paging tables, then enter the given function
void vmm_switch(vmm_switch_fn switch_function);

// Map a page of physical memory to virtual memory
void vmm_map_page(void* root, virt_addr virt, phys_addr phys, uint64_t flags);

#endif  // VMM_H
