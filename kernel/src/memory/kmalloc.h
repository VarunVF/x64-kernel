#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>

// Initialise the heap. Returns zero on success
int kmalloc_init();
void kmalloc_print_debug();

void* kmalloc(size_t size);
void kfree(void* ptr);

#endif  // KMALLOC_H
