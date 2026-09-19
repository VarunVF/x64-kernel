#include "memory/kmalloc.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <limine.h>

#include "kprint.h"
#include "macros.h"
#include "memory/pmm.h"
#include "memory/util.h"
#include "memory/vmm.h"
#include "types.h"

#define HEAP_INITIAL_PAGES 2
#define HEAP_EXPAND_PAGES 2
#define ALIGNMENT 16
#define MIN_NODE_SIZE ALIGNMENT

extern volatile struct limine_executable_file_request executable_file_request;
extern volatile struct limine_executable_address_request executable_address_request;

struct heap_node {
    struct heap_node* prev;
    struct heap_node* next;
    size_t size;
    bool is_used;
};

struct heap_node* heap_start = NULL;
virt_addr heap_limit = 0;

// Returns zero on success
static int heap_expand(size_t pages) {
    void* root = vmm_kernel_table_root();
    for (size_t i = 0; i < pages; i++) {
        phys_addr phys = pmm_alloc(1);
        if (phys == 0)
            return 1;

        vmm_map_page(root, heap_limit, phys, VM_FLAG_WRITE);
        heap_limit += VMM_PAGE_SIZE;
    }
    
    return 0;
}

int kmalloc_init() {
    if (executable_address_request.response == NULL)
        return 1;
    if (executable_file_request.response == NULL)
        return 2;

    virt_addr kernel_start = executable_address_request.response->virtual_base;
    size_t kernel_size = executable_file_request.response->executable_file->size;
    
    // Add an unmapped page to catch buggy accesses
    virt_addr raw_start = kernel_start + kernel_size + VMM_PAGE_SIZE;
    heap_start = (struct heap_node*)align_up(raw_start, VMM_PAGE_SIZE);
    heap_limit = (virt_addr)heap_start;

    // Map initial heap pages
    if (heap_expand(HEAP_INITIAL_PAGES) != 0)
        return 3;

    heap_start->prev = NULL;
    heap_start->next = NULL;
    heap_start->size = heap_limit - (virt_addr)heap_start - sizeof(struct heap_node);
    heap_start->is_used = false;

    return 0;
}

void kmalloc_print_debug() {
    kprintf("Heap start at %p\n", heap_start);
    kprintf("Heap limit at %p\n", heap_limit);
    kprintf("Heap contents:\n");
    
    struct heap_node* current = heap_start;
    while (current != NULL) {
        kprintf("    Heap node at %p: prev=%p, next=%p, size=%d, is_used=%d\n",
            current, current->prev, current->next, current->size, (int)current->is_used);
        current = current->next;
    }

    kprintf("End of heap contents.\n");
}

void* kmalloc(size_t size) {
    KASSERT_MSG(heap_start != NULL && heap_limit != 0, "Heap was not initialised");

    if (size == 0)
        return NULL;

    size_t aligned_size = align_up(size, ALIGNMENT);

    struct heap_node* current = heap_start;
    struct heap_node* last = NULL;

    while (current != NULL) {
        if (!current->is_used && current->size >= aligned_size) {
            // Check if node can be split
            if (current->size >= aligned_size + sizeof(struct heap_node) + MIN_NODE_SIZE) {
                uint8_t* new_node_addr = (uint8_t*)current + sizeof(struct heap_node) + aligned_size;
                struct heap_node* new_node = (struct heap_node*)new_node_addr;

                new_node->size = current->size - aligned_size - sizeof(struct heap_node);
                new_node->is_used = false;
                new_node->next = current->next;
                new_node->prev = current;

                if (current->next != NULL)
                    current->next->prev = new_node;

                current->next = new_node;
                current->size = aligned_size;
            }

            current->is_used = true;
            return (uint8_t*)current + sizeof(struct heap_node);
        }

        last = current;
        current = current->next;
    }

    // No suitable free block found, expand heap
    if (last != NULL && !last->is_used) {
        // Expand existing tail free block
        size_t needed = aligned_size - last->size;
        size_t pages_needed = (needed + VMM_PAGE_SIZE - 1) / VMM_PAGE_SIZE;
        if (pages_needed < HEAP_EXPAND_PAGES)
            pages_needed = HEAP_EXPAND_PAGES;

        if (heap_expand(pages_needed) != 0)
            return NULL;

        last->size = heap_limit - (virt_addr)last - sizeof(struct heap_node);
        return kmalloc(size);
    } else {
        // Append a new trailing free block at current heap end
        virt_addr new_node_addr = (virt_addr)last + sizeof(struct heap_node) + last->size;
        size_t total_needed = sizeof(struct heap_node) + aligned_size;
        
        size_t pages_needed = (total_needed + VMM_PAGE_SIZE - 1) / VMM_PAGE_SIZE;
        if (pages_needed < HEAP_EXPAND_PAGES)
            pages_needed = HEAP_EXPAND_PAGES;

        if (heap_expand(pages_needed) != 0)
            return NULL;

        struct heap_node* new_node = (struct heap_node*)new_node_addr;
        new_node->prev = last;
        new_node->next = NULL;
        new_node->size = heap_limit - new_node_addr - sizeof(struct heap_node);
        new_node->is_used = false;

        if (last != NULL)
            last->next = new_node;

        return kmalloc(size);
    }
}

static void heap_merge_with_next(struct heap_node* node) {
    KASSERT(node != NULL);

    struct heap_node* next = node->next;
    if (next == NULL || next->is_used)
        return;

    node->size += sizeof(struct heap_node) + next->size;
    node->next = next->next;

    if (node->next != NULL)
        node->next->prev = node;
}

void kfree(void* ptr) {
    if (ptr == NULL)
        return;

    uint8_t* node_byte_ptr = (uint8_t*)ptr - sizeof(struct heap_node);
    struct heap_node* node = (struct heap_node*)node_byte_ptr;

    KASSERT_MSG(node->is_used, "Possible double free detected");
    node->is_used = false;

    heap_merge_with_next(node);
    if (node->prev != NULL && !node->prev->is_used) {
        heap_merge_with_next(node->prev);
    }
}
