#include "panic.h"

#include <stddef.h>
#include <stdint.h>

#include "cpu.h"
#include "kprint.h"

#define MAX_STACK_TRACE_DEPTH 32

static void print_stack_trace(uint64_t* instruction_ptr) {
    kprintf("Stack trace:\n");
    kprintf("#0 %p\n", instruction_ptr);
    
    uint64_t* frame_ptr = cpu_read_frame_ptr();
    for (int frame_no = 1; frame_no < MAX_STACK_TRACE_DEPTH; frame_no++) {
        if (frame_ptr == NULL)
            break;

        uint64_t* next_frame_ptr = (uint64_t*)*frame_ptr;
        uint64_t* return_addr = cpu_read_return_addr(frame_ptr);
        
        kprintf("#%d %p\n", frame_no, return_addr);
        frame_ptr = next_frame_ptr;
    }
}

void panic(const char* reason) {
    cpu_disable_interrupts();
    
    kprintf("\nKernel panic! Reason: %s\n", reason);
    print_stack_trace(CPU_READ_INSTRUCTION_PTR());
    kprintf("Panic occured, system halted.\n");

    cpu_halt_loop();
}
