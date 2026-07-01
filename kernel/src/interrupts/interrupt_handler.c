#include "idt.h"
#include "../serial.h"

const char* exception_descriptions[32] = {
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

static void dump_register(const char* name, uint64_t value) {
    serial_write_str(SERIAL_COM1, name);
    serial_write_str(SERIAL_COM1, " = ");
    serial_write_hex(SERIAL_COM1, value);
    serial_write_str(SERIAL_COM1, "\n");
}

static void dump_registers(struct interrupt_registers* regs) {
    dump_register("ss", regs->ss);
    dump_register("rsp", regs->rsp);
    dump_register("rflags", regs->rflags);
    dump_register("cs", regs->cs);
    dump_register("rip", regs->rip);

    dump_register("rax", regs->rax);
    dump_register("rcx", regs->rcx);
    dump_register("rdx", regs->rdx);
    dump_register("rbx", regs->rbx);
    dump_register("rbp", regs->rbp);
    dump_register("rsi", regs->rsi);
    dump_register("rdi", regs->rdi);

    dump_register("r8", regs->r8);
    dump_register("r9", regs->r9);
    dump_register("r10", regs->r10);
    dump_register("r11", regs->r11);
    dump_register("r12", regs->r12);
    dump_register("r13", regs->r13);
    dump_register("r14", regs->r14);
    dump_register("r15", regs->r15);
}

// Signal end of interrupt to the LAPIC
static void lapic_send_eoi() {
    // TODO
}

void interrupt_handler(struct interrupt_registers* regs) {
    if (regs->interrupt_number < 32) {
        // Exception
        serial_write_str(SERIAL_COM1, "CPU exception: ");
        serial_write_str(SERIAL_COM1, exception_descriptions[regs->interrupt_number]);
        serial_write_str(SERIAL_COM1, "\n");
        serial_write_str(SERIAL_COM1, "Error code: ");
        serial_write_dec(SERIAL_COM1, regs->error_code);
        serial_write_str(SERIAL_COM1, "\n");

        // TODO: Not all exceptions are fatal, for now just print debug info and halt
        dump_registers(regs);
        serial_write_str(SERIAL_COM1, "Exception occured, system halted.\n");
        // Disable further interrupts and halt
        __asm__ volatile ("cli; hlt");
    } else {
        // Hardware interrupt
        // TODO: handle...

        lapic_send_eoi();
    }
}
