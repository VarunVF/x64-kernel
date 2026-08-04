#ifndef VMM_H
#define VMM_H

#include <stdint.h>

#include "../types.h"

#define VMM_PAGE_SIZE PMM_FRAME_SIZE

// Virtual memory management

typedef void (*vmm_switch_fn)(void);

enum vm_flags {
    VM_FLAG_NONE = 0,
    VM_FLAG_WRITE = 1ull << 0,
    VM_FLAG_EXEC = 1ull << 1,
    VM_FLAG_USER = 1ull << 2,
};

// Initialise the virtual memory manager. Returns zero on success
int vmm_init();

// Switch to a new stack, apply the new paging tables, then enter the given function
void vmm_switch(vmm_switch_fn switch_function);

// Map a page of physical memory to virtual memory
void vmm_map_page(void* root, virt_addr virt, phys_addr phys, uint64_t vm_flags);

// Unmap a page from virtual memory
void vmm_unmap_page(void* root, virt_addr virt);

#endif  // VMM_H
