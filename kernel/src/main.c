#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../limine-protocol/include/limine.h"

#include "acpi/rsdp.h"
#include "arch/cpu.h"
#include "arch/vmm.h"
#include "kprint.h"
#include "macros.h"
#include "memory/pmm.h"
#include "memory/kmalloc.h"
#include "render.h"
#include "serial.h"

// Set the base revision to 6, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
};
uint64_t hhdm_offset = 0;

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0,
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;


void kernel_init(void);

void serial_putc(char c) {
    // Some serial output displays need carriage return
    if (c == '\n') {
        serial_write_char(SERIAL_COM1, '\r');
    }

    serial_write_char(SERIAL_COM1, c);
}

void render_putc(char c) {
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    render_char(framebuffer, c, 0x00ffffff);
}

// Our kernel's entry point.
void kernel_start(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        cpu_halt_loop();
    }

    // Verify Limine requests
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1
     || hhdm_request.response == NULL
     || rsdp_request.response == NULL) {
        cpu_halt_loop();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    // Colors used for printing
    const uint32_t clear_color = 0x000077U;
    const uint32_t red = 0x00ff0000U;

    render_clear(framebuffer, clear_color);
    kprint_add_output(render_putc);

    // Setup serial port for output
    if (serial_init(SERIAL_COM1) == 0) {
        kprint_add_output(serial_putc);
        kprintf("Serial port initialised\n");
    } else {
        render_str(framebuffer, "Failed to initialise serial port\n", red);
    }

    // Higher-half mapping offset
    hhdm_offset = hhdm_request.response->offset;
    kprintf("Higher-half offset: %p\n", (void*)hhdm_offset);

    cpu_init();
    cpu_setup_interrupts();

    KASSERT_MSG(pmm_init() == 0, "Failed to initialise PMM");
    KASSERT_MSG(pmm_verify_bitmap(), "PMM bitmap verification failed");
    KASSERT_MSG(vmm_init() == 0, "Failed to initialise VMM");

    vmm_switch(kernel_init);

    cpu_halt_loop();
}

void kernel_init(void) {
    kprintf("Switched to new stack and page tables\n");
    
    // Parse RSDP (for ACPI tables)
    void* rsdp_addr = rsdp_request.response->address;
    kprintf("RSDP found at %p\n", rsdp_addr);
    struct rsdp_parse_result rsdp_info = rsdp_parse(rsdp_addr);
    KASSERT_MSG(rsdp_info.parse_successful, "Failed to parse RSDP");

    KASSERT_MSG(kmalloc_init() == 0, "Failed to initialise heap");
    vmm_add_initial_regions();

    void* lapic_phys_base = (void*)(uint64_t)rsdp_info.lapic_base;
    int* lapic_regs = vmm_alloc(0x1000, VM_FLAG_WRITE | VM_FLAG_MMIO, lapic_phys_base);

    vmm_debug_print();

    cpu_halt_loop();
}
