#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>

#include "kprint.h"

// Initialise the heap. Returns zero on success
int kmalloc_init();
void kmalloc_print_debug(const struct kprint_sink* sink);

void* kmalloc(size_t size);
void kfree(void* ptr);

#endif  // KMALLOC_H
