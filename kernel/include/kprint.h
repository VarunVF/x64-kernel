#ifndef KPRINT_H
#define KPRINT_H

typedef void (*putc_fn)(char c);

// Register an output backend for prints. Returns 0 on success
int kprint_add_output(putc_fn output);
int kprintf(const char* format, ...);

#endif  // KPRINT_H
