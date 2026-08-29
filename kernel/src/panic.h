#ifndef PANIC_H
#define PANIC_H

// Disable interrupts, print diagnostic information, and halt.
// Use for unrecoverable errors.
void panic(const char* reason);

#endif  // PANIC_H
