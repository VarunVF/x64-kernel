#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stddef.h>
#include <stdint.h>

#include "kprint.h"

#define PROCESS_NAME_MAX_LENGTH 64

struct cpu_status_t;

enum process_status {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_DEAD,
};

struct process {
    size_t pid;
    uint8_t* stack_base;
    enum process_status status;
    struct cpu_status_t* context;
    char name[PROCESS_NAME_MAX_LENGTH];
    struct process* next;
};

void scheduler_init(void);
void scheduler_start(void);
struct process* process_create(const char* name, void (*function)(void*), void* arg);
void process_exit(void);
struct cpu_status_t* schedule(struct cpu_status_t* context);
void scheduler_print_debug(const struct kprint_sink* sink);

#endif  // SCHEDULER_H
