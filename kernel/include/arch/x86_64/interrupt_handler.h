#ifndef INTERRUPT_HANDLER_H
#define INTERRUPT_HANDLER_H

#include <stdint.h>

typedef void (*handler_fn)(void* context);

struct interrupt_handler_entry {
    handler_fn handler;
    void* context;
};

void interrupt_set_handler(uint8_t vector, handler_fn handler, void* context);

#endif // INTERRUPT_HANDLER_H
