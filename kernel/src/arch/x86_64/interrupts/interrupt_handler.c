#include "arch/x86_64/interrupt_handler.h"

#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/idt.h"
#include "cpu.h"
#include "kprint.h"
#include "macros.h"
#include "memory/util.h"
#include "panic.h"

#define LAPIC_EOI 0x0B0
#define MAX_NESTED_EXCEPTIONS 3
#define MAX_INTERRUPT_VECTORS 256

extern void* lapic_regs;

static const char* exception_descriptions[32] = {
    "Divide Error",
    "Debug Exception",
    "NMI Interrupt",
    "Breakpoint",
    "Overflow",
    "BOUND Range Exceeded",
    "Invalid Opcode (Undefined Opcode)",
    "Device Not Available (No Math Coprocessor)",
    "Double Fault",
    "Coprocessor Segment Overrun (reserved)",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection",
    "Page Fault",
    "(Intel Reserved)",
    "x87 FPU Floating-Point Error (Math Fault)",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "(Intel Reserved)",
    "(Intel Reserved)",
    "(Intel Reserved)",
    "(Intel Reserved)",
    "(Intel Reserved)",
    "(Intel Reserved)",
    "(Intel Reserved)",
    "(Intel Reserved)",
    "(Intel Reserved)",
    "(Intel Reserved)",
};
static int nested_exceptions = 1;
static struct interrupt_handler_entry handler_entries[MAX_INTERRUPT_VECTORS];

static inline uint64_t read_cr2(void) {
    uint64_t addr;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(addr));
    return addr;
}

static void dump_register(const char* name, uint64_t value) {
    kprintf("%s = %p\n", name, (void*)value);
}

static void dump_registers(struct cpu_status_t* regs) {
    dump_register("ss    ", regs->ss);
    dump_register("rsp   ", regs->rsp);
    dump_register("rflags", regs->rflags);
    dump_register("cs    ", regs->cs);
    dump_register("rip   ", regs->rip);

    dump_register("rax   ", regs->rax);
    dump_register("rcx   ", regs->rcx);
    dump_register("rdx   ", regs->rdx);
    dump_register("rbx   ", regs->rbx);
    dump_register("rbp   ", regs->rbp);
    dump_register("rsi   ", regs->rsi);
    dump_register("rdi   ", regs->rdi);

    dump_register("r8    ", regs->r8);
    dump_register("r9    ", regs->r9);
    dump_register("r10   ", regs->r10);
    dump_register("r11   ", regs->r11);
    dump_register("r12   ", regs->r12);
    dump_register("r13   ", regs->r13);
    dump_register("r14   ", regs->r14);
    dump_register("r15   ", regs->r15);

    dump_register("cr2   ", read_cr2());
}

// Signal end of interrupt to the LAPIC
static void lapic_send_eoi() {
    mmio_write32(lapic_regs, LAPIC_EOI, 0);
}

void interrupt_set_handler(uint8_t vector, handler_fn handler, void* context) {
    handler_entries[vector] = (struct interrupt_handler_entry){handler, context};
}

struct cpu_status_t* interrupt_handler(struct cpu_status_t* regs) {
    nested_exceptions++;

    struct cpu_status_t* next_context = regs;
    
    uint64_t vector = regs->interrupt_number;
    KASSERT(vector < 256);

    if (nested_exceptions > MAX_NESTED_EXCEPTIONS) {
        kprintf("CPU exception: v=%d\n", vector);
        dump_registers(regs);
        panic("Too many nested exceptions (max " TO_STRING(MAX_NESTED_EXCEPTIONS) ")");
    }
    
    if (vector < 32) {
        // Exception
        // TODO: Not all exceptions are fatal, for now just print debug info and halt

        cpu_disable_interrupts();
        kprintf("CPU exception: %s\n", exception_descriptions[vector]);
        kprintf("Error code: %d\n", regs->error_code);
        dump_registers(regs);
        panic("CPU Exception");
    } else {
        // Hardware interrupt

        struct interrupt_handler_entry* entry = &handler_entries[vector];
        if (entry->handler != NULL) {
            next_context = entry->handler(regs, entry->context);
        } else {
            kprintf("No interrupt handler found for v=%d\n", vector);
        }
    }
    
    lapic_send_eoi();
    nested_exceptions--;
    return next_context;
}
