#include "../vmm.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../../limine-protocol/include/limine.h"

#include "./cpuid.h"
#include "../cpu.h"
#include "../../memory/kmalloc.h"
#include "../../memory/pmm.h"
#include "../../memory/util.h"
#include "../../kprint.h"
#include "../../kstring.h"
#include "../../macros.h"
#include "../../panic.h"
#include "../../types.h"

#define MIN_ALLOC_ADDR 0xffff800000000000

enum pt_flags {
    PT_FLAG_PRESENT = 1ull << 0,
    PT_FLAG_WRITE = 1ull << 1,
    PT_FLAG_USER = 1ull << 2,
    PT_FLAG_PWT = 1ull << 3,
    PT_FLAG_PCD = 1ull << 4,
    PT_FLAG_NX = 1ull << 63,
};

struct vm_object {
    virt_addr base;
    size_t length;
    uint64_t vm_flags;
    struct vm_object* next;
};

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_executable_file_request executable_file_request;
extern volatile struct limine_executable_address_request executable_address_request;

extern struct heap_node* heap_start;

uint64_t max_phy_addr = 36;
uint64_t* pml4_virt = NULL;
struct vm_object* vm_objs = NULL;

void vmm_switch_stack_and_cr3(uint64_t masked_pml4_phys, uint64_t new_rsp, vmm_switch_fn vmm_switch_function);

static bool vmm_check_features() {
    uint32_t eax, ebx, ecx, edx;
    uint32_t max_phys_addr_leaf = 0x80000008;
    if (!cpuid(max_phys_addr_leaf, &eax, &ebx, &ecx, &edx)) {
        kprintf("Could not call cpuid with leaf %x\n", max_phys_addr_leaf);
        return false;
    }
    max_phy_addr = eax & 0x000000FF;
    kprintf("Max physical address width: %d\n", max_phy_addr);
    return true;
}

static bool vmm_is_entry_present(uint64_t* table, size_t index) {
    return (table[index] & 1) == 1;
}

static uint64_t vmm_get_addr_mask() {
    // Set all bits from 0 up to (MAXPHYADDR - 1), then page-align
    uint64_t phy_addr_mask = (1ull << max_phy_addr) - 1ull;
    return phy_addr_mask & ~0xFFFull;
}

static uint64_t* vmm_find_entry(uint64_t* table, uint64_t index) {
    if (table == NULL || !vmm_is_entry_present(table, index)) {
        return NULL;
    }

    return (uint64_t*)phys_to_virt(table[index] & vmm_get_addr_mask());
}

static uint64_t* vmm_find_or_create_entry(uint64_t* table, uint64_t index, uint64_t pt_flags) {
    if (!vmm_is_entry_present(table, index)) {
        phys_addr pml4_entry_phys = pmm_alloc(1);
        if (pml4_entry_phys == 0)
            return NULL;
        
        memset((void*)phys_to_virt(pml4_entry_phys), 0, VMM_PAGE_SIZE);
        table[index] = pml4_entry_phys | pt_flags;
    }

    return (uint64_t*)phys_to_virt(table[index] & vmm_get_addr_mask());
}

static size_t vmm_extract_index(virt_addr virt, int shift) {
    // Translation uses 9 bits at a time from a 48-bit linear address
    return (virt >> shift) & 0x1FFull;
}

int vmm_init() {
    kprintf("Initialising VMM...\n");

    if (!vmm_check_features()) {
        return 1;
    }

    // Allocate the top-level table
    phys_addr pml4_phys = pmm_alloc(1);
    if (pml4_phys == 0) {
        return 2;
    }
    pml4_virt = (uint64_t*)phys_to_virt(pml4_phys);
    memset(pml4_virt, 0, VMM_PAGE_SIZE);
    kprintf("VMM created PML4 at phys %p\n", pml4_phys);

    // Map the kernel
    if (executable_file_request.response == NULL || executable_address_request.response == NULL) {
        return 3;
    }
    struct limine_file* kernel_file = executable_file_request.response->executable_file;
    uint64_t kernel_size = kernel_file->size;
    phys_addr kernel_phys_start = executable_address_request.response->physical_base;
    virt_addr kernel_virt_start = executable_address_request.response->virtual_base;
    vmm_map_pages(pml4_virt, kernel_size / VMM_PAGE_SIZE, kernel_virt_start, kernel_phys_start, VM_FLAG_WRITE | VM_FLAG_EXEC);
    kprintf("VMM mapped kernel\n");

    // Map memory map entries with HHDM
    if (memmap_request.response == NULL) {
        return 4;
    }
    for (size_t i = 0; i < memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap_request.response->entries[i];
        if (entry->type != LIMINE_MEMMAP_BAD_MEMORY) {
            vmm_map_pages(pml4_virt, entry->length / VMM_PAGE_SIZE, phys_to_virt(entry->base), entry->base, VM_FLAG_WRITE);
        }
    }
    kprintf("VMM mapped memory map entries with HHDM\n");

    kprintf("Initialised VMM\n");
    return 0;
}

