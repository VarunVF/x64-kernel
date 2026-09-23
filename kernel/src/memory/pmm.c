#include "memory/pmm.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <limine.h>

#include "kprint.h"
#include "kstring.h"
#include "memory/util.h"
#include "types.h"

// Bitmap physical memory allocator

// Each bit in the bitmap represents one frame (physical page) of memory, either used (1) or free (0).
#define PMM_USED_VALUE 1
#define PMM_FREE_VALUE 0

extern volatile struct limine_memmap_request memmap_request;

static const char* memory_types[9] = {
    "USABLE",
    "RESERVED",
    "ACPI_RECLAIMABLE",
    "ACPI_NVS",
    "BAD_MEMORY",
    "BOOTLOADER_RECLAIMABLE",
    "EXECUTABLE_AND_MODULES",
    "FRAMEBUFFER",
    "RESERVED_MAPPED",
};

static struct pmm_state {
    uint8_t* bitmap;
    uint64_t bitmap_size;
    phys_addr bitmap_phys_base;
    uint64_t bitmap_start_frame;
    uint64_t bitmap_end_frame;
} pmm;

// Mark a frame as used
static void pmm_set_bit(uint64_t frame) {
    pmm.bitmap[frame / 8] |= 0x80 >> (frame % 8);
}

// Mark a frame as free
static void pmm_clear_bit(uint64_t frame) {
    pmm.bitmap[frame / 8] &= ~(0x80 >> (frame % 8));
}

// Test whether a frame is used
static bool pmm_test_bit(uint64_t frame) {
    return (pmm.bitmap[frame / 8] & (0x80 >> (frame % 8))) != 0;
}

static void pmm_set_value(uint64_t frame, uint8_t value) {
    if (value) {
        pmm_set_bit(frame);
    } else {
        pmm_clear_bit(frame);
    }
}

// Helper function to assign many bits at once, in a half-open range of frames
static void pmm_assign(uint64_t first_frame, uint64_t last_frame, uint8_t value) {
    // Invalid range
    if (first_frame >= last_frame) {
        return;
    }

    uint64_t first_byte_idx = first_frame / 8;
    uint64_t end_byte_idx = last_frame / 8;

    // The entire range fits inside a single byte
    if (first_byte_idx == end_byte_idx) {
        for (uint64_t i = first_frame; i < last_frame; i++) {
            pmm_set_value(i, value);
        }
        return;
    }

    // Clear bits in the first byte
    uint64_t aligned_start_frame = (first_byte_idx + 1) * 8;
    for (uint64_t i = first_frame; i < aligned_start_frame; i++) {
        pmm_set_value(i, value);
    }

    // Clear all the bytes in between
    uint64_t aligned_end_frame = end_byte_idx * 8;
    if (end_byte_idx > first_byte_idx + 1) {
        uint8_t fill_value = value ? 0xFF : 0x00;
        memset(pmm.bitmap + first_byte_idx + 1, fill_value, end_byte_idx - first_byte_idx - 1);
    }
    
    // Clear bits in the last byte
    for (uint64_t i = aligned_end_frame; i < last_frame; i++) {
        pmm_set_value(i, value);
    }
}

int pmm_init(void) {
    memset(&pmm, 0, sizeof(pmm));

    if (memmap_request.response == NULL) {
        return 1;
    }
    
    struct limine_memmap_response* memory_map = memmap_request.response;
    kprintf("Reading the memory map\n");

    // Usable entries are always aligned to 4096 bytes,
    // and do not overlap with any other entry.

    // Calculate highest usable RAM address
    phys_addr highest_ram_address = 0;
    uint64_t usable_ram_bytes = 0;
    for (uint64_t i = 0; i < memory_map->entry_count; i++) {
        struct limine_memmap_entry* entry = memory_map->entries[i];
        kprintf("Memory region: base=%p, length=%p, type=%s\n",
            entry->base, entry->length, memory_types[entry->type]);        
            
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            usable_ram_bytes += entry->length;
            
            // To save space, only include up to the last usable memory entry in the bitmap
            uint64_t entry_end = entry->base + entry->length;
            if (highest_ram_address < entry_end) {
                highest_ram_address = entry_end;
            }
        }
    }
    
    pmm.bitmap_size = highest_ram_address / PMM_FRAME_SIZE / 8;

    kprintf("Highest RAM address found: %p\n", highest_ram_address);
    kprintf("Found %dMiB of usable RAM\n", usable_ram_bytes / 1024 / 1024);
    kprintf("Bitmap allocator requires %dKiB of RAM\n", pmm.bitmap_size / 1024);

    // Find space to store the bitmap
    bool found_space = false;
    for (uint64_t i = 0; i < memory_map->entry_count; i++) {
        struct limine_memmap_entry* entry = memory_map->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= pmm.bitmap_size) {
            pmm.bitmap = (uint8_t*)phys_to_virt(entry->base);
            pmm.bitmap_phys_base = entry->base;
            found_space = true;
            kprintf("Found space for bitmap at %p\n", (void*)pmm.bitmap);
            break;
        }
    }
    if (!found_space) {
        kprintf("Could not find space for bitmap\n");
        return 2;
    }

    // Mark all frames as used initially
    memset(pmm.bitmap, 0xFF, pmm.bitmap_size);

    // Clear bits for usable memory regions
    for (uint64_t i = 0; i < memory_map->entry_count; i++) {
        struct limine_memmap_entry* entry = memory_map->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t start_frame = entry->base / PMM_FRAME_SIZE;
            uint64_t end_frame = start_frame + entry->length / PMM_FRAME_SIZE;
            pmm_assign(start_frame, end_frame, PMM_FREE_VALUE);
        }
    }

    // The frame containing address 0 should be reserved, since 0 indicates allocation failure
    pmm_set_bit(0);
    kprintf("PMM reserved frame 0\n");

    // Mark the frames containing the bitmap itself as used
    // Integer division rounding up
    uint64_t bitmap_frame_count = (pmm.bitmap_size + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    pmm.bitmap_start_frame = pmm.bitmap_phys_base / PMM_FRAME_SIZE;
    pmm.bitmap_end_frame = pmm.bitmap_start_frame + bitmap_frame_count;
    pmm_assign(pmm.bitmap_start_frame, pmm.bitmap_end_frame, PMM_USED_VALUE);

    // Debug
    kprintf("Bitmap is in frames %d to %d (phys %p to %p)\n",
        pmm.bitmap_start_frame, pmm.bitmap_end_frame, pmm.bitmap_start_frame * PMM_FRAME_SIZE,
        pmm.bitmap_end_frame * PMM_FRAME_SIZE);

    kprintf("Initialised PMM\n");
    return 0;
}

