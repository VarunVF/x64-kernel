#include "scheduler.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "cpu.h"
#include "kprint.h"
#include "kstring.h"
#include "macros.h"
#include "memory/kmalloc.h"
#include "memory/vmm.h"
#include "types.h"

#define STACK_SIZE (4 * VMM_PAGE_SIZE)

static uint64_t next_free_pid = 0;
static struct process* process_list = NULL;
static struct process* current_process = NULL;
static struct process* idle_process = NULL;

static void process_add(struct process* process) {
    if (process_list == NULL) {
        process_list = process;
        process_list->next = NULL;
        return;
    }

    struct process* prev = NULL;
    struct process* current = process_list;
    while (current != NULL) {
        prev = current;
        current = current->next;
    }
    prev->next = process;
    process->next = NULL;
}

void process_exit(void) {
    KASSERT(current_process != idle_process);

    current_process->status = PROCESS_DEAD;
    cpu_halt_loop();  // TODO: or yield
}

void scheduler_init(void) {
    idle_process = kmalloc(sizeof(struct process));
    KASSERT(idle_process != NULL);
    memset(idle_process, 0, sizeof(*idle_process));
    strncpy(idle_process->name, "Idle", PROCESS_NAME_MAX_LENGTH);
    idle_process->pid = next_free_pid++;
    idle_process->status = PROCESS_RUNNING;
    idle_process->stack_base = NULL;
    idle_process->context = NULL;

    // This thread (boot thread) becomes the idle process.
    current_process = idle_process;
}

void scheduler_start(void) {
    // starts scheduling via PIT
    cpu_enable_interrupts();
}

struct process* process_create(const char* name, void (*function)(void*), void* arg) {
    struct process* process = kmalloc(sizeof(struct process));
    if (process == NULL) {
        return NULL;
    }
    memset(process, 0, sizeof(*process));
    strncpy(process->name, name, PROCESS_NAME_MAX_LENGTH);
    process->name[PROCESS_NAME_MAX_LENGTH - 1] = '\0';
    process->pid = next_free_pid++;
    process->status = PROCESS_READY;
    
    process->stack_base = vmm_alloc(STACK_SIZE, VM_FLAG_WRITE, NULL);
    if (process->stack_base == NULL) {
        kfree(process);
        return NULL;
    }
    memset(process->stack_base, 0, STACK_SIZE);
    uint8_t* stack_top = process->stack_base + STACK_SIZE;
    uint64_t* ret_addr = (uint64_t*)(stack_top - 8);
    *ret_addr = (virt_addr)process_exit;
    struct cpu_status_t* frame = (struct cpu_status_t*)((uint8_t*)ret_addr - sizeof(struct cpu_status_t));

    frame->ss = GDT_KERNEL_SS;
    frame->rsp = (uint64_t)ret_addr;
    frame->rflags = 0x202;
    frame->cs = GDT_KERNEL_CS;
    frame->rip = (uint64_t)function;

    frame->interrupt_number = 0;
    frame->error_code = 0;

    frame->rdi = (uint64_t)arg;
    frame->rbp = 0;
    
    process->context = frame;
    process_add(process);
    return process;
}

static struct process* find_ready_process(struct process* start, struct process* stop) {
    for (struct process* p = start; p != stop; p = p->next) {
        if (p->status == PROCESS_READY) {
            return p;
        }
    }
    return NULL;
}

static void reap_dead_processes(void) {
    struct process* prev = NULL;
    struct process* p = process_list;

    while (p != NULL) {
        struct process* next = p->next;

        if (p->status == PROCESS_DEAD && p != current_process) {
            if (prev == NULL) {
                process_list = next;  // removing the head
            } else {
                prev->next = next;
            }
            vmm_free(p->stack_base);
            kfree(p);
            // prev stays the same
        } else {
            prev = p;
        }
        p = next;
    }
}

struct cpu_status_t* schedule(struct cpu_status_t* context) {
    KASSERT(current_process != NULL);

    // Save the current process context
    current_process->context = context;
    if (current_process->status == PROCESS_RUNNING) {
        current_process->status = PROCESS_READY;
    }

    reap_dead_processes();

    struct process* start = current_process->next;
    if (current_process == idle_process || current_process->next == NULL) {
        start = process_list;
    }

    // Find the next ready task
    struct process* next = find_ready_process(start, NULL);
    if (next == NULL) {
        // Wrap around to the start.
        next = find_ready_process(process_list, start);
    }
    if (next == NULL) {
        // No tasks are ready.
        next = idle_process;
    }

    next->status = PROCESS_RUNNING;
    current_process = next;
    return next->context;
}

void scheduler_print_debug(const struct kprint_sink* sink) {
    kfprintf(sink, "Current process: '%s'\n", current_process->name);
    kfprintf(sink, "Process list:\n");
    
    struct process* current = process_list;
    kfprintf(sink, "    Process at %p: pid=%d, next=%p: '%s'\n",
        idle_process, idle_process->pid, idle_process->next, idle_process->name);
    while (current != NULL) {
        kfprintf(sink, "    Process at %p: pid=%d, next=%p: '%s'\n",
            current, current->pid, current->next, current->name);
        current = current->next;
    }

    kfprintf(sink, "End of process list.\n");
}
