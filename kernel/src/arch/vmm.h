#ifndef VMM_H
#define VMM_H

#include <stdint.h>

#define VMM_PAGE_SIZE PMM_FRAME_SIZE

// Virtual memory management

// Initialise the virtual memory manager. Returns zero on success
int vmm_init();

// Switch to a new stack, apply the new paging tables, then enter the given function
void vmm_switch(void (* vmm_switch_function)(void));

// Map a page of physical memory to virtual memory
void vmm_map_page(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags);

#endif  // VMM_H
