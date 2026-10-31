#include "shell.h"

#include <limine.h>
#include <stddef.h>
#include <stdint.h>

#include "cpu.h"
#include "drivers/pit.h"
#include "drivers/ps2kbd.h"
#include "drivers/render.h"
#include "kprint.h"
#include "kstring.h"
#include "panic.h"
#include "scheduler.h"
#include "string_type.h"
#include "memory/kmalloc.h"
#include "memory/vmm.h"

static void shell_fn_clear(struct shell_state* shell);
static void shell_fn_color(struct shell_state* shell);
static void shell_fn_heap(struct shell_state* shell);
static void shell_fn_help(struct shell_state* shell);
static void shell_fn_ps(struct shell_state* shell);
static void shell_fn_timer(struct shell_state* shell);
static void shell_fn_vmm(struct shell_state* shell);
static void shell_fn_panic(struct shell_state* shell);

static const struct shell_fn_entry commands[] = {
    { shell_fn_clear, "clear", "clear the shell screen" },
    { shell_fn_color, "color", "cycle the shell background color" },
    { shell_fn_heap, "heap", "list heap-allocated objects" },
    { shell_fn_help, "help", "show this help message" },
    { shell_fn_ps, "ps", "list active processes" },
    { shell_fn_timer, "timer", "print timer ticks" },
    { shell_fn_vmm, "vmm", "list VMM entries" },
    { shell_fn_panic, "panic", "trigger a panic" },
};

static const char scancode_to_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

static inline struct cell make_cell(char c) {
    return (struct cell){ c };
}

static void shell_clear(struct shell_state* shell) {
    struct shell_display* display = &shell->display;
    for (size_t i = 0; i < display->width * display->height; i++) {
        display->cells[i] = make_cell(' ');
    }

    display->x = 0;
    display->y = 0;
}

static void shell_write_char(struct shell_state* shell, char c) {
    struct shell_display* display = &shell->display;
    size_t index = display->y * display->width + display->x;
    
    if (c == '\n') {
        // fill the rest of the line
        for (size_t x = display->x; x < display->width; x++) {
            display->cells[display->y * display->width + x] = make_cell(' ');
        }

        // advance
        display->cells[index] = make_cell('\n');
        display->x = 0;
        display->y++;

        // scroll
        if (display->y >= display->height) {
            memmove(display->cells,
                display->cells + display->width,
                display->width * (display->height - 1) * sizeof(struct cell));
            display->y = display->height - 1;
        }

        // clear the next line
        for (size_t x = 0; x < display->width; x++) {
            display->cells[display->y * display->width + x] = make_cell(' ');
        }
    } else if (display->x < display->width) {
        display->cells[index] = make_cell(c);
        display->x++;
    }
}

static void shell_sink_putc(char c, void* context) {
    shell_write_char(context, c);
}

static void shell_erase_char(struct shell_state* shell) {
    struct shell_display* display = &shell->display;
    if (display->x > 0) {
        display->x--;
        size_t index = display->y * display->width + display->x;
        display->cells[index] = make_cell(' ');
    }
}

static void shell_write_str(struct shell_state* shell, const char* str) {
    while (*str) {
        shell_write_char(shell, *str++);
    }
}

static void shell_refresh(struct shell_state* shell) {
    struct shell_display* display = &shell->display;
    for (size_t y = 0; y < display->height; y++) {
        size_t x = 0;
        for (; x < display->width; x++) {
            size_t index = y * display->width + x;
            char c = display->cells[index].character;
            render_char_at(shell->framebuffer, c, x, y, shell->text_color, shell->clear_color);
        }
        render_char_at(shell->framebuffer, '\n', x, y, shell->text_color, shell->clear_color);
    }
}

static struct shell_state shell_init(struct shell_init_arg* arg) {
    uint64_t width_in_cells = arg->framebuffer->width / RENDER_CELL_SIZE;
    uint64_t height_in_cells = arg->framebuffer->height / RENDER_CELL_SIZE;

    struct cell* cells = kmalloc(width_in_cells * height_in_cells * sizeof(struct cell));
    if (cells == NULL) {
        panic("Failed to allocate memory for shell display");
    } else {
        kprintf("Allocated %dx%d shell display\n", width_in_cells, height_in_cells);
    }
    
