#ifndef MACROS_H
#define MACROS_H

#include "panic.h"

#define STRINGIFY(value) #value
#define TO_STRING(value) STRINGIFY(value)

#define KASSERT(condition)                                          \
    do {                                                            \
        if (!(condition)) {                                         \
            panic("Failed assertion in "                            \
            __FILE__ ":" TO_STRING(__LINE__) ": " #condition);      \
        }                                                           \
    } while (0)

#define KASSERT_MSG(condition, message)                             \
    do {                                                            \
        if (!(condition)) {                                         \
            panic(message "\nFailed assertion in "                  \
            __FILE__ ":" TO_STRING(__LINE__) ": " #condition);      \
        }                                                           \
    } while (0)

#endif  // MACROS_H
