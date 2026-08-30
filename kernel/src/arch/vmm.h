#ifndef VMM_H
#define VMM_H

#include <stddef.h>
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
    VM_FLAG_MMIO = 1ull << 3,
    VM_FLAG_UC = 1ull << 4,
};

// Initialise the virtual memory manager. Returns zero on success
int vmm_init();

// Add several important memory regions.
// Should be called before any calls to vmm_alloc and only after the heap is initialised.
void vmm_add_initial_regions();

void* vmm_kernel_table_root();

// Switch to a new stack, apply the new paging tables, then enter the given function
void vmm_switch(vmm_switch_fn switch_function);

// Map a page of physical memory to virtual memory
void vmm_map_page(void* root, virt_addr virt, phys_addr phys, uint64_t vm_flags);

// Map multiple pages of physical memory to virtual memory
void vmm_map_pages(void* root, size_t pages, virt_addr virt, phys_addr phys, uint64_t vm_flags);

// Unmap a page from virtual memory
void vmm_unmap_page(void* root, virt_addr virt);

// Unmap multiple pages from virtual memory
void vmm_unmap_pages(void* root, size_t pages, virt_addr virt);

// Walk the page tables to find the mapped physical address
phys_addr vmm_find_phys(void* root, virt_addr virt);

// Find and create a region of virtual memory
void* vmm_alloc(size_t length, uint64_t vm_flags, void* arg);

// Free a region of virtual memory
void vmm_free(void* addr);

void vmm_debug_print();

#endif  // VMM_H
