#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stddef.h>

#include "arch/x86_64/idt.h"

#define PROCESS_NAME_MAX_LENGTH 64

enum process_status {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_DEAD,
};

struct process {
    size_t pid;
    enum process_status status;
    struct cpu_status_t* context;
    char name[PROCESS_NAME_MAX_LENGTH];
    struct process* next;
};

void scheduler_init();
struct process* process_create(const char* name, void (*function)(void*), void* arg);
struct cpu_status_t* schedule(struct cpu_status_t* context);
void scheduler_debug_print();

#endif  // SCHEDULER_H