    struct shell_display display = {
        cells,
        width_in_cells,
        height_in_cells,
        0, 0
    };

    struct shell_state shell = {
        display,
        arg->ps2kbd,
        arg->framebuffer,
        RENDER_DEFAULT_TEXT_COLOR,
        RENDER_DEFAULT_CLEAR_COLOR
    };
    
    shell_clear(&shell);
    return shell;
}

static void shell_free(struct shell_state* shell) {
    kfree(shell->display.cells);
}

void shell_run(void* shell_init_arg) {
    struct shell_init_arg* arg = shell_init_arg;
    struct shell_state shell = shell_init(arg);
    struct string input = string_init("");
    
    shell_write_str(&shell, "Entered shell. Type 'help' to list commands.\n");
    shell_write_str(&shell, "> ");
    shell_refresh(&shell);
    
    while (true) {
        bool should_refresh = false;

        if (!ps2kbd_has_event(arg->ps2kbd)) {
            cpu_halt();
            continue;
        }
    
        struct ps2kbd_event event = {0};
        ps2kbd_get_event(arg->ps2kbd, &event);
        if (event.type == KBD_EVENT_TYPE_PRESS) {
            char ascii = scancode_to_ascii[event.scancode];
            if (ascii == '\b' && input.size > 0) {
                string_pop(&input);
                shell_erase_char(&shell);
                should_refresh = true;
            } else if (ascii == '\n') {
                shell_write_char(&shell, '\n');

                bool found_command = false;
                for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
                    if (c_str_equal(input.elements, commands[i].name)) {
                        commands[i].command(&shell);
                        found_command = true;
                        break;
                    }
                }
                if (!found_command && input.size > 0) {
                    shell_write_str(&shell, "Unknown command '");
                    shell_write_str(&shell, input.elements);
                    shell_write_str(&shell, "'.\n");
                }

                string_clear(&input);
                shell_write_str(&shell, "> ");
                should_refresh = true;
            } else if (ascii != '\b') {
                shell_write_char(&shell, ascii);
                string_push(&input, ascii);
                should_refresh = true;
            }
        }

        if (should_refresh) {
            shell_refresh(&shell);
        }
    }

    string_free(&input);
    shell_free(&shell);

    cpu_halt_loop();
}

static void shell_fn_clear(struct shell_state* shell) {
    render_clear(shell->framebuffer, shell->clear_color);
    shell_clear(shell);
    render_reset_pos();
}

static void shell_fn_color(struct shell_state* shell) {
    const uint32_t default_color = RENDER_DEFAULT_CLEAR_COLOR;
    const uint32_t black = 0x00000000;
    const uint32_t purple = 0x00770077;
    const uint32_t red = 0x00770000;
    
    switch (shell->clear_color) {
    case default_color:
        shell->clear_color = black;
        break;
    case black:
        shell->clear_color = purple;
        break;
    case purple:
        shell->clear_color = red;
        break;
    case red:
    default:
        shell->clear_color = default_color;
        break;
    }
}

static void shell_fn_heap(struct shell_state* shell) {
    struct kprint_sink sink = { shell_sink_putc, shell };
    kmalloc_print_debug(&sink);
}

static void shell_fn_help(struct shell_state* shell) {
    struct kprint_sink sink = { shell_sink_putc, shell };
    kfprintf(&sink, "Available shell commands:\n");
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        kfprintf(&sink, "%s - %s\n", commands[i].name, commands[i].help);
    }
    kfprintf(&sink, "\n");
}

static void shell_fn_ps(struct shell_state* shell) {
    struct kprint_sink sink = { shell_sink_putc, shell };
    scheduler_print_debug(&sink);
}

static void shell_fn_timer(struct shell_state* shell) {
    struct kprint_sink sink = { shell_sink_putc, shell };
    kfprintf(&sink, "Timer ticks: %d\n", pit_get_ticks());
}

static void shell_fn_vmm(struct shell_state* shell) {
    struct kprint_sink sink = { shell_sink_putc, shell };
    vmm_print_debug(&sink);
}

static void shell_fn_panic(struct shell_state* shell) {
    struct kprint_sink sink = { shell_sink_putc, shell };
    kfprintf(&sink, "Entering panic.\n");
    panic("Triggered by user command");
}
