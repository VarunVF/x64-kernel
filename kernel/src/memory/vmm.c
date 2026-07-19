#include "vmm.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../limine-protocol/include/limine.h"

#include "pmm.h"
#include "util.h"
#include "../arch/cpu.h"
#include "../kprint.h"
#include "../kstring.h"

extern volatile struct limine_memmap_request memmap_request;

uint64_t max_phy_addr = 36;
uint64_t* pml4_virt = NULL;
uint64_t pml4_phys = 0;

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_file_request executable_file_request = {
    .id = LIMINE_EXECUTABLE_FILE_REQUEST_ID,
    .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request executable_address_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0,
};

void vmm_switch_stack_and_cr3(uint64_t masked_pml4_phys, uint64_t new_rsp, void (* vmm_switch_function)(void));

static bool vmm_check_features() {
    uint32_t eax, ebx, ecx, edx;
    uint32_t max_phys_addr_leaf = 0x80000008;
    if (!cpu_cpuid(max_phys_addr_leaf, &eax, &ebx, &ecx, &edx)) {
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

static uint64_t* vmm_find_or_create_entry(uint64_t* table, uint64_t index, uint64_t flags) {
    if (!vmm_is_entry_present(table, index)) {
        uint64_t pml4_entry_phys = pmm_alloc(1);
        memset((void*)phys_to_virt(pml4_entry_phys), 0, VMM_PAGE_SIZE);
        table[index] = pml4_entry_phys | flags;
    }

    return (uint64_t*)phys_to_virt(table[index] & vmm_get_addr_mask());
}

static size_t vmm_extract_index(uint64_t virt, int shift) {
    // Translation uses 9 bits at a time from a 48-bit linear address
    return (virt >> shift) & 0x1FFull;
}

void vmm_map_page(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    size_t pml4_index = vmm_extract_index(virt, 39);
    size_t pdpt_index = vmm_extract_index(virt, 30);
    size_t pd_index   = vmm_extract_index(virt, 21);
    size_t pt_index   = vmm_extract_index(virt, 12);

    uint64_t* pdpt = vmm_find_or_create_entry(pml4, pml4_index, VMM_PRESENT | VMM_WRITABLE);
    uint64_t* pd = vmm_find_or_create_entry(pdpt, pdpt_index, VMM_PRESENT | VMM_WRITABLE);
    uint64_t* pt = vmm_find_or_create_entry(pd, pd_index, VMM_PRESENT | VMM_WRITABLE);

    uint64_t masked_phys = phys & vmm_get_addr_mask();
    pt[pt_index] = masked_phys | flags;

    // Flush the entry in the TLB or we might not notice the change
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

int vmm_init() {
    if (!vmm_check_features()) {
        return 1;
    }

    // Allocate the top-level table
    pml4_phys = pmm_alloc(1);
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
    uint64_t kernel_phys_start = executable_address_request.response->physical_base;
    uint64_t kernel_virt_start = executable_address_request.response->virtual_base;
    for (uint64_t virt = kernel_virt_start; virt < kernel_virt_start + kernel_size; virt += VMM_PAGE_SIZE) {
        uint64_t phys = virt - kernel_virt_start + kernel_phys_start;
        vmm_map_page(pml4_virt, virt, phys, VMM_PRESENT | VMM_WRITABLE);
    }
    kprintf("VMM mapped kernel\n");

    // Map memory map entries with HHDM
    if (memmap_request.response == NULL) {
        return 4;
    }
    for (size_t i = 0; i < memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap_request.response->entries[i];
        if (entry->type != LIMINE_MEMMAP_BAD_MEMORY) {
            for (uint64_t phys = entry->base; phys < entry->base + entry->length; phys += VMM_PAGE_SIZE) {
                uint64_t virt = phys_to_virt(phys);
                vmm_map_page(pml4_virt, virt, phys, VMM_PRESENT | VMM_WRITABLE);
            }
        }
    }
    kprintf("VMM mapped memory map entries with HHDM\n");

    return 0;
}

void vmm_switch(void (* vmm_switch_function)(void)) {
    // Allocate a new stack
    uint64_t stack_phys_base = pmm_alloc(4);  // 16KiB stack
    uint64_t stack_virt_base = phys_to_virt(stack_phys_base);
    for (int i = 0; i < 4; i++) {
        uint64_t offset = i * VMM_PAGE_SIZE;
        vmm_map_page(pml4_virt, stack_virt_base + offset, stack_phys_base + offset, VMM_PRESENT | VMM_WRITABLE);
    }
    memset((void*)stack_virt_base, 0, 4 * VMM_PAGE_SIZE);
    
    uint64_t masked_pml4_phys = pml4_phys & vmm_get_addr_mask();
    uint64_t new_rsp = stack_virt_base + 4 * VMM_PAGE_SIZE;
    vmm_switch_stack_and_cr3(masked_pml4_phys, new_rsp, vmm_switch_function);

    while (true) {
        // Unreachable
    }
}