bool pmm_verify_bitmap(void) {
    if (memmap_request.response == NULL) {
        return false;
    }

    struct limine_memmap_response* memory_map = memmap_request.response;
    for (size_t i = 0; i < memory_map->entry_count; i++) {
        struct limine_memmap_entry* entry = memory_map->entries[i];
        
        uint64_t start_frame = entry->base / PMM_FRAME_SIZE;
        uint64_t end_frame = (entry->base + entry->length) / PMM_FRAME_SIZE;

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            // Check usable memory areas
            for (uint64_t f = start_frame; f < end_frame; f++) {
                // Check frames containing bitmap (should be marked used)
                if (f >= pmm.bitmap_start_frame && f < pmm.bitmap_end_frame) {
                    if (pmm_test_bit(f) == PMM_FREE_VALUE) {
                        kprintf("PMM Test Failed: Frame %d containing bitmap is marked as FREE!\n", f);
                        return false;
                    }
                    continue;
                }
                // Check other frames (should be free, except address 0)
                if (f != 0 && pmm_test_bit(f) == PMM_USED_VALUE) {
                    kprintf("PMM Test Failed: Usable RAM frame %d is marked as USED!\n", f);
                    return false;
                }
                if (f == 0 && pmm_test_bit(f) == PMM_FREE_VALUE) {
                    kprintf("PMM Test Failed: First RAM frame (frame 0) is marked as FREE!\n", f);
                    return false;
                }
            }
        } else {
            // Ensure reserved areas remained 1
            for (uint64_t f = start_frame; f < end_frame; f++) {
                // We only care about frames tracked by the bitmap
                if (f >= pmm.bitmap_end_frame) {
                    break;
                }

                if (pmm_test_bit(f) == PMM_FREE_VALUE) {
                    kprintf("PMM Test Failed: Reserved memory frame %d is marked as FREE!\n", f);
                    return false;
                }
            }
        }
    }

    kprintf("PMM Test Passed!\n");
    return true;
}

phys_addr pmm_alloc(uint64_t size) {
    if (size == 0) {
        return 0;
    }

    const uint64_t max_frame = pmm.bitmap_size * 8;
    uint64_t left = 0;
    
    // We might want to check 64 bits at a time instead of every bit.
    while (left < max_frame) {
        if (pmm_test_bit(left) == PMM_FREE_VALUE) {
            bool is_space_found = true;
            // Ensure there's space for all frames.
            if (left + size > max_frame) {
                break;
            }

            for (uint64_t right = left; right < left + size; right++) {
                if (pmm_test_bit(right) == PMM_USED_VALUE) {
                    left = right;
                    is_space_found = false;
                    break;
                }
            }

            if (is_space_found) {
                pmm_assign(left, left + size, PMM_USED_VALUE);
                return left * PMM_FRAME_SIZE;
            }
        }

        left++;
    }

    // Out of memory.
    return 0;
}

void pmm_free(phys_addr address, uint64_t size) {
    if (address == 0) {
        return;
    }

    uint64_t frame = address / PMM_FRAME_SIZE;
    pmm_assign(frame, frame + size, PMM_FREE_VALUE);
}

void pmm_print_debug(uint64_t frame, uint64_t count) {
    kprintf("PMM view of frames %d to %d: ", frame, frame + count);
    for (uint64_t i = frame; i < frame + count; i++) {
        kprintf(pmm_test_bit(i) == PMM_USED_VALUE ? "1" : "0");
    }
    kprintf("\n");
}