static void vmm_add_region(virt_addr virt, size_t length, uint64_t vm_flags) {
    length = (size_t)align_up(length, VMM_PAGE_SIZE);

    struct vm_object* current = vm_objs;
    struct vm_object* prev = NULL;

    while (current != NULL && current->base < virt) {
        prev = current;
        current = current->next;
    }

    if (prev != NULL && (prev->base + prev->length > virt)) {
        kprintf("Unable to place node: (prev->base + prev->length)=%p, virt=%p\n", prev->base + prev->length, virt);
        panic("vmm_add_region overlap with previous region");
    }

    if (current != NULL && (virt + length > current->base)) {
        kprintf("Unable to place node: (virt + length)=%p, current->base=%p\n", virt + length, current->base);
        panic("vmm_add_region overlap with next region");
    }

    struct vm_object* latest = kmalloc(sizeof(struct vm_object));
    latest->base = virt;
    latest->length = length;
    latest->vm_flags = vm_flags;
    latest->next = current;
    
    if (prev == NULL) {
        vm_objs = latest;
    } else {
        prev->next = latest;
    }
}

void vmm_add_initial_regions() {
    // Kernel
    KASSERT(executable_address_request.response != NULL && executable_file_request.response != NULL);
    virt_addr kernel = executable_address_request.response->virtual_base;
    size_t kernel_size = executable_file_request.response->executable_file->size;
    vmm_add_region(kernel, kernel_size, VM_FLAG_WRITE | VM_FLAG_EXEC);
    
    // Kernel heap
    const virt_addr highest_virt_addr = (~(virt_addr)0);
    size_t kernel_heap_size = highest_virt_addr - (virt_addr)heap_start;
    vmm_add_region((virt_addr)heap_start, kernel_heap_size, VM_FLAG_WRITE);
    
    // Higher half direct map
    KASSERT(memmap_request.response != NULL);
    for (size_t i = 0; i < memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap_request.response->entries[i];
        if (entry->type != LIMINE_MEMMAP_BAD_MEMORY) {
            vmm_add_region(phys_to_virt(entry->base), entry->length, VM_FLAG_WRITE);
        }
    }
}

void* vmm_kernel_table_root() {
    return pml4_virt;
}

uint64_t vmm_convert_vm_flags(uint64_t vm_flags) {
    uint64_t pt_flags = PT_FLAG_PRESENT;
    if (vm_flags & VM_FLAG_WRITE)
        pt_flags |= PT_FLAG_WRITE;
    if (vm_flags & VM_FLAG_USER)
        pt_flags |= PT_FLAG_USER;
    if ((vm_flags & VM_FLAG_EXEC) == 0)
        pt_flags |= PT_FLAG_NX;
    if (vm_flags & VM_FLAG_UC)
        pt_flags |= PT_FLAG_PWT | PT_FLAG_PCD;
    
    return pt_flags;
}

void vmm_switch(vmm_switch_fn switch_function) {
    // Allocate a new stack
    const size_t stack_size = 16 * 1024;  // 16KiB stack
    phys_addr stack_phys_base = pmm_alloc(stack_size / PMM_FRAME_SIZE);
    if (stack_phys_base == 0)
        return;

    virt_addr stack_virt_base = phys_to_virt(stack_phys_base);
    vmm_map_pages(pml4_virt, 4, stack_virt_base, stack_phys_base, VM_FLAG_WRITE);
    memset((void*)stack_virt_base, 0, 4 * VMM_PAGE_SIZE);
    
    uint64_t masked_pml4_phys = virt_to_phys((virt_addr)pml4_virt) & vmm_get_addr_mask();
    uint64_t new_rsp = stack_virt_base + 4 * VMM_PAGE_SIZE;
    vmm_switch_stack_and_cr3(masked_pml4_phys, new_rsp, switch_function);

    cpu_halt_loop();
}

void vmm_map_page(void* root, virt_addr virt, phys_addr phys, uint64_t vm_flags) {
    size_t pml4_index = vmm_extract_index(virt, 39);
    size_t pdpt_index = vmm_extract_index(virt, 30);
    size_t pd_index   = vmm_extract_index(virt, 21);
    size_t pt_index   = vmm_extract_index(virt, 12);

    uint64_t* pdpt = vmm_find_or_create_entry(root, pml4_index, PT_FLAG_PRESENT | PT_FLAG_WRITE);
    uint64_t* pd = vmm_find_or_create_entry(pdpt, pdpt_index, PT_FLAG_PRESENT | PT_FLAG_WRITE);
    uint64_t* pt = vmm_find_or_create_entry(pd, pd_index, PT_FLAG_PRESENT | PT_FLAG_WRITE);

    uint64_t masked_phys = phys & vmm_get_addr_mask();
    pt[pt_index] = masked_phys | vmm_convert_vm_flags(vm_flags);

    // Flush the entry in the TLB or we might not notice the change
    cpu_invalidate_tlb_page(virt);
}

