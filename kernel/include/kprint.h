#ifndef KPRINT_H
#define KPRINT_H

typedef void (*putc_fn)(char c);

struct kprint_sink {
    void (*putc)(char c, void* context);
    void* context;
};

// Register an output backend for prints. Returns 0 on success
int kprint_add_output(putc_fn output);
int kfprintf(const struct kprint_sink* sink, const char* format, ...);
int kprintf(const char* format, ...);

#endif  // KPRINT_H
