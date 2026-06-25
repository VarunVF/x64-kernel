#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "../limine-protocol/include/limine.h"

#include "display.h"
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

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

// Halt function.
static void halt(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

// Our kernel's entry point.
void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        halt();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        halt();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    // Colors used for printing
    uint32_t clear_color = 0x000077U;
    uint32_t white = 0x00ffffffU;
    uint32_t green = 0x0000ff00U;
    uint32_t red = 0x00ff0000U;

    display_clear(framebuffer, clear_color);
    display_str(framebuffer, "Hello, world!\n", white);
    
    // Test char outside the font data values
    display_str(framebuffer, "Test invalid char display: ", white);
    display_char(framebuffer, -1, red);
    display_char(framebuffer, '\n', white);

    // Output to serial port
    const uint16_t port = SERIAL_COM1;
    if (serial_init(port) == 0) {
        display_str(framebuffer, "Serial port initialised\n", green);

        serial_write_str(port, "Framebuffer address: 0x");
        serial_write_hex(port, (uint64_t)framebuffer->address);
        serial_write_char(port, '\n');
        serial_write_str(port, "Framebuffer dimensions: ");
        serial_write_dec(port, framebuffer->width);
        serial_write_char(port, 'x');
        serial_write_dec(port, framebuffer->height);
        serial_write_char(port, '\n');
        serial_write_str(port, "Framebuffer bits per pixel: ");
        serial_write_dec(port, framebuffer->bpp);
        serial_write_char(port, '\n');
    }

    // We're done, just hang...
    halt();
}
