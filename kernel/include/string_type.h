#ifndef STRING_TYPE_H
#define STRING_TYPE_H

#include <stdbool.h>
#include <stddef.h>

struct string {
    char* elements;     // pointer to heap buffer
    size_t size;        // string length, excluding NUL terminator
    size_t capacity;    // heap buffer size
};

size_t c_str_length(const char* c_str);
bool c_str_equal(const char* s1, const char* s2);

struct string string_init(const char* c_str);
void string_free(struct string* str);
void string_push(struct string* str, char c);
char string_pop(struct string* str);
void string_clear(struct string* str);

#endif  // STRING_TYPE_H
