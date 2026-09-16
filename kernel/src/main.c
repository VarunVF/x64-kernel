#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <limine.h>

#include "acpi/madt.h"
#include "acpi/rsdp.h"
#include "arch/x86_64/apic.h"
#include "cpu.h"
#include "drivers/pit.h"
#include "drivers/ps2kbd.h"
#include "drivers/render.h"
#include "drivers/serial.h"
#include "kprint.h"
#include "limine_requests.h"
#include "macros.h"
#include "memory/kmalloc.h"
#include "memory/pmm.h"
#include "vmm.h"

static const uint32_t text_color = 0x00ffffff;
static const uint32_t clear_color = 0x00000077;
static const uint8_t pit_isa_irq = 0;
static const uint8_t ps2kbd_isa_irq = 1;
static const uint8_t pit_vector = 48;
static const uint8_t ps2kbd_vector = 49;

uint64_t hhdm_offset = 0;
void* lapic_regs = NULL;
void* ioapic_regs = NULL;

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
    render_char(framebuffer, c, text_color, clear_color);
}

// Our kernel's entry point.
void kernel_start(void) {
    cpu_disable_interrupts();

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

    render_clear(framebuffer, clear_color);
    kprint_add_output(render_putc);

    // Setup serial port for output
    if (serial_init(SERIAL_COM1) == 0) {
        kprint_add_output(serial_putc);
        kprintf("Serial port initialised\n");
    } else {
        kprintf("Warning: Failed to initialise serial port\n");
    }
    
    kprintf("Framebuffer: size=%dx%d bpp=%d pitch=%d\n",
        framebuffer->width, framebuffer->height, framebuffer->bpp, framebuffer->pitch);

    // Higher-half mapping offset
    hhdm_offset = hhdm_request.response->offset;
    kprintf("Higher-half offset: %p\n", (void*)hhdm_offset);

    cpu_init();
    cpu_setup_interrupts();

    KASSERT_MSG(pmm_init() == 0, "Failed to initialise PMM");
    KASSERT_MSG(pmm_verify_bitmap(), "PMM bitmap verification failed");
    KASSERT_MSG(vmm_init() == 0, "Failed to initialise VMM");

    vmm_switch(kernel_init);
    
    panic("Failed to switch to new stack / page tables");
    cpu_halt_loop();
}

void kernel_init(void) {
    kprintf("Switched to new stack and page tables\n");
    
    // Parse RSDP (for ACPI tables)
    void* rsdp_addr = rsdp_request.response->address;
    kprintf("RSDP found at %p\n", rsdp_addr);
    struct madt_parse_result madt_info = rsdp_parse(rsdp_addr).madt_info;
    KASSERT_MSG(madt_info.parse_successful, "Failed to parse MADT");

    KASSERT_MSG(kmalloc_init() == 0, "Failed to initialise heap");
    vmm_add_initial_regions();
    
    // Map LAPIC and IOAPIC registers as strong uncacheable (UC)
    uint64_t mmio_flags = VM_FLAG_WRITE | VM_FLAG_MMIO | VM_FLAG_UC;
    void* lapic_phys_base = (void*)(uint64_t)madt_info.lapic_base;
    void* ioapic_phys_base = (void*)(uint64_t)madt_info.ioapic_base;
    lapic_regs = vmm_alloc(VMM_PAGE_SIZE, mmio_flags, lapic_phys_base);
    ioapic_regs = vmm_alloc(VMM_PAGE_SIZE, mmio_flags, ioapic_phys_base);
    KASSERT_MSG(lapic_init(lapic_regs) == 0, "Failed to initialise APIC");

    pit_init(pit_vector, 100);
    ioapic_set_irq(ioapic_regs, &madt_info, pit_isa_irq, pit_vector, 0);
    kprintf("Initialised PIT\n");

    struct ps2kbd_state* ps2kbd = ps2kbd_init(ps2kbd_vector);
    ioapic_set_irq(ioapic_regs, &madt_info, ps2kbd_isa_irq, ps2kbd_vector, 0);
    kprintf("Initialised PS/2 Keyboard\n");

    cpu_enable_interrupts();

    kprintf("Press 'T' to view timer ticks.\n");
    while (true) {
        if (!ps2kbd_has_event(ps2kbd)) {
            cpu_halt();
            continue;
        }
    
        struct ps2kbd_event event = {0};
        ps2kbd_get_event(ps2kbd, &event);
        
        const uint8_t t_scancode = 20;
        if (event.scancode == t_scancode) {
            kprintf("Timer ticks: %d\n", pit_get_ticks());
        } else {
            kprintf("Scancode: %d, %s\n", event.scancode,
                event.type == KBD_EVENT_TYPE_PRESS ? "pressed" : "released");
        }
    }

    ps2kbd_free(ps2kbd, ps2kbd_vector);
    cpu_halt_loop();
}
