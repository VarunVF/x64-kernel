#include "panic.h"

#include "arch/cpu.h"
#include "kprint.h"

void panic(const char* reason) {
    cpu_disable_interrupts();
    
    kprintf("\nKernel panic! Reason: %s\n", reason);
    // TODO: Walk up the call stack and print function names / addresses
    
    kprintf("Panic occured, system halted.\n");
    cpu_halt_loop();
}
