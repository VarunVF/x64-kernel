#ifndef PMM_H
#define PMM_H

#include <stdbool.h>
#include <stdint.h>

#define PMM_FRAME_SIZE 4096

// Physical memory management

// Initialise the physical memory manager. Returns zero on success
int pmm_init(void);

// Verify against the memory map that the bitmap was initialised correctly
bool pmm_verify_bitmap(void);

// Allocate frames of memory, returns a physical address
uint64_t pmm_alloc(uint64_t size);

// Free frames of memory starting at the given physical address
void pmm_free(uint64_t address, uint64_t size);

// Debug: print bitmap state
void pmm_debug_print(uint64_t frame, uint64_t count);

#endif  // PMM_H
