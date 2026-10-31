#ifndef PANIC_H
#define PANIC_H

// Register a function to be called before a panic occurs
void panic_register(void (*callback)(void));

// Disable interrupts, print diagnostic information, and halt.
// Use for unrecoverable errors.
void panic(const char* reason);

#endif  // PANIC_H
