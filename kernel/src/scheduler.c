#include "scheduler.h"

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

#define STACK_SIZE (4 * VMM_PAGE_SIZE)

static uint64_t next_free_pid = 0;
static struct process* process_list = NULL;
static struct process* current_process = NULL;

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

static void init_task(void* arg) {
    (void)arg;

    cpu_enable_interrupts();  // starts scheduling via PIT
    cpu_halt_loop();
}

void scheduler_init() {
    struct process* init = process_create("Init", init_task, NULL);
    process_add(init);
    current_process = init;

    init_task(NULL);  // enter this context before scheduling
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
    
    uint8_t* stack_base = vmm_alloc(STACK_SIZE, VM_FLAG_WRITE, NULL);
    if (stack_base == NULL) {
        return NULL;
    }
    memset(stack_base, 0, STACK_SIZE);
    uint8_t* stack_top = stack_base + STACK_SIZE;
    struct cpu_status_t* frame = (struct cpu_status_t*)(stack_top - sizeof(struct cpu_status_t));

    frame->ss = GDT_KERNEL_SS;
    frame->rsp = (uint64_t)stack_top;
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

struct cpu_status_t* schedule(struct cpu_status_t* context) {
    KASSERT(current_process != NULL);

    // Save the current process context
    current_process->context = context;

    // Select the next process
    if (current_process->next != NULL) {
        current_process = current_process->next;
    } else {
        current_process = process_list;
    }

    // Return the saved context of the next process
    return current_process->context;
}

void scheduler_debug_print() {
    kprintf("Current process: '%s'\n", current_process->name);
    kprintf("Process list:\n");
    
    struct process* current = process_list;
    while (current != NULL) {
        kprintf("    Process at %p: pid=%d, next=%p: '%s'\n",
            current, current->pid, current->next, current->name);
        current = current->next;
    }

    kprintf("End of process list.\n");
}
