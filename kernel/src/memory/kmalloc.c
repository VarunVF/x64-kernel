#include "kmalloc.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../limine-protocol/include/limine.h"

#include "../arch/vmm.h"
#include "../kprint.h"
#include "../types.h"
#include "pmm.h"

#define HEAP_INITIAL_PAGES 2
#define HEAP_EXPAND_PAGES 2

extern volatile struct limine_executable_file_request executable_file_request;
extern volatile struct limine_executable_address_request executable_address_request;

struct heap_node {
    size_t size;
    bool is_used;
};

struct heap_node* heap_start = NULL;
struct heap_node* current_heap_pos = NULL;
virt_addr heap_limit = 0;

// Returns zero on success
static int heap_expand(size_t pages) {
    void* root = vmm_kernel_table_root();
    for (size_t i = 0; i < pages; i++) {
        virt_addr virt = (virt_addr)current_heap_pos + i * VMM_PAGE_SIZE;
        phys_addr phys = pmm_alloc(1);
        if (phys == 0)
            return 1;

        vmm_map_page(root, virt, phys, VM_FLAG_WRITE);
        heap_limit += VMM_PAGE_SIZE;
    }
    
    return 0;
}

// Will a new node of this size require expansion?
static bool heap_requires_expansion(size_t size) {
    return (virt_addr)current_heap_pos + sizeof(struct heap_node) + size >= heap_limit;
}

int kmalloc_init() {
    if (executable_address_request.response == NULL)
        return 1;
    if (executable_file_request.response == NULL)
        return 2;

    virt_addr kernel_start = executable_address_request.response->virtual_base;
    size_t kernel_size = executable_file_request.response->executable_file->size;
    
    // Add an unmapped page to catch buggy accesses
    heap_start = (struct heap_node*)(kernel_start + kernel_size + VMM_PAGE_SIZE);
    current_heap_pos = heap_start;
    heap_limit = (virt_addr)heap_start;

    // Map initial heap
    if (heap_expand(HEAP_INITIAL_PAGES) != 0)
        return 3;

    return 0;
}

void kmalloc_print_debug() {
    kprintf("Heap start at %p and current position at %p\n", heap_start, current_heap_pos);
    kprintf("Heap limit at %p\n", heap_limit);
    kprintf("Heap contents:\n");
    
    struct heap_node* current = heap_start;
    while (current < current_heap_pos) {
        kprintf("    Heap node: size=%d, is_used=%d\n", current->size, current->is_used);
        uint8_t* next = (uint8_t*)current + sizeof(struct heap_node) + current->size;
        current = (struct heap_node*)next;
    }

    kprintf("End of heap contents.\n");
}

void* kmalloc(size_t size) {
    struct heap_node* current = heap_start;
    while (current < current_heap_pos) {
        struct heap_node* current_node = (struct heap_node*)heap_start;
        
        // Free node with enough space
        if (current_node->size >= size && !current_node->is_used) {
            current_node->is_used = true;
            return (uint8_t*)current_node + sizeof(struct heap_node);
        }

        current += current_node->size + sizeof(struct heap_node);
    }

    if (heap_requires_expansion(size)) {
        if (heap_expand(HEAP_EXPAND_PAGES) != 0)
            return NULL;
    }

    // Append new node
    current_heap_pos->size = size;
    current_heap_pos->is_used = true;

    uint8_t* ptr = (uint8_t*)current_heap_pos + sizeof(struct heap_node);
    current_heap_pos = (struct heap_node*)(ptr + size);
    return ptr;
}

void kfree(void* ptr) {
    uint8_t* byte_ptr = (uint8_t*)ptr - sizeof(struct heap_node);
    struct heap_node* node = (struct heap_node*)byte_ptr;
    if (node->is_used)
        node->is_used = false;
}
