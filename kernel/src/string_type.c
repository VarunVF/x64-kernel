#include "string_type.h"

#include "kstring.h"
#include "memory/kmalloc.h"

#define STRING_GROWTH 2
#define STRING_MIN_CAPACITY 4

size_t c_str_length(const char* c_str) {
    size_t length = 0;
    while (*(c_str++) != '\0') {
        length++;
    }

    return length;
}

bool c_str_equal(const char* s1, const char* s2) {
    for (size_t i = 0;; i++) {
        if (s1[i] != s2[i]) {
            return false;
        } else if (s1[i] == '\0') {
            return true;
        }
    }
}

struct string string_init(const char* c_str) {
    size_t length = c_str_length(c_str);
    size_t capacity = (length > STRING_MIN_CAPACITY) ? length * STRING_GROWTH : STRING_MIN_CAPACITY;
    char* elements = kmalloc(capacity);
    memcpy(elements, c_str, length);
    elements[length] = '\0';

    return (struct string){
        elements,
        length,
        capacity
    };
}

void string_free(struct string* str) {
    kfree(str);
}

void string_push(struct string* str, char c) {
    if (str->size == str->capacity) {
        char* new_elements = kmalloc(str->capacity * STRING_GROWTH);
        memcpy(new_elements, str->elements, str->size);
        kfree(str->elements);
        str->elements = new_elements;
    }

    str->elements[str->size++] = c;
    str->elements[str->size] = '\0';
}

char string_pop(struct string* str) {
    char result = 0;
    if (str->size > 0) {
        result = str->elements[str->size];
        str->elements[str->size] = '\0';
        str->size -= 1;
    }

    return result;
}

void string_clear(struct string* str) {
    memset(str->elements, 0, str->size);
    str->size = 0;
}
