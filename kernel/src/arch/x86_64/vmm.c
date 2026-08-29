#include "../vmm.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../../limine-protocol/include/limine.h"

#include "../cpu.h"
#include "../../memory/pmm.h"
#include "../../memory/util.h"
#include "../../kprint.h"
#include "../../kstring.h"
#include "../../types.h"

enum pt_flags {
    PT_FLAG_PRESENT = 1ull << 0,
    PT_FLAG_WRITE = 1ull << 1,
    PT_FLAG_USER = 1ull << 2,
    PT_FLAG_NX = 1ull << 63,
};

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_executable_file_request executable_file_request;
extern volatile struct limine_executable_address_request executable_address_request;

uint64_t max_phy_addr = 36;
uint64_t* pml4_virt = NULL;

void vmm_switch_stack_and_cr3(uint64_t masked_pml4_phys, uint64_t new_rsp, vmm_switch_fn vmm_switch_function);

static inline void real_cpuid(uint32_t leaf, uint32_t sub_leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile (
        "cpuid"
        : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
        : "a" (leaf), "c" (sub_leaf)
    );
}

static inline uint32_t cpuid_max(uint32_t leaf) {
    uint32_t eax, ebx, ecx, edx;
    real_cpuid(leaf, 0, &eax, &ebx, &ecx, &edx);
    return eax;
}

static inline int cpuid(uint32_t leaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    uint32_t max_leaf = cpuid_max(leaf & 0x80000000);

    if (max_leaf == 0 || max_leaf < leaf) {
        return 0;
    }

    real_cpuid(leaf, 0, eax, ebx, ecx, edx);
    return 1;
}

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
    for (virt_addr virt = kernel_virt_start; virt < kernel_virt_start + kernel_size; virt += VMM_PAGE_SIZE) {
        phys_addr phys = virt - kernel_virt_start + kernel_phys_start;
        vmm_map_page(pml4_virt, virt, phys, VM_FLAG_WRITE | VM_FLAG_EXEC);
    }
    kprintf("VMM mapped kernel\n");

    // Map memory map entries with HHDM
    if (memmap_request.response == NULL) {
        return 4;
    }
    for (size_t i = 0; i < memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap_request.response->entries[i];
        if (entry->type != LIMINE_MEMMAP_BAD_MEMORY) {
            for (phys_addr phys = entry->base; phys < entry->base + entry->length; phys += VMM_PAGE_SIZE) {
                virt_addr virt = phys_to_virt(phys);
                vmm_map_page(pml4_virt, virt, phys, VM_FLAG_WRITE);
            }
        }
    }
    kprintf("VMM mapped memory map entries with HHDM\n");

    kprintf("Initialised VMM\n");
    return 0;
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
    
    return pt_flags;
}

void vmm_switch(vmm_switch_fn switch_function) {
    // Allocate a new stack
    phys_addr stack_phys_base = pmm_alloc(4);  // 16KiB stack
    virt_addr stack_virt_base = phys_to_virt(stack_phys_base);
    for (int i = 0; i < 4; i++) {
        uint64_t offset = i * VMM_PAGE_SIZE;
        vmm_map_page(pml4_virt, stack_virt_base + offset, stack_phys_base + offset, VM_FLAG_WRITE);
    }
    memset((void*)stack_virt_base, 0, 4 * VMM_PAGE_SIZE);
    
    uint64_t masked_pml4_phys = virt_to_phys((virt_addr)pml4_virt) & vmm_get_addr_mask();
    uint64_t new_rsp = stack_virt_base + 4 * VMM_PAGE_SIZE;
    vmm_switch_stack_and_cr3(masked_pml4_phys, new_rsp, switch_function);

    while (true) {
        // Unreachable
    }
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
