#include "shell.h"

#include <limine.h>
#include <stddef.h>

#include "cpu.h"
#include "drivers/pit.h"
#include "drivers/ps2kbd.h"
#include "drivers/render.h"
#include "kprint.h"
#include "string_type.h"
#include "memory/kmalloc.h"
#include "memory/vmm.h"

struct shell_context {
    struct limine_framebuffer* framebuffer;
    uint32_t clear_color;
};

typedef void (*shell_fn)(struct shell_context* context);

struct shell_command {
    const char* name;
    const shell_fn command;
};

static void shell_clear(struct shell_context* context);
static void shell_heap(struct shell_context* context);
static void shell_help(struct shell_context* context);
static void shell_timer(struct shell_context* context);
static void shell_vmm(struct shell_context* context);

static const struct shell_command commands[] = {
    { "clear", shell_clear },
    { "heap", shell_heap },
    { "help", shell_help },
    { "timer", shell_timer },
    { "vmm", shell_vmm },
};

static const char scancode_to_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

void shell_run(struct ps2kbd_state* ps2kbd, struct limine_framebuffer* framebuffer, uint32_t clear_color) {
    struct string input = string_init("");
    struct shell_context context = {
        framebuffer,
        clear_color
    };

    kprintf("> ");
    while (true) {
        if (!ps2kbd_has_event(ps2kbd)) {
            cpu_halt();
            continue;
        }
    
        struct ps2kbd_event event = {0};
        ps2kbd_get_event(ps2kbd, &event);
        if (event.type == KBD_EVENT_TYPE_PRESS) {
            char ascii = scancode_to_ascii[event.scancode];
            if (ascii == '\b' && input.size > 0) {
                render_move_pos(-1, 0);
                render_clear_pos(framebuffer, clear_color);
                string_pop(&input);
            } else if (ascii == '\n') {
                kprintf("\n");

                bool found_command = false;
                for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
                    if (c_str_equal(input.elements, commands[i].name)) {
                        commands[i].command(&context);
                        found_command = true;
                        break;
                    }
                }
                if (!found_command && input.size > 0) {
                    kprintf("Unknown command '%s'.\n", input.elements);
                }

                string_clear(&input);
                kprintf("> ");
            } else if (ascii != '\b') {
                kprintf("%c", ascii);
                string_push(&input, ascii);
            }
        }
    }

    string_free(&input);
}

static void shell_clear(struct shell_context* context) {
    render_clear(context->framebuffer, context->clear_color);
    render_reset_pos();
}

static void shell_heap(struct shell_context* context) {
    (void)context;
    kmalloc_print_debug();
}

static void shell_help(struct shell_context* context) {
    (void)context;
    kprintf("Available shell commands: ");
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        kprintf("%s ", commands[i].name);
    }
    kprintf("\n");
}

static void shell_timer(struct shell_context* context) {
    (void)context;
    kprintf("Timer ticks: %d\n", pit_get_ticks());
}

static void shell_vmm(struct shell_context* context) {
    (void)context;    
    vmm_debug_print();
}