void vmm_map_pages(void* root, size_t pages, virt_addr virt, phys_addr phys, uint64_t vm_flags) {
    for (size_t i = 0; i < pages; i++) {
        uint64_t offset = i * VMM_PAGE_SIZE;
        vmm_map_page(root, virt + offset, phys + offset, vm_flags);
    }
}

void vmm_unmap_page(void* root, virt_addr virt) {
    size_t pml4_index = vmm_extract_index(virt, 39);
    size_t pdpt_index = vmm_extract_index(virt, 30);
    size_t pd_index   = vmm_extract_index(virt, 21);
    size_t pt_index   = vmm_extract_index(virt, 12);

    uint64_t* pdpt = vmm_find_entry(root, pml4_index);
    uint64_t* pd = vmm_find_entry(pdpt, pdpt_index);
    uint64_t* pt = vmm_find_entry(pd, pd_index);

    if (pt != NULL) {
        // If this was the last entry in a lower-level table, we can optionally free the table.

        // Clear the entry and flush it in the TLB
        pt[pt_index] = 0;
        cpu_invalidate_tlb_page(virt);
    }
}

void vmm_unmap_pages(void* root, size_t pages, virt_addr virt) {
    for (size_t i = 0; i < pages; i++) {
        uint64_t offset = i * VMM_PAGE_SIZE;
        vmm_unmap_page(root, virt + offset);
    }
}

phys_addr vmm_find_phys(void* root, virt_addr virt) {
    size_t pml4_index = vmm_extract_index(virt, 39);
    size_t pdpt_index = vmm_extract_index(virt, 30);
    size_t pd_index   = vmm_extract_index(virt, 21);
    size_t pt_index   = vmm_extract_index(virt, 12);

    uint64_t* pdpt = vmm_find_entry(root, pml4_index);
    uint64_t* pd = vmm_find_entry(pdpt, pdpt_index);
    uint64_t* pt = vmm_find_entry(pd, pd_index);

    if (pt == NULL) {
        return 0;
    }

    return pt[pt_index] & vmm_get_addr_mask();
}

void* vmm_alloc(size_t length, uint64_t vm_flags, void* arg) {
    length = (size_t)align_up(length, VMM_PAGE_SIZE);

    struct vm_object* current = vm_objs;
    struct vm_object* prev = NULL;
    virt_addr prev_end = MIN_ALLOC_ADDR;

    while (current != NULL) {
        if (prev)
            prev_end = prev->base + prev->length;
            
        // Is there enough space between two vm_objects to hold length bytes?
        if (prev_end + length <= current->base) {
            break;
        }

        prev = current;
        current = current->next;
    }

    struct vm_object* latest = kmalloc(sizeof(struct vm_object));
    if (prev == NULL) {
        vm_objs = latest;
    } else {
        prev_end = prev->base + prev->length;
        prev->next = latest;
    }
    latest->next = current;
    latest->base = prev_end;
    latest->length = length;
    latest->vm_flags = vm_flags;

    phys_addr phys = 0;
    if (vm_flags & VM_FLAG_MMIO)
        phys = (phys_addr)arg;
    else
        phys = pmm_alloc(length / VMM_PAGE_SIZE);  // Immediate backing
    
    if (phys == 0)
        return NULL;

    vmm_map_pages(pml4_virt, length / VMM_PAGE_SIZE, latest->base, phys, vm_flags);
    return (void*)latest->base;
}

void vmm_free(void* addr) {
    struct vm_object* current = vm_objs;
    struct vm_object* prev = NULL;

    while (current != NULL) {
        if (current->base == (virt_addr)addr)
            break;

        prev = current;
        current = current->next;
    }
    
    if (current == NULL)
        panic("Bad pointer passed to vmm_free");

    if (current == vm_objs)
        vm_objs = current->next;  // Removing first node
    else
        prev->next = current->next;  // Removing middle node
    current->next = NULL;
    kfree(current);

    if (!(current->vm_flags & VM_FLAG_MMIO)) {
        phys_addr phys_base = vmm_find_phys(pml4_virt, current->base);
        pmm_free(phys_base, current->length / VMM_PAGE_SIZE);
    }
    vmm_unmap_pages(pml4_virt, current->length / VMM_PAGE_SIZE, current->base);
}

void vmm_debug_print() {
    kprintf("vm_objs list:\n");

    struct vm_object* current = vm_objs;
    while (current != NULL) {
        kprintf("    vm_object at %p: base=%p, length=%p, flags=0b%b, next=%p\n",
            current, current->base, current->length, current->vm_flags, current->next);
        current = current->next;
    }

    kprintf("End of vm_objs list.\n");
}
