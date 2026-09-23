#ifndef INTERRUPT_HANDLER_H
#define INTERRUPT_HANDLER_H

#include <stdint.h>

#include "arch/x86_64/idt.h"

typedef struct cpu_status_t* (*handler_fn)(struct cpu_status_t* regs, void* context);

struct interrupt_handler_entry {
    handler_fn handler;
    void* context;
};

void interrupt_set_handler(uint8_t vector, handler_fn handler, void* context);

#endif // INTERRUPT_HANDLER_H
